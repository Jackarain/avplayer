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
#include <process.h>

// C 运行时头文件
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <string>
#include "avplayer.h"

// 获得文件名后辍.
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


void play_thread(void *param);

int main(int argc, char* argv[])
{
	// 判断播放参数是否足够.
	if (argc != 2)
	{
		printf("usage: avplayer.exe <video>\n");
		return -1;
	}

	// 设置语言环境为"中文".
	setlocale(LC_ALL, "chs");

	// 创建播放器.
	avplayer win;
	if (win.create_window("main") == NULL)
		return -1;

	// 判断打开的媒体类型, 根据媒体文件类型选择不同的方式打开.
	std::string filename = std::string(argv[1]);
	std::string ext = extension<std::string>(filename);
	if (ext == ".torrent")
	{
		if (!win.open(filename.c_str(), MEDIA_TYPE_BT))
			return -1;
	}
	else
	{
		std::string str = filename;
		std::string is_url = str.substr(0, 7);
		if (is_url == "http://")
		{
			if (!win.open(filename.c_str(), MEDIA_TYPE_HTTP))
				return -1;
		}
		else if (is_url == "rtsp://")
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

	// 在线程中运行或直接运行win.play();
	_beginthread(play_thread, NULL, (void*)&win);

	// 消息循环.
	MSG msg;
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

	// 关闭播放器.
	win.close();

	return 0;
}

void play_thread(void *param)
{
	avplayer *play = (avplayer*)param;
	play->play();
	// play->load_subtitle("d:\\media\\dfsschs.srt");
	// 一直等待直到播放完成.
	play->wait_for_completion();
	// 播放完成后, 处理各种事件.
}
