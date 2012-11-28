/*
 * avplay.h
 * ~~~~~~~~
 *
 * Copyright (c) 2011 Jack (jack.wgm@gmail.com)
 *
 */

#ifndef AVPLAY_H_
#define AVPLAY_H_

#ifdef _MSC_VER
#	include <windows.h>
#	define inline
#	define __CRT__NO_INLINE
#	ifdef API_EXPORTS
#		define EXPORT_API __declspec(dllexport)
#	else
#		define EXPORT_API __declspec(dllimport)
#	endif
#else
#	define EXPORT_API
#endif

#include <pthread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
//#include <libavcodec/audioconvert.h>
#include <assert.h>
#include "defs.h"

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
#define VIDEO_RENDER_OPENGL		2
#define VIDEO_RENDER_SOFT		3

/* 队列.	*/
typedef struct _av_queue
{
	void *m_first_pkt, *m_last_pkt;
	int m_size; /* 队列大小.	*/
	int m_type; /* 队列类型.	*/
	int abort_request;
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
} av_queue;

/* 数据源结构分配和释放. */
EXPORT_API source_context* alloc_media_source(int type, const char *addition, int addition_len, int64_t size);
EXPORT_API void free_media_source(source_context *ctx);

/* 音频结构分配和释放. */
EXPORT_API ao_context* alloc_audio_render();
EXPORT_API void free_audio_render(ao_context *ctx);

/* 视频渲染结构分配和释放. */
EXPORT_API vo_context* alloc_video_render(void *user_data);
EXPORT_API void free_video_render(vo_context *ctx);

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
// 	AVAudioConvert *m_audio_convert_ctx;
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
	source_context *m_source_ctx;
	AVIOContext *m_avio_ctx;
	unsigned char *m_io_buffer;
	/* 当前音频渲染器.	*/
	ao_context *m_ao_ctx;
	/* 当前视频渲染器. */
	vo_context *m_vo_ctx;

	/* 当前音频播放buffer大小.	*/
	uint32_t m_audio_buf_size;

	/* 当前音频已经播放buffer的位置.	*/
	uint32_t m_audio_buf_index;
	int32_t m_audio_write_buf_size;
	double m_audio_current_pts_drift;
	double m_audio_current_pts_last;

	/* 播放状态. */
	play_status m_play_status;
	int m_rendering;

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
	double m_start_time;

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
 * @param sc source_context use to read media data.
 * @return 0 on success, a negative AVERROR on failure.
 * example:
 * avplayer* play = alloc_avplay_context();
 * int ret;
 * source_context sc = alloc_media_source(MEDIA_TYPE_FILE, "test.mp4", strlen("test.mp4") + 1, filesize("test.mp4"));
 * ret = initialize(play, sc);
 * if (ret != 0)
 *    return ret; // ERROR!
 */
EXPORT_API int initialize(avplay *play, source_context *sc);

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
 * @param fact at time, percent of duration.
 * @param index Specifies the index of the file to play.
 * @param Returns 0 if successful, or an error value otherwise. 
 */
EXPORT_API int av_start(avplay *play, double fact, int index);

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
EXPORT_API void av_stop(avplay *play);

/*
 * The Pause method pauses the current player.
 * @param play pointer to the player. 
 * @This function does not return a value.
 */
EXPORT_API void av_pause(avplay *play);

/*
 * The Resume function starts the player from the current position, after a Pause function call. 
 * @param play pointer to the player.
 * @This function does not return a value.
 */
EXPORT_API void av_resume(avplay *play);

/*
 * Moves the current seek percent.
 * @param play pointer to the player.
 * @param fact at time, percent of duration.
 * @This function does not return a value.
 */
EXPORT_API void av_seek(avplay *play, double fact);

/* Set audio volume.
 * @param play pointer to the player.
 * @param l is left channel.
 * @param r is right channel.
 * @This function does not return a value.
 */
EXPORT_API void av_volume(avplay *play, double l, double r);

/* Sets mute.
 * @param play pointer to the player.
 * @param vol is mute.
 * @This function does not return a value.
 */
EXPORT_API void mute_set(avplay *play, int s);

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

/*
 * Blurring algorithm to the input video.
 * @param frame pointer to the frame.
 * @param fw is the width of the video.
 * @param fh is the height of the video.
 * @param dx is the x start coordinates of the target location.
 * @param dy is the y start coordinates of the target location.
 * @param dcx is width of the target range.
 * @param dcx is height of the target range.
 */
EXPORT_API void blurring(AVFrame *frame,
	int fw, int fh, int dx, int dy, int dcx, int dcy);

/*
 * Alpha blend image mixing.
 * @param frame pointer to the frame.
 * @param rgba pointer to the RGBA image.
 * @param fw is the width of the video.
 * @param fh is the height of the video.
 * @param rgba_w is the width of the image.
 * @param rgba_h is the height of the image.
 * @param x is the x start coordinates of the target location.
 * @param y is the y start coordinates of the target location.
 */
EXPORT_API void alpha_blend(AVFrame *frame, uint8_t *rgba,
	int fw, int fh, int rgba_w, int rgba_h, int x, int y);


/*
 * Set log to file.
 * @param logfile write log to logfile file.
 */
EXPORT_API int logger_to_file(const char* logfile);

/*
 * Close log file.
 */
EXPORT_API int close_logger_file();

/*
 * Write formatted output to log.
 * @param format string that contains the text to be written to log.
 */
EXPORT_API int logger(const char *fmt, ...);

#ifdef  __cplusplus
}
#endif

#endif /* AVPLAY_H_ */
