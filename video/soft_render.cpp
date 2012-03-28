#include "ins.h"

// 使用gdi+渲染.
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

#include "soft_render.h"


soft_render::soft_render()
   : m_hwnd(NULL)
   , m_hdc(NULL)
   , m_image_width(0)
   , m_image_height(0)
   , m_keep_aspect(true)
   , m_window_aspect(0.0f)
   , m_framebuffer(NULL)
   , m_swsctx(NULL)
{
   GdiplusStartupInput gdiplusStartupInput;
   GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

soft_render::~soft_render()
{
   GdiplusShutdown(m_gdiplusToken);
}

bool soft_render::init_render(void* ctx, int w, int h, int pix_fmt)
{
   m_hwnd = (HWND)ctx;
   m_image_width = w;
   m_image_height = h;

   m_window_aspect = (float)w / (float)h;

   // 获得内存dc.
   m_hdc = GetDC(m_hwnd);
   if (!m_hdc)
   {
      printf("GetDC failed.\n");
      return false;
   }

   m_swsctx = sws_getContext(m_image_width, m_image_height, PIX_FMT_YUV420P, m_image_width, m_image_height, 
      PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

   m_framebuffer = (uint8_t*)malloc(w * h * sizeof(uint32_t));

   return true;
}

bool soft_render::render_one_frame(AVFrame* data, int pix_fmt)
{
   Graphics Gs(m_hdc);

   uint8_t* pixels[3] = { data->data[0], 
      data->data[1], 
      data->data[2] };

   int linesize[3] = { data->linesize[0], 
      data->linesize[1], 
      data->linesize[2] };

   AVFrame* pic = avcodec_alloc_frame();
   avpicture_fill((AVPicture*)pic, m_framebuffer, PIX_FMT_BGR24, m_image_width, m_image_height);
   sws_scale(m_swsctx, pixels, linesize, 0, m_image_height, pic->data, pic->linesize);
   Bitmap bmp(m_image_width, m_image_height, pic->linesize[0], PixelFormat24bppRGB, pic->data[0]);

   RECT rect_client = { 0 };
   GetClientRect(m_hwnd, &rect_client);

   int width = rect_client.right - rect_client.left;
   int height = rect_client.bottom - rect_client.top;

   if (m_keep_aspect)
   {
      GetClientRect(m_hwnd, &rect_client);
      int win_width = width = rect_client.right - rect_client.left;
      int win_height = height = rect_client.bottom - rect_client.top;

      int tmpheight = ((float)width / m_window_aspect);
      tmpheight += tmpheight % 2;
      if(tmpheight > height)
      {
         width = ((float)height * m_window_aspect);
         width += width % 2;
      }
      else 
      {
         height = tmpheight;
      }

      // 居中对齐.
      rect_client.left += ((win_width - width) / 2);
      rect_client.top += ((win_height - height) / 2);
      rect_client.bottom -= ((win_height - height) / 2);
      rect_client.right -= ((win_width - width) / 2);
   }

   Gs.DrawImage(&bmp, rect_client.left, rect_client.top, 
      rect_client.right - rect_client.left, rect_client.bottom - rect_client.top);

   av_free(pic);

   return true;
}

void soft_render::re_size(int width, int height)
{

}

void soft_render::aspect_ratio(int srcw, int srch, bool enable_aspect)
{
   if (enable_aspect)
   {
      enable_aspect = true;
      m_window_aspect = (float)srcw / (float)srch;
   }
   else
   {
      enable_aspect = false;
   }
}

void soft_render::destory_render()
{
   if (m_hwnd && m_hdc)
   {
      ReleaseDC(m_hwnd, m_hdc);
      m_hwnd = NULL;
      m_hdc = NULL;
   }

   if (m_framebuffer)
   {
      free(m_framebuffer);
      m_framebuffer = NULL;
   }

   if (m_swsctx)
   {
      sws_freeContext(m_swsctx);
      m_swsctx = NULL;
   }
}

