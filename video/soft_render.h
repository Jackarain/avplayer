//
// soft_render.h
// ~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __SOFT_RENDER_H__
#define __SOFT_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "video_render.h"

class soft_render
   : public video_render
{
public:
   soft_render();
   virtual ~soft_render();

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
   // 窗口句柄.
   HWND m_hwnd;

   // 画面dc.
   HDC m_hdc;

   // GDI+的指针.
   ULONG_PTR m_gdiplusToken;

   // 帧缓冲.
   uint8_t* m_framebuffer;

   // 用于转换帧数据格式.
   SwsContext* m_swsctx;

   // 视频宽.
   int m_image_width;

   // 视频高.
   int m_image_height;

   // 是否启用宽高比.
   bool m_keep_aspect;

   // 宽高比.
   float m_window_aspect;
};

#endif // __SOFT_RENDER_H__

