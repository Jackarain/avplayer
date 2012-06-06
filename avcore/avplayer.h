//
// avplayer.h
// ~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __AVPLAYER_H__
#define __AVPLAYER_H__

#ifdef API_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif

#pragma once

// 打开媒体类型.
#define MEDIA_TYPE_FILE	0
#define MEDIA_TYPE_BT	1
#define MEDIA_TYPE_HTTP 2
#define MEDIA_TYPE_RTSP 3

// 渲染模式.
#define RENDER_DDRAW	0
#define RENDER_D3D	1
#define RENDER_OGL	2

class player_impl;
// avplayer封装类.
class EXPORT_API avplayer
{
public:
	avplayer(void);
	~avplayer(void);

public:
	// 创建窗口, 也可以使用subclasswindow附加到一个指定的窗口.
	HWND create_window(const char *player_name);

	// 销毁窗口, 只能撤销是由create_window创建的窗口.
	BOOL destory_window();

	// 子类化一个存在的窗口, in_process参数表示窗口是否在同一进程中.
	BOOL subclasswindow(HWND hwnd, BOOL in_process = TRUE);

	// 撤消子类化.
	BOOL unsubclasswindow(HWND hwnd);

public:
	// 打开一个媒体文件
	// movie 文件名.
	// media_type 表示打开的媒体类型.
	// render_type 表示播放渲染模式, 默认是ddraw渲染.
	// 注意, 这个函数只打开文件, 但并不播放, 重新打开文件前, 必
	// 须关闭之前的媒体文件, 否则可能产生内存泄漏! 另外, 在播放
	// 前, avplayer必须拥有一个窗口.
	BOOL open(const char *movie, int media_type, int render_type = RENDER_DDRAW);

	// 播放索引为index的文件, index表示在播放列表中的
	// 位置计数, 从0开始计算, index主要用于播放多文件的bt
	// 文件, 单个文件播放可以使用直接默认为0而不需要填写
	// 参数.
	BOOL play(int index = 0);

	// 加载字幕.
	BOOL load_subtitle(const char *subtitle);

	// 暂停播放.
	BOOL pause();

	// 继续播放.
	BOOL resume();

	// 停止播放.
	BOOL stop();

	// 等待播放直到完成.
	BOOL wait_for_completion();

	// 关闭媒体, 如果打开的是一个bt文件, 那么
	// 在这个bt文件中的所有视频文件将被关闭.
	BOOL close();

	// seek到某个时间播放, 按视频时长的百分比.
	void seek_to(double fact);

	// 设置声音音量大小.
	void volume(double vol);

	// 全屏切换.
	// 注意: 不支持非顶层窗口全屏操作!
	BOOL full_screen(BOOL fullscreen);

	// 返回当前播放时间.
	double curr_play_time();

	// 当前播放视频的时长, 单位秒.
	double duration();

	// 当前播放视频的高, 单位像素.
	int video_width();

	// 当前播放视频的宽, 单位像素.
	int video_height();

	// 返回当前播放列表中的媒体文件数.
	int media_count();

	// 返回播放列表.
	// 参数list在模块内分配内存, 通过size参数传出列表大小.
	// @ example begin
	//   char **list;
	//   int size = 0;
	//   play->media_list(&list, &size);
	//   ...
	//   play->free_media_list(list, size);
	// @ example end
	int media_list(char ***list, int* size);

	// 释放播放列表的分配的内存资源.
	void free_media_list(char **list, int size);

	// 返回当前窗口句柄.
	HWND get_wnd();

private:
	player_impl *m_impl;
};

#endif // __AVPLAYER_H__

