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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <queue>
#include <sys/socket.h>
#include <SDL/SDL_audio.h>
#include <boost/thread/condition.hpp>

#include <avplay.h>
#include "sdl_render.h"

#define EXPORT_API

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int sdl_init_audio(void* ctx, uint32_t channels,
	uint32_t bits_per_sample, uint32_t sample_rate, int format)
{
	ao_context *ao = (ao_context*)ctx;
	sdl_audio_render* sdl = NULL;
	ao->audio_dev = (void*)(sdl = new sdl_audio_render);
	return sdl->init_audio((void*)ao->audio_dev, channels, bits_per_sample, sample_rate, format) ? 0 : -1;
}

EXPORT_API int sdl_play_audio(void* ctx, uint8_t* data, uint32_t size)
{
	ao_context *ao = (ao_context*)ctx;
	sdl_audio_render* sdl = (sdl_audio_render*)ao->audio_dev;
	return sdl->play_audio(data, size);
}

EXPORT_API void sdl_audio_control(void* ctx, double l, double r)
{
	ao_context *ao = (ao_context*)ctx;
	sdl_audio_render* sdl = (sdl_audio_render*)ao->audio_dev;
	control_vol_t ctrl_vol = { l, r };
	sdl->audio_control(CONTROL_SET_VOLUME, &ctrl_vol);
}

EXPORT_API void sdl_mute_set(void* ctx, int s)
{
	ao_context *ao = (ao_context*)ctx;
	sdl_audio_render* sdl = (sdl_audio_render*)ao->audio_dev;
	control_vol_t ctrl_vol;
	ctrl_vol.mute = s;
	sdl->audio_control(CONTROL_MUTE_SET, &ctrl_vol);
}

EXPORT_API void sdl_destory_audio(void* ctx)
{
	ao_context *ao = (ao_context*)ctx;
	sdl_audio_render* sdl = (sdl_audio_render*)ao->audio_dev;
	if (sdl)
	{
		sdl->destory_audio();
		delete sdl;
		ao->audio_dev = NULL;
	}
}

#ifdef  __cplusplus
}
#endif

void sdl_audio_render::sdl_audio_callback(void* userdata, Uint8* stream, int len)
{
	sdl_audio_render *my = reinterpret_cast<sdl_audio_render *>(userdata);	
	my->audio_callback(stream,len);
}

void sdl_audio_render::audio_callback(Uint8* stream, int len)
{
	ssize_t readed=0;
	logger("sdl ask for %d length of data %p\n",len,stream);

	while(readed < len){

		ssize_t ret = 	read(adfd[1],stream,len - readed);
		if(ret >= 0){
			readed += ret;
		}else{
			logger("unexpeted read return\n");
			exit(1);
		}
	}
}

int sdl_audio_render::play_audio(uint8_t* data, uint32_t size)
{
	// push to stack
	ssize_t ret =  write(adfd[0],data,size);
	if(ret != size){
		logger("write audio error\b");
		exit(1);
	}
	return ret;
}

void sdl_audio_render::audio_control(int cmd, void* arg)
{

}

void sdl_audio_render::destory_audio()
{

}

bool sdl_audio_render::init_audio(void* ctx, int channels, int bits_per_sample, int sample_rate, int format)
{
	socketpair(AF_UNIX,SOCK_CLOEXEC|SOCK_STREAM,0,adfd);
	logger("socket created for audio %d %d\n",adfd[0],adfd[1]);
	SDL_AudioSpec fmt[1];// = new SDL_AudioSpec;

    /* Set 16-bit stereo audio at 22Khz */
	fmt->silence = 0;
    fmt->freq = sample_rate;
    fmt->format = AUDIO_S16;
    fmt->channels = channels;
    fmt->samples = 1024;    /* A good value for games */
    fmt->callback = sdl_audio_callback;
    fmt->userdata = this;

	bool ret = SDL_OpenAudio(fmt,0)>=0;
	SDL_PauseAudio(0);
	return ret;
}
