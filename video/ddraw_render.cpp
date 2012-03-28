#include "ins.h"
#include "ddraw_render.h"

directx_fourcc dx_fourcc[] =
{
	{"I420 ", FOURCC_PIX_FMT_YUV420P, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('I','4','2','0'), 0,0,0,0,0}},   // yv12 with swapped uv
	{"YV12 ", FOURCC_PIX_FMT_YVU420, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('Y','V','1','2'), 0,0,0,0,0}},
	{"IYUV ", FOURCC_PIX_FMT_IYUV, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('I','Y','U','V'), 0,0,0,0,0}},      // same as i420
	//    {"YVU9 ", FOURCC_PIX_FMT_YVU410, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('Y','V','U','9'), 0,0,0,0,0}},
	//    {"YUY2 ", FOURCC_PIX_FMT_YUY2, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0,0,0,0,0}},
	//    {"UYVY ", FOURCC_PIX_FMT_UYVY, 0, {sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0,0,0,0,0}},
	//    {"BGR8 ", FOURCC_PIX_FMT_BGR8, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 8, 0x00000000, 0x00000000, 0x00000000, 0}},
	//    {"RGB15", FOURCC_PIX_FMT_RGB15, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000001F, 0x000003E0, 0x00007C00, 0}},   //RGB 5:5:5
	//    {"BGR15", FOURCC_PIX_FMT_BGR15, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x00007C00, 0x000003E0, 0x0000001F, 0}},
	//    {"RGB16", FOURCC_PIX_FMT_RGB16, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000001F, 0x000007E0, 0x0000F800, 0}},   //RGB 5:6:5
	//    {"BGR16", FOURCC_PIX_FMT_BGR16, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0}},
	//    {"RGB24", FOURCC_PIX_FMT_RGB24, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0}},
	//    {"BGR24", FOURCC_PIX_FMT_BGR24, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0}},
	//    {"RGB32", FOURCC_PIX_FMT_RGB32, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0}},
	//    {"BGR32", FOURCC_PIX_FMT_BGR32, 0, {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0}}
};

ddraw_render::ddraw_render()
	: m_ddraw(NULL)
	, m_main_face(NULL)
	, m_back_face(NULL)
	, m_overlay_face(NULL)
	, m_clipper(NULL)
	, m_image_width(0)
	, m_image_height(0)
	, m_hWnd(NULL)
	, m_destcolorkey(CLR_INVALID)
	, m_windowcolor(RGB(0, 0, 1))
	, m_support_overlay(true)
	, m_support_double_buffer(true)
	, m_can_blit_fourcc(false)
	, m_swsctx(NULL)
	, m_swsbuffer(NULL)
	, m_pix_fmt(0)
	, m_video_fmt(PIX_FMT_YUV420P)
	, m_keep_aspect(false)
	, m_window_aspect(0.0f)
{
	memset(&m_last_rect, 0, sizeof(m_last_rect));
}

ddraw_render::~ddraw_render()
{
	destory_render();
}

bool ddraw_render::init_render(void* ctx, int w, int h, int pix_fmt)
{
	m_hWnd = (HWND)ctx;
	m_image_width = w;
	m_image_height = h;

	m_monitor_rect.left = m_monitor_rect.top = 0;
	m_monitor_rect.right  = GetSystemMetrics(SM_CXSCREEN);
	m_monitor_rect.bottom = GetSystemMetrics(SM_CYSCREEN);

	if (pix_fmt != -1)
		m_video_fmt = pix_fmt;
	else
		m_video_fmt = PIX_FMT_YUV420P;

	HRESULT hr;

	if (m_ddraw == NULL)
	{
		// 创建ddraw句柄.
		if (DirectDrawCreateEx(NULL, (VOID**)&m_ddraw, IID_IDirectDraw7, NULL) != DD_OK)
		{
			printf("Create ddraw failed.\n");
			destory_render();
			return false;
		}

		// 设置协作级别.
		if (m_ddraw->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL) != DD_OK)
		{
			printf("Set cooperative level failed.\n");
			destory_render();
			return false;
		}

		// 检查ddraw是否支持overlay.
		check_overlay();
	}

	if (m_main_face)
	{
		m_main_face->Release();
		m_main_face = NULL;
	}

	// 创建主表层.
	DDSURFACEDESC2 ddsd;
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	if ((hr = m_ddraw->CreateSurface(&ddsd, &m_main_face, NULL)) != DD_OK)
	{
		printf("Create primary surface failed.\n");
		destory_render();
		return false;
	}

	if (m_clipper)
	{
		m_clipper->Release();
		m_clipper = NULL;
	}

	// 创建栽剪器.
	LPDIRECTDRAWCLIPPER cliper;
	if(m_ddraw->CreateClipper(0, &cliper, NULL) != DD_OK)
	{
		printf("Create clipper failed.\n");
		cliper->Release();
		destory_render();
		return false;
	}

	if(cliper->SetHWnd(0, m_hWnd) != DD_OK)
	{
		printf("Set hwnd failed.\n");
		cliper->Release();
		destory_render();
		return false;
	}

	if(m_main_face->SetClipper(cliper) != DD_OK)
	{
		printf("Set clipper failed.\n");
		cliper->Release();
		destory_render();
		return false;
	}

	m_clipper = cliper;

	// 查找颜色key.
	COLORREF rgbT;
	HDC hdc;
	m_destcolorkey = CLR_INVALID;
	if (m_windowcolor != CLR_INVALID && m_main_face->GetDC(&hdc) == DD_OK)
	{
		rgbT = GetPixel(hdc, 0, 0);
		SetPixel(hdc, 0, 0, m_windowcolor);
		m_main_face->ReleaseDC(hdc);
	}
	// read back the converted color.
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	while ((hr = m_main_face->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING)
		;
	if (hr == DD_OK)
	{
		m_destcolorkey = *(DWORD *)ddsd.lpSurface;
		if (ddsd.ddpfPixelFormat.dwRGBBitCount < 32)
			m_destcolorkey &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount) - 1;
		m_main_face->Unlock(NULL);
	}
	if (m_windowcolor != CLR_INVALID && m_main_face->GetDC(&hdc) == DD_OK)
	{
		SetPixel(hdc, 0, 0, rgbT);
		m_main_face->ReleaseDC(hdc);
	}

	// 创建表层.
	m_pix_fmt = -1;
	for (uint32_t i = 0; i < DDPF_NUM_FORMATS; i++)
	{
		if (create_overlay_face(dx_fourcc[i].img_format, true)) {
			m_pix_fmt = i;
			break;
		}
	}

	if (m_pix_fmt == -1)
	{
		printf("Create overlay surface failed.\n");
		// 使用普通surface来渲染.
		m_support_overlay = false;
		m_support_double_buffer = false;

		m_pix_fmt = -1;
		for (uint32_t i = 0; i < DDPF_NUM_FORMATS; i++)
		{
			if (create_overlay_face(dx_fourcc[i].img_format, false)) {
				m_pix_fmt = i;
				break;
			}
		}

		if (m_pix_fmt == -1)
		{
			printf("Create off screen plain surface failed.\n");
			return false;
		}
	}

	// 刷新下窗口, 否则可能会沾住窗口下面的元素图像.
	InvalidateRect(m_hWnd, NULL, TRUE);

	m_window_aspect = (float)w / (float)h;

	return true;
}

bool ddraw_render::render_one_frame(AVFrame* data, int pix_fmt)
{
	uint8_t* src_yuv[3] = { data->data[0], 
		data->data[1],
		data->data[2] };

	int src_linesize[3] = { data->linesize[0], 
		data->linesize[1], 
		data->linesize[2] };

	HRESULT hr = DD_OK;
	LPDIRECTDRAWSURFACE7 surface = NULL;

	if (m_support_overlay)
		surface = m_overlay_face;
	else
		surface = m_back_face;

	if (!surface)
	{
		printf("DDraw surface is null.\n");
		return false;
	}

	if (m_main_face->IsLost() == DDERR_SURFACELOST)
	{
		m_main_face->Restore();
		if (m_support_overlay)
		{
			/* Position and show the overlay */
			DDOVERLAYFX ddofx;

			ZeroMemory(&ddofx, sizeof(ddofx));
			ddofx.dwSize = sizeof(ddofx);
			ddofx.dckDestColorkey.dwColorSpaceLowValue = m_destcolorkey;
			ddofx.dckDestColorkey.dwColorSpaceHighValue = m_destcolorkey;

			RECT rs;
			rs.left   = 0;
			rs.top    = 0;
			rs.right  = m_image_width;
			rs.bottom = m_image_height;

			hr = m_overlay_face->UpdateOverlay(&rs, m_main_face, &rs, DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE, &ddofx);
			if (hr != DD_OK)
				printf("DirectDrawUpdateOverlay cannot move/resize overlay.\n");
		}
	}

	if (IsWindow(m_hWnd))
		manage_display();

	DDSURFACEDESC2 ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags= DDSD_ALL;

	hr = m_back_face->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (hr == DDERR_SURFACELOST)
	{
		if (m_main_face->Restore() == DD_OK)
			m_overlay_face->Restore();
	}

	if (!ddsd.lpSurface)
		return false;

	uint8_t* dst_yuv[3] = {
		(uint8_t*)((uint8_t*)ddsd.lpSurface),
		(uint8_t*)((uint8_t*)ddsd.lpSurface + ddsd.lPitch * m_image_height),
		(uint8_t*)((uint8_t*)ddsd.lpSurface + ddsd.lPitch * m_image_height + ((ddsd.lPitch >> 1) * (m_image_height >> 1)))
	};

	int dst_linesize[3] = { ddsd.lPitch, 
		ddsd.lPitch >> 1, 
		ddsd.lPitch >> 1 };

	// ddraw 像素格式.
	PixelFormat ddraw_fmt;
	if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YUV420P ||
		dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_IYUV)
		ddraw_fmt = PIX_FMT_YUV420P;
	else if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YUY2 ||
		dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YVYU)
		ddraw_fmt = PIX_FMT_YUYV422;
	else if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_UYVY)
		ddraw_fmt = PIX_FMT_UYVY422;
	else if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YVU410)
		ddraw_fmt = PIX_FMT_YUV410P;

	if (m_video_fmt == PIX_FMT_YUV420P)
	{
		if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YUV420P ||
			dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_IYUV)
		{
			// copy Y
			copy_yuv_pic(dst_yuv[0], src_yuv[0], m_image_width, m_image_height, dst_linesize[0], src_linesize[0]);
			// copy U
			copy_yuv_pic(dst_yuv[1], src_yuv[1], m_image_width >> 1, m_image_height >> 1, dst_linesize[1], src_linesize[1]);
			// copy V
			copy_yuv_pic(dst_yuv[2], src_yuv[2], m_image_width >> 1, m_image_height >> 1, dst_linesize[2], src_linesize[2]);
		}
		else if (dx_fourcc[m_pix_fmt].img_format == FOURCC_PIX_FMT_YVU420)
		{
			// copy Y
			copy_yuv_pic(dst_yuv[0], src_yuv[0], m_image_width, m_image_height, dst_linesize[0], src_linesize[0]);
			// copy V
			copy_yuv_pic(dst_yuv[1], src_yuv[2], m_image_width >> 1, m_image_height >> 1, dst_linesize[1], src_linesize[2]);
			// copy U
			copy_yuv_pic(dst_yuv[2], src_yuv[1], m_image_width >> 1, m_image_height >> 1, dst_linesize[2], src_linesize[1]);
		}
	}
	else
	{
		BOOST_ASSERT(0);
		// #if 0
		if (!m_swsctx)
		{
			m_swsctx = sws_getContext(m_image_width, m_image_height, (PixelFormat)m_video_fmt, m_image_width, m_image_height, 
				ddraw_fmt, SWS_BICUBIC, NULL, NULL, NULL);
		}

		if (!m_swsbuffer)
		{
			m_swsbuffer = (uint8_t*)malloc(m_image_width * m_image_height * sizeof(uint32_t));
		}

		AVFrame* pic = avcodec_alloc_frame();
		avpicture_fill((AVPicture*)pic, m_swsbuffer, ddraw_fmt, m_image_width, m_image_height);
		sws_scale(m_swsctx, src_yuv, src_linesize, 0, m_image_height, pic->data, pic->linesize);

		uint8_t* src = pic->data[0];
		uint8_t* dst = dst_yuv[0];

		for (int i = 0; i < m_image_height; i++)
		{
			memcpy(dst, src, pic->linesize[0]);
			dst += dst_linesize[0];
			src += pic->linesize[0];
		}

		for (int i = 0; i < m_image_height; i++)
		{
			memcpy(dst, src, pic->linesize[1]);
			dst += pic->linesize[1]; // ? dst_linesize[1]
			src += pic->linesize[1];
		}

		for (int i = 0; i < m_image_height; i++)
		{
			memcpy(dst, src, pic->linesize[2]);
			dst += pic->linesize[2]; // ? dst_linesize[2]
			src += pic->linesize[2];
		}

		av_free(pic);
		// #endif
	}

	m_back_face->Unlock(NULL);

	if (m_support_double_buffer)
	{
		hr = m_overlay_face->Flip(NULL, DDFLIP_WAIT);
		if(hr == DDERR_SURFACELOST)
		{
			m_back_face->Restore();
			// restore overlay and primary before calling
			// Directx_ManageDisplay() to avoid error messages
			m_overlay_face->Restore();
			m_main_face->Restore();
			// update overlay in case we return from screensaver
			manage_display();
			hr = m_overlay_face->Flip(NULL, DDFLIP_WAIT);
		}
		if(hr != DD_OK)
			printf("can't flip page.\n");
	}
	else // 非overlay 模式渲染.
	{
		DDBLTFX  ddbltfx;
		// ask for the "NOTEARING" option.
		memset( &ddbltfx, 0, sizeof(DDBLTFX) );
		ddbltfx.dwSize = sizeof(DDBLTFX);
		ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

		POINT pt = { 0 };
		RECT rect_client = { 0 };
		RECT rect_window = { 0 };
		int width, height;

		GetClientRect(m_hWnd, &rect_client);
		int win_width = width = rect_client.right - rect_client.left;
		int win_height = height = rect_client.bottom - rect_client.top;

		ClientToScreen(m_hWnd, &pt);
		rect_window.left = pt.x;
		rect_window.top  = pt.y;
		rect_window.right  = pt.x + width;
		rect_window.bottom = pt.y + height;

		if (m_keep_aspect)
		{
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
			rect_window.left += ((win_width - width) / 2);
			rect_window.top += ((win_height - height) / 2);
			rect_window.bottom -= ((win_height - height) / 2);
			rect_window.right -= ((win_width - width) / 2);
		}

		m_main_face->Blt(&rect_window, m_back_face, NULL, DDBLT_WAIT, &ddbltfx);
	}

	return true;
}

void ddraw_render::re_size(int width, int height)
{
	HRESULT hr;
	RECT overlay_rect = { 0 }, rect_client = { 0 };
	DDOVERLAYFX ddofx = { 0 };

	if (!m_ddraw || !(int)m_window_aspect)
		return ;

	POINT pt = { 0, 0 };
	ClientToScreen(m_hWnd, &pt);
	GetClientRect(m_hWnd, &rect_client);

	int win_width = width = rect_client.right - rect_client.left;
	int win_height = height = rect_client.bottom - rect_client.top;

	rect_client.left = pt.x;
	rect_client.top  = pt.y;
	rect_client.right  = pt.x + width;
	rect_client.bottom = pt.y + height;

	ddofx.dwSize = sizeof(DDOVERLAYFX);
	DWORD dwFlags = DDOVER_KEYDESTOVERRIDE;
	if (!width || !height)
		dwFlags |= DDOVER_HIDE;
	else
		dwFlags |= DDOVER_SHOW;

	SetRect(&overlay_rect, 0, 0, width, height);

	if (m_support_overlay)
	{
		ddofx.dckDestColorkey.dwColorSpaceLowValue = m_destcolorkey;
		ddofx.dckDestColorkey.dwColorSpaceHighValue = m_destcolorkey;

		if (m_keep_aspect)
		{
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

		hr = m_overlay_face->UpdateOverlay(NULL, m_main_face, &rect_client, dwFlags, &ddofx);
		if (hr != DD_OK)
		{
			if (hr == DDERR_SURFACELOST)
			{
				if (m_main_face->Restore() == DD_OK)
					m_overlay_face->Restore();
				return ;
			}
			printf("DirectDraw UpdateOverlay cannot move/resize overlay.\n");
			return ;
		}
	}
}

void ddraw_render::aspect_ratio(int srcw, int srch, bool enable_aspect)
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

void ddraw_render::destory_render()
{
	if (m_ddraw)
	{
		m_ddraw->Release();
		m_ddraw = NULL;
	}

	if (m_main_face)
	{
		m_main_face->Release();
		m_main_face = NULL;
	}

	if (m_back_face)
	{
		m_back_face->Release();
		m_back_face = NULL;
	}

	if (m_overlay_face)
	{
		if (m_support_double_buffer || m_support_overlay)
			m_overlay_face->Release();
		m_overlay_face = NULL;
	}

	if (m_clipper)
	{
		m_clipper->Release();
		m_clipper = NULL;
	}

	m_image_width = 0;
	m_image_height = 0;
	m_hWnd = NULL;
	m_destcolorkey = CLR_INVALID;
	m_windowcolor = RGB(12, 0, 12);
	m_support_overlay = true;
	m_support_double_buffer = true;
	m_can_blit_fourcc = false;
	m_pix_fmt = 0;
	m_keep_aspect = true;
	m_window_aspect = 0.0f;

	memset(&m_last_rect, 0, sizeof(m_last_rect));
}

void ddraw_render::check_overlay()
{
	DDCAPS ddcaps = { 0 };

	ddcaps.dwSize = sizeof(ddcaps);

	if (m_ddraw->GetCaps(&ddcaps, NULL) != DD_OK)
	{
		printf("Cannot get caps.\n");
		return ;
	}

	/* Determine if the hardware supports overlay surfaces */
	const bool has_overlay = ddcaps.dwCaps & DDCAPS_OVERLAY;
	/* Determine if the hardware supports overlay surfaces */
	const bool has_overlay_fourcc = ddcaps.dwCaps & DDCAPS_OVERLAYFOURCC;
	/* Determine if the hardware supports overlay deinterlacing */
	const bool can_deinterlace = ddcaps.dwCaps & DDCAPS2_CANFLIPODDEVEN;
	/* Determine if the hardware supports colorkeying */
	const bool has_color_key = ddcaps.dwCaps & DDCAPS_COLORKEY;
	/* Determine if the hardware supports scaling of the overlay surface */
	const bool can_stretch = ddcaps.dwCaps & DDCAPS_OVERLAYSTRETCH;
	/* Determine if the hardware supports color conversion during a blit */
	m_can_blit_fourcc = ddcaps.dwCaps & DDCAPS_BLTFOURCC;
	/* Determine overlay source boundary alignment */
	const bool align_boundary_src  = ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC;
	/* Determine overlay destination boundary alignment */
	const bool align_boundary_dest = ddcaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST;
	/* Determine overlay destination size alignment */
	const bool align_size_src  = ddcaps.dwCaps & DDCAPS_ALIGNSIZESRC;
	/* Determine overlay destination size alignment */
	const bool align_size_dest = ddcaps.dwCaps & DDCAPS_ALIGNSIZEDEST;

	m_support_overlay = has_overlay && has_overlay_fourcc && can_stretch;
}

bool ddraw_render::create_overlay_face(uint32_t fmt, bool must_overlay)
{
	HRESULT ddrval;
	DDSURFACEDESC2 ddsdOverlay;
	uint32_t i = 0;

	// 查找支持的格式.
	while (i < DDPF_NUM_FORMATS && fmt != dx_fourcc[i].img_format)
		i++;

	if (!m_ddraw || !m_main_face || i == DDPF_NUM_FORMATS)
		return false;

	// cleanup.
	if (m_back_face)
		m_back_face->Release();
	if (m_overlay_face)
		m_overlay_face->Release();

	m_overlay_face = NULL;
	m_back_face    = NULL;

	ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
	ddsdOverlay.dwSize = sizeof(ddsdOverlay);
	ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
	ddsdOverlay.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_BACKBUFFERCOUNT| DDSD_PIXELFORMAT;
	ddsdOverlay.dwWidth        = m_image_width;
	ddsdOverlay.dwHeight       = m_image_height;
	ddsdOverlay.dwBackBufferCount = 2;
	ddsdOverlay.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
	ddsdOverlay.ddpfPixelFormat   = dx_fourcc[i].ddpf_overlay;

	if (m_ddraw->CreateSurface(&ddsdOverlay, &m_overlay_face, NULL) == DD_OK)
	{
		// get the surface directly attached to the primary (the back buffer)
		ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
		if(m_overlay_face->GetAttachedSurface(&ddsdOverlay.ddsCaps, &m_back_face) != DD_OK)
		{
			m_support_overlay = true;
			m_support_double_buffer = false; // disable tribblebuffering.
			return false;
		}

		m_support_overlay = true;
		m_support_double_buffer = true;  // enable tribblebuffering.
		printf("DDraw is supported %s\n", dx_fourcc[i].img_format_name);
		return true;
	}
	if (must_overlay)
	{
		printf("DDraw is no supported %s\n", dx_fourcc[i].img_format_name);
		return false;
	}
	// single buffer.
	ddsdOverlay.dwBackBufferCount = 0;
	ddsdOverlay.ddsCaps.dwCaps    = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
	ddsdOverlay.dwFlags           = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsdOverlay.ddpfPixelFormat   = dx_fourcc[i].ddpf_overlay;
	// try to create the overlay surface.
	ddrval = m_ddraw->CreateSurface(&ddsdOverlay, &m_overlay_face, NULL);
	if(ddrval != DD_OK && !must_overlay)
	{
		ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
		ddsdOverlay.dwSize = sizeof(ddsdOverlay);
		ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		ddsdOverlay.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		ddsdOverlay.dwWidth = m_image_width;
		ddsdOverlay.dwHeight = m_image_height;
		ddsdOverlay.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsdOverlay.ddpfPixelFormat.dwFlags = DDPF_FOURCC | DDPF_YUV;
		ddsdOverlay.ddpfPixelFormat.dwFourCC = dx_fourcc[i].img_format;
		ddsdOverlay.dwBackBufferCount = 0;

		if ((ddrval = m_ddraw->CreateSurface(&ddsdOverlay, &m_overlay_face, NULL)) != DD_OK)
			return false;
		m_support_overlay = false;
		m_support_double_buffer = false; // disable tribblebuffering.
	}

	if (ddrval != DD_OK)
		return false;

	m_back_face = m_overlay_face;
	return true;
}

bool ddraw_render::manage_display()
{
	HRESULT       ddrval;
	DDCAPS        capsDrv;
	DDOVERLAYFX   ovfx;
	DWORD         dwUpdateFlags=0;
	int           width, height, win_width, win_height;
	RECT          rd = { 0 };
	RECT current_rect = {0, 0, 0, 0};

	if (!m_support_overlay && !m_support_double_buffer)
		return false;

	if(IsWindow(m_hWnd))
	{
		GetWindowRect(m_hWnd, &current_rect);

		if ((current_rect.left   == m_last_rect.left)
			&&  (current_rect.top    == m_last_rect.top)
			&&  (current_rect.right  == m_last_rect.right)
			&&  (current_rect.bottom == m_last_rect.bottom))
			return 0;

		m_last_rect = current_rect;
	}

	if (IsWindow(m_hWnd))
	{
		POINT pt = { 0 };

		ClientToScreen(m_hWnd, &pt);
		GetClientRect(m_hWnd, &rd);

		win_width = width  = rd.right - rd.left;
		win_height = height = rd.bottom - rd.top;

		pt.x -= m_monitor_rect.left;    /* move coordinates from global to local monitor space */
		pt.y -= m_monitor_rect.top;
		rd.right  -= m_monitor_rect.left;
		rd.bottom -= m_monitor_rect.top;
		rd.left = pt.x;
		rd.top  = pt.y;

		if(m_support_overlay && (!width || !height))
		{
			/* window is minimized */
			ddrval = m_overlay_face->UpdateOverlay(NULL, m_main_face, NULL, DDOVER_HIDE, NULL);
			return true;
		}
		if (m_keep_aspect)
		{
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
			rd.left += ((win_width - width) / 2);
			rd.top += ((win_height - height) / 2);
		}
	}

	rd.right  = rd.left + width;
	rd.bottom = rd.top + height;

	RECT rs = { 0 };
	int vo_screenwidth = m_monitor_rect.right - m_monitor_rect.left;
	int vo_screenheight = m_monitor_rect.bottom - m_monitor_rect.top;

	/* ok, let's workaround some overlay limitations */
	if(m_support_overlay)
	{
		uint32_t uStretchFactor1000;  // minimum stretch
		uint32_t xstretch1000, ystretch1000;

		/* get driver capabilities */
		ZeroMemory(&capsDrv, sizeof(capsDrv));
		capsDrv.dwSize = sizeof(capsDrv);

		if(m_ddraw->GetCaps(&capsDrv, NULL) != DD_OK)
			return false;

		/* get minimum stretch, depends on display adaptor and mode (refresh rate!) */
		uStretchFactor1000 = capsDrv.dwMinOverlayStretch > 1000 ? capsDrv.dwMinOverlayStretch : 1000;
		rd.right = ((width + rd.left) * uStretchFactor1000 + 999) / 1000;
		rd.bottom = (height + rd.top) * uStretchFactor1000 / 1000;
		/* calculate xstretch1000 and ystretch1000 */
		xstretch1000 = ((rd.right - rd.left) * 1000)/ m_image_width;
		ystretch1000 = ((rd.bottom - rd.top) * 1000)/ m_image_height;

		rs.right  = m_image_width;
		rs.bottom = m_image_height;

		if(rd.left < 0)
			rs.left = (-rd.left * 1000) / xstretch1000;

		if(rd.top < 0)
			rs.top = (-rd.top * 1000) / ystretch1000;

		if(rd.right > vo_screenwidth)
			rs.right = ((vo_screenwidth - rd.left) * 1000) / xstretch1000;

		if(rd.bottom > vo_screenheight)
			rs.bottom = ((vo_screenheight - rd.top) * 1000) / ystretch1000;

		/* do not allow to zoom or shrink if hardware isn't able to do so. */
		if((width < m_image_width) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKX))
			rd.right = rd.left + m_image_width;
		else if((width > m_image_width) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHX))
			rd.right = rd.left + m_image_width;
		if((height < m_image_height) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKY))
			rd.bottom = rd.top + m_image_height;
		else if((height > m_image_height ) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHY))
			rd.bottom = rd.top + m_image_height;

		/* the last thing to check are alignment restrictions
		these expressions (x & -y) just do alignment by dropping low order bits...
		so to round up, we add first, then truncate
		*/
		if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) && capsDrv.dwAlignBoundarySrc)
			rs.left = (rs.left + capsDrv.dwAlignBoundarySrc / 2) & -(signed)(capsDrv.dwAlignBoundarySrc);
		if((capsDrv.dwCaps & DDCAPS_ALIGNSIZESRC) && capsDrv.dwAlignSizeSrc)
			rs.right = rs.left + ((rs.right - rs.left + capsDrv.dwAlignSizeSrc / 2) & -(signed) (capsDrv.dwAlignSizeSrc));
		if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) && capsDrv.dwAlignBoundaryDest)
			rd.left = (rd.left + capsDrv.dwAlignBoundaryDest / 2) & -(signed)(capsDrv.dwAlignBoundaryDest);
		if((capsDrv.dwCaps & DDCAPS_ALIGNSIZEDEST) && capsDrv.dwAlignSizeDest)
			rd.right = rd.left + ((rd.right - rd.left) & -(signed) (capsDrv.dwAlignSizeDest));
		/* create an overlay FX structure to specify a destination color key. */

		ZeroMemory(&ovfx, sizeof(ovfx));
		ovfx.dwSize = sizeof(ovfx);
		ovfx.dckDestColorkey.dwColorSpaceLowValue = m_destcolorkey;
		ovfx.dckDestColorkey.dwColorSpaceHighValue = m_destcolorkey;

		// set the flags we'll send to UpdateOverlay      
		// DDOVER_AUTOFLIP|DDOVERFX_MIRRORLEFTRIGHT|DDOVERFX_MIRRORUPDOWN could be useful?;
		dwUpdateFlags = DDOVER_SHOW | DDOVER_DDFX;
		/* if hardware can't do colorkeying set the window on top. */
		if(capsDrv.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY)
			dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE;
		// else if (!tmp_image) 
		//    vo_ontop = 1;
	}
	else
	{
		m_clipper->SetHWnd(0, m_hWnd);
	}

	if(rd.left < 0)
		rd.left = 0;
	if(rd.right > vo_screenwidth)
		rd.right = vo_screenwidth;
	if(rd.top < 0)
		rd.top = 0;
	if(rd.bottom > vo_screenheight)
		rd.bottom = vo_screenheight;

	/*for nonoverlay mode we are finished, for overlay mode we have to display the overlay first. */
	if(!m_support_overlay)
		return true;

	// printf("overlay: %i %i %ix%i\n",rd.left,rd.top,rd.right - rd.left,rd.bottom - rd.top);
	ddrval = m_overlay_face->UpdateOverlay(&rs, m_main_face, &rd, dwUpdateFlags, &ovfx);
	if(FAILED(ddrval))
	{
		// one cause might be the driver lied about minimum stretch
		// we should try upping the destination size a bit, or
		// perhaps shrinking the source size
		switch (ddrval)
		{
		case DDERR_NOSTRETCHHW:
		case DDERR_INVALIDRECT:
		case DDERR_INVALIDPARAMS:
		case DDERR_HEIGHTALIGN:
		case DDERR_XALIGN:
		case DDERR_UNSUPPORTED:
		case DDERR_INVALIDSURFACETYPE:
		case DDERR_INVALIDOBJECT:
		case DDERR_SURFACELOST:
			{
				m_overlay_face->Restore(); // restore and try again.
				m_main_face->Restore();
				ddrval = m_overlay_face->UpdateOverlay(&rs, m_main_face, &rd, dwUpdateFlags, &ovfx);
				if(ddrval != DD_OK)
					printf("UpdateOverlay failed again.\n");
				break;
			}
		default:
			printf("UpdateOverlay failed: 0x%x.\n", ddrval);
		}
		/* ok we can't do anything about it -> hide overlay. */
		if(ddrval != DD_OK)
		{
			ddrval = m_overlay_face->UpdateOverlay(NULL, m_main_face, NULL, DDOVER_HIDE, NULL);
			return false;
		}
	}

	return true;
}

void* ddraw_render::copy_yuv_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	if(dstStride == srcStride)
	{
		if (srcStride < 0)
		{
			src = (const uint8_t*)src + (height-1)*srcStride;
			dst = (uint8_t*)dst + (height-1)*dstStride;
			srcStride = -srcStride;
		}

		memcpy(dst, src, srcStride*height);
	}
	else
	{
		for(i=0; i<height; i++)
		{
			memcpy(dst, src, bytesPerLine);
			src = (const uint8_t*)src + srcStride;
			dst = (uint8_t*)dst + dstStride;
		}
	}

	return retval;
}

bool ddraw_render::draw_slice(uint8_t* dst, uint8_t* src[], int stride[], int dstStride, int w, int h, int x, int y)
{
	uint8_t *s;
	uint8_t *d;
	uint32_t uvstride = dstStride / 2;

	// copy Y
	d = dst + dstStride * y + x;
	s = src[0];

	copy_yuv_pic(d, s, w, h, dstStride, stride[0]);

	w /= 2; h /= 2; x /= 2; y /= 2;

	// copy U
	d = dst + dstStride * m_image_height + uvstride * y + x;
	//    if(image_format == IMGFMT_YV12)
	//       s = src[2];
	//    else 
	s = src[1];

	copy_yuv_pic(d, s, w, h, uvstride, stride[1]);

	// copy V
	d = dst + dstStride * m_image_height + uvstride * (m_image_height / 2) + uvstride * y + x;
	//    if(image_format == IMGFMT_YV12)
	//       s = src[1];
	//    else
	s = src[2];

	copy_yuv_pic(d, s, w, h, uvstride, stride[2]);

	return true;
}

