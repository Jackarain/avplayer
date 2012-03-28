//
// ddraw_render.h
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __DDRAW_RENDER_H__
#define __DDRAW_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "video_render.h"

typedef struct directx_fourcc
{
	char*  img_format_name;      // human readable name
	uint32_t img_format;         // as MPlayer image format
	uint32_t drv_caps;           // what hw supports with this format
	DDPIXELFORMAT ddpf_overlay;  // as Directx Sourface description
} directx_fourcc;

extern directx_fourcc dx_fourcc[];
#define DDPF_NUM_FORMATS (sizeof(dx_fourcc) / sizeof(dx_fourcc[0]))

class ddraw_render
	: public video_render
{
public:
	ddraw_render();
	virtual ~ddraw_render();

public:
	// 初始render.
	virtual bool init_render(void* ctx, int w, int h, int pix_fmt);

	// 渲染一帧.
	virtual bool render_one_frame(AVFrame* data, int pix_fmt);

	// 调整大小.
	virtual void re_size(int width, int height);

	// 设置宽高比.
	virtual void aspect_ratio(int srcw, int srch, bool enable_aspect);

	// 是否使用overlay模式渲染.
	virtual bool use_overlay() { return m_support_double_buffer; }

	// 撤销render.
	virtual void destory_render();

private:
	void check_overlay();
	bool create_overlay_face(uint32_t fmt, bool must_overlay);
	bool manage_display();
	void* copy_yuv_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride);
	bool draw_slice(uint8_t* dst, uint8_t* src[], int stride[], int dstStride, int w, int h, int x, int y);

private:
	// ddraw指针.
	LPDIRECTDRAW7 m_ddraw;

	// 主表层.
	LPDIRECTDRAWSURFACE7 m_main_face;

	// 离屏表层.
	LPDIRECTDRAWSURFACE7 m_back_face;

	// overlay表层.
	LPDIRECTDRAWSURFACE7 m_overlay_face;

	// 裁剪器.
	LPDIRECTDRAWCLIPPER m_clipper;

	// DDRAW是否支持overlay.
	bool m_support_overlay;

	// DDRAW是否支持doublebuffer.
	bool m_support_double_buffer;

	// DDRAW是否支持fourcc.
	bool m_can_blit_fourcc;

	// colorkey for our surface.
	DWORD m_destcolorkey;

	// windowcolor == colorkey
	DWORD m_windowcolor;

	// 转换颜色上下文指针.
	SwsContext* m_swsctx;

	// 转换颜色buffer.
	uint8_t* m_swsbuffer;

	// 当前表层像素格式.
	DWORD m_pix_fmt;

	// 视频数据像素格式.
	int m_video_fmt;

	// 当前显示器的区域.
	RECT m_monitor_rect;

	// 当前区域.
	RECT m_last_rect;

	// 保持宽高.
	bool m_keep_aspect;

	// 宽高比.
	float m_window_aspect;

	// 宽.
	int m_image_width;

	// 高.
	int m_image_height;

	// 窗口句柄.
	HWND m_hWnd;
};

#endif // __DDRAW_RENDER_H__

