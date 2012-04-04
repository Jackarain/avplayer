#include "ins.h"
#include "dsound_render.h"
#include <math.h>


// namespace libavplayer {

int channel_mask[] = {
   SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
   SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT,
   SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT   | SPEAKER_LOW_FREQUENCY,
   SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT     | SPEAKER_LOW_FREQUENCY
};

dsound_render::dsound_render()
   : m_dsound(NULL)
   , m_dsbuffer_primary(NULL)
   , m_dsbuffer_second(NULL)
   , m_device_num(0)
   , m_channels(0)
   , m_bitrate(0)
{

}

dsound_render::~dsound_render()
{
   destory_audio();
}

char* dsound_render::dserr2str(int err)
{
   switch (err) {
      case DS_OK: return "DS_OK";
      case DS_NO_VIRTUALIZATION: return "DS_NO_VIRTUALIZATION";
      case DSERR_ALLOCATED: return "DS_NO_VIRTUALIZATION";
      case DSERR_CONTROLUNAVAIL: return "DSERR_CONTROLUNAVAIL";
      case DSERR_INVALIDPARAM: return "DSERR_INVALIDPARAM";
      case DSERR_INVALIDCALL: return "DSERR_INVALIDCALL";
      case DSERR_GENERIC: return "DSERR_GENERIC";
      case DSERR_PRIOLEVELNEEDED: return "DSERR_PRIOLEVELNEEDED";
      case DSERR_OUTOFMEMORY: return "DSERR_OUTOFMEMORY";
      case DSERR_BADFORMAT: return "DSERR_BADFORMAT";
      case DSERR_UNSUPPORTED: return "DSERR_UNSUPPORTED";
      case DSERR_NODRIVER: return "DSERR_NODRIVER";
      case DSERR_ALREADYINITIALIZED: return "DSERR_ALREADYINITIALIZED";
      case DSERR_NOAGGREGATION: return "DSERR_NOAGGREGATION";
      case DSERR_BUFFERLOST: return "DSERR_BUFFERLOST";
      case DSERR_OTHERAPPHASPRIO: return "DSERR_OTHERAPPHASPRIO";
      case DSERR_UNINITIALIZED: return "DSERR_UNINITIALIZED";
      case DSERR_NOINTERFACE: return "DSERR_NOINTERFACE";
      case DSERR_ACCESSDENIED: return "DSERR_ACCESSDENIED";
      default: return "unknown";
   }
}

int dsound_render::af_fmt2bits(int format)
{
   if (AF_FORMAT_IS_AC3(format)) return 16;
   //    return (format & AF_FORMAT_BITS_MASK)+8;
   return (((format & AF_FORMAT_BITS_MASK)>>3)+1) * 8;
   return -1;
}

bool dsound_render::init_audio(void* ctx, DWORD channels, DWORD bits_per_sample, DWORD sample_rate, int format)
{
   m_channels = channels;
   m_bitrate = sample_rate;
   m_format = format;

   HRESULT hr;
   
   // 创建direct sound.
   hr = DirectSoundCreate8(NULL, &m_dsound, NULL);
   if (FAILED(hr))
   {
      printf("Cannot create a DirectSound device.\n");
      return false;
   }

   // 设置dsound协作级别.
   hr = m_dsound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_EXCLUSIVE);
   if (FAILED(hr))
   {
      printf("Cannot set direct sound cooperative level.\n");
      destory_audio();
      return false;
   }

   printf("DirectSound initialized.\n");

   // 得到描述信息.
   DSCAPS dscaps = { 0 };
   dscaps.dwSize = sizeof(DSCAPS);
   hr = m_dsound->GetCaps(&dscaps);
   if (FAILED(hr))
   {
      printf("Cannot get device capabilities.\n");
      destory_audio();
      return false;
   }

   WAVEFORMATEXTENSIBLE wformat = { 0 };
   ZeroMemory(&wformat, sizeof(WAVEFORMATEXTENSIBLE));
   wformat.Format.cbSize          = (channels > 2) ? sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) : 0;
   wformat.Format.nChannels       = channels;
   wformat.Format.nSamplesPerSec  = sample_rate;
   if (AF_FORMAT_IS_AC3(format)) {
      wformat.Format.wFormatTag      = WAVE_FORMAT_DOLBY_AC3_SPDIF;
      wformat.Format.wBitsPerSample  = 16;
      wformat.Format.nBlockAlign     = 4;
   } else {
      wformat.Format.wFormatTag      = (channels > 2) ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
      wformat.Format.wBitsPerSample  = bits_per_sample;// af_fmt2bits(format);
      wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
   }

   DSBUFFERDESC dsbpridesc;
   DSBUFFERDESC dsbdesc;

   // fill in primary sound buffer descriptor
   memset(&dsbpridesc, 0, sizeof(DSBUFFERDESC));
   dsbpridesc.dwSize = sizeof(DSBUFFERDESC);
   dsbpridesc.dwFlags       = DSBCAPS_PRIMARYBUFFER;
   dsbpridesc.dwBufferBytes = 0;
   dsbpridesc.lpwfxFormat   = NULL;

   // fill in the secondary sound buffer (=stream buffer) descriptor
   memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
   dsbdesc.dwSize = sizeof(DSBUFFERDESC);
   dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /** Better position accuracy */
      | DSBCAPS_CTRLPAN
      | DSBCAPS_GLOBALFOCUS         /** Allows background playing */
      | DSBCAPS_CTRLVOLUME;         /** volume control enabled */

   if (channels > 2) {
      wformat.dwChannelMask = channel_mask[channels - 3];
      wformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
      wformat.Samples.wValidBitsPerSample = wformat.Format.wBitsPerSample;
      // Needed for 5.1 on emu101k - shit soundblaster
      dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
   }
   wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;

   dsbdesc.dwBufferBytes = sample_rate / 8 * 4;
   dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wformat;
   m_buffer_size = dsbdesc.dwBufferBytes;
   m_write_offset = 0;
   m_min_free_space = wformat.Format.nBlockAlign;
   // ao_data.outburst = wformat.Format.nBlockAlign * 512;

   // create primary buffer and set its format
   hr = m_dsound->CreateSoundBuffer(&dsbpridesc, &m_dsbuffer_primary, NULL);
   if (FAILED(hr))
   {
      printf("Cannot create primary buffer (%s)\n", dserr2str(hr));
      destory_audio();
      return false;
   }

   hr = m_dsbuffer_primary->SetFormat((WAVEFORMATEX*)&wformat);
   if (FAILED(hr))
   {
      printf("Cannot set primary buffer format (%s), "
         "using standard setting (bad quality)", dserr2str(hr));
      destory_audio();
      return false;
   }

   printf("primary buffer created.\n");

   // now create the stream buffer
   hr = m_dsound->CreateSoundBuffer(&dsbdesc, &m_dsbuffer_second, NULL);
   if (FAILED(hr))
   {
      printf("Cannot create secondary (stream)buffer (%s)\n", dserr2str(hr));
      destory_audio();
      return false;
   }
   printf("Secondary (stream)buffer created.\n");

   return true;
}

int dsound_render::play_audio(uint8_t* data, uint32_t size)
{
   HRESULT hr;
   uint8_t* dst_data1 = NULL;
   uint8_t* dst_data2 = NULL;
   uint32_t dst_bytes1 = 0;
   uint32_t dst_bytes2 = 0;

   DWORD play_offset;
   int space;

   if (!m_dsound)
      return 0;

   // make sure we have enough space to write data
   hr = m_dsbuffer_second->GetCurrentPosition(&play_offset, NULL);
   if (FAILED(hr))
      return 0;

   space = m_buffer_size - (m_write_offset - play_offset);
   if(space > m_buffer_size)
      space -= m_buffer_size; // write_offset < play_offset
   // 保证最小空余空间.
   if (space <= m_min_free_space)
      return 0;
   if(space <= size) 
      size = space - m_min_free_space;

   // 锁定direct sound 的循环 buffer.
   hr = m_dsbuffer_second->Lock(m_write_offset, size, 
      (LPVOID*)&dst_data1, (LPDWORD)&dst_bytes1, (LPVOID*)&dst_data2, (LPDWORD)&dst_bytes2, 0);
   if (DSERR_BUFFERLOST == hr)
   {
      m_dsbuffer_second->Restore();
      hr = m_dsbuffer_second->Lock(m_write_offset, size, 
         (LPVOID*)&dst_data1, (LPDWORD)&dst_bytes1, (LPVOID*)&dst_data2, (LPDWORD)&dst_bytes2, 0);
   }

   if (SUCCEEDED(hr))
   {
      if (m_channels == 6 && !AF_FORMAT_IS_AC3(m_format))
      {
         // reorder channels while writing to pointers.
         // it's this easy because buffer size and len are always
         // aligned to multiples of channels*bytespersample
         // there's probably some room for speed improvements here
         const int chantable[6] = {0, 1, 4, 5, 2, 3}; // reorder "matrix"
         int i, j;
         int numsamp, sampsize;

         sampsize = af_fmt2bits(m_format) >> 3; // bytes per sample
         numsamp = dst_bytes1 / (m_channels * sampsize);  // number of samples for each channel in this buffer

         for( i = 0; i < numsamp; i++ ) for( j = 0; j < m_channels; j++ ) {
            memcpy(dst_data1 + (i * m_channels * sampsize)
               + (chantable[j] * sampsize), data + (i * m_channels * sampsize) + (j * sampsize), sampsize);
         }

         if (dst_data2)
         {
            numsamp = dst_bytes2 / (m_channels * sampsize);
            for( i = 0; i < numsamp; i++ ) for( j = 0; j < m_channels; j++ ) {
               memcpy((void*)(dst_data2 + (i * m_channels * sampsize) 
                  + (chantable[j] * sampsize)), data + dst_bytes1 + (i * m_channels * sampsize) + (j * sampsize), sampsize);
            }
         }

         m_write_offset += dst_bytes1 + dst_bytes2;
         if(m_write_offset >= m_buffer_size)
            m_write_offset = dst_bytes2;
      } else {
         memcpy(dst_data1, data, dst_bytes1);
         if (dst_data2)
            memcpy((void*)dst_data2, data + dst_bytes1, dst_bytes2);
         m_write_offset += dst_bytes1 + dst_bytes2;
         if (m_write_offset >= m_buffer_size)
            m_write_offset = dst_bytes2;
      }

      // Release the data back to DirectSound.
      hr = m_dsbuffer_second->Unlock(dst_data1, dst_bytes1, (LPVOID)dst_data2, dst_bytes2);
      if (SUCCEEDED(hr))
      {
         // Success.
         DWORD status;
         m_dsbuffer_second->GetStatus(&status);
         if (!(status & DSBSTATUS_PLAYING))
         {
            m_dsbuffer_second->Play(0, 0, DSBPLAY_LOOPING);
         }

         return dst_bytes1 + dst_bytes2;
      }
   }

   return 0;
}

void dsound_render::audio_control(int cmd, void* arg)
{
   DWORD volume;
   switch (cmd) {
   case CONTROL_GET_VOLUME:
      {
         control_vol_t* vol = (control_vol_t*)arg;
         m_dsbuffer_second->GetVolume((LPLONG)&volume);
         vol->left = vol->right = pow(10.0, (float)(volume+10000) / 5000.0);
      }
      break;
   case CONTROL_SET_VOLUME:
      {
         control_vol_t* vol = (control_vol_t*)arg;
         volume = (DWORD)(log10(vol->right) * 5000.0) - 10000;
         m_dsbuffer_second->SetVolume(volume);
      }
      break;
   }
}

void dsound_render::destory_audio()
{
   if (m_dsbuffer_primary)
   {
      m_dsbuffer_primary->Release();
      m_dsbuffer_primary = NULL;
   }

   if (m_dsbuffer_second)
   {
      m_dsbuffer_second->Release();
      m_dsbuffer_second = NULL;
   }

   if (m_dsound)
   {
      m_dsound->Release();
      m_dsound = NULL;
   }
}

// } // namespace libavplayer

