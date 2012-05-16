//
// avplay.h
// ~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef AVPLAY_H_
#define AVPLAY_H_

#ifdef _MSC_VER
#include <windows.h>
#define inline
#define __CRT__NO_INLINE
#ifdef API_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

#include "pthread.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavcodec/audioconvert.h"
#include <assert.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* 播放器状态. */
typedef enum play_status
{
	inited, playing, paused, completed, stoped
} play_status;

/* 用于config_render参数表示所配置的render.  */
#define MEDIA_SOURCE			0
#define AUDIO_RENDER			1
#define VIDEO_RENDER			2

/* 用于标识渲染器类型. */
#define VIDEO_RENDER_D3D		0
#define VIDEO_RENDER_DDRAW		1
#define VIDEO_RENDER_OPENGL	2
#define VIDEO_RENDER_SOFT		3

/* 队列.	*/
typedef struct av_queue
{
	void *m_first_pkt, *m_last_pkt;
	int m_size; /* 队列大小.	*/
	int m_type; /* 队列类型.	*/
	int abort_request;
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
} av_queue;

/* 媒体数据源接口. */
#define MEDIA_TYPE_FILE	0
#define MEDIA_TYPE_BT	1
#define MEDIA_TYPE_HTTP 2
#define MEDIA_TYPE_RTSP 3

/* 媒体文件信息. */
typedef struct media_info 
{
	char *name;			// 文件名.
	int64_t start_pos;	// 起始位置.
	int64_t file_size;	// 文件大小.
} media_info;

/* 媒体数据源接口. */
typedef struct media_source
{
	int (*init_source)(void **ctx, char *data, int len, char *save_path);
	/*
	 * name 输出视频名称, 需要调用者分配内存.
	 * pos  输入查询的序号, 从0开始; 输出为视频在bt下载中的起始位置偏移.
	 * size 输入name缓冲区长度, 输出视频大小.
	 * 返回torrent中的视频媒体个数, 返回-1表示出错.
	 */
	int (*bt_media_info)(void *ctx, char *name, int64_t *pos, int64_t *size);
	int (*read_data)(void *ctx, char* buff, int64_t offset, int buf_size);
	void (*close)(void *ctx);
	void (*destory)(void *ctx);
	void *ctx;
	/*
	 * 数据类型, 可以是以下值
	 * MEDIA_TYPE_FILE、MEDIA_TYPE_BT、MEDIA_TYPE_HTTP、MEDIA_TYPE_RTSP 
	 * 说明: 由于http和rtsp直接使用了ffmpeg的demux, 所以就无需初始
	 * 化上面的各函数指针.
	 */
	int type;
	/*
	 * 附加数据.
	 * 如果类型是MEDIA_TYPE_FILE, 则此指向文件名.
	 * 如果类型是MEDIA_TYPE_BT, 则此指向bt种子数据.
	 * 如果类型是MEDIA_TYPE_HTTP, 则指向url.
	 * 如果类型是MEDIA_TYPE_RTSP, 则指向url.
	 */
	char *data;
	int data_len;
	/* 媒体文件信息.	*/
	media_info *media;
	/*
	 * 媒体文件信息个数, 主要为BT文件播放设计, 因为一个torrent中可
	 * 能存在多个视频文件.
	 */
	int media_size;
	/* 当前播放数据偏移, 绝对位置.*/
	int64_t offset;
} media_source;

/* 数据源结构分配和释放. */
EXPORT_API media_source* alloc_media_source(int type, char *addition, int addition_len, int64_t size);
EXPORT_API void free_media_source(media_source *src);


/* 音频播放接口. */
typedef struct audio_render
{
	int (*init_audio)(void **ctx, uint32_t channels, uint32_t bits_per_sample,
		uint32_t sample_rate, int format);
	int (*play_audio)(void *ctx, uint8_t *data, uint32_t size);
	void (*audio_control)(void *ctx, double vol);
	void (*destory_audio)(void *ctx);
	void *ctx;
} audio_render;

/* 音频结构分配和释放. */
EXPORT_API audio_render* alloc_audio_render();
EXPORT_API void free_audio_render(audio_render *render);

/* 视频播放接口. */
typedef struct video_render
{
	int (*init_video)(void **ctx, void* user_data,int w, int h, int pix_fmt);
	int (*render_one_frame)(void *ctx, AVFrame* data, int pix_fmt);
	void (*re_size)(void *ctx, int width, int height);
	void (*aspect_ratio)(void *ctx, int srcw, int srch, int enable_aspect);
	int (*use_overlay)(void *ctx);
	void (*destory_video)(void *ctx);
	void *ctx;
	void *user_data; /* for window hwnd. */
} video_render;

/* 视频渲染结构分配和释放. */
EXPORT_API video_render* alloc_video_render(void *user_data);
EXPORT_API void free_video_render(video_render *render);


/* 计算视频实时帧率和实时码率的时间单元. */
#define MAX_CALC_SEC 5

typedef struct avplay
{
	/* 文件打开指针. */
	AVFormatContext *m_format_ctx;

	/* 音视频队列.	*/
	av_queue m_audio_q;
	av_queue m_video_q;
	av_queue m_audio_dq;
	av_queue m_video_dq;

	/* 各解码渲染线程.	*/
	pthread_t m_audio_dec_thrd;
	pthread_t m_video_dec_thrd;
	pthread_t m_audio_render_thrd;
	pthread_t m_video_render_thrd;
	pthread_t m_read_pkt_thrd;

	/* 重采样音频指针.	*/
	struct SwsContext *m_swsctx;
	AVAudioConvert *m_audio_convert_ctx;
	ReSampleContext *m_resample_ctx;

	/* 音频和视频的AVStream、AVCodecContext指针和index.	*/
	AVCodecContext *m_audio_ctx;
	AVCodecContext *m_video_ctx;
	AVStream *m_audio_st;
	AVStream *m_video_st;
	int m_audio_index;
	int m_video_index;

	/* 读取数据包占用缓冲大小.	*/
	long volatile m_pkt_buffer_size;
	pthread_mutex_t m_buf_size_mtx;

	/* 同步类型. */
	int m_av_sync_type;

	/*
	 * 用于计算视频播放时间
	 * 即:  m_video_current_pts_drift = m_video_current_pts - time();
	 *      m_video_current_pts是当前播放帧的pts时间, 所以在pts向前推进
	 *      的同时, time也同样在向前推进, 所以pts_drift基本保存在一个
	 *      time_base范围内浮动.
	 * 播放时间 = m_video_current_pts_drift - time()
	 */
	double m_video_current_pts_drift;
	double m_video_current_pts;

	/* 以下变量用于计算音视频同步.	*/
	double m_frame_timer;
	double m_frame_last_pts;
	double m_frame_last_duration;
	double m_frame_last_delay;
	double m_frame_last_filter_delay;
	double m_frame_last_dropped_pts;
	double m_frame_last_returned_time;
	int64_t m_frame_last_dropped_pos;
	int64_t m_video_current_pos;
	int m_drop_frame_num;

	/* seek实现. */
	int m_read_pause_return;
	int m_seek_req;
	int m_seek_flags;
	int64_t m_seek_pos;
	int64_t m_seek_rel;
	int m_seek_by_bytes;
	int m_seeking;

	/* 最后一个解码帧的pts, 解码帧缓冲大小为2, 也就是当前播放帧的下一帧.	*/
	double m_audio_clock;
	double m_video_clock;
	double m_external_clock;
	double m_external_clock_time;

	/* 当前数据源读取器. */
	media_source *m_media_source;
	AVIOContext *m_avio_ctx;
	unsigned char *m_io_buffer;
	/* 当前音频渲染器.	*/
	audio_render *m_audio_render;
	/* 当前视频渲染器. */
	video_render *m_video_render;

	/* 当前音频播放buffer大小.	*/
	uint32_t m_audio_buf_size;

	/* 当前音频已经播放buffer的位置.	*/
	uint32_t m_audio_buf_index;
	int32_t m_audio_write_buf_size;
	double m_audio_current_pts_drift;

	/* 播放状态. */
	play_status m_play_status;

	/* 实时视频输入位率. */
	int m_enable_calc_video_bite;
	int m_real_bit_rate;
	int m_read_bytes[MAX_CALC_SEC]; /* 记录5秒内的字节数. */
	int m_last_vb_time;
	int m_vb_index;

	/* 帧率. */
	int m_enable_calc_frame_rate;
	int m_real_frame_rate;
	int m_frame_num[MAX_CALC_SEC]; /* 记录5秒内的帧数. */
	int m_last_fr_time;
	int m_fr_index;

	/* 正在播放的索引, 只用于BT文件播放. */
	int m_current_play_index;

	/* 停止标志.	*/
	int m_abort;

} avplay;

/*
 * Assign a player structural context.
 * @If the function succeeds, the return value is a pointer to the avplay,
 * If the function fails, the return value is NULL.
 */
EXPORT_API avplay* alloc_avplay_context();

/*
 * The release of the structural context of the player.
 * @param ctx allocated by alloc_avplay_context.
 * @This function does not return a value.
 */
EXPORT_API void free_avplay_context(avplay *ctx);

/*
 * Initialize the player.
 * @param play pointer to user-supplied avplayer (allocated by alloc_avplay_context).
 * @param filename filename Name of the stream to open.
 * @return 0 on success, a negative AVERROR on failure.
 * example:
 * avplayer* play = alloc_avplay_context();
 * int ret;
 * 
 * ret = initialize(play, "/tmp/test.avi");
 * if (ret != 0)
 *    return ret; // ERROR!
 */
EXPORT_API int initialize(avplay *play, media_source *ms);

/*
 * The Configure render or source to palyer.
 * @param play pointer to the player. 
 * @param param video render or audio render or media_source.
 * @param type Specifies render type, MEDIA_SOURCE	or AUDIO_RENDER VIDEO_RENDER.
 * @This function does not return a value.
 */
EXPORT_API void configure(avplay *play, void *param, int type);

/*
 * The start action player to play. 
 * @param play pointer to the player. 
 * @param index Specifies the index of the file to play.
 * @param Returns 0 if successful, or an error value otherwise. 
 */
EXPORT_API int start(avplay *play, int index);

/*
 * Wait for playback to complete.
 * @param play pointer to the player. 
 * @This function does not return a value.
 */
EXPORT_API void wait_for_completion(avplay *play);

/*
 * The Stop function stops playing the media. 
 * @param play pointer to the player. 
 * @This function does not return a value.
 */
EXPORT_API void stop(avplay *play);

/*
 * The Pause method pauses the current player.
 * @param play pointer to the player. 
 * @This function does not return a value.
 */
EXPORT_API void pause(avplay *play);

/*
 * The Resume function starts the player from the current position, after a Pause function call. 
 * @param play pointer to the player.
 * @This function does not return a value.
 */
EXPORT_API void resume(avplay *play);

/*
 * Moves the current seek second.
 * @param play pointer to the player.
 * @param sec at time, second.
 * @This function does not return a value.
 */
EXPORT_API void seek(avplay *play, double sec);

/* Set audio volume.
 * @param play pointer to the player.
 * @param vol is volume.
 * @This function does not return a value.
 */
EXPORT_API void volume(avplay *play, double vol);

/*
 * The current playback time position
 * @param play pointer to the player.
 * @return current play time position, a negative on failure.
 */
EXPORT_API double curr_play_time(avplay *play);

/*
 * The Duration function return the playing duration of the media, in second units.
 * @param play pointer to the player.
 * @return the playing duration of the media, in second units.
 */
EXPORT_API double duration(avplay *play);

/*
 * Destroys an player. 
 * @param play pointer to the player.
 * @This function does not return a value.
 */
EXPORT_API void destory(avplay *play);

/*
 * Allows the calculation of the real-time frame rate.
 * @param play pointer to the player.
 * @This function does not return a value.
 */
EXPORT_API void enable_calc_frame_rate(avplay *play);

/*
 * Allows the calculation of the real-time bit rate.
 * @param play pointer to the player.
 * @This function does not return a value.
 */
EXPORT_API void enable_calc_bit_rate(avplay *play);

/*
 *	Get current real-time bit rate.
 * @param play pointer to the player.
 * @This function return bit rate(kpbs).
 */
EXPORT_API int current_bit_rate(avplay *play);

/*
 *	Get current real-time frame rate.
 * @param play pointer to the player.
 * @This function return frame rate(fps).
 */
EXPORT_API int current_frame_rate(avplay *play);


#ifdef  __cplusplus
}
#endif

#endif /* AVPLAY_H_ */
