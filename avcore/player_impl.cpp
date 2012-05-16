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
public:
	win_data();
	~win_data();

public:
	BOOL add_window(HWND hwnd, player_impl *win);
	BOOL remove_window(HWND hwnd);
	player_impl *lookup_window(HWND hwnd);

	inline HHOOK get_hook_handle();
	inline void set_hook_handle(HHOOK hook);

	inline player_impl *get_current_window();
	inline void set_current_window(player_impl *win);

private:
	CRITICAL_SECTION m_cs;
	std::map<HWND, player_impl*> m_maps;
	HHOOK m_hook_cbt_filter;
	player_impl *m_current_win;
};

// 线程本地存储.
template <typename T>
class thread_tls_ptr 
{
public:
	thread_tls_ptr()
		: m_tls_index(0)
	{
		m_tls_index = TlsAlloc();
	}
	~thread_tls_ptr()
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

// 本地线程存储, 用于存储win_data, 并且在各线程独立.
thread_tls_ptr<win_data> win_data_ptr;

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
	std::map<HWND, player_impl*>::iterator finder = m_maps.find(hwnd);
	if (finder != m_maps.end())
	{
		LeaveCriticalSection(&m_cs);
		return FALSE;
	}
	m_maps.insert(std::make_pair(hwnd, win));
	LeaveCriticalSection(&m_cs);
	return TRUE;
}

BOOL win_data::remove_window(HWND hwnd)
{
	EnterCriticalSection(&m_cs);
	std::map<HWND, player_impl*>::iterator finder = m_maps.find(hwnd);
	if (finder != m_maps.end())
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
	std::map<HWND, player_impl*>::iterator finder = m_maps.find(hwnd);
	if (finder == m_maps.end())
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	LeaveCriticalSection(&m_cs);
	return finder->second;
}

HHOOK win_data::get_hook_handle()
{
	return m_hook_cbt_filter;
}

void win_data::set_hook_handle(HHOOK hook)
{
	m_hook_cbt_filter = hook;
}

player_impl *win_data::get_current_window()
{
	return m_current_win;
}

void win_data::set_current_window(player_impl *win)
{
	m_current_win = win;
}

static uint64_t file_size(LPCTSTR filename)
{
	WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };

	if (!::GetFileAttributesEx(filename, ::GetFileExInfoStandard, &fad))
		return -1;
	return (static_cast<uint64_t>(fad.nFileSizeHigh)
		<< (sizeof(fad.nFileSizeLow) * 8)) + fad.nFileSizeLow;
}

LRESULT CALLBACK win_cbt_filter_hook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code != HCBT_CREATEWND)
	{
		return CallNextHookEx(win_data_ptr->get_hook_handle(),
			code, wParam, lParam);
	}
	else
	{
		HWND hwnd = (HWND)wParam;
		player_impl *win = win_data_ptr->get_current_window();
		win_data_ptr->add_window(hwnd, win);
	}

	return CallNextHookEx(win_data_ptr->get_hook_handle(),
		code, wParam, lParam);
}

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
, m_video_width(0)
, m_video_height(0)
, m_wnd_style(0)
, m_full_screen(FALSE)
{
	// 初始化线程局部存储对象.
	win_data_ptr.set(new win_data());
}

player_impl::~player_impl(void)
{
	if (m_brbackground)
	{
		DeleteObject(m_brbackground);
		m_brbackground = NULL;
	}
}

HWND player_impl::create_window(LPCTSTR player_name)
{
	WNDCLASSEX wcex;

	// 得到进程实例句柄.
	m_hinstance = (HINSTANCE)GetModuleHandle(NULL);
	// 创建非纯黑色的画刷, 用于ddraw播放时刷背景色.
	m_brbackground = CreateSolidBrush(RGB(0, 0, 1));
	wcex.cbSize = sizeof(WNDCLASSEX);

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

	if (!RegisterClassEx(&wcex))
		return NULL;

	// 创建hook, 以便在窗口创建之前得到HWND句柄, 使HWND与this绑定.
	HHOOK hook = SetWindowsHookEx(WH_CBT, 
		win_cbt_filter_hook, NULL, GetCurrentThreadId());
	win_data_ptr->set_hook_handle(hook);
	win_data_ptr->set_current_window(this);

	// 创建窗口.
	m_hwnd = CreateWindowEx(/*WS_EX_APPWINDOW*/0,
		player_name, player_name, WS_OVERLAPPEDWINDOW/* | WS_CLIPSIBLINGS | WS_CLIPCHILDREN*/,
		0, 0, 800, 600, NULL, NULL, m_hinstance, NULL);

	// 	m_hwnd = CreateWindow(player_name, player_name,
	// 		WS_OVERLAPPEDWINDOW, 100, 100, 300, 300,
	// 		0, NULL, m_hinstance, NULL);

	// 撤销hook.
	UnhookWindowsHookEx(hook);
	win_data_ptr->set_hook_handle(NULL);

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

	win_data_ptr->remove_window(m_hwnd);

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
	win_data_ptr->add_window(hwnd, this);
	m_old_win_proc = (WNDPROC)::SetWindowLongPtr(hwnd, 
		GWLP_WNDPROC, (LONG_PTR)&player_impl::static_win_wnd_proc);
	if (!m_old_win_proc)
	{
		win_data_ptr->remove_window(hwnd);
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
	win_data_ptr->remove_window(hwnd);
	m_hwnd = NULL;

	return TRUE;
}

LRESULT CALLBACK player_impl::static_win_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	player_impl* this_ptr = win_data_ptr->lookup_window(hwnd);
	if (!this_ptr)
	{
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
				&& (fact >= 0.0f && fact <=1.0f))
				::seek(m_avplay, fact);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		if (m_avplay)
			win_paint(hwnd, hdc);
		EndPaint(hwnd, &ps);
		break;
	case WM_ERASEBKGND:
		{
			if (m_video && m_avplay->m_play_status == playing)
				return 1;
			else
				return DefWindowProc(hwnd, msg, wparam, lparam);
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
			if (m_video && m_video->ctx)
				m_video->re_size(m_video->ctx, 
				LOWORD(lparam), HIWORD(lparam));
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
		return m_old_win_proc(hwnd, msg, wparam, lparam);
	else
		return DefWindowProc(hwnd, msg, wparam, lparam);
}

void player_impl::win_paint(HWND hwnd, HDC hdc)
{
	if (m_video && m_video->ctx &&
		m_video->use_overlay(m_video->ctx) != -1)
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

void player_impl::init_file_source(media_source *ms)
{
	ms->init_source = file_init_source;
	ms->read_data = file_read_data;
	ms->close = file_close;
	ms->destory = file_destory;
	ms->offset = 0;
}

void player_impl::init_torrent_source(media_source *ms)
{
	ms->init_source = bt_init_source;
	ms->read_data = bt_read_data;
	ms->bt_media_info = bt_media_info;
	ms->close = bt_close;
	ms->destory = bt_destory;
	ms->offset = 0;
}

void player_impl::init_audio(audio_render *ao)
{
	ao->init_audio = wave_init_audio;
	ao->play_audio = wave_play_audio;
	ao->audio_control = wave_audio_control;
	ao->destory_audio = wave_destory_audio;
}

void player_impl::init_video(video_render *vo)
{
	void *ctx = NULL;
	int ret = 0;

	do 
	{
		ret = ddraw_init_video(&ctx, (void*)m_hwnd, 10, 10, PIX_FMT_YUV420P);
		ddraw_destory_render(ctx);
		if (ret == 0)
		{
			vo->init_video = ddraw_init_video;
			vo->render_one_frame = ddraw_render_one_frame;
			vo->re_size = ddraw_re_size;
			vo->aspect_ratio = ddraw_aspect_ratio;
			vo->use_overlay = ddraw_use_overlay;
			vo->destory_video = ddraw_destory_render;
			break;
		}

		ret = d3d_init_video(&ctx, (void*)m_hwnd, 10, 10, PIX_FMT_YUV420P);
		d3d_destory_render(ctx);
		if (ret == 0)
		{
			vo->init_video = d3d_init_video;
			vo->render_one_frame = d3d_render_one_frame;
			vo->re_size = d3d_re_size;
			vo->aspect_ratio = d3d_aspect_ratio;
			vo->use_overlay = d3d_use_overlay;
			vo->destory_video = d3d_destory_render;
			break;
		}

		ret = ogl_init_video(&ctx, (void*)m_hwnd, 10, 10, PIX_FMT_YUV420P);
		ogl_destory_render(ctx);
		if (ret == 0)
		{
			vo->init_video = ogl_init_video;
			vo->render_one_frame = ogl_render_one_frame;
			vo->re_size = ogl_re_size;
			vo->aspect_ratio = ogl_aspect_ratio;
			vo->use_overlay = ogl_use_overlay;
			vo->destory_video = ogl_destory_render;
			break;
		}

		// 表示视频渲染器初始化失败!!!
		assert(0);
	} while (0);
}

BOOL player_impl::open(LPCTSTR movie, int media_type)
{
	// 如果未关闭原来的媒体, 则返回失败.
	if (m_avplay || m_source)
		return FALSE;

	// 未创建窗口, 无法播放, 返回失败.
	if (!IsWindow(m_hwnd))
		return FALSE;

	char filename[MAX_PATH];
	int len = _tcslen(movie) + 1;

#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, movie, len, filename, 2 * len, NULL, NULL);
#else
	strcpy(filename, movie);
#endif

	uint64_t file_lentgh = 0;
	if (media_type == MEDIA_TYPE_FILE || media_type == MEDIA_TYPE_BT)
	{
		file_lentgh = file_size(movie);
		if (file_lentgh < 0)
		{
			printf("get file size failed!\n");
			return FALSE;
		}
	}

	do 
	{
		// 创建avplay.
		m_avplay = alloc_avplay_context();
		if (!m_avplay)
			break;

		// 根据打开的文件类型, 创建不同媒体源.
		if (media_type == MEDIA_TYPE_FILE)
		{
			len = strlen(filename);
			m_source = alloc_media_source(MEDIA_TYPE_FILE, filename, len + 1, file_lentgh);
			if (!m_source)
				break;

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));

			// 初始化文件媒体源.
			init_file_source(m_source);
			m_source->data_len = len + 1;
		}

		if (media_type == MEDIA_TYPE_BT)
		{
			// 先读取bt种子数据, 然后作为附加数据保存到媒体源.
			FILE *fp = fopen(filename, "r+b");
			char *torrent_data = (char*)malloc(file_lentgh);
			int readbytes = fread(torrent_data, 1, file_lentgh, fp);
			if (readbytes != file_lentgh)
			{
				assert(0);
				break;
			}
			m_source = alloc_media_source(MEDIA_TYPE_BT, torrent_data, file_lentgh, 0);
			free(torrent_data);

			// 初始化torrent媒体源.
			m_source->data_len = file_lentgh;
			init_torrent_source(m_source);
		}

		if (media_type == MEDIA_TYPE_HTTP)
		{
			len = strlen(filename) + 1;
			m_source = alloc_media_source(MEDIA_TYPE_HTTP, filename, len, 0);
			if (!m_source)
				break;
			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));
			m_source->data_len = len;
		}

		if (media_type == MEDIA_TYPE_RTSP)
		{
			len = strlen(filename) + 1;
			m_source = alloc_media_source(MEDIA_TYPE_RTSP, filename, len, 0);
			if (!m_source)
				break;
			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename, filename));
			m_source->data_len = len;
		}

		// 初始化avplay.
		if (initialize(m_avplay, m_source) != 0)
			break;

		// 如果是bt类型, 则在此得到视频文件列表, 并添加到m_media_list.
		if (media_type == MEDIA_TYPE_BT)
		{
			int i = 0;
			media_info *media = m_avplay->m_media_source->media;
			for (; i < m_avplay->m_media_source->media_size; i++)
			{
				std::string name;
				name = media->name;
				m_media_list.insert(std::make_pair(filename, name));
			}
		}

		// 分配音频和视频的渲染器.
		m_audio = alloc_audio_render();
		if (!m_audio)
			break;
		m_video = alloc_video_render(m_hwnd);
		if (!m_video)
			break;

		// 初始化音频和视频渲染器.
		init_audio(m_audio);
		init_video(m_video);

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

	return FALSE;
}

BOOL player_impl::play(int index /*= 0*/)
{
	// 重复播放, 返回错误.
	if (m_cur_index == index)
		return FALSE;

	// 如果是文件数据, 则直接播放.
	if (::start(m_avplay, index) != 0)
		return FALSE;
	m_cur_index = index;

	return TRUE;
}

BOOL player_impl::pause()
{
	if (m_avplay && m_avplay->m_play_status == playing)
	{
		::pause(m_avplay);
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::resume()
{
	if (m_avplay && m_avplay->m_play_status == paused)
	{
		::resume(m_avplay);
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::stop()
{
	if (m_avplay)
	{
		::stop(m_avplay);
		m_cur_index = -1;
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::wait_for_completion()
{
	if (m_avplay)
	{
		::wait_for_completion(m_avplay);
		return TRUE;
	}

	return FALSE;
}

BOOL player_impl::close()
{
	if (m_avplay)
	{
		::destory(m_avplay);
		m_cur_index = -1;
		return TRUE;
	}
	return FALSE;
}

void player_impl::seek_to(double sec)
{
	if (m_avplay)
	{
		::seek(m_avplay, sec);
	}
}

void player_impl::volume(double vol)
{
	if (m_avplay)
	{
		::volume(m_avplay, vol);
	}
}

BOOL player_impl::full_screen(BOOL fullscreen)
{
	HWND hparent = GetParent(m_hwnd);

	// 不支持非顶层窗口全屏操作.
	if (IsWindow(hparent))
		return FALSE;

	/* Save the current windows placement/placement to restore
	when fullscreen is over */
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
			/* Retrieve current window position so fullscreen will happen
			* on the right screen */
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
			/* Maximize non embedded window */
			ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
		}

		if (IsWindow(hparent))
		{
			/* Hide the previous window */
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
		/* Change window style, no borders and no title bar */
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
			/* return to normal window for non embedded vout */
			SetWindowPlacement(m_hwnd, &window_placement);
			ShowWindow(m_hwnd, SW_SHOWNORMAL);
		}
		return TRUE;
	}

	return FALSE;
}

double player_impl::curr_play_time()
{
	return ::curr_play_time(m_avplay);
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

std::map<std::string, std::string>& player_impl::play_list()
{
	return m_media_list;
}

HWND player_impl::GetWnd()
{
	return m_hwnd;
}
