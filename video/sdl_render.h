/*
    Copyright (C) 2012  microcai <microcai@fedoraproject.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef SDL_RENDER_H
#define SDL_RENDER_H

#include "video_render.h"
#include <boost/thread/pthread/mutex.hpp>

class sdl_render : public video_render
{
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
    virtual bool use_overlay();
private:
    SDL_Overlay* m_yuv;
    SDL_Surface* sfc;
    boost::mutex	renderlock;
    int		m_pix_fmt;
    int	m_image_width, m_image_height;
    SwsContext* m_swsctx;	
};

#endif // SDL_RENDER_H
