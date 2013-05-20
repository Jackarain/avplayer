#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "avplay.h"

/* 定义bool值 */
#ifndef _MSC_VER
enum bool_type
{
	FALSE, TRUE
};
#endif


/* 队列类型.	*/
#define QUEUE_PACKET			0
#define QUEUE_AVFRAME			1

#define IO_BUFFER_SIZE			32768
#define MAX_PKT_BUFFER_SIZE		5242880
#define MIN_AUDIO_BUFFER_SIZE	MAX_PKT_BUFFER_SIZE /* 327680 */
#define MIN_AV_FRAMES			5
#define AUDIO_BUFFER_MAX_SIZE	(AVCODEC_MAX_AUDIO_FRAME_SIZE * 2)
#define AVDECODE_BUFFER_SIZE	2
#define DEVIATION				6

#define AV_SYNC_THRESHOLD		0.01f
#define AV_NOSYNC_THRESHOLD		10.0f
#define AUDIO_DIFF_AVG_NB		20

#define SEEKING_FLAG			-1
#define NOSEEKING_FLAG			0


/* INT64最大最小取值范围. */
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL - 1)
#endif
#ifndef INT64_MAX
#define INT64_MAX (9223372036854775807LL)
#endif

/* rgb和yuv互换. */
#define _r(c) ((c) & 0xFF)
#define _g(c) (((c) >> 8) & 0xFF)
#define _b(c) (((c) >> 16) & 0xFF)
#define _a(c) ((c) >> 24)

#define rgba2y(c)  ( (( 263*_r(c) + 516*_g(c) + 100*_b(c)) >> 10) + 16  )
#define rgba2u(c)  ( ((-152*_r(c) - 298*_g(c) + 450*_b(c)) >> 10) + 128 )
#define rgba2v(c)  ( (( 450*_r(c) - 376*_g(c) -  73*_b(c)) >> 10) + 128 )

#define MAX_TRANS   255
#define TRANS_BITS  8


typedef struct AVFrameList
{
	AVFrame pkt;
	struct AVFrameList *next;
} AVFrameList;

/* 队列操作. */
static void queue_init(av_queue *q);
static void queue_flush(av_queue *q);
static void queue_end(av_queue *q);

/* 入队出队列操作. */
static int get_queue(av_queue *q, void *p);
static int put_queue(av_queue *q, void *p);
static void chk_queue(avplay *play, av_queue *q, int size);

/* ffmpeg相关操作函数. */
static int stream_index(enum AVMediaType type, AVFormatContext *ctx);
static int open_decoder(AVCodecContext *ctx);
static int open_decoder2(AVCodecContext **context, enum AVCodecID codec_id);

/* 读取数据线程.	*/
static void* read_pkt_thrd(void *param);
static void* video_dec_thrd(void *param);
static void* audio_dec_thrd(void *param);

/* 渲染线程. */
static void* audio_render_thrd(void *param);
static void* video_render_thrd(void *param);

/* 视频帧复制. */
static void video_copy(avplay *play, AVFrame *dst, AVFrame *src);
static void audio_copy(avplay *play, AVFrame *dst, AVFrame *src);

/* 更新视频pts. */
static void update_video_pts(avplay *play, double pts, int64_t pos);

/* 时钟函数. */
static double video_clock(avplay *play);
static double audio_clock(avplay *play);
static double external_clock(avplay *play);
static double master_clock(avplay *play);

/* 读写数据函数. */
static int read_packet(void *opaque, uint8_t *buf, int buf_size);
static int write_packet(void *opaque, uint8_t *buf, int buf_size);
static int64_t seek_packet(void *opaque, int64_t offset, int whence);

AVPacket flush_pkt;
AVFrame flush_frm;

static
void
queue_init(av_queue *q)
{
	q->abort_request = 0;
	q->m_first_pkt = q->m_last_pkt = NULL;
	q->m_size = 0;

	pthread_mutex_init(&q->m_mutex, NULL);
	pthread_cond_init(&q->m_cond, NULL);

	if (q->m_type == QUEUE_PACKET)
		put_queue(q, (void*) &flush_pkt);
	else if (q->m_type == QUEUE_AVFRAME)
		put_queue(q, (void*) &flush_frm);
}

static
void queue_flush(av_queue *q)
{
	if (q->m_size == 0)
		return;

	if (q->m_type == QUEUE_PACKET)
	{
		AVPacketList *pkt, *pkt1;
		pthread_mutex_lock(&q->m_mutex);
		for (pkt = q->m_first_pkt; pkt != NULL; pkt = pkt1)
		{
			pkt1 = pkt->next;
			if (pkt->pkt.data != flush_pkt.data)
				av_free_packet(&pkt->pkt);
			av_freep(&pkt);
		}
		q->m_last_pkt = NULL;
		q->m_first_pkt = NULL;
		q->m_size = 0;
		pthread_mutex_unlock(&q->m_mutex);
	}
	else if (q->m_type == QUEUE_AVFRAME)
	{
		AVFrameList *pkt, *pkt1;
		pthread_mutex_lock(&q->m_mutex);
		for (pkt = q->m_first_pkt; pkt != NULL; pkt = pkt1)
		{
			pkt1 = pkt->next;
			if (pkt->pkt.data[0] != flush_frm.data[0])
				av_free(pkt->pkt.data[0]);
			av_freep(&pkt);
		}
		q->m_last_pkt = NULL;
		q->m_first_pkt = NULL;
		q->m_size = 0;
		pthread_mutex_unlock(&q->m_mutex);
	}
}

static
void queue_end(av_queue *q)
{
	queue_flush(q);
#ifdef WIN32
	if (q->m_cond)
		pthread_cond_destroy(&q->m_cond);
	if (q->m_mutex)
		pthread_mutex_destroy(&q->m_mutex);
#else
	pthread_cond_destroy(&q->m_cond);
	pthread_mutex_destroy(&q->m_mutex);
#endif
}

#define PRIV_OUT_QUEUE \
	pthread_mutex_lock(&q->m_mutex); \
	for (;;) \
	{ \
		if (q->abort_request) \
		{ \
			ret = -1; \
			break; \
		} \
		pkt1 = q->m_first_pkt; \
		if (pkt1) \
		{ \
			q->m_first_pkt = pkt1->next; \
			if (!q->m_first_pkt) \
				q->m_last_pkt = NULL; \
			q->m_size--; \
			*pkt = pkt1->pkt; \
			av_free(pkt1); \
			ret = 1; \
			break; \
		} \
		else \
		{ \
			pthread_cond_wait(&q->m_cond, &q->m_mutex); \
		} \
	} \
	pthread_mutex_unlock(&q->m_mutex);

static
int get_queue(av_queue *q, void *p)
{
	if (q->m_type == QUEUE_PACKET)
	{
		AVPacketList *pkt1;
		AVPacket *pkt = (AVPacket*) p;
		int ret;
		PRIV_OUT_QUEUE
		return ret;
	}
	else if (q->m_type == QUEUE_AVFRAME)
	{
		AVFrameList *pkt1;
		AVFrame *pkt = (AVFrame*) p;
		int ret;
		PRIV_OUT_QUEUE
		return ret;
	}
	return -1;
}

#define PRIV_PUT_QUEUE(type) \
	pkt1 = av_malloc(sizeof(type)); \
	if (!pkt1) \
		return -1; \
	pkt1->pkt = *pkt; \
	pkt1->next = NULL; \
	\
	pthread_mutex_lock(&q->m_mutex); \
	if (!q->m_last_pkt) \
		q->m_first_pkt = pkt1; \
	else \
		((type*) q->m_last_pkt)->next = pkt1; \
	q->m_last_pkt = pkt1; \
	q->m_size++; \
	pthread_cond_signal(&q->m_cond); \
	pthread_mutex_unlock(&q->m_mutex);

static
int put_queue(av_queue *q, void *p)
{
	if (q->m_type == QUEUE_PACKET)
	{
		AVPacketList *pkt1;
		AVPacket *pkt = (AVPacket*) p;
		/* duplicate the packet */
		if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
			return -1;
      PRIV_PUT_QUEUE(AVPacketList)
		return 0;
	}
	else if (q->m_type == QUEUE_AVFRAME)
	{
		AVFrameList *pkt1;
		AVFrame *pkt = (AVFrame*) p;
      PRIV_PUT_QUEUE(AVFrameList)
		return 0;
	}

	return -1;
}

static
media_info* find_media_info(source_context *sc, int index)
{
	bt_source_info *bt_info = &sc->info.bt;

	// 检查参数有效性.
	if (sc->type != MEDIA_TYPE_BT || index < 0 || index > bt_info->info_size)
		return NULL;

	// 返回对应的media_info信息.
	return &bt_info->info[index];
}

static
int stream_index(enum AVMediaType type, AVFormatContext *ctx)
{
	unsigned int i;

	for (i = 0; (unsigned int) i < ctx->nb_streams; i++)
		if (ctx->streams[i]->codec->codec_type == type)
			return i;
	return -1;
}

static
int open_decoder(AVCodecContext *ctx)
{
   int ret = 0;
   AVCodec *codec = NULL;

   /* 查找解码器. */
   codec = avcodec_find_decoder(ctx->codec_id);
   if (!codec)
      return -1;

   /* 打开解码器.	*/
   ret = avcodec_open2(ctx, codec, NULL);
   if (ret != 0)
      return ret;

   return ret;
}

static
int open_decoder2(AVCodecContext **context, enum AVCodecID codec_id)
{
	int ret = 0;
	AVCodec *codec = NULL;

	/* 查找解码器. */
	codec = avcodec_find_decoder(codec_id);
	if (!codec)
		return -1;

	/*
	*context = avcodec_alloc_context3(codec);
	if (avcodec_get_context_defaults3(*context, codec) < 0)
		return -1;
	*/

	/* 打开解码器.	*/
	if (avcodec_open2(*context, codec, NULL) < 0)
	{
		avcodec_close(*context);
		av_free(*context);
		*context = NULL;
		return -1;
	}

	return 0;
}

source_context* alloc_media_source(int type, const char *data, int len, int64_t size)
{
	/* 分配一个source_context结构. */
	struct source_context *ptr = calloc(1, sizeof(source_context));

	/* 参数赋值. */
	ptr->type = type;

	if (type == MEDIA_TYPE_FILE)
	{
		file_source_info *file_info = &ptr->info.file;
		strcpy(file_info->file_name, (const char*)data);	/* 保存文件名. */
	}
	else if (type == MEDIA_TYPE_BT)
	{
		bt_source_info *bt_info = &ptr->info.bt;
		bt_info->torrent_data = (char*)malloc(len);
		memcpy(bt_info->torrent_data, data, len);			/* 保存torrent数据. */
		bt_info->torrent_length = len;
	}
	else if (type == MEDIA_TYPE_YK)
	{
		yk_source_info *yk_info = &ptr->info.yk;
		strcpy(yk_info->url, data);							/* 保存url. */
	}
	else if (type == MEDIA_TYPE_HTTP)
	{
		http_source_info *http_info = &ptr->info.http;
		strcpy(http_info->url, data);						/* 保存url. */
	}
	else if (type == MEDIA_TYPE_RTSP)
	{
		rtsp_source_info *rtsp_info = &ptr->info.rtsp;
		strcpy(rtsp_info->url, data);						/* 保存url. */
	}
	else
	{
		assert(0);
	}

	return ptr;
}

/*
void set_download_path(avplay *play, const char *save_path)
{
	if (play->m_source_ctx->type == MEDIA_TYPE_BT)
	{
		bt_source_info *bt_info = &play->m_source_ctx->info.bt;
		strcpy(bt_info->save_path, save_path);
	}
	else if	(play->m_source_ctx->type == MEDIA_TYPE_YK)
	{
		yk_source_info *yk_info = &play->m_source_ctx->info.yk;
		strcpy(yk_info->save_path, save_path);
	}
}
*/

/*
void set_youku_type(avplay *play, int type)
{
	play->m_source_ctx->info.yk.type = type;
}
*/

void free_media_source(source_context *ctx)
{
	/* 释放priv指针. */
	if (ctx->priv)
		ctx->destory(ctx);

	/* 释放bt类型中的torrent内存. */
	if (ctx->type == MEDIA_TYPE_BT)
	{
		free(ctx->info.bt.torrent_data);
	}

	/* 最后释放整个source_context. */
	free(ctx);
}

avplay* alloc_avplay_context()
{
	struct avplay *ptr = calloc(1, sizeof(avplay));
	return ptr;
}

void free_avplay_context(avplay *ctx)
{
	free(ctx);
}

ao_context* alloc_audio_render()
{
	ao_context *ptr = calloc(1, sizeof(ao_context));
	return ptr;
}

void free_audio_render(ao_context *ctx)
{
	if (ctx->priv)
		ctx->destory_audio(ctx);
	free(ctx);
}

vo_context* alloc_video_render(void *user_data)
{
	struct vo_context *ptr = calloc(1, sizeof(vo_context));
	ptr->user_data = user_data;
	return ptr;
}

void free_video_render(vo_context *ctx)
{
	if (ctx->priv)
		ctx->destory_video(ctx);
	free(ctx);
}

demux_context* alloc_demux_context()
{
	return calloc(1, sizeof(demux_context));
}

void free_demux_context(demux_context *ctx)
{
	if (ctx)
		ctx->destory(ctx);
	free(ctx);
}

int initialize(avplay *play, const char *file_name, int source_type, demux_context *dc)
{
	if (!play || !file_name || !dc)
		return -1;

	av_register_all();
	avcodec_register_all();
	avformat_network_init();

	/* 置0. */
	memset(play, 0, sizeof(avplay));

	/* 保存demux_context. */
	play->m_demux_context = dc;

	/* 设置source type. */
	play->m_generic_info = &dc->info.generic;
	dc->type = source_type;
	strcpy(play->m_generic_info->file_name, file_name);	/* 保存文件名到generic_info.file_name, 以便其在初始化demux时使用. */

	/* 初始化demux的source. */
	if (dc->init_demux(dc) == -1)
		goto FAILED_FLG;

	/* 得到audio和video在streams中的index.	*/
	play->m_video_index = 
		dc->stream_index(dc, AVMEDIA_TYPE_VIDEO);
	play->m_audio_index = 
		dc->stream_index(dc, AVMEDIA_TYPE_AUDIO);

	play->m_base_info = &dc->base_info;

	if (play->m_video_index == -1 && play->m_audio_index == -1)
		goto FAILED_FLG;

	/* 打开解码器. */
	if (play->m_audio_index != -1)
	{
		AVCodec *codec = NULL;
		/* 查找解码器. */
		codec = avcodec_find_decoder(play->m_base_info->audio_codec->codec_id);
		play->m_audio_ctx = avcodec_alloc_context3(codec);
		avcodec_copy_context(play->m_audio_ctx, play->m_base_info->audio_codec);
		if (open_decoder2(&play->m_audio_ctx, dc->query_avcodec_id(dc, play->m_audio_index)) != 0)
			goto FAILED_FLG;
	}
	if (play->m_video_index != -1)
	{
		AVCodec *codec = NULL;
		/* 查找解码器. */
		codec = avcodec_find_decoder(play->m_base_info->video_codec->codec_id);
		play->m_video_ctx = avcodec_alloc_context3(codec);
		avcodec_copy_context(play->m_video_ctx, play->m_base_info->video_codec);
		if (open_decoder2(&play->m_video_ctx, dc->query_avcodec_id(dc, play->m_video_index)) != 0)
			goto FAILED_FLG;
	}

	/* 默认同步到音频.	*/
	play->m_av_sync_type = AV_SYNC_AUDIO_MASTER;
	play->m_abort = TRUE;

	/* 初始化各变量. */
	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t*)"FLUSH";
	flush_frm.data[0] = (uint8_t*)"FLUSH";
	play->m_abort = 0;

	/* 初始化队列. */
	if (play->m_audio_index != -1)
	{
		play->m_audio_q.m_type = QUEUE_PACKET;
		queue_init(&play->m_audio_q);
		play->m_audio_dq.m_type = QUEUE_AVFRAME;
		queue_init(&play->m_audio_dq);
	}
	if (play->m_video_index != -1)
	{
		play->m_video_q.m_type = QUEUE_PACKET;
		queue_init(&play->m_video_q);
		play->m_video_dq.m_type = QUEUE_AVFRAME;
		queue_init(&play->m_video_dq);
	}

	/* 初始化读取文件数据缓冲计数mutex. */
	pthread_mutex_init(&play->m_buf_size_mtx, NULL);

	/* 打开各线程.	*/
	return 0;

FAILED_FLG:
	if (play->m_demux_context)
	{
		play->m_demux_context->destory(play->m_demux_context);	// ? free_demux_context(play->m_demux_context);
	}

	return -1;
}

int av_start(avplay *play, double fact, int index)
{
	pthread_attr_t attr;
	int ret;

	/* 保存正在播放的索引号. */
	play->m_current_play_index = index;

	/* 计算时长. */
	play->m_duration = (double)play->m_demux_context->base_info.duration / AV_TIME_BASE;
	/* 保存起始播放时间. */
	play->m_start_time = fact;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* 创建线程. */
	ret = pthread_create(&play->m_read_pkt_thrd, &attr, read_pkt_thrd,
		(void*) play);
	if (ret)
	{
		printf("ERROR; return code from pthread_create() is %d\n", ret);
		return ret;
	}
	if (play->m_audio_index != -1)
	{
		/*play->m_audio_current_pts_drift = av_gettime() / 1000000.0f;*/
		ret = pthread_create(&play->m_audio_dec_thrd, &attr, audio_dec_thrd,
			(void*) play);
		if (ret)
		{
			printf("ERROR; return code from pthread_create() is %d\n", ret);
			return ret;
		}
	}
	if (play->m_video_index != -1)
	{
		ret = pthread_create(&play->m_video_dec_thrd, &attr, video_dec_thrd,
			(void*) play);
		if (ret)
		{
			printf("ERROR; return code from pthread_create() is %d\n", ret);
			return ret;
		}
	}
	if (play->m_audio_index != -1)
	{
		ret = pthread_create(&play->m_audio_render_thrd, &attr, audio_render_thrd,
			(void*) play);
		if (ret)
		{
			printf("ERROR; return code from pthread_create() is %d\n", ret);
			return ret;
		}
	}
	if (play->m_video_index != -1)
	{
		ret = pthread_create(&play->m_video_render_thrd, &attr, video_render_thrd,
			(void*) play);
		if (ret)
		{
			printf("ERROR; return code from pthread_create() is %d\n", ret);
			return ret;
		}
	}
	play->m_play_status = playing;

	return 0;
}

void configure(avplay *play, void *param, int type)
{
	switch (type)
	{
	case AUDIO_RENDER:
		{
			if (play->m_ao_ctx && play->m_ao_ctx->priv)
				free_audio_render(play->m_ao_ctx);
			play->m_ao_ctx = (ao_context*)param;
		}
		break;
	case VIDEO_RENDER:
		{
			if (play->m_vo_ctx && play->m_vo_ctx->priv)
				free_video_render(play->m_vo_ctx);
			play->m_vo_ctx = (vo_context*)param;
		}
		break;
	case MEDIA_DEMUX:
		{
			if (play->m_demux_context && play->m_demux_context->priv)
				play->m_demux_context->destory(play->m_demux_context);
			free_demux_context(play->m_demux_context);
			play->m_demux_context = (demux_context*)param;
		}
		break;
	default:
		assert(0);
	}
}

void enable_calc_frame_rate(avplay *play)
{
	play->m_enable_calc_frame_rate = 1;
}

void enable_calc_bit_rate(avplay *play)
{
	play->m_enable_calc_video_bite = 1;
}

int current_bit_rate(avplay *play)
{
	return play->m_real_bit_rate;
}

int current_frame_rate(avplay *play)
{
	return play->m_real_frame_rate;
}

void wait_for_completion(avplay *play)
{
	while (play->m_play_status == playing ||
		play->m_play_status == paused)
	{
		av_usleep(100000);
	}
}

void wait_for_threads(avplay *play)
{
	void *status = NULL;
	pthread_join(play->m_read_pkt_thrd, &status);
	if (play->m_audio_index != -1)
		pthread_join(play->m_audio_dec_thrd, &status);
	if (play->m_video_index != -1)
		pthread_join(play->m_video_dec_thrd, &status);
	if (play->m_audio_index != -1)
		pthread_join(play->m_audio_render_thrd, &status);
	if (play->m_video_index != -1)
		pthread_join(play->m_video_render_thrd, &status);
}

void av_stop(avplay *play)
{
	play->m_abort = TRUE;
	if (play->m_demux_context)
		play->m_demux_context->abort = TRUE;

	/* 通知各线程退出. */
	play->m_audio_q.abort_request = TRUE;
	pthread_cond_signal(&play->m_audio_q.m_cond);
	play->m_video_q.abort_request = TRUE;
	pthread_cond_signal(&play->m_video_q.m_cond);
	play->m_audio_dq.abort_request = TRUE;
	pthread_cond_signal(&play->m_audio_dq.m_cond);
	play->m_video_dq.abort_request = TRUE;
	pthread_cond_signal(&play->m_video_dq.m_cond);

	/* 先等线程退出, 再释放资源. */
	wait_for_threads(play);

	queue_end(&play->m_audio_q);
	queue_end(&play->m_video_q);
	queue_end(&play->m_audio_dq);
	queue_end(&play->m_video_dq);

	/* 关闭解码器以及渲染器. */
	if (play->m_demux_context)
	{
		if (play->m_audio_ctx)
		{
			avcodec_close(play->m_audio_ctx);
			av_free(play->m_audio_ctx);
			play->m_audio_ctx = NULL;
		}
		if (play->m_video_ctx)
		{
			avcodec_close(play->m_video_ctx);
			av_free(play->m_video_ctx);
			play->m_video_ctx = NULL;
		}
	}
	if (play->m_swr_ctx)
		swr_free(&play->m_swr_ctx);
	if (play->m_resample_ctx)
		audio_resample_close(play->m_resample_ctx);
#ifdef WIN32
	if (play->m_buf_size_mtx)
#endif
	pthread_mutex_destroy(&play->m_buf_size_mtx);
	if (play->m_ao_ctx)
	{
		free_audio_render(play->m_ao_ctx);
		play->m_ao_ctx = NULL;
		play->m_ao_inited = 0;
	}
	if (play->m_vo_ctx)
	{
		free_video_render(play->m_vo_ctx);
		play->m_vo_ctx = NULL;
	}
	/* 更改播放状态. */
	play->m_play_status = stoped;

	avformat_network_deinit();
}

void av_pause(avplay *play)
{
	/* 一直等待为渲染状态时才控制为暂停, 原因是这样可以在暂停时继续渲染而不至于黑屏. */
	while (!play->m_rendering)
		av_usleep(1000);
	/* 更改播放状态. */
	play->m_play_status = paused;
}

void av_resume(avplay *play)
{
	/* 更改播放状态. */
	play->m_play_status = playing;
}

void av_seek(avplay *play, double fact)
{
	/* 正在seek中, 只保存当前sec, 在seek完成后, 再seek. */
	if (play->m_seeking == SEEKING_FLAG || 
		(play->m_seeking > NOSEEKING_FLAG && play->m_seek_req))
	{
		play->m_seeking = fact * 1000;
		return ;
	}

	/* 正常情况下的seek. */
	if (fabs(play->m_duration - 0) < DBL_EPSILON)
	{
		int64_t size = play->m_demux_context->base_info.file_size;
		if (!play->m_seek_req)
		{
			play->m_seek_req = 1;
			play->m_seeking = SEEKING_FLAG;
			play->m_seek_pos = fact * size;
			play->m_seek_rel = 0;
			play->m_seek_flags &= ~AVSEEK_FLAG_BYTE;
			play->m_seek_flags |= AVSEEK_FLAG_BYTE;
		}
	}
	else
	{
		if (!play->m_seek_req)
		{
			play->m_seek_req = 1;
			play->m_seeking = SEEKING_FLAG;
			play->m_seek_pos = fact * play->m_duration;
			play->m_seek_rel = 0;
			play->m_seek_flags &= ~AVSEEK_FLAG_BYTE;
			/* play->m_seek_flags |= AVSEEK_FLAG_BYTE; */
		}
	}
}

int av_volume(avplay *play, double l, double r)
{
	if (play->m_ao_inited)
	{
		play->m_ao_ctx->audio_control(play->m_ao_ctx, l, r);
		return 0;
	}
	return -1;
}

void av_mute_set(avplay *play, int s)
{
	play->m_ao_ctx->mute_set(play->m_ao_ctx, s);
}

double av_curr_play_time(avplay *play)
{
	return master_clock(play);
}

double av_duration(avplay *play)
{
	return play->m_duration;
}

void av_destory(avplay *play)
{
	/* 如果正在播放, 则关闭播放. */
	if (play->m_play_status != stoped && play->m_play_status != inited)
	{
		play->m_abort = TRUE;
		if (play->m_demux_context)
			play->m_demux_context->abort = TRUE;

		/* 关闭demux和source. */
		if (play->m_demux_context && play->m_demux_context->priv)
			play->m_demux_context->destory(play->m_demux_context);
		av_stop(play);
	}

	free(play);
}

static
void audio_copy(avplay *play, AVFrame *dst, AVFrame* src)
{
	int nb_sample;
	int dst_buf_size;
	int out_channels;
	int bytes_per_sample = 0;

	dst->linesize[0] = src->linesize[0];
	*dst = *src;
	dst->data[0] = NULL;
	dst->type = 0;
	/* 备注: FFMIN(play->m_audio_ctx->channels, 2); 会有问题, 因为swr_alloc_set_opts的out_channel_layout参数. */
	out_channels = play->m_audio_ctx->channels;
	bytes_per_sample = av_get_bytes_per_sample(play->m_audio_ctx->sample_fmt);
	/* 备注: 由于 src->linesize[0] 可能是错误的, 所以计算得到的nb_sample会不正确, 直接使用src->nb_samples即可. */
	nb_sample = src->nb_samples;/* src->linesize[0] / play->m_audio_ctx->channels / bytes_per_sample; */
	bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	dst_buf_size = nb_sample * bytes_per_sample * out_channels;
	dst->data[0] = (uint8_t*) av_malloc(dst_buf_size);
	assert(dst->data[0]);
	avcodec_fill_audio_frame(dst, out_channels, AV_SAMPLE_FMT_S16, dst->data[0], dst_buf_size, 0);

	/* 重采样到AV_SAMPLE_FMT_S16格式. */
	if (play->m_audio_ctx->sample_fmt != AV_SAMPLE_FMT_S16)
	{
		if (!play->m_swr_ctx)
		{
			uint64_t in_channel_layout = av_get_default_channel_layout(play->m_audio_ctx->channels);
			uint64_t out_channel_layout = av_get_default_channel_layout(out_channels);
			play->m_swr_ctx = swr_alloc_set_opts(NULL,
				out_channel_layout, AV_SAMPLE_FMT_S16, play->m_audio_ctx->sample_rate,
				in_channel_layout, play->m_audio_ctx->sample_fmt, play->m_audio_ctx->sample_rate,
				0, NULL);
			swr_init(play->m_swr_ctx);
		}

		if (play->m_swr_ctx)
		{
			int ret, out_count;
			out_count = dst_buf_size / out_channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
			ret = swr_convert(play->m_swr_ctx, dst->data, out_count, src->data, nb_sample);
			if (ret < 0)
				assert(0);
			src->linesize[0] = dst->linesize[0] = ret * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * out_channels;
			memcpy(src->data[0], dst->data[0], src->linesize[0]);
		}
	}

	/* 重采样到双声道. */
	if (play->m_audio_ctx->channels > 2)
	{
		if (!play->m_resample_ctx)
		{
			play->m_resample_ctx = av_audio_resample_init(
					FFMIN(2, play->m_audio_ctx->channels),
					play->m_audio_ctx->channels, play->m_audio_ctx->sample_rate,
					play->m_audio_ctx->sample_rate, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16,
					16, 10, 0, 0.f);
		}

		if (play->m_resample_ctx)
		{
			int samples = src->linesize[0] / (av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * play->m_audio_ctx->channels);
			dst->linesize[0] = audio_resample(play->m_resample_ctx,
				(short *) dst->data[0], (short *) src->data[0], samples) * 4;
		}
	}
	else
	{
		dst->linesize[0] = dst->linesize[0];
		memcpy(dst->data[0], src->data[0], dst->linesize[0]);
	}
}

static
void video_copy(avplay *play, AVFrame *dst, AVFrame *src)
{
	uint8_t *buffer;
	int len = avpicture_get_size(PIX_FMT_YUV420P, play->m_video_ctx->width,
			play->m_video_ctx->height);
	*dst = *src;
	buffer = (uint8_t*) av_malloc(len);
	assert(buffer);

	avpicture_fill((AVPicture*) &(*dst), buffer, PIX_FMT_YUV420P,
			play->m_video_ctx->width, play->m_video_ctx->height);

	play->m_swsctx = sws_getContext(play->m_video_ctx->width,
			play->m_video_ctx->height, play->m_video_ctx->pix_fmt,
			play->m_video_ctx->width, play->m_video_ctx->height,
			PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	sws_scale(play->m_swsctx, src->data, src->linesize, 0,
			play->m_video_ctx->height, dst->data, dst->linesize);

	sws_freeContext(play->m_swsctx);
}

static
void update_video_pts(avplay *play, double pts, int64_t pos)
{
   double time = (double)av_gettime() / 1000000.0;
   /* update current video pts */
   play->m_video_current_pts = pts;
   play->m_video_current_pts_drift = play->m_video_current_pts - time;
   play->m_video_current_pos = pos;
   play->m_frame_last_pts = pts;
}

static
double audio_clock(avplay *play)
{
	double pts;
	int hw_buf_size, bytes_per_sec;
	pts = play->m_audio_clock;
	hw_buf_size = play->m_audio_buf_size - play->m_audio_buf_index;
	bytes_per_sec = 0;
	if (play->m_base_info->has_audio != -1)
	{
		/* 固定为2通道, 采样格式为AV_SAMPLE_FMT_S16.*/
		bytes_per_sec = play->m_base_info->sample_rate * 2 * FFMIN(play->m_base_info->channels, 2);
	}

	if (fabs(play->m_audio_current_pts_drift) <= DBL_EPSILON)
	{
		if (fabs(play->m_start_time) > DBL_EPSILON)
			play->m_audio_current_pts_drift = pts - play->m_audio_current_pts_last;
		else
			play->m_audio_current_pts_drift = pts;
	}

	if (bytes_per_sec)
		pts -= (double) hw_buf_size / bytes_per_sec;
	return pts - play->m_audio_current_pts_drift;
}

static
double video_clock(avplay *play)
{
	if (play->m_play_status == paused)
		return play->m_video_current_pts;
	return play->m_video_current_pts_drift + (double)av_gettime() / 1000000.0f;
}

static
double external_clock(avplay *play)
{
	int64_t ti;
	ti = (double)av_gettime();
	return play->m_external_clock + ((ti - play->m_external_clock_time) * 1e-6);
}

static
double master_clock(avplay *play)
{
	double val;

	if (play->m_av_sync_type == AV_SYNC_VIDEO_MASTER)
	{
		if (play->m_base_info->has_video != -1)
			val = video_clock(play);
		else
			val = audio_clock(play);
	}
	else if (play->m_av_sync_type == AV_SYNC_AUDIO_MASTER)
	{
		if (play->m_base_info->has_audio != -1)
			val = audio_clock(play);
		else
			val = video_clock(play);
	}
	else
	{
		val = external_clock(play);
	}

	return val;
}

static
void chk_queue(avplay *play, av_queue *q, int size)
{
	/* 防止内存过大.	*/
	while (1)
	{
		pthread_mutex_lock(&q->m_mutex);
		if (q->m_size >= size && !play->m_abort)
		{
			pthread_mutex_unlock(&q->m_mutex);
			av_usleep(4000);
		}
		else
		{
			pthread_mutex_unlock(&q->m_mutex);
			return;
		}
	}
}

static
void* read_pkt_thrd(void *param)
{
	AVPacket packet = { 0 };
	int ret;
	avplay *play = (avplay*) param;
	demux_context *dc = play->m_demux_context;
	int last_paused = play->m_play_status;

	// 起始时间不等于0, 则先seek至指定时间.
	if (fabs(play->m_start_time) > 1.0e-6)
	{
		av_seek(play, play->m_start_time);
	}

	play->m_buffering = 0.0f;
	play->m_real_bit_rate = 0;

	for (; !play->m_abort;)
	{
		/* Initialize optional fields of a packet with default values.	*/
		av_init_packet(&packet);

		/* 如果暂定状态改变. */
		if (last_paused != play->m_play_status)
		{
			last_paused = play->m_play_status;
			if (play->m_play_status == playing)
				dc->read_play(dc);
			if (play->m_play_status == paused)
				dc->read_pause(dc);
		}

		/* 如果seek未完成又来了新的seek请求. */
		if (play->m_seeking > NOSEEKING_FLAG)
			av_seek(play, (double)play->m_seeking / 1000.0f);

		if (play->m_seek_req)
		{
			int64_t seek_target = play->m_seek_pos * AV_TIME_BASE;
			int64_t seek_min    = /*play->m_seek_rel > 0 ? seek_target - play->m_seek_rel + 2:*/ INT64_MIN;
			int64_t seek_max    = /*play->m_seek_rel < 0 ? seek_target - play->m_seek_rel - 2:*/ INT64_MAX;
			int seek_flags = 0 & (~AVSEEK_FLAG_BYTE);
			int ns, hh, mm, ss;
			int tns, thh, tmm, tss;
			double frac = (double)play->m_seek_pos / play->m_duration;

			tns = play->m_duration;
			thh = tns / 3600.0f;
			tmm = (tns % 3600) / 60.0f;
			tss = tns % 60;

			ns = frac * tns;
			hh = ns / 3600.0f;
			mm = (ns % 3600) / 60.0f;
			ss = ns % 60;

			seek_target = frac * play->m_duration * AV_TIME_BASE;
			if (dc->base_info.start_time != AV_NOPTS_VALUE)
				seek_target += dc->base_info.start_time;

			if (play->m_audio_index >= 0)
			{
				queue_flush(&play->m_audio_q);
				put_queue(&play->m_audio_q, &flush_pkt);
			}
			if (play->m_video_index >= 0)
			{
				queue_flush(&play->m_video_q);
				put_queue(&play->m_video_q, &flush_pkt);
			}
			play->m_pkt_buffer_size = 0;

			ret = dc->seek_packet(dc, seek_target);
			/* ret = avformat_seek_file(play->m_format_ctx, -1, seek_min, seek_target, seek_max, seek_flags); */
			if (ret < 0)
			{
				fprintf(stderr, "%s: error while seeking\n", play->m_generic_info->file_name);
			}

			printf("Seek to %2.0f%% (%02d:%02d:%02d) of total duration (%02d:%02d:%02d)\n",
				frac * 100, hh, mm, ss, thh, tmm, tss);

			play->m_seek_req = 0;
		}

		/* 缓冲读满, 在这休眠让出cpu.	*/
		while (play->m_pkt_buffer_size > MAX_PKT_BUFFER_SIZE && !play->m_abort && !play->m_seek_req)
			av_usleep(32000);
		if (play->m_abort)
			break;

		/* Return 0 if OK, < 0 on error or end of file.	*/
		ret = dc->read_packet(dc, &packet);
		if (ret < 0)
		{
			if (play->m_video_q.m_size == 0 && play->m_audio_q.m_size == 0 &&
				play->m_video_dq.m_size == 0 && play->m_audio_dq.m_size == 0)
				play->m_play_status = completed;
			av_usleep(100000);
			continue;
		}

		if (play->m_play_status == completed)
			play->m_play_status = playing;

		/* 更新缓冲字节数.	*/
		if (packet.stream_index == play->m_video_index || packet.stream_index == play->m_audio_index)
		{
			pthread_mutex_lock(&play->m_buf_size_mtx);
			play->m_pkt_buffer_size += packet.size;
			play->m_buffering = (double)play->m_pkt_buffer_size / (double)MAX_PKT_BUFFER_SIZE;
			pthread_mutex_unlock(&play->m_buf_size_mtx);
		}

		/* 在这里计算码率.	*/
		if (play->m_enable_calc_video_bite)
		{
			int current_time = 0;
			/* 计算时间是否足够一秒钟. */
			if (play->m_last_vb_time == 0)
				play->m_last_vb_time = (double)av_gettime() / 1000000.0f;
			current_time = (double)av_gettime() / 1000000.0f;
			if (current_time - play->m_last_vb_time >= 1)
			{
				play->m_last_vb_time = current_time;
				if (++play->m_vb_index == MAX_CALC_SEC)
					play->m_vb_index = 0;

				/* 计算bit/second. */
				do
				{
					int sum = 0;
					int i = 0;
					for (; i < MAX_CALC_SEC; i++)
						sum += play->m_read_bytes[i];
					play->m_real_bit_rate = ((double)sum / (double)MAX_CALC_SEC) * 8.0f / 1024.0f;
				} while (0);
				/* 清空. */
				play->m_read_bytes[play->m_vb_index] = 0;
			}

			/* 更新读取字节数. */
			play->m_read_bytes[play->m_vb_index] += packet.size;
		}

		av_dup_packet(&packet);

		if (packet.stream_index == play->m_video_index)
			put_queue(&play->m_video_q, &packet);

		if (packet.stream_index == play->m_audio_index)
			put_queue(&play->m_audio_q, &packet);
	}

	/* 设置为退出状态.	*/
	play->m_abort = TRUE;
	return NULL;
}

static
void* audio_dec_thrd(void *param)
{
	AVPacket pkt, pkt2;
	int ret, n;
	AVFrame avframe = { 0 }, avcopy = { 0 };
	avplay *play = (avplay*) param;
	int64_t v_start_time = 0;
	int64_t a_start_time = 0;

	if (play->m_base_info->has_video != -1 && play->m_base_info->has_audio != -1)
	{
		v_start_time = play->m_base_info->video_start_time;
		a_start_time = play->m_base_info->audio_start_time;
	}

	for (; !play->m_abort;)
	{
		av_init_packet(&pkt);
		while (play->m_play_status == paused && !play->m_abort)
			av_usleep(10000);
		ret = get_queue(&play->m_audio_q, &pkt);
		if (ret != -1)
		{
			if (pkt.data == flush_pkt.data)
			{
				AVFrameList* lst = NULL;
				avcodec_flush_buffers(play->m_audio_ctx);
				while (play->m_audio_dq.m_size && !play->m_audio_dq.abort_request)
						av_usleep(1000);
				pthread_mutex_lock(&play->m_audio_dq.m_mutex);
				lst = (AVFrameList*)play->m_audio_dq.m_first_pkt;
				for (; lst != NULL; lst = lst->next)
					lst->pkt.type = 1;	/*type为1表示skip.*/
				pthread_mutex_unlock(&play->m_audio_dq.m_mutex);
				continue;
			}

			/* 使用pts更新音频时钟. */
			if (pkt.pts != AV_NOPTS_VALUE)
				play->m_audio_clock = av_q2d(play->m_base_info->audio_time_base) * (pkt.pts - v_start_time);

			if (fabs(play->m_audio_current_pts_last) < 1.0e-6)
				play->m_audio_current_pts_last = play->m_audio_clock;

			/* 计算pkt缓冲数据大小. */
			pthread_mutex_lock(&play->m_buf_size_mtx);
			play->m_pkt_buffer_size -= pkt.size;
			pthread_mutex_unlock(&play->m_buf_size_mtx);

			/* 解码音频. */
			pkt2 = pkt;
			avcodec_get_frame_defaults(&avframe);

			while (!play->m_abort)
			{
				int got_frame = 0;
				ret = avcodec_decode_audio4(play->m_audio_ctx, &avframe, &got_frame, &pkt2);
				if (ret < 0)
				{
					printf("Audio error while decoding one frame!!!\n");
					break;
				}
				pkt2.size -= ret;
				pkt2.data += ret;

				/* 不足一个帧, 并且packet中还有数据, 继续解码当前音频packet. */
				if (!got_frame && pkt2.size > 0)
					continue;

				/* packet中已经没有数据了, 并且不足一个帧, 丢弃这个音频packet. */
				if (pkt2.size == 0 && !got_frame)
					break;

				if (avframe.linesize[0] != 0)
				{
					/* copy并转换音频格式. */
					audio_copy(play, &avcopy, &avframe);

					/* 将计算的pts复制到avcopy.pts.  */
					memcpy(&avcopy.pts, &play->m_audio_clock, sizeof(double));

					/* 计算下一个audio的pts值.  */
					n = 2 * FFMIN(play->m_audio_ctx->channels, 2);

					play->m_audio_clock += ((double) avcopy.linesize[0] / (double) (n * play->m_audio_ctx->sample_rate));

					/* 如果不是以音频同步为主, 则需要计算是否移除一些采样以同步到其它方式.	*/
					if (play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK ||
						play->m_av_sync_type == AV_SYNC_VIDEO_MASTER && play->m_base_info->has_video != -1)
					{
						/* 暂无实现.	*/
					}

					/* 防止内存过大.	*/
					chk_queue(play, &play->m_audio_dq, AVDECODE_BUFFER_SIZE);

					/* 丢到播放队列中.	*/
					put_queue(&play->m_audio_dq, &avcopy);

					/* packet中数据已经没有数据了, 解码下一个音频packet. */
					if (pkt2.size <= 0)
						break;
				}
			}
			av_free_packet(&pkt);
		}
	}

	return NULL;
}

static
void* video_dec_thrd(void *param)
{
	AVPacket pkt, pkt2;
	AVFrame *avframe, avcopy;
	int got_picture = 0;
	int ret = 0;
	avplay *play = (avplay*) param;
	int64_t v_start_time = 0;
	int64_t a_start_time = 0;

	avframe = avcodec_alloc_frame();

	if (play->m_base_info->has_video != -1 && play->m_base_info->has_audio != -1)
	{
		v_start_time = play->m_base_info->video_start_time;
		a_start_time = play->m_base_info->audio_start_time;

		play->m_video_ctx->width = play->m_base_info->width;
		play->m_video_ctx->height = play->m_base_info->height;
	}

	for (; !play->m_abort;)
	{
		av_init_packet(&pkt);
		while (play->m_play_status == paused && !play->m_abort)
			av_usleep(10000);
		ret = get_queue(&play->m_video_q, (AVPacket*) &pkt);
		if (ret != -1)
		{
			if (pkt.data == flush_pkt.data)
			{
				AVFrameList* lst = NULL;

				avcodec_flush_buffers(play->m_video_ctx);

				while (play->m_video_dq.m_size && !play->m_video_dq.abort_request)
					av_usleep(1000);

				pthread_mutex_lock(&play->m_video_dq.m_mutex);
				lst = (AVFrameList*)play->m_video_dq.m_first_pkt;
				for (; lst != NULL; lst = lst->next)
					lst->pkt.type = 1; /* type为1表示skip. */
				play->m_video_current_pos = -1;
				play->m_frame_last_dropped_pts = AV_NOPTS_VALUE;
				play->m_frame_last_duration = 0;
				play->m_frame_timer = (double)av_gettime() / 1000000.0f;
				play->m_video_current_pts_drift = -play->m_frame_timer;
				play->m_frame_last_pts = AV_NOPTS_VALUE;
				pthread_mutex_unlock(&play->m_video_dq.m_mutex);

				continue;
			}

			pthread_mutex_lock(&play->m_buf_size_mtx);
			play->m_pkt_buffer_size -= pkt.size;
			pthread_mutex_unlock(&play->m_buf_size_mtx);
			pkt2 = pkt;

			while (pkt2.size > 0 && !play->m_abort)
			{
				ret = avcodec_decode_video2(play->m_video_ctx, avframe, &got_picture, &pkt2);
				if (ret < 0)
				{
					printf("Video error while decoding one frame!!!\n");
					break;
				}
				if (got_picture)
					break;
				pkt2.size -= ret;
				pkt2.data += ret;
			}

			if (got_picture)
			{
				double pts1 = 0.0f;
				double frame_delay, pts;

				/*
				 * 复制帧, 并输出为PIX_FMT_YUV420P.
				 */

				video_copy(play, &avcopy, avframe);

				/*
				 * 初始化m_frame_timer时间, 使用系统时间.
				 */
				if (play->m_frame_timer == 0.0f)
					play->m_frame_timer = (double)av_gettime() / 1000000.0f;

				/*
				 * 计算pts值.
				 */
				pts1 = (avcopy.best_effort_timestamp - a_start_time) * av_q2d(play->m_base_info->video_time_base);
				if (pts1 == AV_NOPTS_VALUE)
					pts1 = 0;
				pts = pts1;

				/* 如果以音频同步为主, 则在此判断是否进行丢包. */
				if (play->m_base_info->has_audio != -1 &&
					((play->m_av_sync_type == AV_SYNC_AUDIO_MASTER) || play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK))
				{
					pthread_mutex_lock(&play->m_video_dq.m_mutex);
					/*
					 * 最后帧的pts是否为AV_NOPTS_VALUE 且 pts不等于0
					 * 计算视频时钟和主时钟源的时间差.
					 * 计算pts时间差, 当前pts和上一帧的pts差值.
					 */
					ret = 1;
					if (play->m_frame_last_pts != AV_NOPTS_VALUE && pts)
					{
						double clockdiff = video_clock(play) - master_clock(play);
						double ptsdiff = pts - play->m_frame_last_pts;

						/*
						 * 如果clockdiff和ptsdiff同时都在同步阀值范围内
						 * 并且clockdiff与ptsdiff之和与m_frame_last_filter_delay的差
						 * 如果小于0, 则丢弃这个视频帧.
						 */
						if (fabs(clockdiff) < AV_NOSYNC_THRESHOLD && ptsdiff > 0
								&& ptsdiff < AV_NOSYNC_THRESHOLD
								&& clockdiff + ptsdiff - play->m_frame_last_filter_delay < 0)
						{
							play->m_frame_last_dropped_pos = pkt.pos;
							play->m_frame_last_dropped_pts = pts;
							play->m_drop_frame_num++;
							printf("\nDROP: %3d drop a frame of pts is: %.3f\n", play->m_drop_frame_num, pts);
							ret = 0;
						}
					}
					pthread_mutex_unlock(&play->m_video_dq.m_mutex);
					if (ret == 0)
					{
						/* 丢掉该帧. */
						av_free(avcopy.data[0]);
						continue;
					}
				}

				/* 计录最后有效帧时间. */
				play->m_frame_last_returned_time = (double)av_gettime() / 1000000.0f;
				/* m_frame_last_filter_delay基本都是0吧. */
				play->m_frame_last_filter_delay = (double)av_gettime() / 1000000.0f
						- play->m_frame_last_returned_time;
				/* 如果m_frame_last_filter_delay还可能大于1, 那么m_frame_last_filter_delay置0. */
				if (fabs(play->m_frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0f)
					play->m_frame_last_filter_delay = 0.0f;

				/*
				 *	更新当前m_video_clock为当前解码pts.
				 */
				if (pts != 0)
					play->m_video_clock = pts;
				else
					pts = play->m_video_clock;

				/*
				 *	计算当前帧的延迟时长.
				 */
				frame_delay = av_q2d(play->m_video_ctx->time_base);
				frame_delay += avcopy.repeat_pict * (frame_delay * 0.5);

				/*
				 * m_video_clock加上该帧延迟时长,
				 * m_video_clock是估算出来的下一帧的pts.
				 */
				play->m_video_clock += frame_delay;

				/*
				 * 防止内存过大.
				 */
				chk_queue(play, &play->m_video_dq, AVDECODE_BUFFER_SIZE);

				/* 保存frame_delay为该帧的duration, 保存到.pts字段中. */
				memcpy(&avcopy.pkt_dts, &frame_delay, sizeof(double));
				/* 保存pts. */
				memcpy(&avcopy.pts, &pts, sizeof(double));
				/* 保存pos, pos即是文件位置. */
				avcopy.pkt_pos = pkt.pos;
				/* type为0表示no skip. */
				avcopy.type = 0;

				/* 丢进播放队列.	*/
				put_queue(&play->m_video_dq, &avcopy);
			}
			av_free_packet(&pkt);
		}
	}
	av_free(avframe);
	return NULL;
}

static
void* audio_render_thrd(void *param)
{
	avplay *play = (avplay*) param;
	AVFrame audio_frame;
	int audio_size = 0;
	int ret, temp, inited = 0;
	int bytes_per_sec;

	while (!play->m_abort)
	{
		ret = get_queue(&play->m_audio_dq, &audio_frame);
		if (audio_frame.data[0] == flush_frm.data[0])
			continue;
		if (ret != -1)
		{
			if (!inited && play->m_ao_ctx)
			{
				inited = 1;
				/* 配置渲染器. */
				ret = play->m_ao_ctx->init_audio(play->m_ao_ctx,
					FFMIN(play->m_audio_ctx->channels, 2), 16, play->m_audio_ctx->sample_rate, 0);
				if (ret != 0)
					inited = -1;
				else
				{
					/* 更改播放状态. */
					play->m_play_status = playing;
				}
				bytes_per_sec = play->m_audio_ctx->sample_rate *
					FFMIN(play->m_audio_ctx->channels, 2) * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
				/* 修改音频设备初始化状态, 置为1. */
				if (inited != -1)
					play->m_ao_inited = 1;
			}
			else if (!play->m_ao_ctx)
			{
				av_free(audio_frame.data[0]);
				break;
			}

			if (audio_frame.type == 1)
			{
				av_free(audio_frame.data[0]);
				continue;
			}

			audio_size = audio_frame.linesize[0];
			/* 清空. */
			play->m_audio_buf_size = audio_size;
			play->m_audio_buf_index = 0;

			/* 已经开始播放, 清空seeking的状态. */
			if (play->m_seeking == SEEKING_FLAG)
				play->m_seeking = NOSEEKING_FLAG;

			while (audio_size > 0)
			{
				if (inited == 1 && play->m_ao_ctx)
				{
					temp = play->m_ao_ctx->play_audio(play->m_ao_ctx,
						audio_frame.data[0] + play->m_audio_buf_index, play->m_audio_buf_size - play->m_audio_buf_index);
					play->m_audio_buf_index += temp;
					/* 如果缓冲已满, 则休眠一小会. */
					if (temp == 0)
					{
						if (play->m_audio_dq.m_size > 0)
						{
							if (((AVFrameList*) play->m_audio_dq.m_last_pkt)->pkt.type == 1)
								break;
						}
						av_usleep(10000);
					}
				}
				else
				{
					assert(0);
				}
				audio_size = play->m_audio_buf_size - play->m_audio_buf_index;
			}

			av_free(audio_frame.data[0]);
		}
	}
	return NULL;
}

static
void* video_render_thrd(void *param)
{
	avplay *play = (avplay*) param;
	AVFrame video_frame;
	int ret = 0;
	int inited = 0;
	double sync_threshold;
	double current_pts;
	double last_duration;
	double duration;
	double delay = 0.0f;
	double time;
	double next_pts;
	double diff = 0.0f;
	int64_t frame_num = 0;
	double diff_sum = 0;
	double avg_diff = 0.0f;

	while (!play->m_abort)
	{
		/* 如果视频队列为空 */
		if (play->m_video_dq.m_size == 0)
		{
			pthread_mutex_lock(&play->m_video_dq.m_mutex);
			/*
			 * 如果最后丢弃帧的pts不为空, 且大于最后pts则
			 * 使用最后丢弃帧的pts值更新其它相关的pts值.
			 */
			if (play->m_frame_last_dropped_pts != AV_NOPTS_VALUE && play->m_frame_last_dropped_pts > play->m_frame_last_pts)
			{
				update_video_pts(play, play->m_frame_last_dropped_pts, play->m_frame_last_dropped_pos);
				play->m_frame_last_dropped_pts = AV_NOPTS_VALUE;
			}
			pthread_mutex_unlock(&play->m_video_dq.m_mutex);
		}
		/* 获得下一帧视频. */
		ret = get_queue(&play->m_video_dq, &video_frame);
		if (ret != -1)
		{
			// 状态为正在渲染.
			play->m_rendering = 1;
			// 如果没有初始化渲染器, 则初始化渲染器.
			if (!inited && play->m_vo_ctx)
			{
				inited = 1;
				play->m_vo_ctx->fps = (float)play->m_base_info->video_frame_rate.num
					/*play->m_video_st->r_frame_rate.num*/ / (float)play->m_base_info->video_frame_rate.den/*play->m_video_st->r_frame_rate.den*/;
				ret = play->m_vo_ctx->init_video(play->m_vo_ctx,
					play->m_video_ctx->width, play->m_video_ctx->height, play->m_video_ctx->pix_fmt);
				if (ret != 0)
					inited = -1;
				else
					play->m_play_status = playing;
			}

			if (video_frame.data[0] == flush_frm.data[0])
				continue;

			do {
				/* 判断是否skip. */
				if (video_frame.type == 1)
				{
					/* 跳过该帧. */
					break;
				}

				/* 计算last_duration. */
				memcpy(&current_pts, &video_frame.pts, sizeof(double));
				last_duration = current_pts - play->m_frame_last_pts;
				if (last_duration > 0 && last_duration < 10.0)
				{
					/* 更新m_frame_last_duration. */
					play->m_frame_last_duration = last_duration;
				}

				/* 更新延迟同步到主时钟源. */
				delay = play->m_frame_last_duration;
				if ((play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK) ||
					(play->m_av_sync_type == AV_SYNC_AUDIO_MASTER && play->m_base_info->has_audio != -1))
				{
					diff = video_clock(play) - master_clock(play);
					sync_threshold = FFMAX(AV_SYNC_THRESHOLD, delay) * 0.75;
					if (fabs(diff) < AV_NOSYNC_THRESHOLD)
					{
						if (diff <= -sync_threshold)
							delay = 0.0f;
						else if (diff >= sync_threshold)
							delay = 2.0f * delay;
					}
					else
					{
						if (diff < 0.0f)
							delay = 0.0f;
						else
							av_usleep(1000);
					}
				}

				/* 得到当前系统时间. */
				time = (double)av_gettime() / 1000000.0f;

				/* 如果当前系统时间小于播放时间加延迟时间, 则过一会重试. */
				if (time < play->m_frame_timer + delay)
				{
					av_usleep(1000);
					continue;
				}

				/* 更新m_frame_timer. */
				if (delay > 0.0f)
					play->m_frame_timer += delay * FFMAX(1, floor((time - play->m_frame_timer) / delay));

				pthread_mutex_lock(&play->m_video_dq.m_mutex);
				update_video_pts(play, current_pts, video_frame.pkt_pos);
				pthread_mutex_unlock(&play->m_video_dq.m_mutex);

				/* 计算下一帧的时间.  */
				if (play->m_video_dq.m_size > 0)
				{
					memcpy(&next_pts, &(((AVFrameList*) play->m_video_dq.m_last_pkt)->pkt.pts), sizeof(double));
					duration = next_pts - current_pts;
				}
				else
				{
					memcpy(&duration, &video_frame.pkt_dts, sizeof(double));
				}

				if (play->m_base_info->has_audio != -1 && time > play->m_frame_timer + duration)
				{
					if (play->m_video_dq.m_size > 1)
					{
						pthread_mutex_lock(&play->m_video_dq.m_mutex);
						play->m_drop_frame_num++;
						pthread_mutex_unlock(&play->m_video_dq.m_mutex);
						printf("\nDROP: %3d drop a frame of pts is: %.3f\n", play->m_drop_frame_num, current_pts);
						break;
					}
				}

				if (diff < 1000)
				{
					frame_num++;
					diff_sum += fabs(diff);
					avg_diff = (double)diff_sum / frame_num;
				}
				printf("%7.3f A-V: %7.3f A: %7.3f V: %7.3f FR: %d/fps, VB: %d/kbps\r",
					master_clock(play), diff, audio_clock(play), video_clock(play), play->m_real_frame_rate, play->m_real_bit_rate);

				/*	在这里计算帧率.	*/
				if (play->m_enable_calc_frame_rate)
				{
					int current_time = 0;
					/* 计算时间是否足够一秒钟. */
					if (play->m_last_fr_time == 0)
						play->m_last_fr_time = (double)av_gettime() / 1000000.0f;
					current_time = (double)av_gettime() / 1000000.0f;
					if (current_time - play->m_last_fr_time >= 1)
					{
						play->m_last_fr_time = current_time;
						if (++play->m_fr_index == MAX_CALC_SEC)
							play->m_fr_index = 0;

						/* 计算frame_rate. */
						do
						{
							int sum = 0;
							int i = 0;
							for (; i < MAX_CALC_SEC; i++)
								sum += play->m_frame_num[i];
							play->m_real_frame_rate = (double)sum / (double)MAX_CALC_SEC;
						} while (0);
						/* 清空. */
						play->m_frame_num[play->m_fr_index] = 0;
					}

					/* 更新读取字节数. */
					play->m_frame_num[play->m_fr_index]++;
				}

				/* 已经开始播放, 清空seeking的状态. */
				if (play->m_seeking == SEEKING_FLAG)
					play->m_seeking = NOSEEKING_FLAG;

				if (inited == 1 && play->m_vo_ctx)
				{
					play->m_vo_ctx->render_one_frame(play->m_vo_ctx, &video_frame, play->m_video_ctx->pix_fmt, av_curr_play_time(play));
					if (delay != 0)
						av_usleep(4000);
				}
				break;
			} while (TRUE);

			/* 渲染完成. */
			play->m_rendering = 0;

			/* 如果处于暂停状态, 则直接渲染窗口, 以免黑屏. */
			while (play->m_play_status == paused && inited == 1 && play->m_vo_ctx && !play->m_abort)
			{
				play->m_vo_ctx->render_one_frame(play->m_vo_ctx, &video_frame, play->m_video_ctx->pix_fmt, av_curr_play_time(play));
				av_usleep(16000);
			}

			/* 释放视频帧缓冲. */
			av_free(video_frame.data[0]);
		}
	}

	/* 释放render. */
	if (play->m_vo_ctx)
	{
		free_video_render(play->m_vo_ctx);
		play->m_vo_ctx = NULL;
	}

	return NULL;
}

/* 下面模糊代码来自ffdshow. */
void blurring(AVFrame* frame, int fw, int fh, int dx, int dy, int dcx, int dcy)
{
	uint8_t* tempLogo = malloc(dcx * dcy);
	uint8_t* borderN = malloc(dcx);
	uint8_t* borderS = malloc(dcx);
	uint8_t* borderW = malloc(dcy);
	uint8_t* borderE = malloc(dcy);
	double* uwetable = malloc(dcx * dcy * sizeof(double));
	double* uweweightsum = malloc(dcx * dcy * sizeof(double));
	uint8_t* pic[3] = { frame->data[0],
		frame->data[1], frame->data[2]};
	int i = 0;

	for (i = 0; i < 3; i++)
	{
		int shift = (i == 0) ? 0 : 1;
		int sx = dx >> shift;
		int sy = dy >> shift;
		int w = dcx >> shift;
		int h = dcy >> shift;
		int stride = fw >> shift;
		int x;
		int y;
		int k = 0;

		memcpy(borderN, pic[i] + sx + stride * sy, w);
		memcpy(borderS, pic[i] + sx + stride * (sy + h), w);

		for (k = 0; k < h; k++)
		{
			borderW[k] = *(pic[i] + sx + stride * (sy + k));
			borderE[k] = *(pic[i] + sx + w + stride * (sy + k));
		}

		memcpy(tempLogo, borderN, w);
		memcpy(tempLogo + w * (h - 1), borderS, w);
		for (k = 0; k < h; k++)
		{
			tempLogo[w * k] = borderW[k];
			tempLogo[w * k + w - 1] = borderE[k];
		}

		{
			int power = 3;
			double e = 1.0 + (0.3 * power);
			for (x = 0; x < w; x++)
			{
				for (y = 0; y < h; y++)
				{
					if(x + y != 0)
					{
						uwetable[x + y * w] = 1.0 / pow(sqrt((double)(x * x + y * y)), e);
					}
					else
					{
						uwetable[x + y * w] = 1.0;
					}
				}
			}

			for (x = 1; x < w - 1; x++)
			{
				for (y = 1; y < h - 1; y++)
				{
					double weightsum = 0;
					int bx;
					int by;
					for (bx = 0; bx < w; bx++)
					{
						weightsum += uwetable[abs(bx - x) + y * w];
						weightsum += uwetable[abs(bx - x) + abs(h - 1 - y) * w];
					}
					for (by = 1; by < h - 1; by++)
					{
						weightsum += uwetable[x + abs(by - y) * w];
						weightsum += uwetable[abs(w - 1 - x) + abs(by - y) * w];
					}
					uweweightsum[y * w + x] = weightsum;
				}
			}
		}

		for (x = 1; x < w - 1; x++)
		{
			for (y = 1; y < h - 1; y++)
			{
				double r = 0;
				const unsigned char *lineN = borderN, *lineS = borderS;
				const unsigned char *lineW = borderW, *lineE = borderE;
				int bx;
				int by;
				for (bx = 0; bx < w; bx++)
				{
					r += lineN[bx] * uwetable[abs(bx - x) + y * w];
					r += lineS[bx] * uwetable[abs(bx - x) + abs(h - 1 - y) * w];
				}
				for (by = 1; by < h - 1; by++)
				{
					r += lineW[by] * uwetable[x + abs(by - y) * w];
					r += lineE[by] * uwetable[abs(w - 1 - x) + abs(by - y) * w];
				}
				tempLogo[y * w + x] = (uint8_t)(r / uweweightsum[y * w + x]);
			}
		}

		for (k = 0; k < h; k++)
		{
			memcpy(pic[i] + sx + stride * (sy + k), tempLogo + w * k, w);
		}
	}

	free(uweweightsum);
	free(uwetable);
	free(tempLogo);
	free(borderN);
	free(borderS);
	free(borderW);
	free(borderE);
}

/* 下面alpha_blend代码来自vlc. */
void alpha_blend(AVFrame* frame, uint8_t* rgba,
	int fw, int fh, int rgba_w, int rgba_h, int x, int y)
{
	uint8_t *dsty, *dstu, *dstv;
	uint32_t* src, color;
	unsigned char cy, cu, cv, opacity;
	int b_even_scanline;
	int i, j;

	src = (uint32_t*)rgba;

	b_even_scanline = y % 2;

	// Y份量的开始位置.
	dsty = frame->data[0] + y * fw + x;
	dstu = frame->data[1] + ((y / 2) * (fw / 2)) + x / 2;
	dstv = frame->data[2] + ((y / 2) * (fw / 2)) + x / 2;

	// alpha融合YUV各分量.
	for (i = 0; i < rgba_h
		; i++
		, dsty += fw
		, dstu += b_even_scanline ? fw / 2 : 0
		, dstv += b_even_scanline ? fw / 2 : 0
		)
	{
		b_even_scanline = !b_even_scanline;
		for (j = 0; j < rgba_w; j++)
		{
			color = src[i * rgba_w + j];
			cy = rgba2y(color);
			cu = rgba2u(color);
			cv = rgba2v(color);

			opacity = _a(color);
			if (!opacity) // 全透明.
				continue;

			if (opacity == MAX_TRANS)
			{
				dsty[j] = cy;
				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = cu;
					dstv[j / 2] = cv;
				}
			}
			else
			{
				dsty[j] = (cy * opacity + dsty[j] *
					(MAX_TRANS - opacity)) >> TRANS_BITS;

				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = (cu * opacity + dstu[j / 2] *
						(MAX_TRANS - opacity)) >> TRANS_BITS;
					dstv[j / 2] = (cv * opacity + dstv[j / 2] *
						(MAX_TRANS - opacity)) >> TRANS_BITS;
				}
			}
		}
	}
}

FILE *logfp = NULL;
int log_ref = 0;

int logger_to_file(const char* logfile)
{
	if (log_ref++ == 0)
	{
		logfp = fopen(logfile, "w+b");
		if (!logfp)
		{
			log_ref--;
			return -1;
		}
	}
	return 0;
}

int close_logger_file()
{
	if (!logfp)
		return -1;

	if (--log_ref == 0)
	{
		fclose(logfp);
		logfp = NULL;
	}

	return 0;
}

/* 内部日志输出函数实现.	*/
void get_current_time(char *buffer)
{
	struct tm current_time;
	time_t tmp_time;

	time(&tmp_time);

	current_time = *(localtime(&tmp_time));

	if (current_time.tm_year > 50)
	{
		sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
			current_time.tm_year + 1900, current_time.tm_mon + 1, current_time.tm_mday,
			current_time.tm_hour, current_time.tm_min, current_time.tm_sec);
	}
	else
	{
		sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
			current_time.tm_year + 2000, current_time.tm_mon + 1, current_time.tm_mday,
			current_time.tm_hour, current_time.tm_min, current_time.tm_sec);
	}
}

int logger(const char *fmt, ...)
{
	char buffer[65536];
	char time_buf[1024];
	va_list va;
	int ret = 0;

	va_start(va, fmt);
	vsprintf(buffer, fmt, va);

	get_current_time(time_buf);

	// 输出到屏幕.
	ret = printf("[%s] %s", time_buf, buffer);

	// 输出到文件.
	if (logfp)
	{
		fprintf(logfp, "[%s] %s", time_buf, buffer);
		fflush(logfp);
	}

	va_end(va);

	return ret;
}

double buffering(avplay *play)
{
	return play->m_buffering;
}

int audio_is_inited(avplay *play)
{
	return play->m_ao_inited;
}

