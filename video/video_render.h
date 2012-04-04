//
// video_render.h
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __VIDEO_RENDER_H__
#define __VIDEO_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#define FOURCC_PIX_FMT_YUV420P MAKEFOURCC('I','4','2','0') /* 16  YVU420 planar */
#define FOURCC_PIX_FMT_IYUV    MAKEFOURCC('I','Y','U','V') /* 16  IYUV   planar */
#define FOURCC_PIX_FMT_YVU410  MAKEFOURCC('Y','V','U','9') /*  9  YVU 4:1:0     */
#define FOURCC_PIX_FMT_YVU420  MAKEFOURCC('Y','V','1','2') /* 12  YVU 4:2:0     */
#define FOURCC_PIX_FMT_YUY2    MAKEFOURCC('Y','U','Y','2') /* 16  YUYV 4:2:2 */
#define FOURCC_PIX_FMT_UYVY    MAKEFOURCC('U','Y','V','Y') /* 16  UYVY 4:2:2 */
#define FOURCC_PIX_FMT_YVYU    MAKEFOURCC('Y','V','Y','U') /* 16  UYVY 4:2:2 */

#define FOURCC_PIX_FMT_RGB332  MAKEFOURCC('R','G','B','1') /*  8  RGB-3-3-2     */
#define FOURCC_PIX_FMT_RGB555  MAKEFOURCC('R','G','B','O') /* 16  RGB-5-5-5     */
#define FOURCC_PIX_FMT_RGB565  MAKEFOURCC('R','G','B','P') /* 16  RGB-5-6-5     */
#define FOURCC_PIX_FMT_RGB555X MAKEFOURCC('R','G','B','Q') /* 16  RGB-5-5-5 BE  */
#define FOURCC_PIX_FMT_RGB565X MAKEFOURCC('R','G','B','R') /* 16  RGB-5-6-5 BE  */
#define FOURCC_PIX_FMT_BGR15   MAKEFOURCC('B','G','R',15)  /* 15  BGR-5-5-5     */
#define FOURCC_PIX_FMT_RGB15   MAKEFOURCC('R','G','B',15)  /* 15  RGB-5-5-5     */
#define FOURCC_PIX_FMT_BGR16   MAKEFOURCC('B','G','R',16)  /* 32  BGR-5-6-5     */
#define FOURCC_PIX_FMT_RGB16   MAKEFOURCC('R','G','B',16)  /* 32  RGB-5-6-5     */
#define FOURCC_PIX_FMT_BGR24   MAKEFOURCC('B','G','R',24)  /* 24  BGR-8-8-8     */
#define FOURCC_PIX_FMT_RGB24   MAKEFOURCC('R','G','B',24)  /* 24  RGB-8-8-8     */
#define FOURCC_PIX_FMT_BGR32   MAKEFOURCC('B','G','R',32)  /* 32  BGR-8-8-8-8   */
#define FOURCC_PIX_FMT_RGB32   MAKEFOURCC('R','G','B',32)  /* 32  RGB-8-8-8-8   */
#define FOURCC_PIX_FMT_BGR     (('B'<<24)|('G'<<16)|('R'<<8))
#define FOURCC_PIX_FMT_BGR8    (FOURCC_PIX_FMT_BGR|8)

class video_render
{
public:
   video_render() {}
   virtual ~video_render() {}

public:
   // 初始render.
   virtual bool init_render(void* ctx, int w, int h, int pix_fmt) = 0;

   // 渲染一帧.
   virtual bool render_one_frame(AVFrame* data, int pix_fmt) = 0;

   // 调整大小.
   virtual void re_size(int width, int height) = 0;

   // 设置宽高比.
   virtual void aspect_ratio(int srcw, int srch, bool enable_aspect) = 0;

   // 是否使用overlay模式渲染.
   virtual bool use_overlay() { return false; }

   // 撤销render.
   virtual void destory_render() = 0;
};

#endif // __VIDEO_RENDER_H__
