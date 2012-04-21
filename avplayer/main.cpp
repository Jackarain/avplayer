#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif	

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN

// Windows 头文件:
#include <windows.h>
#include <ShellAPI.h>

// C 运行时头文件
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <string>
#include "avplayer.h"

template <typename T>
static T extension(const T &filename)
{
	if (filename == _T(".") || filename == _T(".."))
		return T(_T(""));
	T::size_type pos(filename.rfind(_T(".")));
	return pos == T::npos
		? T(_T(""))
		: T(filename.c_str() + pos);
}

#ifdef UNICODE
typedef std::wstring auto_string;
#else
typedef std::string auto_string;
#endif // UNICODE

int APIENTRY _tWinMain(HINSTANCE hInstance,
							  HINSTANCE hPrevInstance,
							  LPTSTR    lpCmdLine,
							  int       nCmdShow)
{
	setlocale(LC_ALL, "chs");

	auto_string filename;
	MSG msg;
	int ret = NULL;

	if (_tcslen(lpCmdLine) == 0)
		return -1;

	filename.resize(_tcslen(lpCmdLine));
	_tcscpy((TCHAR*)filename.data(), lpCmdLine);

	avplayer win;

	if (win.create_window(_T("main")) == NULL)
		return -1;

	auto_string ext = extension(filename);
	if (ext == _T(".torrent"))
	{
		if (!win.open(filename.c_str(), MEDIA_TYPE_BT))
			return -1;
	}
	else
	{
		auto_string str = filename;
		auto_string is_url = str.substr(0, 7);
		if (is_url == _T("http://"))
		{
			if (!win.open(filename.c_str(), MEDIA_TYPE_HTTP))
				return -1;
		}
		else if (is_url == _T("rtsp://"))
		{
			if (!win.open(filename.c_str(), MEDIA_TYPE_RTSP))
				return -1;
		}
		else
		{
			if (!win.open(filename.c_str(), MEDIA_TYPE_FILE))
			return -1;
		}
	}

	win.play();

	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;
			else
			{
				TranslateMessage(&msg);	// Translate The Message
				DispatchMessage(&msg);	// Dispatch The Message
			}
		}
		else
		{
			Sleep(10); // do some...
		}
	}

	win.close();

	return 0;
}
