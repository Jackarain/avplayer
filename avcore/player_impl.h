//
// player_impl.h
// ~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//
#ifndef __PLAYER_IMPL_H__
#define __PLAYER_IMPL_H__

#include "avplay.h"
#include "audio_out.h"
#include "video_out.h"
#include "source.h"

#pragma once

class player_impl
{
public:
	player_impl(void);
	~player_impl(void);

public:
	// 创建窗口, 也可以使用subclasswindow附加到一个指定的窗口.
	HWND create_window(LPCTSTR player_name);

	// 销毁窗口, 只能撤销是由create_window创建的窗口.
	BOOL destory_window();

	// 子类化一个存在的窗口, in_process参数表示窗口是否在同一进程中.
	BOOL subclasswindow(HWND hwnd, BOOL in_process);

	// 撤消子类化.
	BOOL unsubclasswindow(HWND hwnd);

public:
	// 打开一个媒体文件, movie是文件名, media_type可以是MEDIA_TYPE_FILE,
	// 也可以是MEDIA_TYPE_BT, 注意, 这个函数只打开文件, 但并不播放.
	// 重新打开文件前, 必须关闭之前的媒体文件, 否则可能产生内存泄漏!
	// 另外, 在播放前, avplayer必须拥有一个窗口.
	BOOL open(LPCTSTR movie, int media_type);

	// 播放索引为index的文件, index表示在播放列表中的
	// 位置计数, 从0开始计算, index主要用于播放多文件的bt
	// 文件, 单个文件播放可以使用直接默认为0而不需要填写
	// 参数.
	BOOL play(int index = 0);

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

	// seek到某个时间播放, 单位秒.
	void seek_to(double sec);

	// 设置声音音量大小.
	void volume(double vol);

	// 全屏切换.
	BOOL full_screen(BOOL fullscreen);

	// 返回当前播放时间.
	double curr_play_time();

	// 当前播放视频的时长, 单位秒.
	double duration();

	// 当前播放视频的高, 单位像素.
	int video_width();

	// 当前播放视频的宽, 单位像素.
	int video_height();

	// 返回当前播放列表, key对应的是打开的媒体文件名.
	// value是打开的媒体文件下的视频文件.
	// 比如说打开一个bt种子文件名为avtest.torrent, 在这
	// 个avtest.torrent种子里包括了2个视频文件, 假如为:
	// av1.mp4, av2.mp4, 那么这个列表应该为:
	// avtest.torrent->av1.mp4
	// avtest.torrent->av2.mp4
	std::map<std::string, std::string>& play_list();

	// 返回当前窗口句柄.
	HWND GetWnd();

private:
	// 窗口绘制相关.
	void fill_rectange(HWND hWnd, HDC hdc, RECT win_rect, RECT client_rect);
	void win_paint(HWND hwnd, HDC hdc);

private:
	// 窗口过程.
	static LRESULT CALLBACK static_win_wnd_proc(HWND, UINT, WPARAM, LPARAM);
	LRESULT win_wnd_proc(HWND, UINT, WPARAM, LPARAM);

	// 播放器相关的函数.
	void init_file_source(media_source *ms);
	void init_torrent_source(media_source *ms);
	void init_audio(audio_render *ao);
	void init_video(video_render *vo);

private:
	// window相关.
	HWND m_hwnd;
	HINSTANCE m_hinstance;
	HBRUSH m_brbackground;
	WNDPROC m_old_win_proc;

	// 播放器相关.
	avplay *m_avplay;
	audio_render *m_audio;
	video_render *m_video;
	media_source *m_source;

	// 媒体文件信息.
	std::map<std::string, std::string> m_media_list;
	int m_cur_index;

	// 视频宽高.
	int m_video_width;
	int m_video_height;

	// 全屏选项.
	BOOL m_full_screen;
	DWORD m_wnd_style;
};

#endif // __PLAYER_IMPL_H__
