//
// video_out.h
// ~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __VIDEO_OUT_H__
#define __VIDEO_OUT_H__

#ifdef VIDEO_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int d3d_init_video(void *ctx, int w, int h, int pix_fmt);
EXPORT_API int d3d_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void d3d_re_size(void *ctx, int width, int height);
EXPORT_API void d3d_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int d3d_use_overlay(void *ctx);
EXPORT_API void d3d_destory_render(void *ctx);

EXPORT_API int ddraw_init_video(void *ctx, int w, int h, int pix_fmt);
EXPORT_API int ddraw_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void ddraw_re_size(void *ctx, int width, int height);
EXPORT_API void ddraw_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int ddraw_use_overlay(void *ctx);
EXPORT_API void ddraw_destory_render(void *ctx);

EXPORT_API int ogl_init_video(void *ctx, int w, int h, int pix_fmt);
EXPORT_API int ogl_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void ogl_re_size(void *ctx, int width, int height);
EXPORT_API void ogl_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int ogl_use_overlay(void *ctx);
EXPORT_API void ogl_destory_render(void *ctx);


#ifdef  __cplusplus
}
#endif

#endif // __VIDEO_OUT_H__

