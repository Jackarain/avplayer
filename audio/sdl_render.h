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


#ifndef SDL_RENDER_H
#define SDL_RENDER_H
#include <boost/thread/mutex.hpp>
#include "audio_render.h"

class sdl_audio_render:public audio_render
{
public:
	   // 初始化音频输出.
   virtual bool init_audio(void* ctx, int channels, int bits_per_sample, int sample_rate, int format);

   // 播放音频数据.
   virtual int play_audio(uint8_t* data, uint32_t size);

   // 音频播放控制, cmd为CONTROL_开始的宏定义.
   virtual void audio_control(int cmd, void* arg);

   // 销毁音频输出组件.
   virtual void destory_audio();

   static void sdl_audio_callback(void *userdata, Uint8 *stream, int len);
   friend void sdl_audio_callback(void *userdata, Uint8 *stream, int len);
private:
	void audio_callback(Uint8 *stream, int len);
private:
	int adfd[2];
// 		boost::mutex adqueue_mutex;
// 		boost::condition_variable	adqueue_mutex_cond;
// 		std::queue<std::pair<uint8_t*,uint32_t> > adqueue;
};

#endif // SDL_RENDER_H
