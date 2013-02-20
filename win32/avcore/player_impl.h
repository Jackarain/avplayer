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
#include "globals.h"
#include "avplayer.h"

#pragma once


// 日志类.
class avplay_logger
{
public:
	avplay_logger() { ::logger_to_file("avplayer.log"); }
	~avplay_logger() { ::close_logger_file(); }
};

// 字幕插件统一接口类.
class subtitle_plugin
{
public:
	// 字幕插件初始化.
	// vsfilter表示vsfilter.dll的文件名.
	subtitle_plugin() {}
	virtual ~subtitle_plugin() {}

public:
	// 初始化字幕插件.
	virtual bool subtitle_init() = 0;
	// 反初始化字幕插件.
	virtual void subtitle_uninit() = 0;
	// 打开字幕文件, 并指定视频宽高.
	virtual bool subtitle_open(char* fileName, int w, int h) = 0;
	// 渲染一帧yuv视频数据. 传入当前时间戳和yuv视频数据及大小(只支持YUV420).
	virtual void subtitle_do(void* yuv_data, int64_t cur_time, long size) = 0;
	// 判断插件是否加载.
	virtual bool subtitle_is_load() { return false; }
};

class player_impl
{
public:
	player_impl(void);
	~player_impl(void);

public:
	// 创建窗口, 也可以使用subclasswindow附加到一个指定的窗口.
	HWND create_window(const char *player_name);

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
	BOOL open(const char *movie, int media_type, int render_type);

	// 开始播放视频.
	// fact 表示播放视频的起始位置, 按视频百分比计算, 默认从文件头开始播放.
	// index 播放索引为index的文件, index表示在播放列表中的位置计数, 从0开始计
	// 算, index主要用于播放多文件的bt文件, 单个文件播放可以使用直接默认为0而不
	// 需要填写参数.
	BOOL play(double fact = 0.0f, int index = 0);

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
	void volume(double l, double r);

	// 静音切换.
	void toggle_mute();

	// 静音设置.
	void mute_set(bool s);

	// 全屏切换.
	// 注意: 该函数不支持非顶层窗口的全屏操作!
	BOOL full_screen(BOOL fullscreen);

	// 当前下载速率, 单位kB/s.
	int download_rate();

	// 限制下载速率.
	void set_download_rate(int k);

	// 返回当前播放时间.
	double curr_play_time();

	// 当前播放视频的时长, 单位秒.
	double duration();

	// 当前播放视频的高, 单位像素.
	int video_width();

	// 当前播放视频的宽, 单位像素.
	int video_height();

	// 当前缓冲进度, 单位百分比.
	double buffering();

	// 返回当前播放列表, key对应的是打开的媒体文件名.
	// value是打开的媒体文件下的视频文件.
	// 比如说打开一个bt种子文件名为avtest.torrent, 在这
	// 个avtest.torrent种子里包括了2个视频文件, 假如为:
	// av1.mp4, av2.mp4, 那么这个列表应该为:
	// avtest.torrent->av1.mp4
	// avtest.torrent->av2.mp4
	std::map<std::string, std::string>& play_list();

	// 返回当前窗口句柄.
	HWND get_window_handle();

    // 解析yk相关视频文件(暂不支持视频组)
    void parse_yk_videos(const std::string& vid);

private:
	// 窗口绘制相关.
	void fill_rectange(HWND hWnd, HDC hdc, RECT win_rect, RECT client_rect);
	void win_paint(HWND hwnd, HDC hdc);

private:
	// 窗口过程.
	static LRESULT CALLBACK static_win_wnd_proc(HWND, UINT, WPARAM, LPARAM);
	LRESULT win_wnd_proc(HWND, UINT, WPARAM, LPARAM);

	// 播放器相关的函数.
	void init_file_source(source_context *sc);
	void init_torrent_source(source_context *sc);
	void init_yk_source(source_context *sc);
	void init_audio(ao_context *ao);
	void init_video(vo_context *vo, int render_type = RENDER_D3D);

	// 实时处理视频渲染的视频数据, 在这里完成比较加字幕, 加水印等操作.
	static int draw_frame(void *ctx, AVFrame* data, int pix_fmt, double pts);

private:
	avplay_logger m_log;
	// window相关.
	HWND m_hwnd;
	HINSTANCE m_hinstance;
	HBRUSH m_brbackground;
	WNDPROC m_old_win_proc;

	// 播放器主要由下面几个部件组成.
	// avplay 是整合source_context和ao_context,vo_context的大框架.
	// 由 source_context实现数据读入视频数据.
	// 由 avplay负责分离音视并解码.
	// 最后由ao_context和vo_context分别负责输出音频和视频.
	avplay *m_avplay;
	source_context *m_source;
	ao_context *m_audio;
	vo_context *m_video;

	int (*m_draw_frame)(void *ctx, AVFrame* data, int pix_fmt, double pts);

	// 媒体文件信息.
	std::map<std::string, std::string> m_media_list;
	int m_cur_index;

	// 字幕插件.
	CRITICAL_SECTION m_plugin_cs;
	subtitle_plugin *m_plugin;
	std::string m_subtitle;
	bool m_change_subtitle;

	// 视频宽高.
	int m_video_width;
	int m_video_height;

	// 全屏选项.
	BOOL m_full_screen;
	DWORD m_wnd_style;

	// 声音选项.
	bool m_mute;
};

#endif // __PLAYER_IMPL_H__
