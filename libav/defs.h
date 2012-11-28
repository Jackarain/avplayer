//
// defs.h
// ~~~~~~
//
// Copyright (c) 2012 Jack (jack.wgm@gmail.com)
//

#ifndef __DEFS_H__
#define __DEFS_H__

#include <stdint.h>

// #include "libavcodec/avcodec.h"
struct AVFrame;
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

/*  */
typedef struct download_info
{
	int speed;			// 下载下载速率.
	int limit_speed;	// 限制下载速率.
} download_info;


/* 媒体数据源接口. */
typedef struct source_context
{
	int (*init_source)(void *ctx);
	/*
	 * name 输出视频名称, 需要调用者分配内存.
	 * pos  输入查询的序号, 从0开始; 输出为视频在bt下载中的起始位置偏移.
	 * size 输入name缓冲区长度, 输出视频大小.
	 * 返回torrent中的视频媒体个数, 返回-1表示出错.
	 */
	int (*bt_media_info)(void *ctx, char *name, int64_t *pos, int64_t *size);
	int64_t (*read_data)(void *ctx, char* buff, int64_t offset, size_t buf_size);
	void (*close)(void *ctx);
	void (*destory)(void *ctx);
	/* io_dev是保存内部用于访问实际数据的对象指针. */
	void *io_dev;
	/*
	 * 数据类型, 可以是以下值
	 * MEDIA_TYPE_FILE、MEDIA_TYPE_BT、MEDIA_TYPE_HTTP、MEDIA_TYPE_RTSP 
	 * 说明: 由于http和rtsp直接使用了ffmpeg的demux, 所以就无需初始
	 * 化上面的各函数指针.
	 */
	int type;
	/*
	 * 如果类型是MEDIA_TYPE_BT, 则此指向bt种子数据.
	 */
	char *torrent_data;
	int torrent_len;
	char *save_path;

	/* torrent中的媒体文件信息, 只有在打开
	 * torrent之后, 这里面才可能有数据.
	 */
	media_info *media;
	/*
	 * 媒体文件信息个数, 主要为BT文件播放设计, 因为一个torrent中可
	 * 能存在多个视频文件.
	 */
	int media_size;
	/* 记录当前播放数据偏移, 绝对位置. */
	int64_t offset;
	/* 用于控制和获得下载信息. */
	download_info info;
	/* 当前退出标识, 退出时为true */
	int abort;
} source_context;

/* 视频播放结构定义. */
typedef struct vo_context
{
	int (*init_video)(void* ctx, int w, int h, int pix_fmt);
	int (*render_one_frame)(void *ctx, AVFrame* data, int pix_fmt, double pts);
	void (*re_size)(void *ctx, int width, int height);
	void (*aspect_ratio)(void *ctx, int srcw, int srch, int enable_aspect);
	int (*use_overlay)(void *ctx);
	void (*destory_video)(void *ctx);
	void *video_dev;
	void *user_data;	/* for window hwnd. */
	void *user_ctx;		/* for user context. */
	float fps;			/* fps */
} vo_context;

/* 音频播放输出结构定义. */
typedef struct ao_context 
{
	int (*init_audio)(void *ctx, uint32_t channels, uint32_t bits_per_sample,
		uint32_t sample_rate, int format);
	int (*play_audio)(void *ctx, uint8_t *data, uint32_t size);
	void (*audio_control)(void *ctx, double l, double r);
	void (*mute_set)(void *ctx, int s);
	void (*destory_audio)(void *ctx);
	void *audio_dev;
} ao_context;

#endif // __DEFS_H__
