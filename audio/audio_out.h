//
// audio_out.h
// ~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __AUDIO_OUT_H__
#define __AUDIO_OUT_H__

#ifdef AUDIO_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int wave_init_audio(void* ctx, uint32_t channels, 
	uint32_t bits_per_sample, uint32_t sample_rate, int format);
EXPORT_API int wave_play_audio(void* ctx, uint8_t* data, uint32_t size);
EXPORT_API void wave_audio_control(void* ctx, double vol);
EXPORT_API void wave_destory_audio(void* ctx);


EXPORT_API int dsound_init_audio(void* ctx, void* user_data, uint32_t channels, 
	uint32_t bits_per_sample, uint32_t sample_rate, int format);
EXPORT_API int dsound_play_audio(void* ctx, uint8_t* data, uint32_t size);
EXPORT_API void dsound_audio_control(void* ctx, double vol);
EXPORT_API void dsound_destory_audio(void* ctx);

#ifdef  __cplusplus
}
#endif

#endif // __AUDIO_OUT_H__


