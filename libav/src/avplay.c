#include "avplay.h"
#include <stdlib.h>

#ifdef WIN32
typedef struct
{
	long long quot; /* Quotient. */
	long long rem;  /* Remainder. */
} lldiv_t;

lldiv_t lldiv (long long num, long long denom)
{
	lldiv_t d = { num / denom, num % denom };
	return d;
}
#endif /* WIN32 */

/* 定义bool值 */
#ifndef _MSC_VER
enum bool_type
{
	FALSE, TRUE
};
#endif

enum sync_type
{
	AV_SYNC_AUDIO_MASTER, /* 默认选择. */
	AV_SYNC_VIDEO_MASTER, /* 同步到视频时间戳. */
	AV_SYNC_EXTERNAL_CLOCK, /* 同步到外部时钟. */
};

/* 队列类型.	*/
#define QUEUE_PACKET				0
#define QUEUE_AVFRAME			1

#define IO_BUFFER_SIZE			32768
#define MAX_PKT_BUFFER_SIZE	5242880
#define MIN_AUDIO_BUFFER_SIZE	MAX_PKT_BUFFER_SIZE /* 327680 */
#define MIN_AV_FRAMES			5
#define AUDIO_BUFFER_MAX_SIZE	(AVCODEC_MAX_AUDIO_FRAME_SIZE * 2)
#define AVDECODE_BUFFER_SIZE	2
#define DEVIATION					6

#define AV_SYNC_THRESHOLD		0.01f
#define AV_NOSYNC_THRESHOLD	10.0f
#define AUDIO_DIFF_AVG_NB		20

#define SEEKING_FLAG				-1
#define NOSEEKING_FLAG			0

#ifndef _MSC_VER
#define Sleep(x) usleep(x * 1000)
#else
#define av_gettime() (timeGetTime() * 1000.0f)
#endif

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
	if (q->m_mutex)
		pthread_mutex_destroy(&q->m_mutex);
	if (q->m_cond)
		pthread_cond_destroy(&q->m_cond);
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
media_info* find_media_info(media_source *ms, int index)
{
	media_info *mi = ms->media;
	int i = 0;
	if (index > ms->media_size)
		return NULL;
	for (; i < index; i++)
		mi++;
	return mi;
}

static
int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	int read_bytes = 0;
	avplay *play = (avplay*)opaque;
	read_bytes = play->m_media_source->read_data(play->m_media_source->ctx, 
		buf, play->m_media_source->offset, buf_size);
	if (read_bytes == -1)
		return 0;
	play->m_media_source->offset += read_bytes;
	return read_bytes;
}

static
int write_packet(void *opaque, uint8_t *buf, int buf_size)
{
	avplay *play = (avplay*)opaque;
	return 0;
}

static
int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
	avplay *play = (avplay*)opaque;
	int64_t old_off = play->m_media_source->offset;

	if (play->m_abort)
		return -1;

	switch (whence)
	{
	case SEEK_SET:
		{
			media_info *mi = find_media_info(play->m_media_source, 
				play->m_current_play_index);
			if (!mi)
				assert(0);
			/* 从当前文件的起始位置开始计算. */
			play->m_media_source->offset = mi->start_pos + offset;
		}
		break;
	case SEEK_CUR:
		/* 直接根据当前位置计算offset. */
		offset += play->m_media_source->offset;
		play->m_media_source->offset = offset;
		break;
	case SEEK_END:
		{
			int64_t size = 0;

			if (play->m_media_source->type == MEDIA_TYPE_FILE)
				size = play->m_media_source->media->file_size;
			if (play->m_media_source->type == MEDIA_TYPE_BT)
			{
				/* 找到正在播放的文件. */
				media_info *mi = find_media_info(play->m_media_source, 
					play->m_current_play_index);
				if (mi)
					size = mi->file_size;
				else
					assert(0);
				/* 计算文件尾的位置. */
				size += mi->start_pos;
			}
			/* 计算当前位置. */
			offset += size;
			/* 保存当前位置. */
			play->m_media_source->offset = offset;
		}
		break;
	case AVSEEK_SIZE:
		{
			int64_t size = 0;

			if (play->m_media_source->type == MEDIA_TYPE_FILE)
				size = play->m_media_source->media->file_size;
			if (play->m_media_source->type == MEDIA_TYPE_BT)
			{
				/* 找到正在播放的文件. */
				media_info *mi = find_media_info(play->m_media_source, 
					play->m_current_play_index);
				if (mi)
					size = mi->file_size;
				else
					assert(0);
				if (mi)
					size = mi->file_size;
			}
			offset = size;
		}
		break;
	default:
		break;
	}

	return offset;
}

static
int decode_interrupt_cb(void *ctx)
{
	avplay *play = (avplay*)ctx;
	return play->m_abort;
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

media_source* alloc_media_source(int type, char *data, int len, int64_t size)
{
	struct media_source *ptr = malloc(sizeof(media_source));
	memset(ptr, 0, sizeof(media_source));
	ptr->data = calloc(1, len);
	memcpy(ptr->data, data, len);
	ptr->type = type;

	if (type == MEDIA_TYPE_FILE)
	{
		ptr->media = calloc(1, sizeof(media_info));
		ptr->media->start_pos = 0;
		ptr->media->file_size = size;
		/* addition中已经保存了文件名, 这里不再重复保存文件名. */
		ptr->media->name = NULL;
	}

	if (type == MEDIA_TYPE_BT)
	{
		ptr->data_len = len;
	}

	return ptr;
}

void free_media_source(media_source *src)
{
	if (src->data)
		free(src->data);
	if (src->type == MEDIA_TYPE_BT)
	{
		int i = 0;
		for (; i < src->media_size; i++)
		{
			if (src->media[i].name)
				free(src->media[i].name);
		}
	}
	if (src->media)
		free(src->media);
	free(src);
}

avplay* alloc_avplay_context()
{
	struct avplay *ptr = malloc(sizeof(avplay));
	memset(ptr, 0, sizeof(avplay));
	return ptr;
}

void free_avplay_context(avplay *ctx)
{
	free(ctx);
}

audio_render* alloc_audio_render()
{
	struct audio_render *ptr = malloc(sizeof(audio_render));
	memset(ptr, 0, sizeof(audio_render));
	return ptr;
}

void free_audio_render(audio_render *render)
{
	render->destory_audio(render->ctx);
	free(render);
}

video_render* alloc_video_render(void *user_data)
{
	struct video_render *ptr = malloc(sizeof(video_render));
	memset(ptr, 0, sizeof(video_render));
	ptr->user_data = user_data;
	return ptr;
}

void free_video_render(video_render *render)
{
	render->destory_video(render->ctx);
	free(render);
}

int initialize(avplay *play, media_source *ms, int video_out_type)
{
	int ret = 0, i = 0;
	AVInputFormat *iformat = NULL;

	av_register_all();
	avcodec_register_all();

	/* 分配一个format context. */
	play->m_format_ctx = avformat_alloc_context();
	play->m_format_ctx->flags = AVFMT_FLAG_GENPTS;
	play->m_format_ctx->interrupt_callback.callback = decode_interrupt_cb;
	play->m_format_ctx->interrupt_callback.opaque = play;

	/* 保存media_source指针并初始化, 由avplay负责管理释放其内存. */
	play->m_media_source = ms;
	/* 保存视频输出类型. */
	play->m_video_render_type = video_out_type;

	/* 初始化数据源. */
	if (play->m_media_source->init_source &&
		play->m_media_source->init_source(&ms->ctx, 
		ms->data, ms->data_len, NULL) < 0)
	{
		return -1;
	}

	/* 获得媒体文件列表. */
	if (ms->type == MEDIA_TYPE_BT)
	{
		char name[2048] = {0};
		int64_t pos = 0;
		int64_t size = 2048;
		int i = 0;
		media_info *media = NULL;

		for (i = 0; ; i++)
		{
			int ret = play->m_media_source->bt_media_info(
				play->m_media_source->ctx, name, &pos, &size);
			if (ret == -1)
				break;
			if (i == 0)
			{
				play->m_media_source->media = malloc(sizeof(media_info));
				play->m_media_source->media_size = ret;
				media = play->m_media_source->media;
			}
			media->file_size = size;
			media->start_pos = pos;
			media->name = malloc(strlen(name) + 1);
			strcpy(media->name, name);
			size = 2048;
			pos = i + 1;
		}
	}

	if (ms->type == MEDIA_TYPE_BT ||
		ms->type == MEDIA_TYPE_FILE)
	{
		/* 分配用于io的缓冲. */
		play->m_io_buffer = (unsigned char*)av_malloc(IO_BUFFER_SIZE);
		if (!play->m_io_buffer)
		{
			printf("Create buffer failed!\n");
			return -1;
		}

		/* 分配io上下文. */
		play->m_avio_ctx = avio_alloc_context(play->m_io_buffer, 
			IO_BUFFER_SIZE, 0, (void*)play, read_packet, NULL, seek_packet);
		if (!play->m_io_buffer)
		{
			printf("Create io context failed!\n");
			av_free(play->m_io_buffer);
			return -1;
		}
		play->m_avio_ctx->write_flag = 0;

		ret = av_probe_input_buffer(play->m_avio_ctx, &iformat, "", NULL, 0, NULL);
		if (ret < 0)
		{
			printf("av_probe_input_buffer call failed!\n");
			goto FAILED_FLG;
		}

		/* 打开输入媒体流.	*/
		play->m_format_ctx->pb = play->m_avio_ctx;
		ret = avformat_open_input(&play->m_format_ctx, "", iformat, NULL);
		if (ret < 0)
		{
			printf("av_open_input_stream call failed!\n");
			goto FAILED_FLG;
		}
	}
	else
	{
		/* 判断url是否为空. */
		if (!play->m_media_source->data || !play->m_media_source->data_len)
			goto FAILED_FLG;

		/* HTTP和RTSP直接使用ffmpeg来处理.	*/
		ret = avformat_open_input(&play->m_format_ctx, 
			play->m_media_source->data, iformat, NULL);
		if (ret < 0)
		{
			printf("av_open_input_stream call failed!\n");
			goto FAILED_FLG;
		}
	}

	ret = avformat_find_stream_info(play->m_format_ctx, NULL);
	if (ret < 0)
		goto FAILED_FLG;

	av_dump_format(play->m_format_ctx, 0, NULL, 0);

	/* 得到audio和video在streams中的index.	*/
	play->m_video_index = 
		stream_index(AVMEDIA_TYPE_VIDEO, play->m_format_ctx);
	play->m_audio_index = 
		stream_index(AVMEDIA_TYPE_AUDIO, play->m_format_ctx);
	if (play->m_video_index == -1 && play->m_audio_index == -1)
		goto FAILED_FLG;

	/* 保存audio和video的AVCodecContext指针.	*/
	if (play->m_audio_index != -1)
		play->m_audio_ctx = play->m_format_ctx->streams[play->m_audio_index]->codec;
	if (play->m_video_index != -1)
		play->m_video_ctx = play->m_format_ctx->streams[play->m_video_index]->codec;

	/* 打开解码器. */
	if (play->m_audio_index != -1)
	{
		ret = open_decoder(play->m_audio_ctx);
		if (ret != 0)
			goto FAILED_FLG;
	}
	if (play->m_video_index != -1)
	{
		ret = open_decoder(play->m_video_ctx);
		if (ret != 0)
			goto FAILED_FLG;
	}

	/* 保存audio和video的AVStream指针.	*/
	if (play->m_video_index != -1)
		play->m_video_st = play->m_format_ctx->streams[play->m_video_index];
	if (play->m_audio_index != -1)
		play->m_audio_st = play->m_format_ctx->streams[play->m_audio_index];

	/* 默认同步到音频.	*/
	play->m_av_sync_type = AV_SYNC_AUDIO_MASTER;
	play->m_abort = TRUE;

	/* 初始化各变量. */
	av_init_packet(&flush_pkt);
	flush_pkt.data = "FLUSH";
	flush_frm.data[0] = "FLUSH";
	play->m_abort = 0;

	/* 初始化队列. */
	play->m_audio_q.m_type = QUEUE_PACKET;
	queue_init(&play->m_audio_q);
	play->m_video_q.m_type = QUEUE_PACKET;
	queue_init(&play->m_video_q);
	play->m_audio_dq.m_type = QUEUE_AVFRAME;
	queue_init(&play->m_audio_dq);
	play->m_video_dq.m_type = QUEUE_AVFRAME;
	queue_init(&play->m_video_dq);

	/* 初始化读取文件数据缓冲计数mutex. */
	pthread_mutex_init(&play->m_buf_size_mtx, NULL);

	/* 打开各线程.	*/
	return 0;

FAILED_FLG:
	if (play->m_format_ctx)
		avformat_close_input(&play->m_format_ctx);
	if (play->m_avio_ctx)
		av_free(play->m_avio_ctx);
	if (play->m_io_buffer)
		av_free(play->m_io_buffer);

	return -1;
}

int start(avplay *play, int index)
{
	pthread_attr_t attr;
	int ret;

	/* 保存正在播放的索引号. */
	play->m_current_play_index = index;
	if (index > play->m_media_source->media_size)
		return -1;

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

	return 0;
}

void configure(avplay *play, void* render, int type)
{
	if (type == AUDIO_RENDER)
	{
		if (play->m_audio_render)
			play->m_audio_render->destory_audio(play->m_audio_render->ctx);
		play->m_audio_render = render;
	}
	if (type == VIDEO_RENDER)
	{
		if (play->m_video_render)
			play->m_video_render->destory_video(play->m_video_render->ctx);
		play->m_video_render = render;
	}
}

void wait_for_completion(avplay *play)
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
	/* 更改播放状态. */
	play->m_play_status = stoped;
}

void stop(avplay *play)
{
	play->m_abort = TRUE;

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
	wait_for_completion(play);

	queue_end(&play->m_audio_q);
	queue_end(&play->m_video_q);
	queue_end(&play->m_audio_dq);
	queue_end(&play->m_video_dq);

	/* 关闭解码器以及渲染器. */
	if (play->m_audio_ctx)
		avcodec_close(play->m_audio_ctx);
	if (play->m_video_ctx)
		avcodec_close(play->m_video_ctx);
	if (play->m_format_ctx)
		av_close_input_file(play->m_format_ctx);
	if (play->m_audio_convert_ctx)
		av_audio_convert_free(play->m_audio_convert_ctx);
	if (play->m_resample_ctx)
		audio_resample_close(play->m_resample_ctx);
	if (play->m_buf_size_mtx)
		pthread_mutex_destroy(&play->m_buf_size_mtx);
	if (play->m_audio_render)
	{
		free_audio_render(play->m_audio_render);
		play->m_audio_render = NULL;
	}
	if (play->m_video_render)
	{
		free_video_render(play->m_video_render);
		play->m_video_render = NULL;
	}
	if (play->m_io_buffer)
	{
		av_free(play->m_io_buffer);
		play->m_io_buffer = NULL;
	}
}

void pause(avplay *play)
{
	/* 更改播放状态. */
	play->m_play_status = paused;
}

void resume(avplay *play)
{
	/* 更改播放状态. */
	play->m_play_status = playing;
}

void seek(avplay *play, double sec)
{
	double duration = (double)play->m_format_ctx->duration / AV_TIME_BASE;

	/* 正在seek中, 只保存当前sec, 在seek完成后, 再seek. */
	if (play->m_seeking == SEEKING_FLAG || 
		(play->m_seeking > NOSEEKING_FLAG && play->m_seek_req))
	{
		play->m_seeking = sec * 1000;
		return ;
	}

	/* 正常情况下的seek. */
	if (play->m_format_ctx->duration <= 0)
	{
		uint64_t size = avio_size(play->m_format_ctx->pb);
		if (!play->m_seek_req)
		{
			play->m_seek_req = 1;
			play->m_seeking = SEEKING_FLAG;
			play->m_seek_pos = sec * size;
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
			play->m_seek_pos = sec * duration;
			play->m_seek_rel = 0;
			play->m_seek_flags &= ~AVSEEK_FLAG_BYTE;
			/* play->m_seek_flags |= AVSEEK_FLAG_BYTE; */
		}
	}
}

void volume(avplay *play, double vol)
{
	play->m_audio_render->audio_control(
		play->m_audio_render->ctx, vol);
}

double curr_play_time(avplay *play)
{
	return play->m_frame_timer;
}

double duration(avplay *play)
{
	return (double)play->m_format_ctx->duration / AV_TIME_BASE;
}

void destory(avplay *play)
{
	/* 如果正在播放, 则关闭播放. */
	if (play->m_play_status != stoped && play->m_play_status != inited)
	{
		/* 关闭数据源. */
		if (play->m_media_source && play->m_media_source->ctx)
			play->m_media_source->close(play->m_media_source->ctx);
		stop(play);
	}

	free(play);
}

static
void audio_copy(avplay *play, AVFrame *dst, AVFrame* src)
{
	*dst = *src;
	dst->data[0] = (uint8_t*) av_malloc(src->linesize[0]);
	dst->linesize[0] = src->linesize[0];
	dst->type = 0;
	assert(dst->data[0]);

	/*转换音频格式.*/
	if (play->m_audio_ctx->sample_fmt != AV_SAMPLE_FMT_S16)
	{
		if (!play->m_audio_convert_ctx)
			play->m_audio_convert_ctx = av_audio_convert_alloc(
					AV_SAMPLE_FMT_S16, 1, play->m_audio_ctx->sample_fmt, 1,
					NULL, 0);
		if (play->m_audio_convert_ctx)
		{
			const void *ibuf[6] = { src->data[0] };
			void *obuf[6] = { dst->data[0] };
			int istride[6] = { av_get_bytes_per_sample(play->m_audio_ctx->sample_fmt) };
			int ostride[6] = { 2 };
			int len = src->linesize[0] / istride[0];
			if (av_audio_convert(play->m_audio_convert_ctx, obuf, ostride, ibuf,
					istride, len) < 0)
			{
				assert(0);
			}
			src->linesize[0] = len * 2;
			memcpy(src->data[0], dst->data[0], src->linesize[0]);
			/* FIXME: existing code assume that data_size equals framesize*channels*2
			   remove this legacy cruft */
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
					play->m_audio_ctx->sample_rate, AV_SAMPLE_FMT_S16,
					play->m_audio_ctx->sample_fmt, 16, 10, 0, 0.f);
		}

		if (play->m_resample_ctx)
		{
			int samples = src->linesize[0] / (2 * play->m_audio_ctx->channels);
			dst->linesize[0] = audio_resample(play->m_resample_ctx,
					(short *) dst->data[0], (short *) src->data[0], samples)
					* 4;
		}
	}
	else
	{
		dst->linesize[0] = src->linesize[0];
		memcpy(dst->data[0], src->data[0], src->linesize[0]);
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
   double time = av_gettime() / 1000000.0;
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
	if (play->m_audio_st)
		bytes_per_sec = play->m_audio_st->codec->sample_rate * 2
				* FFMIN(play->m_audio_st->codec->channels, 2); /* 固定为2通道.	*/
	if (play->m_audio_current_pts_drift == 0.0f)
		play->m_audio_current_pts_drift = pts;
	if (bytes_per_sec)
		pts -= (double) hw_buf_size / bytes_per_sec;
	return pts - play->m_audio_current_pts_drift;
}

static
double video_clock(avplay *play)
{
	if (play->m_play_status == paused)
		return play->m_video_current_pts;
	return play->m_video_current_pts_drift + av_gettime() / 1000000.0f;
}

static
double external_clock(avplay *play)
{
	int64_t ti;
	ti = av_gettime();
	return play->m_external_clock + ((ti - play->m_external_clock_time) * 1e-6);
}

static
double master_clock(avplay *play)
{
	double val;

	if (play->m_av_sync_type == AV_SYNC_VIDEO_MASTER)
	{
		if (play->m_video_st)
			val = video_clock(play);
		else
			val = audio_clock(play);
	}
	else if (play->m_av_sync_type == AV_SYNC_AUDIO_MASTER)
	{
		if (play->m_audio_st)
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
			Sleep(4);
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
	int last_paused = play->m_play_status;
	AVStream *stream = NULL;

	for (; !play->m_abort;)
	{
		/* Initialize optional fields of a packet with default values.	*/
		av_init_packet(&packet);

		/* 如果暂定状态改变. */
		if (last_paused != play->m_play_status)
		{
			last_paused = play->m_play_status;
			if (play->m_play_status == playing)
				av_read_play(play->m_format_ctx);
			if (play->m_play_status == paused)
				play->m_read_pause_return = av_read_pause(play->m_format_ctx);
		}

		/* 如果seek未完成又来了新的seek请求. */
		if (play->m_seeking > NOSEEKING_FLAG)
			seek(play, (double)play->m_seeking / 1000.0f);

		if (play->m_seek_req)
		{
			int64_t seek_target = play->m_seek_pos * AV_TIME_BASE;
			int64_t seek_min    = /*play->m_seek_rel > 0 ? seek_target - play->m_seek_rel + 2:*/ INT64_MIN;
			int64_t seek_max    = /*play->m_seek_rel < 0 ? seek_target - play->m_seek_rel - 2:*/ INT64_MAX;
			int seek_flags = 0 & (~AVSEEK_FLAG_BYTE);
			int ns, hh, mm, ss;
			int tns, thh, tmm, tss;
			double frac = (double)play->m_seek_pos / ((double)play->m_format_ctx->duration / AV_TIME_BASE);

			tns = play->m_format_ctx->duration / AV_TIME_BASE;
			thh = tns / 3600.0f;
			tmm = (tns % 3600) / 60.0f;
			tss = tns % 60;

			ns = frac * tns;
			hh = ns / 3600.0f;
			mm = (ns % 3600) / 60.0f;
			ss = ns % 60;

			seek_target = frac * play->m_format_ctx->duration;
			if (play->m_format_ctx->start_time != AV_NOPTS_VALUE)
				seek_target += play->m_format_ctx->start_time;

			ret = avformat_seek_file(play->m_format_ctx, -1, 
				seek_min, seek_target, seek_max, seek_flags);

			if (ret < 0)
			{
				fprintf(stderr, "%s: error while seeking\n", play->m_format_ctx->filename);
			}
			else
			{
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
			}
			printf("Seek to %2.0f%% (%02d:%02d:%02d) of total duration (%02d:%02d:%02d)\n",
				frac * 100, hh, mm, ss, thh, tmm, tss);

			play->m_seek_req = 0;
		}

		/* 缓冲读满, 在这休眠让出cpu.	*/
		while (play->m_pkt_buffer_size > MAX_PKT_BUFFER_SIZE && !play->m_abort)
			Sleep(32);
		if (play->m_abort)
			break;

		/* Return 0 if OK, < 0 on error or end of file.	*/
		ret = av_read_frame(play->m_format_ctx, &packet);
		if (ret < 0)
		{
			Sleep(200);
			continue;
		}

		/* 更新缓冲字节数.	*/
		pthread_mutex_lock(&play->m_buf_size_mtx);
		play->m_pkt_buffer_size += packet.size;
		pthread_mutex_unlock(&play->m_buf_size_mtx);

		av_dup_packet(&packet);

		if (packet.stream_index == play->m_video_index)
			put_queue(&play->m_video_q, &packet);

		if (packet.stream_index == play->m_audio_index)
			put_queue(&play->m_audio_q, &packet);
	}

	/* 销毁media_source. */
	if (play->m_media_source)
	{
		if (play->m_media_source->destory && play->m_media_source->ctx)
			play->m_media_source->destory(play->m_media_source->ctx);
		free_media_source(play->m_media_source);
		play->m_media_source = NULL;
	}

	/* 设置为退出状态.	*/
	play->m_abort = TRUE;
	return NULL;
}

static
void* audio_dec_thrd(void *param)
{
	AVPacket pkt, pkt2;
	int ret, n, out_size = 0;
	AVFrame avframe = { 0 }, avcopy;
	avplay *play = (avplay*) param;
	int64_t v_start_time = 0;
	int64_t a_start_time = 0;
	int64_t diff_delay = 0;

	if (play->m_video_st && play->m_audio_st)
	{
		v_start_time = play->m_video_st->start_time;
		a_start_time = play->m_audio_st->start_time;

		diff_delay = ((v_start_time != AV_NOPTS_VALUE) && (a_start_time != AV_NOPTS_VALUE)) ? 
			v_start_time - a_start_time : 0;
	}

	for (; !play->m_abort;)
	{
		av_init_packet(&pkt);
		while (play->m_play_status == paused)
			Sleep(10);
		ret = get_queue(&play->m_audio_q, &pkt);
		if (ret != -1)
		{
			if (pkt.data == flush_pkt.data)
			{
				AVFrameList* lst = NULL;
				avcodec_flush_buffers(play->m_audio_ctx);
				while (play->m_audio_dq.m_size && !play->m_audio_dq.abort_request)
						Sleep(1);
				pthread_mutex_lock(&play->m_audio_dq.m_mutex);
				lst = play->m_audio_dq.m_first_pkt;
				for (; lst != NULL; lst = lst->next)
					lst->pkt.type = 1;	/*type为1表示skip.*/
				pthread_mutex_unlock(&play->m_audio_dq.m_mutex);
				continue;
			}

			/* 使用pts更新音频时钟. */
			if (pkt.pts != AV_NOPTS_VALUE)
				play->m_audio_clock = av_q2d(play->m_audio_st->time_base) * (pkt.pts - v_start_time);

			/* 计算pkt缓冲数据大小. */
			pthread_mutex_lock(&play->m_audio_q.m_mutex);
			play->m_pkt_buffer_size -= pkt.size;
			pthread_mutex_unlock(&play->m_audio_q.m_mutex);

			/* 解码音频. */
			out_size = 0;
			pkt2 = pkt;
			while (!play->m_abort)
			{
				int audio_out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
				ret = avcodec_decode_audio4(play->m_audio_ctx, &avframe,
						&audio_out_size, &pkt2);
				if (ret < 0)
				{
					printf("Audio error while decoding one frame!!!\n");
					break;
				}
				out_size += audio_out_size;
				pkt2.size -= ret;
				pkt2.data += ret;

				if (pkt2.size <= 0)
					break;
			}

			if (out_size > 0)
			{
				avframe.pts = pkt2.pts;
				avframe.best_effort_timestamp = pkt2.pts;

				/* 拷贝到avcopy用于添加到队列. */
				audio_copy(play, &avcopy, &avframe);

				/* 将计算的pts复制到avcopy.pts.	*/
				memcpy(&avcopy.pts, &play->m_audio_clock, sizeof(double));

				/* 计算下一个audio的pts值.	*/
				n = 2 * FFMIN(play->m_audio_ctx->channels, 2);

				play->m_audio_clock += ((double) avcopy.linesize[0]
						/ (double) (n * play->m_audio_ctx->sample_rate));

				/* 如果不是以音频同步为主, 则需要计算是否移除一些采样以同步到其它方式.	*/
				if (((play->m_av_sync_type == AV_SYNC_VIDEO_MASTER
						&& play->m_video_st)
						|| play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK))
				{
					/* 暂无实现.	*/
				}

				/* 防止内存过大.	*/
				chk_queue(play, &play->m_audio_dq, AVDECODE_BUFFER_SIZE);

				/* 丢到播放队列中.	*/
				put_queue(&play->m_audio_dq, &avcopy);
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
	int64_t diff_delay = 0;
	int64_t v_start_time = 0;
	int64_t a_start_time = 0;

	avframe = avcodec_alloc_frame();

	if (play->m_video_st && play->m_audio_st)
	{
		v_start_time = play->m_video_st->start_time;
		a_start_time = play->m_audio_st->start_time;

		diff_delay = ((v_start_time != AV_NOPTS_VALUE) && (a_start_time != AV_NOPTS_VALUE)) ? 
			v_start_time - a_start_time : 0;
	}

	for (; !play->m_abort;)
	{
		av_init_packet(&pkt);
		while (play->m_play_status == paused)
			Sleep(10);
		ret = get_queue(&play->m_video_q, (AVPacket*) &pkt);
		if (ret != -1)
		{
			if (pkt.data == flush_pkt.data)
			{
				AVFrameList* lst = NULL;

				avcodec_flush_buffers(play->m_video_ctx);

				while (play->m_video_dq.m_size && !play->m_video_dq.abort_request)
					Sleep(1);

				pthread_mutex_lock(&play->m_video_dq.m_mutex);
				lst = play->m_video_dq.m_first_pkt;
				for (; lst != NULL; lst = lst->next)
					lst->pkt.type = 1; /* type为1表示skip. */
				play->m_video_current_pos = -1;
				play->m_frame_last_dropped_pts = AV_NOPTS_VALUE;
				play->m_frame_last_duration = 0;
				play->m_frame_timer = (double) av_gettime() / 1000000.0f;
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
				ret = avcodec_decode_video2(play->m_video_ctx, avframe,
						&got_picture, &pkt2);
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
					play->m_frame_timer = (double) av_gettime() / 1000000.0f;

				/*
				 * 计算pts值.
				 */
				pts1 = (avcopy.best_effort_timestamp - a_start_time)
						* av_q2d(play->m_video_st->time_base);
				if (pts1 == AV_NOPTS_VALUE)
					pts1 = 0;
				pts = pts1;

				/* 如果以音频同步为主, 则在此判断是否进行丢包. */
				if (((play->m_av_sync_type == AV_SYNC_AUDIO_MASTER
						&& play->m_audio_st)
						|| play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK)
						&& (play->m_audio_st))
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
							printf("\nDROP: %3d drop a frame of pts is: %.3f\n",
								play->m_drop_frame_num, pts);
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
				play->m_frame_last_returned_time = av_gettime() / 1000000.0f;
				/* m_frame_last_filter_delay基本都是0吧. */
				play->m_frame_last_filter_delay = av_gettime() / 1000000.0f
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
				frame_delay = av_q2d(play->m_video_st->codec->time_base);
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
			if (!inited && play->m_audio_render)
			{
				inited = 1;
				/* 配置渲染器. */
				ret = play->m_audio_render->init_audio(&play->m_audio_render->ctx,
					FFMIN(play->m_audio_ctx->channels, 2), 16,
					play->m_audio_ctx->sample_rate, 0);
				if (ret != 0)
					inited = -1;
				else
				{
					/* 更改播放状态. */
					play->m_play_status = playing;
				}
				bytes_per_sec = play->m_audio_ctx->sample_rate
					* FFMIN(play->m_audio_ctx->channels, 2) 
					* av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
			}
			else if (!play->m_audio_render)
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
				if (inited == 1 && play->m_audio_render)
				{
					temp = play->m_audio_render->play_audio(
						play->m_audio_render->ctx,
						audio_frame.data[0] + play->m_audio_buf_index,
						play->m_audio_buf_size - play->m_audio_buf_index);
					play->m_audio_buf_index += temp;
					/* 如果缓冲已满, 则休眠一小会. */
					if (temp == 0)
					{
						if (play->m_audio_dq.m_size > 0)
						{
							if (((AVFrameList*) play->m_audio_dq.m_last_pkt)->pkt.type == 1)
								break;
						}
						Sleep(10);
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
			/* 如果最后丢弃帧的pts不为空, 且大于最后pts则
			 * 使用最后丢弃帧的pts值更新其它相关的pts值.
			 */
			if (play->m_frame_last_dropped_pts != AV_NOPTS_VALUE
					&& play->m_frame_last_dropped_pts > play->m_frame_last_pts)
			{
				update_video_pts(play, play->m_frame_last_dropped_pts,
						play->m_frame_last_dropped_pos);
				play->m_frame_last_dropped_pts = AV_NOPTS_VALUE;
			}
			pthread_mutex_unlock(&play->m_video_dq.m_mutex);
		}
		/* 获得下一帧视频. */
		ret = get_queue(&play->m_video_dq, &video_frame);
		if (ret != -1)
		{
			if (!inited && play->m_video_render)
			{
				inited = 1;
				ret = play->m_video_render->init_video(&play->m_video_render->ctx,
						play->m_video_render->user_data, play->m_video_ctx->width,
						play->m_video_ctx->height, play->m_video_ctx->pix_fmt);
				if (ret != 0)
					inited = -1;
				else
					play->m_play_status = playing;
			}

			if (video_frame.data[0] == flush_frm.data[0])
				continue;

			do
			{
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
				if (((play->m_av_sync_type == AV_SYNC_AUDIO_MASTER
						&& play->m_audio_st)
						|| play->m_av_sync_type == AV_SYNC_EXTERNAL_CLOCK))
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
							Sleep(0);
					}
				}

				/* 得到当前系统时间. */
				time = av_gettime() / 1000000.0f;

				/* 如果当前系统时间小于播放时间加延迟时间, 则过一会重试. */
				if (time < play->m_frame_timer + delay)
				{
					Sleep(1);
					continue;
				}

				/* 更新m_frame_timer. */
				if (delay > 0.0f)
					play->m_frame_timer +=
					delay * FFMAX(1, floor((time - play->m_frame_timer) / delay));

				pthread_mutex_lock(&play->m_video_dq.m_mutex);
				update_video_pts(play, current_pts, video_frame.pkt_pos);
				pthread_mutex_unlock(&play->m_video_dq.m_mutex);

				/* 计算下一帧的时间.  */
				if (play->m_video_dq.m_size > 0)
				{
					memcpy(&next_pts, 
						&(((AVFrameList*) play->m_video_dq.m_last_pkt)->pkt.pts), sizeof(double));
					duration = next_pts - current_pts;
				}
				else
				{
					memcpy(&duration, &video_frame.pkt_dts, sizeof(double));
				}

				if (play->m_audio_st && time > play->m_frame_timer + duration)
				{
					if (play->m_video_dq.m_size > 1)
					{
						pthread_mutex_lock(&play->m_video_dq.m_mutex);
						play->m_drop_frame_num++;
						pthread_mutex_unlock(&play->m_video_dq.m_mutex);
						printf("\nDROP: %3d drop a frame of pts is: %.3f\n",
							play->m_drop_frame_num, current_pts);
						break;
					}
				}

				if (diff < 1000)
				{
					frame_num++;
					diff_sum += fabs(diff);
					avg_diff = (double)diff_sum / frame_num;
				}
				printf("%7.3f A-V: %7.3f PTS: %7.3f A: %7.3f V: %7.3f AVG: %1.4f N: %lld\r",
					master_clock(play), diff, current_pts, audio_clock(play), video_clock(play), avg_diff/*delay*/, frame_num);

				/* 已经开始播放, 清空seeking的状态. */
				if (play->m_seeking == SEEKING_FLAG)
					play->m_seeking = NOSEEKING_FLAG;

				if (inited == 1 && play->m_video_render)
				{
					play->m_video_render->render_one_frame(
						play->m_video_render->ctx, &video_frame,
						play->m_video_ctx->pix_fmt);
					if (delay != 0)
						Sleep(4);
				}
				break;
			} while (TRUE);
			av_free(video_frame.data[0]);
		}
	}
	return NULL;
}


