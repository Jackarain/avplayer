//
// opengl_render.h
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __OPENGL_RENDER_H__
#define __OPENGL_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "video_render.h"

class opengl_render
	: public video_render
{
public:
	opengl_render();
	virtual ~opengl_render();

public:
	// 初始render.
	virtual bool init_render(void* ctx, int w, int h, int pix_fmt);

	// 渲染一帧.
	virtual bool render_one_frame(AVFrame* data, int pix_fmt);

	// 调整大小.
	virtual void re_size(int width, int height);

	// 设置宽高比.
	virtual void aspect_ratio(int srcw, int srch, bool enable_aspect);

	// 撤销render.
	virtual void destory_render();

private:
	bool InitGL(GLvoid);
	GLvoid KillGLWindow(GLvoid);

private:
	// 窗口着色描述表句柄.
	HGLRC m_hglrc;

	// 保存窗口句柄.
	HWND m_hwnd;

	// OpenGL渲染描述表句柄.
	HDC m_hdc;

	// 转换颜色YUV到RGB的上下文指针.
	SwsContext* m_swsctx;

	// 当前帧的RGB缓冲.
	uint8_t* m_framebuffer;

	// 当前渲染纹理.
	GLuint m_texture;

	// 视频宽.
	int m_image_width;

	// 视频高.
	int m_image_height;

	// 是否启用宽高比.
	bool m_keep_aspect;

	// 宽高比.
	float m_window_aspect;

	// 最后位置参数.
	RECT m_last_rect_client;

	// 当前宽.
	int m_current_width;

	// 当前高.
	int m_current_height;
};

#endif // __OPENGL_RENDER_H__
