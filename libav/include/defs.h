//
// common.h
// ~~~~~~~~
//
// Copyright (c) 2012 Jack (jack.wgm@gmail.com)
//

#ifndef __DEFS_H__
#define __DEFS_H__

#include "libavcodec/avcodec.h"

/* 视频播放结构定义. */
typedef struct vo_context
{
	int (*init_video)(void* ctx, int w, int h, int pix_fmt);
	int (*render_one_frame)(void *ctx, AVFrame* data, int pix_fmt);
	void (*re_size)(void *ctx, int width, int height);
	void (*aspect_ratio)(void *ctx, int srcw, int srch, int enable_aspect);
	int (*use_overlay)(void *ctx);
	void (*destory_video)(void *ctx);
	void *video_dev;
	void *user_data; /* for window hwnd. */
} vo_context;

/* 音频播放输出结构定义. */
typedef struct ao_context 
{
	int (*init_audio)(void *ctx, uint32_t channels, uint32_t bits_per_sample,
		uint32_t sample_rate, int format);
	int (*play_audio)(void *ctx, uint8_t *data, uint32_t size);
	void (*audio_control)(void *ctx, double vol);
	void (*destory_audio)(void *ctx);
	void *audio_dev;
} ao_context;

#endif // __DEFS_H__
