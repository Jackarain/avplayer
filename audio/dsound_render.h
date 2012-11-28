//
// dsound_render.h
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __DSOUND_RENDER_H__
#define __DSOUND_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "audio_render.h"

class dsound_render
   : public audio_render
{
public:
   dsound_render();
   virtual ~dsound_render();

public:
   // 初始化音频输出.
   virtual bool init_audio(void* ctx, int channels, int bits_per_sample, int sample_rate, int format);

   // 播放音频数据.
   virtual int play_audio(uint8_t* data, uint32_t size);

   // 音频播放控制, cmd为CONTROL_开始的宏定义.
   virtual void audio_control(int cmd, void* arg);

   // 销毁音频输出组件.
   virtual void destory_audio();

private:
   int af_fmt2bits(int format);
   char* dserr2str(int err);

private:
   // direct sound object.
   LPDIRECTSOUND8 m_dsound;

   // primary direct sound buffer.
   LPDIRECTSOUNDBUFFER m_dsbuffer_primary;

   // secondary direct sound buffer (stream buffer)
   LPDIRECTSOUNDBUFFER m_dsbuffer_second;

   // wanted device number.
   int m_device_num;

   // size in bytes of the direct sound buffer.
   int m_buffer_size;

   // offset of the write cursor in the direct sound buffer.
   int m_write_offset;

   // if the free space is below this value get_space() will return 0
   int m_min_free_space;

   // 声道数.
   int m_channels;

   // 比特率.
   int m_bitrate;

   // 音频格式.
   int m_format;
};

#endif // __DSOUND_RENDER_H__
