//
// video_out.h
// ~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __VIDEO_OUT_H__
#define __VIDEO_OUT_H__

#ifdef _MSC_VER
#ifdef VIDEO_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#endif

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int d3d_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int d3d_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void d3d_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void d3d_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int d3d_use_overlay(struct vo_context *ctx);
EXPORT_API void d3d_destory_render(struct vo_context *ctx);

EXPORT_API int ddraw_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int ddraw_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void ddraw_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void ddraw_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int ddraw_use_overlay(struct vo_context *ctx);
EXPORT_API void ddraw_destory_render(struct vo_context *ctx);

EXPORT_API int ogl_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int ogl_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void ogl_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void ogl_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int ogl_use_overlay(struct vo_context *ctx);
EXPORT_API void ogl_destory_render(struct vo_context *ctx);

EXPORT_API int gdi_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int gdi_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void gdi_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void gdi_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int gdi_use_overlay(struct vo_context *ctx);
EXPORT_API void gdi_destory_render(struct vo_context *ctx);

EXPORT_API int y4m_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int y4m_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void y4m_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void y4m_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int y4m_use_overlay(struct vo_context *ctx);
EXPORT_API void y4m_destory_render(struct vo_context *ctx);

EXPORT_API int sdl_init_video(struct vo_context *ctx, int w, int h, int pix_fmt);
EXPORT_API int sdl_render_one_frame(struct vo_context *ctx, AVFrame* data, int pix_fmt, double pts);
EXPORT_API void sdl_re_size(struct vo_context *ctx, int width, int height);
EXPORT_API void sdl_aspect_ratio(struct vo_context *ctx, int srcw, int srch, int enable_aspect);
EXPORT_API int sdl_use_overlay(struct vo_context *ctx);
EXPORT_API void sdl_destory_render(struct vo_context *ctx);

#ifdef  __cplusplus
}
#endif

#endif // __VIDEO_OUT_H__

