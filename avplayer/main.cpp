// 如果必须将位于下面指定平台之前的平台作为目标，请修改下列定义。
// 有关不同平台对应值的最新信息，请参考 MSDN。
#ifndef WINVER				// 允许使用特定于 Windows XP 或更高版本的功能。
#define WINVER 0x0501		// 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

#ifndef _WIN32_WINNT		// 允许使用特定于 Windows XP 或更高版本的功能。
#define _WIN32_WINNT 0x0501	// 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif						

#ifndef _WIN32_WINDOWS		// 允许使用特定于 Windows 98 或更高版本的功能。
#define _WIN32_WINDOWS 0x0410 // 将此值更改为适当的值，以指定将 Windows Me 或更高版本作为目标。
#endif

#ifndef _WIN32_IE			// 允许使用特定于 IE 6.0 或更高版本的功能。
#define _WIN32_IE 0x0600	// 将此值更改为相应的值，以适用于 IE 的其他版本。
#endif

#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料

// Windows 头文件:
#include <windows.h>
#include <ShellAPI.h>

// C 运行时头文件
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

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

	// 播放器对象.
	avplayer win;

	// 创建播放器窗口.
	if (win.create_window(_T("main")) == NULL)
		return -1;

#if 0
	// 测试进程外播放窗口播放, 也可以使用外部创建的窗口.
	HWND hWnd = (HWND)0x0002133C;
	if (!win.subclasswindow(hWnd, FALSE))
		return -1;
#endif

	// 根据文件扩展名判断是否为bt种子文件, 如果是bt种子文件
	// 则调用bt方式下载并播放.
	std::string ext = boost::filesystem3::extension(filename);
	if (ext == ".torrent")
	{
		if (!win.open(filename, MEDIA_TYPE_BT))
			return -1;
	}
	else
	{
		if (!win.open(filename, MEDIA_TYPE_FILE))
			return -1;
	}

	// 打开成功, 开始播放.
	win.play();

	// 消息循环.
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
