//
// audio_render.h
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __AUDIO_RENDER_H__
#define __AUDIO_RENDER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif


// namespace libavplayer {

#if !defined(WAVE_FORMAT_PCM)
#define WAVE_FORMAT_PCM     1
#endif

// Signed/unsigned
#define AF_FORMAT_SI		(0<<1) // Signed
#define AF_FORMAT_US		(1<<1) // Unsigned
#define AF_FORMAT_SIGN_MASK	(1<<1)

// Fixed or floating point
#define AF_FORMAT_I		(0<<2) // Int
#define AF_FORMAT_F		(1<<2) // Foating point
#define AF_FORMAT_POINT_MASK	(1<<2)

// Bits used
#define AF_FORMAT_8BIT		   (0<<3)
#define AF_FORMAT_16BIT		   (1<<3)
#define AF_FORMAT_24BIT		   (2<<3)
#define AF_FORMAT_32BIT		   (3<<3)
#define AF_FORMAT_40BIT		   (4<<3)
#define AF_FORMAT_48BIT		   (5<<3)
#define AF_FORMAT_BITS_MASK	(7<<3)

// Special flags refering to non pcm data
#define AF_FORMAT_MU_LAW	(1<<6)
#define AF_FORMAT_A_LAW		(2<<6)
#define AF_FORMAT_MPEG2		(3<<6) // MPEG(2) audio
#define AF_FORMAT_AC3		(4<<6) // Dolby Digital AC3
#define AF_FORMAT_IMA_ADPCM	(5<<6)
#define AF_FORMAT_SPECIAL_MASK	(7<<6)

// use the definitions from the win32 api headers when they define these
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE      0xFFFE

static const  GUID KSDATAFORMAT_SUBTYPE_PCM = {
   0x1,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}
};

#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

#define DSSPEAKER_HEADPHONE         0x00000001
#define DSSPEAKER_MONO              0x00000002
#define DSSPEAKER_QUAD              0x00000003
#define DSSPEAKER_STEREO            0x00000004
#define DSSPEAKER_SURROUND          0x00000005
#define DSSPEAKER_5POINT1           0x00000006

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
   WAVEFORMATEX    Format;
   union {
      WORD wValidBitsPerSample;       /* bits of precision  */
      WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
      WORD wReserved;                 /* If neither applies, set to zero. */
   } Samples;
   DWORD           dwChannelMask;      /* which channels are */
   /* present in stream  */
   GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

// Special flags refering to non pcm data
#define AF_FORMAT_MU_LAW	(1<<6)
#define AF_FORMAT_A_LAW		(2<<6)
#define AF_FORMAT_MPEG2		(3<<6) // MPEG(2) audio
#define AF_FORMAT_AC3		(4<<6) // Dolby Digital AC3
#define AF_FORMAT_IMA_ADPCM	(5<<6)
#define AF_FORMAT_SPECIAL_MASK	(7<<6)

#define AF_FORMAT_IS_AC3(fmt) (((fmt) & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_AC3)

extern int channel_mask[];

typedef struct control_vol_s {
   float left;
   float right;
} control_vol_t;

// 播放控制定义
#define CONTROL_GET_VOLUME 1
#define CONTROL_SET_VOLUME 2

class audio_render
{
public:
   audio_render() {}
   virtual ~audio_render() {}

public:
   // 初始化音频输出.
   virtual bool init_audio(void* ctx, DWORD channels, DWORD bits_per_sample, DWORD sample_rate, int format) = 0;

   // 播放音频数据.
   virtual int play_audio(uint8_t* data, uint32_t size) = 0;

   // 音频播放控制, cmd为CONTROL_开始的宏定义.
   virtual void audio_control(int cmd, void* arg) = 0;

   // 销毁音频输出组件.
   virtual void destory_audio() = 0;
};

// } // namespace libavplayer

#endif // __AUDIO_RENDER_H__
