#include "ins.h"
#include "defs.h"
#include "video_out.h"
#include "d3d_render.h"
#include "ddraw_render.h"
#include "opengl_render.h"

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int d3d_init_video(void *ctx, int w, int h, int pix_fmt)
{
	vo_context *vo = (vo_context*)ctx;
   d3d_render *d3d = NULL;
	vo->video_dev = (void*)(d3d = new d3d_render);
   return d3d->init_render(vo->user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int d3d_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts)
{
	vo_context *vo = (vo_context*)ctx;
	d3d_render *d3d = (d3d_render*)vo->video_dev;
   return d3d->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void d3d_re_size(void *ctx, int width, int height)
{
	vo_context *vo = (vo_context*)ctx;
	d3d_render *d3d = (d3d_render*)vo->video_dev;
   d3d->re_size(width, height);
}

EXPORT_API void d3d_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
	vo_context *vo = (vo_context*)ctx;
	d3d_render *d3d = (d3d_render*)vo->video_dev;
   d3d->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int d3d_use_overlay(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	d3d_render *d3d = (d3d_render*)vo->video_dev;
   return d3d->use_overlay() ? 0 : -1;
}

EXPORT_API void d3d_destory_render(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	d3d_render *d3d = (d3d_render*)vo->video_dev;
	if (d3d)
	{
		d3d->destory_render();
		delete d3d;
		vo->video_dev = NULL;
	}
}


EXPORT_API int ddraw_init_video(void *ctx, int w, int h, int pix_fmt)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = NULL;
	vo->video_dev = (void*)(ddraw = new ddraw_render);
	return ddraw->init_render(vo->user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int ddraw_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = (ddraw_render*)vo->video_dev;
	return ddraw->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void ddraw_re_size(void *ctx, int width, int height)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = (ddraw_render*)vo->video_dev;
	ddraw->re_size(width, height);
}

EXPORT_API void ddraw_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = (ddraw_render*)vo->video_dev;
	ddraw->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int ddraw_use_overlay(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = (ddraw_render*)vo->video_dev;
	return ddraw->use_overlay() ? 0 : -1;
}

EXPORT_API void ddraw_destory_render(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	ddraw_render *ddraw = (ddraw_render*)vo->video_dev;
	if (ddraw)
	{
		ddraw->destory_render();
		delete ddraw;
		vo->video_dev = NULL;
	}
}

EXPORT_API int ogl_init_video(void *ctx, int w, int h, int pix_fmt)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = NULL;
	vo->video_dev = (void*)(ogl = new opengl_render);
	return ogl->init_render(vo->user_data, w, h, pix_fmt) ? 0 : -1;
}

EXPORT_API int ogl_render_one_frame(void *ctx, AVFrame* data, int pix_fmt, double pts)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = (opengl_render*)vo->video_dev;
	return ogl->render_one_frame(data, pix_fmt) ? 0 : -1;
}

EXPORT_API void ogl_re_size(void *ctx, int width, int height)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = (opengl_render*)vo->video_dev;
	ogl->re_size(width, height);
}

EXPORT_API void ogl_aspect_ratio(void *ctx, int srcw, int srch, int enable_aspect)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = (opengl_render*)vo->video_dev;
	ogl->aspect_ratio(srcw, srch, enable_aspect);
}

EXPORT_API int ogl_use_overlay(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = (opengl_render*)vo->video_dev;
	return ogl->use_overlay() ? 0 : -1;
}

EXPORT_API void ogl_destory_render(void *ctx)
{
	vo_context *vo = (vo_context*)ctx;
	opengl_render *ogl = (opengl_render*)vo->video_dev;
	if (ogl)
	{
		ogl->destory_render();
		delete ogl;
		vo->video_dev = NULL;
	}
}

#ifdef  __cplusplus
}
#endif
