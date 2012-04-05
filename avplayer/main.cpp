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

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>	// for boost::filesystem3::extension

#include "avplayer.h"

int APIENTRY _tWinMain(HINSTANCE hInstance,
							  HINSTANCE hPrevInstance,
							  LPTSTR    lpCmdLine,
							  int       nCmdShow)
{
	setlocale(LC_ALL, "chs");

	MSG msg;
	int ret = NULL;
	WCHAR filename[MAX_PATH];

	if (_tcslen(lpCmdLine) == 0)
		return -1;

	_tcscpy(filename, lpCmdLine);

	avplayer win;

	if (win.create_window(_T("main")) == NULL)
		return -1;

	std::string ext = boost::filesystem3::extension(filename);
	if (ext == ".torrent")
	{
		if (!win.open(filename, MEDIA_TYPE_BT))
			return -1;
	}
	else
	{
		std::string str = boost::filesystem3::path(filename).string();
		boost::to_lower(str);
		boost::trim(str);
		std::string is_url = str.substr(0, 7);
		if (is_url == "http://")
		{
			if (!win.open(filename, MEDIA_TYPE_HTTP))
				return -1;
		}
		else if (is_url == "rtsp://")
		{
			if (!win.open(filename, MEDIA_TYPE_RTSP))
				return -1;
		}
		else
		{
			if (!win.open(filename, MEDIA_TYPE_FILE))
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
