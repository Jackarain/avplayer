#include "ins.h"
#include "video_out.h"
#include "d3d_render.h"
#include "ddraw_render.h"
#include "opengl_render.h"

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int d3d_init_video(void **ctx, void* user_data, int w, int h, int pix_fmt)
{
   d3d_render* d3d = new d3d_render;
   *ctx = (void*)d3d;
   return d3d->init_render(user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int d3d_render_one_frame(void *ctx, AVFrame* data, int pix_fmt)
{
   d3d_render* d3d = (d3d_render*)ctx;
   return d3d->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void d3d_re_size(void *ctx, int width, int height)
{
   d3d_render* d3d = (d3d_render*)ctx;
   d3d->re_size(width, height);
}

EXPORT_API void d3d_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
   d3d_render* d3d = (d3d_render*)ctx;
   d3d->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int d3d_use_overlay(void *ctx)
{
   d3d_render* d3d = (d3d_render*)ctx;
   return d3d->use_overlay() ? 0 : -1;
}

EXPORT_API void d3d_destory_render(void *ctx)
{
   d3d_render* d3d = (d3d_render*)ctx;
   d3d->destory_render();
   delete d3d;
}


EXPORT_API int ddraw_init_video(void **ctx, void* user_data, int w, int h, int pix_fmt)
{
	ddraw_render* ddraw = new ddraw_render;
	*ctx = (void*)ddraw;
	return ddraw->init_render(user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int ddraw_render_one_frame(void *ctx, AVFrame* data, int pix_fmt)
{
	ddraw_render* ddraw = (ddraw_render*)ctx;
	return ddraw->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void ddraw_re_size(void *ctx, int width, int height)
{
	ddraw_render* ddraw = (ddraw_render*)ctx;
	ddraw->re_size(width, height);
}

EXPORT_API void ddraw_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
	ddraw_render* ddraw = (ddraw_render*)ctx;
	ddraw->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int ddraw_use_overlay(void *ctx)
{
	ddraw_render* ddraw = (ddraw_render*)ctx;
	return ddraw->use_overlay() ? 0 : -1;
}

EXPORT_API void ddraw_destory_render(void *ctx)
{
	ddraw_render* ddraw = (ddraw_render*)ctx;
	ddraw->destory_render();
	delete ddraw;
}

EXPORT_API int ogl_init_video(void **ctx, void* user_data, int w, int h, int pix_fmt)
{
	opengl_render* ogl = new opengl_render;
	*ctx = (void*)ogl;
	return ogl->init_render(user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int ogl_render_one_frame(void *ctx, AVFrame* data, int pix_fmt)
{
	opengl_render* ogl = (opengl_render*)ctx;
	return ogl->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void ogl_re_size(void *ctx, int width, int height)
{
	opengl_render* ogl = (opengl_render*)ctx;
	ogl->re_size(width, height);
}

EXPORT_API void ogl_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
	opengl_render* ogl = (opengl_render*)ctx;
	ogl->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int ogl_use_overlay(void *ctx)
{
	opengl_render* ogl = (opengl_render*)ctx;
	return ogl->use_overlay() ? 0 : -1;
}

EXPORT_API void ogl_destory_render(void *ctx)
{
	opengl_render* ogl = (opengl_render*)ctx;
	ogl->destory_render();
	delete ogl;
}

#ifdef  __cplusplus
}
#endif
