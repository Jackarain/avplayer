#include "stdafx.h"
#include "player_impl.h"

//////////////////////////////////////////////////////////////////////////
// class win_data 作为局部线程存储, 用于保存HWND对应
// 的player指针. 在消息回调时, 查找所对应的窗口, 正确
// 回调至window的win_wnd_proc消息处理函数.
// 该代码参考MFC的实现.
class player_impl;
class win_data
{
	typedef player_impl* player_impl_ptr;
	typedef std::map<player_impl_ptr, HWND> player_impl_map;
public:
	win_data();
	~win_data();

public:
	BOOL add_window(HWND hwnd, player_impl *win);
	BOOL remove_window(player_impl* win);
	player_impl *lookup_window(HWND hwnd);

private:
	CRITICAL_SECTION m_cs;
	player_impl_map m_maps;
	HHOOK m_hook_cbt_filter;
};

// 线程本地存储.
template <typename T>
class thread_tls
{
public:
	thread_tls()
		: m_tls_index(0)
	{
		m_tls_index = TlsAlloc();
	}
	~thread_tls()
	{
		TlsFree(m_tls_index);
	}

	T* operator->() const
	{
		return get();
	}

	T& operator*() const
	{
		return *get();
	}

	void set(T* data)
	{
		TlsSetValue(m_tls_index, (void*)data);
	}

	T* get() const
	{
		return (T*)TlsGetValue(m_tls_index);
	}

private:
	DWORD m_tls_index;
};

// 本地线程存储, 用于存储hwnd和player_impl之间的关系, 并且在消息回调时能及时查询到对应的窗口.
win_data window_pool;

win_data::win_data()
{
	InitializeCriticalSection(&m_cs);
}

win_data::~win_data()
{
	DeleteCriticalSection(&m_cs);
}

BOOL win_data::add_window(HWND hwnd, player_impl* win)
{
	EnterCriticalSection(&m_cs);
	player_impl_map::iterator finder = m_maps.find(win);
	if (finder != m_maps.end())
	{
		LeaveCriticalSection(&m_cs);
		return FALSE;
	}
	m_maps.insert(std::make_pair(win, hwnd));
	LeaveCriticalSection(&m_cs);
	return TRUE;
}

BOOL win_data::remove_window(player_impl* win)
{
	EnterCriticalSection(&m_cs);
	player_impl_map::iterator finder = m_maps.find(win);
	if (finder == m_maps.end())
	{
		LeaveCriticalSection(&m_cs);
		return FALSE;
	}
	m_maps.erase(finder);
	LeaveCriticalSection(&m_cs);
	return TRUE;
}

player_impl *win_data::lookup_window(HWND hwnd)
{
	EnterCriticalSection(&m_cs);
	for (player_impl_map::iterator it = m_maps.begin(); it != m_maps.end(); it++)
	{
		if (it->second == hwnd)
		{
			LeaveCriticalSection(&m_cs);
			return it->first;
		}
	}
	LeaveCriticalSection(&m_cs);
	return NULL;
}

static uint64_t file_size(LPCTSTR filename)
{
	WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };

	if (!::GetFileAttributesEx(filename, ::GetFileExInfoStandard, &fad))
		return -1;
	return (static_cast<uint64_t>(fad.nFileSizeHigh)
		<< (sizeof(fad.nFileSizeLow) * 8)) + fad.nFileSizeLow;
}

struct win_cbt_context
{
	HHOOK hook;
	player_impl* win;
};

thread_tls<win_cbt_context> win_tls;

LRESULT CALLBACK win_cbt_filter_hook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code != HCBT_CREATEWND)
	{
		return CallNextHookEx(win_tls->hook, code, wParam, lParam);
	}
	else
	{
		HWND hwnd = (HWND)wParam;
		player_impl *win = win_tls->win;
		window_pool.add_window(hwnd, win);
	}

	return CallNextHookEx(win_tls->hook, code, wParam, lParam);
}


//////////////////////////////////////////////////////////////////////////
// 字幕插件.
typedef BOOL WINAPI subtitle_init_vobsub_func();
typedef void WINAPI subtitle_uninit_vobsub_func();
typedef BOOL WINAPI subtitle_open_vobsub_func(char* fileName, int w, int h);
typedef void WINAPI subtitle_vobsub_do_func(void* data, LONGLONG curTime, LONG size);

class vsfilter_interface
	: public subtitle_plugin
{
public:
	vsfilter_interface() {}
	vsfilter_interface(const std::string vsfilter = "")
		: m_vsfilter(NULL)
		, m_subtitle_init_vobsub(NULL)
		, m_subtitle_uninit_vobsub(NULL)
		, m_subtitle_open_vobsub(NULL)
		, m_subtitle_vobsub_do(NULL)
	{
		if (!vsfilter.empty())
			m_vsfilter = LoadLibraryA(vsfilter.c_str());
		if (m_vsfilter)
		{
			m_subtitle_init_vobsub = (subtitle_init_vobsub_func*)
				GetProcAddress(m_vsfilter, "InitGenVobSub");
			m_subtitle_uninit_vobsub = (subtitle_uninit_vobsub_func*)
				GetProcAddress(m_vsfilter, "UnInitGenVobSub");
			m_subtitle_open_vobsub = (subtitle_open_vobsub_func*)
				GetProcAddress(m_vsfilter, "OpenVobSub");
			m_subtitle_vobsub_do = (subtitle_vobsub_do_func*)
				GetProcAddress(m_vsfilter, "VobSubTransform");

			// 加载失败则释放.
			if (!(m_subtitle_init_vobsub &&
				m_subtitle_uninit_vobsub &&
				m_subtitle_open_vobsub &&
				m_subtitle_vobsub_do))
			{
				FreeLibrary(m_vsfilter);
				m_vsfilter = NULL;
			}
		}
	}

	virtual ~vsfilter_interface()
	{
		if (m_vsfilter)
			FreeLibrary(m_vsfilter);
		m_vsfilter = NULL;
	}

public:
	// 初始化字幕插件.
	virtual bool subtitle_init()
	{
		if (!subtitle_is_load())
			return false;
		return m_subtitle_init_vobsub();
	}

	// 反初始化字幕插件.
	virtual void subtitle_uninit()
	{
		if (!subtitle_is_load())
			return ;
		return m_subtitle_uninit_vobsub();
	}

	// 打开字幕文件, 并指定视频宽高.
	virtual bool subtitle_open(char* fileName, int w, int h)
	{
		if (!subtitle_is_load())
			return false;
		return m_subtitle_open_vobsub(fileName, w, h);
	}

	// 渲染一帧yuv视频数据. 传入当前时间戳和yuv视频数据及大小.
	virtual void subtitle_do(void* yuv_data, int64_t cur_time, long size)
	{
		if (!subtitle_is_load())
			return ;
		m_subtitle_vobsub_do(yuv_data, cur_time, size);
	}

	// 判断插件是否加载.
	virtual bool subtitle_is_load()
	{
		return m_vsfilter ? true : false;
	}

protected:
	HMODULE m_vsfilter;
	subtitle_init_vobsub_func* m_subtitle_init_vobsub;
	subtitle_uninit_vobsub_func* m_subtitle_uninit_vobsub;
	subtitle_open_vobsub_func* m_subtitle_open_vobsub;
	subtitle_vobsub_do_func* m_subtitle_vobsub_do;
};


//////////////////////////////////////////////////////////////////////////
// 以下代码为播放器相关的实现.

// 用于第一次得到正确宽高信息的定时器.
#define ID_PLAYER_TIMER		1021

player_impl::player_impl(void)
	: m_hwnd(NULL)
	, m_hinstance(NULL)
	, m_brbackground(NULL)
	, m_old_win_proc(NULL)
	, m_avplay(NULL)
	, m_video(NULL)
	, m_audio(NULL)
	, m_source(NULL)
	, m_cur_index(-1)
	, m_plugin(NULL)
	, m_change_subtitle(false)
	, m_video_width(0)
	, m_video_height(0)
	, m_wnd_style(0)
	, m_full_screen(FALSE)
	, m_mute(false)
{
	// 初始化字幕插件的临界.
	InitializeCriticalSection(&m_plugin_cs);
}

player_impl::~player_impl(void)
{
	if (m_brbackground)
	{
		DeleteObject(m_brbackground);
		m_brbackground = NULL;
	}
	DeleteCriticalSection(&m_plugin_cs);
}

HWND player_impl::create_window(const char *player_name)
{
	WNDCLASSEXA wcex;

	// 得到进程实例句柄.
	m_hinstance = (HINSTANCE)GetModuleHandle(NULL);
	// 创建非纯黑色的画刷, 用于ddraw播放时刷背景色.
	m_brbackground = CreateSolidBrush(RGB(0, 0, 1));
	wcex.cbSize = sizeof(WNDCLASSEXA);

	wcex.style			= CS_CLASSDC/*CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS*/;
	wcex.lpfnWndProc	= player_impl::static_win_wnd_proc;
	wcex.cbClsExtra	= 0;
	wcex.cbWndExtra	= 0;
	wcex.hInstance		= m_hinstance;
	wcex.hIcon			= LoadIcon(m_hinstance, MAKEINTRESOURCE(IDC_ICON));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= m_brbackground;	// (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName		= NULL;				// MAKEINTRESOURCE(IDC_AVPLAYER);
	wcex.lpszClassName	= player_name;
	wcex.hIconSm			= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDC_ICON));

	if (!RegisterClassExA(&wcex))
	{
		::logger("register window class failed!\n");
		return NULL;
	}

	// 创建hook, 以便在窗口创建之前得到HWND句柄, 使HWND与this绑定.
	HHOOK hook = SetWindowsHookEx(WH_CBT, win_cbt_filter_hook, NULL, GetCurrentThreadId());
	win_cbt_context win_cbt_ctx;
	win_tls.set(&win_cbt_ctx);
	win_tls->hook = hook;
	win_tls->win = this;

	// 创建窗口.
	m_hwnd = CreateWindowExA(/*WS_EX_APPWINDOW*/0,
		player_name, player_name, WS_OVERLAPPEDWINDOW/* | WS_CLIPSIBLINGS | WS_CLIPCHILDREN*/,
		0, 0, 800, 600, NULL, NULL, m_hinstance, NULL);

	// 撤销hook.
	UnhookWindowsHookEx(hook);
	win_tls->hook = NULL;
	win_tls->win = NULL;

	ShowWindow(m_hwnd, SW_SHOW);

	return m_hwnd;
}

BOOL player_impl::destory_window()
{
	if (!IsWindow(m_hwnd))
		return FALSE;

	BOOL ret = DestroyWindow(m_hwnd);
	if (m_brbackground)
	{
		DeleteObject(m_brbackground);
		m_brbackground = NULL;
	}

	window_pool.remove_window(this);

	return ret;
}

BOOL player_impl::subclasswindow(HWND hwnd, BOOL in_process)
{
	if (!IsWindow(hwnd))
		return FALSE;
	if (IsWindow(m_hwnd))
		return FALSE;
	// 创建非纯黑色的画刷, 用于ddraw播放时刷背景色.
	m_brbackground = CreateSolidBrush(RGB(0, 0, 1));
	window_pool.add_window(hwnd, this);
	m_old_win_proc = (WNDPROC)::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)&player_impl::static_win_wnd_proc);
	if (!m_old_win_proc)
	{
		window_pool.remove_window(this);
		if (in_process)
			return FALSE;
	}
	m_hwnd = hwnd;
	return TRUE;
}

BOOL player_impl::unsubclasswindow(HWND hwnd)
{
	if (!IsWindow(m_hwnd) || !IsWindow(hwnd)
		|| m_hwnd != hwnd || !m_old_win_proc)
	{
		return FALSE;
	}

	// 设置为原来的窗口过程.
	::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)m_old_win_proc);
	window_pool.remove_window(this);
	m_hwnd = NULL;
	m_old_win_proc = NULL;

	return TRUE;
}

LRESULT CALLBACK player_impl::static_win_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	player_impl* this_ptr = window_pool.lookup_window(hwnd);
	if (!this_ptr)
	{
		::logger("Impossible running to here!\n");
		assert(0); // 不可能执行到此!!!
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return this_ptr->win_wnd_proc(hwnd, msg, wparam, lparam);
}

LRESULT player_impl::win_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (msg)
	{
	case WM_CREATE:
		{
			SetTimer(hwnd, ID_PLAYER_TIMER, 100, NULL);
		}
		break;
	case WM_TIMER:
		{
			if (wparam != ID_PLAYER_TIMER)
				break;
			// 计算显示客户区.
			int more_y = GetSystemMetrics(SM_CYCAPTION)
				+ (GetSystemMetrics(SM_CYBORDER) * 2)
				+ (GetSystemMetrics(SM_CYDLGFRAME) * 2);
			int more_x = (GetSystemMetrics(SM_CXDLGFRAME) * 2)
				+ (GetSystemMetrics(SM_CXBORDER) * 2);

			if (!m_avplay || !m_avplay->m_video_ctx)
				break;

			if (m_avplay && m_avplay->m_video_ctx &&
				(m_avplay->m_video_ctx->width != 0
				|| m_avplay->m_video_ctx->height != 0
				|| m_avplay->m_play_status == playing))
			{
				RECT rc = { 0 };
				GetWindowRect(hwnd, &rc);
				SetWindowPos(hwnd, HWND_NOTOPMOST, rc.left, rc.top,
					m_avplay->m_video_ctx->width + more_x,
					m_avplay->m_video_ctx->height + more_y,
					SWP_FRAMECHANGED);
				KillTimer(hwnd, ID_PLAYER_TIMER);
			}

			// 得到正确的宽高信息.
			m_video_width = m_avplay->m_video_ctx->width;
			m_video_height = m_avplay->m_video_ctx->height;
		}
		break;
	case WM_KEYDOWN:
		if (wparam == VK_F2)
		{
			full_screen(!m_full_screen);
		}
		break;
	case WM_RBUTTONDOWN:
		{
			if (m_avplay && m_avplay->m_play_status == playing)
				pause();
			else if (m_avplay && m_avplay->m_play_status == paused)
				resume();
		}
		break;
	case WM_LBUTTONDOWN:
		{
			RECT rc = { 0 };
			GetWindowRect(hwnd, &rc);
			int width = rc.right - rc.left;
			int xpos = LOWORD(lparam);
			double fact = (double)xpos / width;

			if (m_avplay && (m_avplay->m_play_status == playing
				|| m_avplay->m_play_status == completed)
				&& (fact >= 0.0f && fact <= 1.0f))
				::av_seek(m_avplay, fact);
		}
		break;
// 	case WM_PAINT:
// 		hdc = BeginPaint(hwnd, &ps);
// 		if (m_avplay)
// 			win_paint(hwnd, hdc);
// 		EndPaint(hwnd, &ps);
// 		break;
	case WM_ERASEBKGND:
		{
			return 1;
// 			if (m_video && m_avplay->m_play_status == playing)
// 				return 1;
// 			else
// 				return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	case WM_ACTIVATE:
	case WM_SYNCPAINT:
		break;
	case WM_MOVING:
	case WM_MOVE:
	case WM_SIZE:
		{
			RECT window;
			GetClientRect(hwnd, &window);
			if (m_avplay && m_avplay->m_vo_ctx &&
				m_video->video_dev)
			{
				m_video->re_size(m_video, LOWORD(lparam), HIWORD(lparam));
			}
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	if (m_old_win_proc)
		return CallWindowProc(m_old_win_proc, hwnd, msg, wparam, lparam);// m_old_win_proc(hwnd, msg, wparam, lparam);
	else
		return DefWindowProc(hwnd, msg, wparam, lparam);
}

void player_impl::win_paint(HWND hwnd, HDC hdc)
{
	if (m_avplay &&
		 m_avplay->m_vo_ctx &&
		 m_video->video_dev &&
		 m_video->use_overlay(m_video) != -1)
	{
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		fill_rectange(hwnd, hdc, client_rect, client_rect);
	}
	else
	{
#if 0	// 默认不开启比例.
		RECT client_rect;
		int win_width, win_height;
		float window_aspect = (float)m_video_width / (float)m_video_height;
		GetClientRect(hwnd, &client_rect);
		int nWidth = win_width = client_rect.right - client_rect.left;
		int nHeight = win_height = client_rect.bottom - client_rect.top;
		{
			int tmpheight = ((float)nWidth / window_aspect);
			tmpheight += tmpheight % 2;
			if(tmpheight > nHeight)
			{
				nWidth = ((float)nHeight * window_aspect);
				nWidth += nWidth % 2;
			}
			else 
			{
				nHeight = tmpheight;
			}

			// 居中对齐.
			client_rect.left += ((win_width - nWidth) / 2);
			client_rect.top += ((win_height - nHeight) / 2);
			client_rect.bottom -= ((win_height - nHeight) / 2);
			client_rect.right -= ((win_width - nWidth) / 2);
		}
		RECT win_rect = { 0 };
		GetClientRect(hwnd, &win_rect);
		if (win_rect.left != client_rect.left &&
			win_rect.right != client_rect.right)
		{
			RECT lr = win_rect;
			// first left.
			lr.right = client_rect.left;

			fill_rectange(hwnd, hdc, win_rect, lr);

			// second right.
			lr.left = client_rect.right;
			lr.right = win_rect.right;

			fill_rectange(hwnd, hdc, win_rect, lr);
		}

		if (win_rect.top != client_rect.top &&
			win_rect.bottom != client_rect.bottom)
		{
			RECT tb = win_rect;
			// first top.
			tb.bottom = client_rect.top;
			fill_rectange(hwnd, hdc, win_rect, tb);

			// second bottom.
			tb.top = client_rect.bottom;
			tb.bottom = win_rect.bottom;
			fill_rectange(hwnd, hdc, win_rect, tb);
		}
#endif
	}
}

void player_impl::fill_rectange(HWND hWnd, HDC hdc, RECT win_rect, RECT client_rect)
{
	HDC mem_dc = CreateCompatibleDC(hdc);
	HBITMAP memBM = CreateCompatibleBitmap(hdc,
		win_rect.right - win_rect.left, win_rect.bottom - win_rect.top);
	SelectObject(mem_dc, memBM);
	FillRect(mem_dc, &win_rect, m_brbackground);
	BitBlt(hdc,
		client_rect.left, client_rect.top,
		client_rect.right - client_rect.left, client_rect.bottom - client_rect.top,
		mem_dc,
		0, 0,
		SRCCOPY);
	DeleteDC(mem_dc);
	DeleteObject(memBM);
}

void player_impl::init_file_source(source_context *sc)
{
	sc->init_source = file_init_source;
	sc->read_data = file_read_data;
	sc->close = file_close;
	sc->destory = file_destory;
	sc->offset = 0;
}

void player_impl::init_torrent_source(source_context *sc)
{
	sc->init_source = bt_init_source;
	sc->read_data = bt_read_data;
	sc->video_media_info = bt_media_info;
	sc->read_seek = bt_read_seek;
	sc->close = bt_close;
	sc->destory = bt_destory;
	sc->offset = 0;
	sc->save_path = strdup(".");
}

void player_impl::init_yk_source(source_context *sc)
{
    sc->init_source = yk_init_source;
    sc->read_data = yk_read_data;
    sc->video_media_info = yk_media_info;
    sc->read_seek = yk_read_seek;
    sc->close = yk_close;
    sc->destory = yk_destory;
    sc->offset = 0;
    sc->save_path = strdup(".");
}

void player_impl::init_audio(ao_context *ao)
{
	ao->init_audio = wave_init_audio;
	ao->play_audio = wave_play_audio;
	ao->audio_control = wave_audio_control;
	ao->mute_set = wave_mute_set;
	ao->destory_audio = wave_destory_audio;
}

void player_impl::init_video(vo_context *vo, int render_type/* = RENDER_D3D*/)
{
	int ret = 0;
	int check = 0;

	do
	{
#ifdef USE_Y4M_OUT
		if (render_type == RENDER_Y4M || check == -1)
		{
			ret = y4m_init_video((void*)vo, 10, 10, PIX_FMT_YUV420P);
			y4m_destory_render(vo);
			if (ret == 0)
			{
				vo->init_video = y4m_init_video;
				m_draw_frame = y4m_render_one_frame;
				vo->re_size = y4m_re_size;
				vo->aspect_ratio = y4m_aspect_ratio;
				vo->use_overlay = y4m_use_overlay;
				vo->destory_video = y4m_destory_render;
				vo->render_one_frame = &player_impl::draw_frame;

				::logger("init video render to y4m.\n");

				break;
			}
		}
#endif

		if (render_type == RENDER_D3D || check == -1)
		{
			ret = d3d_init_video((void*)vo, 10, 10, PIX_FMT_YUV420P);
			d3d_destory_render(vo);
			if (ret == 0)
			{
				vo->init_video = d3d_init_video;
				m_draw_frame = d3d_render_one_frame;
				vo->re_size = d3d_re_size;
				vo->aspect_ratio = d3d_aspect_ratio;
				vo->use_overlay = d3d_use_overlay;
				vo->destory_video = d3d_destory_render;
				vo->render_one_frame = &player_impl::draw_frame;

				::logger("init video render to d3d.\n");

				break;
			}
		}

		if (render_type == RENDER_DDRAW || check == -1)
		{
			ret = ddraw_init_video((void*)vo, 10, 10, PIX_FMT_YUV420P);
			ddraw_destory_render(vo);
			if (ret == 0)
			{
				vo->init_video = ddraw_init_video;
				m_draw_frame = ddraw_render_one_frame;
				vo->re_size = ddraw_re_size;
				vo->aspect_ratio = ddraw_aspect_ratio;
				vo->use_overlay = ddraw_use_overlay;
				vo->destory_video = ddraw_destory_render;
				vo->render_one_frame = &player_impl::draw_frame;

				::logger("init video render to ddraw.\n");

				break;
			}
		}

		if (render_type == RENDER_OGL || check == -1)
		{
			ret = ogl_init_video((void*)vo, 10, 10, PIX_FMT_YUV420P);
			ogl_destory_render(vo);
			if (ret == 0)
			{
				vo->init_video = ogl_init_video;
				m_draw_frame = ogl_render_one_frame;
				vo->re_size = ogl_re_size;
				vo->aspect_ratio = ogl_aspect_ratio;
				vo->use_overlay = ogl_use_overlay;
				vo->destory_video = ogl_destory_render;
				vo->render_one_frame = &player_impl::draw_frame;

				::logger("init video render to ogl.\n");

				break;
			}
		}

		if (render_type == RENDER_SOFT || check == -1)
		{
			ret = gdi_init_video((void*)vo, 10, 10, PIX_FMT_YUV420P);
			gdi_destory_render(vo);
			if (ret == 0)
			{
				vo->init_video = gdi_init_video;
				m_draw_frame = gdi_render_one_frame;
				vo->re_size = gdi_re_size;
				vo->aspect_ratio = gdi_aspect_ratio;
				vo->use_overlay = gdi_use_overlay;
				vo->destory_video = gdi_destory_render;
				vo->render_one_frame = &player_impl::draw_frame;

				::logger("init video render to gdi.\n");

				break;
			}
		}

	} while (check-- == 0);

	// 表示视频渲染器初始化失败!!!
	assert(check != -2);

	// 保存this为user_ctx.
	vo->user_ctx = (void*)this;
}

BOOL player_impl::open(const char *movie, int media_type, int render_type)
{
	// 如果未关闭原来的媒体, 则先关闭.
	if (m_avplay || m_source)
		close();

	// 未创建窗口, 无法播放, 返回失败.
	if (!IsWindow(m_hwnd))
		return FALSE;

	char filename[MAX_PATH];
	int len = strlen(movie) + 1;

	strcpy(filename, movie);

	uint64_t file_lentgh = 0;
	if (media_type == MEDIA_TYPE_FILE || media_type == MEDIA_TYPE_BT)
	{
		file_lentgh = file_size(movie);
		if (file_lentgh < 0)
		{
			::logger("get file size failed!\n");
			return FALSE;
		}
	}

	do {
		// 创建avplay.
		m_avplay = alloc_avplay_context();
		if (!m_avplay)
		{
			::logger("allocate avplay context failed!\n");
			break;
		}

		// 根据打开的文件类型, 创建不同媒体源.
		if (media_type == MEDIA_TYPE_FILE)
		{
			len = strlen(filename);
			m_source = alloc_media_source(MEDIA_TYPE_FILE, filename, len + 1, file_lentgh);
			if (!m_source)
			{
				::logger("allocate media source failed, type is file.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));

			// 初始化文件媒体源.
			init_file_source(m_source);
		}

		if (media_type == MEDIA_TYPE_BT)
		{
			// 先读取bt种子数据, 然后作为附加数据保存到媒体源.
			FILE *fp = fopen(filename, "r+b");
			if (!fp)
			{
				::logger("open torrent file \'%s\' failed!\n", filename);
				break;
			}
			char *torrent_data = (char*)malloc(file_lentgh);
			int readbytes = fread(torrent_data, 1, file_lentgh, fp);
			if (readbytes != file_lentgh)
			{
				::logger("read torrent file \'%s\' failed!\n", filename);
				break;
			}
			m_source = alloc_media_source(MEDIA_TYPE_BT, torrent_data, file_lentgh, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is torrent.\n");
				break;
			}

			free(torrent_data);

			// 初始化torrent媒体源.
			init_torrent_source(m_source);
		}

		if (media_type == MEDIA_TYPE_YK)
		{
			m_source = alloc_media_source(MEDIA_TYPE_YK, filename, 0, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is yk.\n");
				break;
			}

			// 初始化yk媒体源.
			init_yk_source(m_source);
		}

		if (media_type == MEDIA_TYPE_HTTP)
		{
			len = strlen(filename) + 1;
			m_source = alloc_media_source(MEDIA_TYPE_HTTP, filename, len, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is youku.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));
		}

		if (media_type == MEDIA_TYPE_RTSP)
		{
			len = strlen(filename) + 1;
			m_source = alloc_media_source(MEDIA_TYPE_RTSP, filename, len, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is rtsp.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));
		}

		// 初始化avplay.
		if (initialize(m_avplay, m_source) != 0)
		{
			::logger("initialize avplay failed!\n");
			break;
		}

		// 如果是bt类型, 则在此得到视频文件列表, 并添加到m_media_list.
		if (media_type == MEDIA_TYPE_BT)
		{
			int i = 0;
			media_info *media = m_avplay->m_source_ctx->media;
			for (; i < m_avplay->m_source_ctx->media_size; i++)
			{
				std::string name;
				name = media->name;
				m_media_list.insert(std::make_pair(filename, name));
			}
		}

		// 分配音频和视频的渲染器.
		m_audio = alloc_audio_render();
		if (!m_audio)
		{
			::logger("allocate audio render failed!\n");
			break;
		}

		m_video = alloc_video_render(m_hwnd);
		if (!m_video)
		{
			::logger("allocate video render failed!\n");
			break;
		}

		// 初始化音频和视频渲染器.
		init_audio(m_audio);
		init_video(m_video, render_type);

		// 配置音频视频渲染器.
		configure(m_avplay, m_video, VIDEO_RENDER);
		configure(m_avplay, m_audio, AUDIO_RENDER);

		// 得到视频宽高.
		if (m_avplay->m_video_ctx)
		{
			m_video_width = m_avplay->m_video_ctx->width;
			m_video_height = m_avplay->m_video_ctx->height;
		}

		// 打开视频实时码率和帧率计算.
		enable_calc_frame_rate(m_avplay);
		enable_calc_bit_rate(m_avplay);

		return TRUE;

	} while (0);

	if (m_avplay)
		free_avplay_context(m_avplay);
	m_avplay = NULL;
	if (m_source)
		free_media_source(m_source);
	if (m_audio)
		free_audio_render(m_audio);
	if (m_video)
		free_video_render(m_video);

	::logger("open avplay failed!\n");

	return FALSE;
}

BOOL player_impl::play(double fact/* = 0.0f*/, int index /*= 0*/)
{
	// 重复播放, 返回错误.
	if (m_cur_index == index)
		return FALSE;

	// 如果是文件数据, 则直接播放.
	if (::av_start(m_avplay, fact, index) != 0)
		return FALSE;

	m_cur_index = index;

	return TRUE;
}

BOOL player_impl::pause()
{
	if (m_avplay && m_avplay->m_play_status == playing)
	{
		::av_pause(m_avplay);
		::logger("set to pause.\n");
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::resume()
{
	if (m_avplay && m_avplay->m_play_status == paused)
	{
		::av_resume(m_avplay);
		::logger("set to resume.\n");
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::stop()
{
	if (m_avplay)
	{
		::av_stop(m_avplay);
		m_cur_index = -1;
		::logger("stop play.\n");
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::wait_for_completion()
{
	if (m_avplay)
	{
		::wait_for_completion(m_avplay);
		::logger("play completed.\n");
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::close()
{
	if (m_avplay)
	{
		::av_destory(m_avplay);
		m_avplay = NULL;	// 清空指针, 避免下一次重新open时出错.
		m_source = NULL;	// m_source 在 read_pkt_thrd 线程退出时, 会自动释放, 这里只需要简单清空即可.
		m_cur_index = -1;
		::logger("close avplay.\n");
		return TRUE;
	}
	else
	{
		// m_avplay已经不存在, 手动释放m_source.
		if (m_source)
		{
			free_media_source(m_source);
			m_source = NULL;
		}
	}
	return FALSE;
}

void player_impl::seek_to(double fact)
{
	if (m_avplay)
	{
		::av_seek(m_avplay, fact);
		::logger("seek to %.2f.\n", fact);
	}
}

void player_impl::volume(double l, double r)
{
	if (m_avplay)
	{
		::av_volume(m_avplay, l, r);
		::logger("set volume to left: %.2f, right: %.2f.\n", l, r);
	}
}

BOOL player_impl::full_screen(BOOL fullscreen)
{
	HWND hparent = GetParent(m_hwnd);

	// 不支持非顶层窗口全屏操作.
	if (IsWindow(hparent))
	{
		::logger("do\'nt support full screen mode\n");
		return FALSE;
	}

	// Save the current windows placement/placement to
	// restore when fullscreen is over
	WINDOWPLACEMENT window_placement;
	window_placement.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(m_hwnd, &window_placement);

	if (fullscreen && !m_full_screen)
	{
		m_full_screen = true;
		m_wnd_style = GetWindowLong(m_hwnd, GWL_STYLE);
		printf("entering fullscreen mode.\n");
		SetWindowLong(m_hwnd, GWL_STYLE, WS_CLIPCHILDREN | WS_VISIBLE);

		if (IsWindow(hparent))
		{
			// Retrieve current window position so fullscreen will happen
			// on the right screen
			HMONITOR hmon = MonitorFromWindow(hparent, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi;
			mi.cbSize = sizeof(MONITORINFO);
			if (::GetMonitorInfo(hmon, &mi))
				::SetWindowPos(m_hwnd, 0,
				mi.rcMonitor.left,
				mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		else
		{
			// Maximize non embedded window
			ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
		}

		if (IsWindow(hparent))
		{
			// Hide the previous window
			RECT rect;
			GetClientRect(m_hwnd, &rect);
			// SetParent(hwnd, hwnd);
			SetWindowPos(m_hwnd, 0, 0, 0,
				rect.right, rect.bottom,
				SWP_NOZORDER|SWP_FRAMECHANGED);
			HWND topLevelParent = GetAncestor(hparent, GA_ROOT);
			ShowWindow(topLevelParent, SW_HIDE);
		}
		SetForegroundWindow(m_hwnd);
		return TRUE;
	}

	if (!fullscreen && m_full_screen)
	{
		m_full_screen = FALSE;
		printf("leaving fullscreen mode.\n");
		// Change window style, no borders and no title bar
		SetWindowLong(m_hwnd, GWL_STYLE, m_wnd_style);

		if (hparent)
		{
			RECT rect;
			GetClientRect(hparent, &rect);
			// SetParent(hwnd, hparent);
			SetWindowPos(m_hwnd, 0, 0, 0,
				rect.right, rect.bottom,
				SWP_NOZORDER | SWP_FRAMECHANGED);

			HWND topLevelParent = GetAncestor(hparent, GA_ROOT);
			ShowWindow(topLevelParent, SW_SHOW);
			SetForegroundWindow(hparent);
			ShowWindow(m_hwnd, SW_HIDE);
		}
		else
		{
			// return to normal window for non embedded vout
			SetWindowPlacement(m_hwnd, &window_placement);
			ShowWindow(m_hwnd, SW_SHOWNORMAL);
		}
		return TRUE;
	}

	return FALSE;
}

double player_impl::curr_play_time()
{
	return ::av_curr_play_time(m_avplay);
}

double player_impl::duration()
{
	if (!m_avplay || !m_avplay->m_format_ctx)
		return -1.0f;
	return (double)m_avplay->m_format_ctx->duration / AV_TIME_BASE;
}

int player_impl::video_width()
{
	if (!m_avplay || !m_avplay->m_format_ctx)
		return 0;
	return m_avplay->m_video_ctx->width;
}

int player_impl::video_height()
{
	if (!m_avplay || !m_avplay->m_format_ctx)
		return 0;
	return m_avplay->m_video_ctx->height;
}

double player_impl::buffering()
{
	if (!m_avplay)
		return 0.0f;
	return ::buffering(m_avplay);
}

std::map<std::string, std::string>& player_impl::play_list()
{
	return m_media_list;
}

HWND player_impl::get_window_handle()
{
	return m_hwnd;
}

BOOL player_impl::load_subtitle(const char *subtitle)
{
	EnterCriticalSection(&m_plugin_cs);
	if (!m_plugin)
		m_plugin = new vsfilter_interface("vsfilter.dll");
	if (subtitle)
		m_subtitle = subtitle;
	else
		m_subtitle = "";
	m_change_subtitle = true;
	LeaveCriticalSection(&m_plugin_cs);
	return TRUE;
}

int player_impl::draw_frame(void *ctx, AVFrame* data, int pix_fmt, double pts)
{
	vo_context *vo = (vo_context*)ctx;
	player_impl *this_ptr = (player_impl*)vo->user_ctx;

	// 更改字幕.
	if (this_ptr->m_change_subtitle)
	{
		EnterCriticalSection(&this_ptr->m_plugin_cs);
		this_ptr->m_plugin->subtitle_uninit();
		if (this_ptr->m_subtitle.empty())
		{
			delete this_ptr->m_plugin;
			this_ptr->m_plugin = NULL;
		}
		else
		{
			this_ptr->m_plugin->subtitle_init();
			bool b = this_ptr->m_plugin->subtitle_open((char*)this_ptr->m_subtitle.c_str(),
				this_ptr->m_video_width, this_ptr->m_video_height);
			if (!b)
			{
				this_ptr->m_plugin->subtitle_uninit();
				delete this_ptr->m_plugin;
				this_ptr->m_plugin = NULL;
			}
		}

		this_ptr->m_change_subtitle = false;
		LeaveCriticalSection(&this_ptr->m_plugin_cs);
	}

	if (this_ptr->m_plugin)
	{
		// 添加字幕.
		int size = this_ptr->m_video_width * this_ptr->m_video_height * 3 / 2;
		int64_t nanosecond = this_ptr->curr_play_time() * 10000000;
		this_ptr->m_plugin->subtitle_do(data->data[0], nanosecond, size);
	}

	// 实际渲染.
	return this_ptr->m_draw_frame(ctx, data, pix_fmt, pts);
}

int player_impl::download_rate()
{
	if (m_source)
		return m_source->info.speed;
}

void player_impl::set_download_rate(int k)
{
	if (m_source)
		m_source->info.limit_speed = k;
}

void player_impl::toggle_mute()
{
	if (m_avplay)
	{
		m_mute = !m_mute;
		::av_mute_set(m_avplay, m_mute);
	}
}

void player_impl::mute_set(bool s)
{
	if (m_avplay)
	{
		m_mute = s;
		::av_mute_set(m_avplay, s);
	}
}
