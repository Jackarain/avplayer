//
// audio_out.h
// ~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __AUDIO_OUT_H__
#define __AUDIO_OUT_H__

#ifdef _MSC_VER
#	ifdef AUDIO_EXPORTS
#		define EXPORT_API __declspec(dllexport)
#	else
#		define EXPORT_API __declspec(dllimport)
#	endif
#else
#	define EXPORT_API
#endif

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int wave_init_audio(struct ao_context *ctx, uint32_t channels, 
	uint32_t bits_per_sample, uint32_t sample_rate, int format);
EXPORT_API int wave_play_audio(struct ao_context *ctx, uint8_t *data, uint32_t size);
EXPORT_API void wave_audio_control(struct ao_context *ctx, double l, double r);
EXPORT_API void wave_mute_set(struct ao_context *ctx, int s);
EXPORT_API void wave_destory_audio(struct ao_context *ctx);


EXPORT_API int dsound_init_audio(struct ao_context *ctx, uint32_t channels, 
	uint32_t bits_per_sample, uint32_t sample_rate, int format);
EXPORT_API int dsound_play_audio(struct ao_context *ctx, uint8_t *data, uint32_t size);
EXPORT_API void dsound_audio_control(struct ao_context *ctx, double l, double r);
EXPORT_API void dsound_mute_set(struct ao_context *ctx, int s);
EXPORT_API void dsound_destory_audio(struct ao_context *ctx);

EXPORT_API int sdl_init_audio(struct ao_context *ctx, uint32_t channels,
	uint32_t bits_per_sample, uint32_t sample_rate, int format);
EXPORT_API int sdl_play_audio(struct ao_context *ctx, uint8_t *data, uint32_t size);
EXPORT_API void sdl_audio_control(struct ao_context *ctx, double l, double r);
EXPORT_API void sdl_mute_set(struct ao_context *ctx, int s);
EXPORT_API void sdl_destory_audio(struct ao_context *ctx);


#ifdef  __cplusplus
}
#endif

#endif // __AUDIO_OUT_H__


