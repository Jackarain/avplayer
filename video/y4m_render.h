//
// y4m_render.h
// ~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __Y4M_RENDER_H__
#define __Y4M_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "video_render.h"


// 用于调试测试输出视频文件.
class y4m_render
	: public video_render
{
public:
	y4m_render(void);
	~y4m_render(void);

public:
	/* 初始render.	*/
	virtual bool init_render(void* ctx, int w, int h, int pix_fmt);
	virtual bool init_render(void* ctx, int w, int h, int pix_fmt, float fps);

	/* 渲染一帧.	*/
	virtual bool render_one_frame(AVFrame* data, int pix_fmt);

	/* 调整大小.	*/
	virtual void re_size(int width, int height);

	/* 设置宽高比.	*/
	virtual void aspect_ratio(int srcw, int srch, bool enable_aspect);

	/* 撤销render.		*/
	virtual void destory_render();

private:
	FILE *m_yuv_out;
	char *m_image;
	int m_image_width;
	int m_image_height;
};

#endif // __Y4M_RENDER_H__

