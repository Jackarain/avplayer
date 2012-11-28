/*
    Copyright (C) 2012  microcai <microcai@fedoraproject.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef PLAYER_H
#define PLAYER_H
#include <map>
#include <string>
#include <avplay.h>

class player
{
	bool HasWindow(){return true;};

	// 播放器相关的函数.
	void init_file_source(source_context *sc);
	void init_torrent_source(source_context *sc);
	void init_audio(ao_context *ao);
	void init_video(vo_context *vo);

	int (*m_draw_frame)(void *ctx, AVFrame* data, int pix_fmt, double pts);

	// 实时处理视频渲染的视频数据, 在这里完成比较加字幕, 加水印等操作.
	static int draw_frame(void *ctx, AVFrame* data, int pix_fmt, double pts);

public:
	int open(const char *movie, int media_type, int render_type);
    void close();
private:
    avplay* m_avplay;
    source_context* m_source;
	// 媒体文件信息.
	std::map<std::string, std::string> m_media_list;
    ao_context* m_audio;
    vo_context* m_video;
    int m_video_width;
    int m_video_height;
};

#endif // PLAYER_H
