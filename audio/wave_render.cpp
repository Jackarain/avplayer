#include "ins.h"
#include "wave_render.h"

#define SAMPLESIZE   1024
#define BUFFER_SIZE  4096
#define BUFFER_COUNT 8

wave_render::wave_render()
   : m_hwaveout(NULL)
   , m_buffersize(0)
   , m_wave_blocks(NULL)
   , m_buf_write(0)
   , m_buf_read(0)
{

}

wave_render::~wave_render()
{
   destory_audio();
}

void __stdcall wave_render::waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance,
   DWORD dwParam1, DWORD dwParam2)
{
   wave_render* this_ptr = (wave_render*)dwInstance;

   if(uMsg != WOM_DONE)
      return;
   this_ptr->m_buf_read = (this_ptr->m_buf_read + 1) % BUFFER_COUNT;
}

bool wave_render::init_audio(void* ctx, DWORD channels, DWORD bits_per_sample, DWORD sample_rate, int format)
{
   WAVEFORMATEXTENSIBLE wformat = { 0 };
   MMRESULT result;

   unsigned char* buffer;
   int i;

   // FIXME multichannel mode is buggy.
   if(channels > 2)
      channels = 2;

   // fill waveformatex.
   ZeroMemory(&wformat, sizeof(WAVEFORMATEXTENSIBLE));
   wformat.Format.cbSize          = (channels > 2) ? sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) : 0;
   wformat.Format.nChannels       = channels;
   wformat.Format.nSamplesPerSec  = sample_rate;

   if(AF_FORMAT_IS_AC3(format))
   {
      wformat.Format.wFormatTag      = WAVE_FORMAT_DOLBY_AC3_SPDIF;
      wformat.Format.wBitsPerSample  = 16;
      wformat.Format.nBlockAlign     = 4;
   }
   else
   {
      wformat.Format.wFormatTag      = (channels > 2) ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
      wformat.Format.wBitsPerSample  = bits_per_sample; // af_fmt2bits(format);
      wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
   }

   wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;

   // open sound device
   // WAVE_MAPPER always points to the default wave device on the system
   result = waveOutOpen(&m_hwaveout, WAVE_MAPPER, (WAVEFORMATEX*)&wformat, 
      (DWORD_PTR)wave_render::waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
   if(result == WAVERR_BADFORMAT)
   {
      printf("format not supported switching to default.\n");
      wformat.Format.wBitsPerSample  = 16;
      wformat.Format.wFormatTag      = WAVE_FORMAT_PCM;
      wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
      wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;
      m_buffersize = (wformat.Format.wBitsPerSample >> 3) * wformat.Format.nChannels * SAMPLESIZE;
      result = waveOutOpen(&m_hwaveout, WAVE_MAPPER, (WAVEFORMATEX*)&wformat, 
         (DWORD_PTR)wave_render::waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
   }
   if(result != MMSYSERR_NOERROR)
   {
      printf("unable to open wave mapper device (result=%i)\n", result);
      return false;
   }

   // allocate buffer memory as one big block.
   buffer = (unsigned char*)calloc(BUFFER_COUNT, BUFFER_SIZE + sizeof(WAVEHDR));
   // and setup pointers to each buffer.
   m_wave_blocks = (WAVEHDR*)buffer;
   buffer += sizeof(WAVEHDR) * BUFFER_COUNT;
   for(i = 0; i < BUFFER_COUNT; i++) {
      m_wave_blocks[i].lpData = (LPSTR)buffer;
      buffer += BUFFER_SIZE;
   }
   m_buf_write = 0;
   m_buf_read  = 0;

   return true;
}

int wave_render::play_audio(uint8_t* data, uint32_t size)
{
   WAVEHDR* current;
   int len2 = 0;
   int x = 0;

   while(size > 0) {
      int buf_next = (m_buf_write + 1) % BUFFER_COUNT;
      current = &m_wave_blocks[m_buf_write];
      if(buf_next == m_buf_read)
         break;
      // unprepare the header if it is prepared.
      if(current->dwFlags & WHDR_PREPARED)
         waveOutUnprepareHeader(m_hwaveout, current, sizeof(WAVEHDR));

      x = BUFFER_SIZE;
      if(x > size)
         x = size;

      memcpy(current->lpData, data + len2, x);
      len2 += x; 
      size -= x;
      // prepare header and write data to device.
      current->dwBufferLength = x;
      waveOutPrepareHeader(m_hwaveout, current, sizeof(WAVEHDR));
      waveOutWrite(m_hwaveout, current, sizeof(WAVEHDR));

      m_buf_write = buf_next;
   }
   return len2;
}

void wave_render::audio_control(int cmd, void* arg)
{
   DWORD volume;
   switch (cmd)
   {
   case CONTROL_GET_VOLUME:
      {
         control_vol_t* vol = (control_vol_t*)arg;
         waveOutGetVolume(m_hwaveout, &volume);
         vol->left = (float)(LOWORD(volume) / 655.35);
         vol->right = (float)(HIWORD(volume) / 655.35);
         printf("volume left:%f volume right:%f\n", vol->left, vol->right);
      }
      break;
   case CONTROL_SET_VOLUME:
      {
         control_vol_t* vol = (control_vol_t*)arg;
         volume = MAKELONG(vol->left * 655.35, vol->right * 655.35);
         waveOutSetVolume(m_hwaveout, volume);
      }
      break;
   }
}

void wave_render::destory_audio()
{
   if (m_hwaveout)
   {
      waveOutReset(m_hwaveout);
      while (waveOutClose(m_hwaveout) == WAVERR_STILLPLAYING) 
         Sleep(0);
      m_hwaveout = NULL;
   }

   if (m_wave_blocks)
   {
      free(m_wave_blocks);
      m_wave_blocks = NULL;
   }
}
