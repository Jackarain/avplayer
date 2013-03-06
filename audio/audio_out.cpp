#include "ins.h"
#include "globals.h"
#include "wave_render.h"
#include "dsound_render.h"
#include "audio_out.h"


#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int wave_init_audio(struct ao_context *ctx, uint32_t channels,
	uint32_t bits_per_sample, uint32_t sample_rate, int format)
{
	ao_context *ao = (ao_context*)ctx;
	wave_render* wave = NULL;
	ao->priv = (void*)(wave = new wave_render);
	return wave->init_audio((void*)ao->priv, channels, bits_per_sample, sample_rate, format) ? 0 : -1;
}

EXPORT_API int wave_play_audio(struct ao_context *ctx, uint8_t* data, uint32_t size)
{
	ao_context *ao = (ao_context*)ctx;
	wave_render* wave = (wave_render*)ao->priv;
	return wave->play_audio(data, size);
}

EXPORT_API void wave_audio_control(struct ao_context *ctx, double l, double r)
{
	ao_context *ao = (ao_context*)ctx;
	wave_render* wave = (wave_render*)ao->priv;
	control_vol_t ctrl_vol = { l, r };
	wave->audio_control(CONTROL_SET_VOLUME, &ctrl_vol);
}

EXPORT_API void wave_mute_set(struct ao_context *ctx, int s)
{
	ao_context *ao = (ao_context*)ctx;
	wave_render* wave = (wave_render*)ao->priv;
	control_vol_t ctrl_vol;
	ctrl_vol.mute = s;
	wave->audio_control(CONTROL_MUTE_SET, &ctrl_vol);
}

EXPORT_API void wave_destory_audio(struct ao_context *ctx)
{
	ao_context *ao = (ao_context*)ctx;
	wave_render* wave = (wave_render*)ao->priv;
	if (wave)
	{
		wave->destory_audio();
		delete wave;
		ao->priv = NULL;
	}
}


EXPORT_API int dsound_init_audio(struct ao_context *ctx, uint32_t channels,
	uint32_t bits_per_sample, uint32_t sample_rate, int format)
{
	ao_context *ao = (ao_context*)ctx;
	dsound_render* dsound = NULL;
	ao->priv = (void*)(dsound = new dsound_render);
	return dsound->init_audio((void*)dsound, channels, bits_per_sample, sample_rate, format) ? 0 : -1;
}

EXPORT_API int dsound_play_audio(struct ao_context *ctx, uint8_t* data, uint32_t size)
{
	ao_context *ao = (ao_context*)ctx;
	dsound_render* dsound = (dsound_render*)ao->priv;
	return dsound->play_audio(data, size);
}

EXPORT_API void dsound_audio_control(struct ao_context *ctx, double l, double r)
{
	ao_context *ao = (ao_context*)ctx;
	dsound_render* dsound = (dsound_render*)ao->priv;
	control_vol_t ctrl_vol = { l, r };
	dsound->audio_control(CONTROL_SET_VOLUME, &ctrl_vol);
}

EXPORT_API void dsound_mute_set(struct ao_context *ctx, int s)
{
	ao_context *ao = (ao_context*)ctx;
	dsound_render* dsound = (dsound_render*)ao->priv;
	control_vol_t ctrl_vol;
	ctrl_vol.mute = s;
	dsound->audio_control(CONTROL_MUTE_SET, &ctrl_vol);
}

EXPORT_API void dsound_destory_audio(struct ao_context *ctx)
{
	ao_context *ao = (ao_context*)ctx;
	dsound_render* dsound = (dsound_render*)ao->priv;
	if (dsound)
	{
		dsound->destory_audio();
		delete dsound;
		ao->priv = NULL;
	}
}

#ifdef  __cplusplus
}
#endif

