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
#define UINT64_C(c)     c ## ULL

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>


#include <stdio.h>
#include <cstdio>
#include <stdint.h>
#include <SDL/SDL.h>
#include <boost/filesystem.hpp>
namespace fs=boost::filesystem;

#include "player.h"
#include "source/source.h"
#include "video/video_out.h"
#include "audio/audio_out.h"

void player::init_file_source(source_context* sc)
{
	sc->init_source = file_init_source;
	sc->read_data = file_read_data;
	sc->close = file_close;
	sc->destory = file_destory;
	sc->offset = 0;
}

int player::open(const char* movie, int media_type)
{	// 如果未关闭原来的媒体, 则先关闭.
	if (m_avplay || m_source);
		//close();

	// 未创建窗口, 无法播放, 返回失败.
	if (!HasWindow())
		return -1;

	fs::path filename(movie);

	uint64_t file_lentgh = 0;
	if (media_type == MEDIA_TYPE_FILE || media_type == MEDIA_TYPE_BT)
	{
		file_lentgh =fs::file_size(filename);
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
			//size_t len = strlen(filename);
			m_source = alloc_media_source(MEDIA_TYPE_FILE, filename.string().c_str(), filename.string().length()+1, file_lentgh);
			
			if (!m_source)
			{
				::logger("allocate media source failed, type is file.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename.string(), filename.string()));

			// 初始化文件媒体源.
			init_file_source(m_source);
		}
#if 0
		if (media_type == MEDIA_TYPE_BT)
		{
			// 先读取bt种子数据, 然后作为附加数据保存到媒体源.
			FILE *fp = std::fopen(filename.c_str(), "r+b");
			if (!fp)
			{
				::logger("open torrent file \'%s\' failed!\n", filename.c_str());
				break;
			}
			char *torrent_data = (char*)malloc(file_lentgh);
			int readbytes = fread(torrent_data, 1, file_lentgh, fp);
			if (readbytes != file_lentgh)
			{
				::logger("read torrent file \'%s\' failed!\n", filename.c_str());
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

		if (media_type == MEDIA_TYPE_HTTP)
		{
			m_source = alloc_media_source(MEDIA_TYPE_HTTP, filename.c_str(), filename.string().length()+1, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is http.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename.string(), filename.string()));
		}

		if (media_type == MEDIA_TYPE_RTSP)
		{
			m_source = alloc_media_source(MEDIA_TYPE_RTSP, filename.c_str(), filename.string().length()+1, 0);
			if (!m_source)
			{
				::logger("allocate media source failed, type is rtsp.\n");
				break;
			}

			// 插入到媒体列表.
			m_media_list.insert(std::make_pair(filename.string(), filename.string()));
		}
#endif
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
				m_media_list.insert(std::make_pair(filename.string(), name));
			}
		}

		// 分配音频和视频的渲染器.
		m_audio = alloc_audio_render();
		if (!m_audio)
		{
			::logger("allocate audio render failed!\n");
			break;
		}

		m_video = alloc_video_render(0);
		if (!m_video)
		{
			::logger("allocate video render failed!\n");
			break;
		}

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

		return 0;

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

	return -1;
}

void player::init_video(vo_context* vo)
{
	//创建第一个窗口
	vo->user_data = SDL_SetVideoMode(800, 600, 32, SDL_RESIZABLE);

	vo->init_video = sdl_init_video;
	//m_draw_frame = sdl_render_one_frame;
	vo->re_size = sdl_re_size;
	vo->aspect_ratio = sdl_aspect_ratio;
	vo->use_overlay = sdl_use_overlay;
	vo->destory_video = sdl_destory_render;
	vo->render_one_frame = sdl_render_one_frame;//()  &draw_frame;

	::logger("init video render to sdl.\n");
}

void player::init_audio(ao_context* ao)
{
	ao->init_audio = sdl_init_audio;
	ao->play_audio = sdl_play_audio;
	ao->audio_control = sdl_audio_control;
	ao->mute_set = sdl_mute_set;
	ao->destory_audio = sdl_destory_audio;
}

bool player::play(double fact, int index)
{
	// 重复播放, 返回错误.
	if (m_cur_index == index)
		return false;

	// 如果是文件数据, 则直接播放.
	if (::av_start(m_avplay, fact, index) != 0)
		return false;

	m_cur_index = index;

	return true;
}

bool player::wait_for_completion()
{
	if (m_avplay)
	{
		::wait_for_completion(m_avplay);
		::logger("play completed.\n");
		return true;
	}
	return false;
}

void player::fwd()
{	
	av_seek(m_avplay,curr_play_time(m_avplay) / duration(m_avplay) + 0.05);
}
