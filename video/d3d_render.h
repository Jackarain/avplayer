//
// d3d_render.h
// ~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __D3D_RENDER_H__
#define __D3D_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "video_render.h"


typedef struct {
	unsigned int  mplayer_fmt;   /**< Given by MPlayer */
	D3DFORMAT     fourcc;        /**< Required by D3D's test function */
} struct_fmt_table;

typedef enum back_buffer_action {
	BACKBUFFER_CREATE,
	BACKBUFFER_RESET
} back_buffer_action_e;

extern struct_fmt_table fmt_table[];

#define DISPLAY_FORMAT_TABLE_ENTRIES (sizeof(fmt_table) / sizeof(fmt_table[0]))

class d3d_render
	: public video_render
{
public:
	d3d_render();
	virtual ~d3d_render();

public:
	/* 初始render.	*/
	virtual bool init_render(void* ctx, int w, int h, int pix_fmt);

	/* 渲染一帧.	*/
	virtual bool render_one_frame(AVFrame* data, int pix_fmt);

	/* 调整大小.	*/
	virtual void re_size(int width, int height);

	/* 设置宽高比.	*/
	virtual void aspect_ratio(int srcw, int srch, bool enable_aspect);

	/* 撤销render.		*/
	virtual void destory_render();

private:
	void fill_d3d_presentparams(D3DPRESENT_PARAMETERS *present_params);
	bool configure_d3d();
	void destroy_d3d_surfaces();
	bool change_d3d_backbuffer(back_buffer_action_e action);
	bool create_d3d_surfaces();
	bool reconfigure_d3d();
	int query_format(uint32_t movie_fmt);

private:
	LPDIRECT3D9 m_d3d_handle;                 /**< Direct3D Handle */
	LPDIRECT3DDEVICE9 m_d3d_device;           /**< The Direct3D Adapter */
	LPDIRECT3DSURFACE9 m_d3d_surface;         /**< Offscreen Direct3D Surface. */
	LPDIRECT3DTEXTURE9 m_d3d_texture_osd;     /**< Direct3D Texture. Uses RGBA */
	LPDIRECT3DTEXTURE9 m_d3d_texture_system;  /**< Direct3D Texture. System memory cannot lock a normal texture. Uses RGBA */
	LPDIRECT3DSURFACE9 m_d3d_backbuf;         /**< Video card's back buffer (used to display next frame) */
	D3DLOCKED_RECT m_locked_rect;             /**< The locked offscreen surface */
	D3DFORMAT m_desktop_fmt;                  /**< Desktop (screen) colorspace format. */
	D3DFORMAT m_movie_src_fmt;                /**< Movie colorspace format (depends on the movie's codec) */
	D3DPRESENT_PARAMETERS m_present_params;
	RECT m_last_client_rect;

	int m_cur_backbuf_width;         /**< Current backbuffer width */
	int m_cur_backbuf_height;        /**< Current backbuffer height */
	int m_device_caps_power2_only;   /**< 1 = texture sizes have to be power 2
												0 = texture sizes can be anything */
	int m_device_caps_square_only;   /**< 1 = textures have to be square
												0 = textures do not have to be square */
	int m_device_texture_sys;        /**< 1 = device can texture from system memory
												0 = device requires shadow */
	int m_max_texture_width;         /**< from the device capabilities */
	int m_max_texture_height;        /**< from the device capabilities */

	int m_osd_width;                 /**< current width of the OSD */
	int m_osd_height;                /**< current height of the OSD */
	int m_osd_texture_width;         /**< current width of the OSD texture */
	int m_osd_texture_height;        /**< current height of the OSD texture */

	HWND m_hwnd;
	int m_image_width;
	int m_image_height;

	/* 保持宽高.	*/
	bool m_keep_aspect;

	/* 宽高比.		*/
	float m_window_aspect;
};

#endif // __D3D_RENDER_H__

