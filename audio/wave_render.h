//
// wave_render.h
// ~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __WAVE_RENDER_H__
#define __WAVE_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "audio_render.h"

class wave_render
   : public audio_render
{
public:
   wave_render();
   virtual ~wave_render();

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
   static void __stdcall waveOutProc(HWAVEOUT hWaveOut,UINT uMsg,DWORD dwInstance,
      DWORD dwParam1,DWORD dwParam2);

private:
   // waveOut句柄.
   HWAVEOUT m_hwaveout;

   // 缓冲大小.
   int m_buffersize;

   // pointer to our ringbuffer memory.
   WAVEHDR* m_wave_blocks;

   // 写入位置.
   unsigned int m_buf_write;

   // 播放位置.
   volatile int m_buf_read;

   // 保存当前音量大小.
   control_vol_t m_volume;
};

#endif // __WAVE_RENDER_H__
