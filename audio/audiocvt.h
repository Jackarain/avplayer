//
// audiocvt.h
// ~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __AUDIOCVT_H__
#define __AUDIOCVT_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

namespace libavplayer {

#define LIL_ENDIAN	1234
#define BIG_ENDIAN	4321

#ifndef BYTEORDER
#if defined(__hppa__) || \
   defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
   (defined(__MIPS__) && defined(__MISPEB__)) || \
   defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
   defined(__sparc__)
#define BYTEORDER	BIG_ENDIAN
#else
#define BYTEORDER	LIL_ENDIAN
#endif
#endif /* !BYTEORDER */

#define AUDIO_U8	0x0008	   /**< Unsigned 8-bit samples */
#define AUDIO_S8	0x8008	   /**< Signed 8-bit samples */
#define AUDIO_U16LSB	0x0010	/**< Unsigned 16-bit samples */
#define AUDIO_S16LSB	0x8010	/**< Signed 16-bit samples */
#define AUDIO_U16MSB	0x1010	/**< As above, but big-endian byte order */
#define AUDIO_S16MSB	0x9010	/**< As above, but big-endian byte order */
#define AUDIO_U16	AUDIO_U16LSB
#define AUDIO_S16	AUDIO_S16LSB

#if BYTEORDER == LIL_ENDIAN
#define AUDIO_U16SYS	AUDIO_U16LSB
#define AUDIO_S16SYS	AUDIO_S16LSB
#else
#define AUDIO_U16SYS	AUDIO_U16MSB
#define AUDIO_S16SYS	AUDIO_S16MSB
#endif

class audio_convert
{
public:
   audio_convert()
      : m_needed(0)
      , m_src_format(0)
      , m_dst_format(0)
      , m_rate_incr(0)
      , m_buf(NULL)
      , m_len(0)
      , m_len_cvt(0)
      , m_len_mult(0)
      , m_len_ratio(0)
      , m_filter_index(0)
   {}

   ~audio_convert() {}

public:

   bool init_audio_convert(uint16_t src_format, uint8_t src_channels, int src_rate, 
      uint16_t dst_format, uint8_t dst_channels, int dst_rate)
   {
      m_needed = 0;
      m_filter_index = 0;
      m_filters[0] = NULL;
      m_len_mult = 1;
      m_len_ratio = 1.0;

      /* First filter:  Endian conversion from src to dst */
      if ((src_format & 0x1000) != (dst_format & 0x1000)
         && ((src_format & 0xff) == 16) && ((dst_format & 0xff) == 16)) {
            m_filters[m_filter_index++] = &audio_convert::ConvertEndian;
      }

      /* Second filter: Sign conversion -- signed/unsigned */
      if ((src_format & 0x8000) != (dst_format & 0x8000)) {
         m_filters[m_filter_index++] = &audio_convert::ConvertSign;
      }

      /* Next filter:  Convert 16 bit <--> 8 bit PCM */
      if ((src_format & 0xFF) != (dst_format & 0xFF)) {
         switch (dst_format & 0x10FF) {
         case AUDIO_U8:
            m_filters[m_filter_index++] =
               &audio_convert::Convert8;
            m_len_ratio /= 2;
            break;
         case AUDIO_U16LSB:
            m_filters[m_filter_index++] =
               &audio_convert::Convert16LSB;
            m_len_mult *= 2;
            m_len_ratio *= 2;
            break;
         case AUDIO_U16MSB:
            m_filters[m_filter_index++] =
               &audio_convert::Convert16MSB;
            m_len_mult *= 2;
            m_len_ratio *= 2;
            break;
         }
      }

      /* Last filter:  Mono/Stereo conversion */
      if (src_channels != dst_channels) {
         if ((src_channels == 1) && (dst_channels > 1)) {
            m_filters[m_filter_index++] = 
               &audio_convert::ConvertStereo;
            m_len_mult *= 2;
            src_channels = 2;
            m_len_ratio *= 2;
         }
         if ((src_channels == 2) &&
            (dst_channels == 6)) {
               m_filters[m_filter_index++] =
                  &audio_convert::ConvertSurround;
               src_channels = 6;
               m_len_mult *= 3;
               m_len_ratio *= 3;
         }
         if ((src_channels == 2) &&
            (dst_channels == 4)) {
               m_filters[m_filter_index++] =
                  &audio_convert::ConvertSurround_4;
               src_channels = 4;
               m_len_mult *= 2;
               m_len_ratio *= 2;
         }
         while ((src_channels*2) <= dst_channels) {
            m_filters[m_filter_index++] = 
               &audio_convert::ConvertStereo;
            m_len_mult *= 2;
            src_channels *= 2;
            m_len_ratio *= 2;
         }
         if ((src_channels == 6) &&
            (dst_channels <= 2)) {
               m_filters[m_filter_index++] =
                  &audio_convert::ConvertStrip;
               src_channels = 2;
               m_len_ratio /= 3;
         }
         if ((src_channels == 6) &&
            (dst_channels == 4)) {
               m_filters[m_filter_index++] =
                  &audio_convert::ConvertStrip_2;
               src_channels = 4;
               m_len_ratio /= 2;
         }
         /* This assumes that 4 channel audio is in the format:
            Left {front/back} + Right {front/back}
            so converting to L/R stereo works properly.
         */
         while (((src_channels%2) == 0) &&
            ((src_channels/2) >= dst_channels)) {
               m_filters[m_filter_index++] =
                  &audio_convert::ConvertMono;
               src_channels /= 2;
               m_len_ratio /= 2;
         }
         if (src_channels != dst_channels) {
            /* Uh oh.. */;
         }
      }

      /* Do rate conversion */
      m_rate_incr = 0.0;
      if ( (src_rate/100) != (dst_rate/100) ) {
         uint32_t hi_rate, lo_rate;
         int len_mult;
         double len_ratio;
         void (audio_convert::*rate_cvt)(uint16_t format);

         if ( src_rate > dst_rate ) {
            hi_rate = src_rate;
            lo_rate = dst_rate;
            switch (src_channels) {
            case 1: rate_cvt = &audio_convert::RateDIV2; break;
            case 2: rate_cvt = &audio_convert::RateDIV2_c2; break;
            case 4: rate_cvt = &audio_convert::RateDIV2_c4; break;
            case 6: rate_cvt = &audio_convert::RateDIV2_c6; break;
            default: return false;
            }
            len_mult = 1;
            len_ratio = 0.5;
         } else {
            hi_rate = dst_rate;
            lo_rate = src_rate;
            switch (src_channels) {
            case 1: rate_cvt = &audio_convert::RateMUL2; break;
            case 2: rate_cvt = &audio_convert::RateMUL2_c2; break;
            case 4: rate_cvt = &audio_convert::RateMUL2_c4; break;
            case 6: rate_cvt = &audio_convert::RateMUL2_c6; break;
            default: return false;
            }
            len_mult = 2;
            len_ratio = 2.0;
         }
         /* If hi_rate = lo_rate*2^x then conversion is easy */
         while ( ((lo_rate*2)/100) <= (hi_rate/100) ) {
            m_filters[m_filter_index++] = rate_cvt;
            m_len_mult *= len_mult;
            lo_rate *= 2;
            m_len_ratio *= len_ratio;
         }
         /* We may need a slow conversion here to finish up */
         if ( (lo_rate/100) != (hi_rate/100) ) {
#if 1
            /* The problem with this is that if the input buffer is
            say 1K, and the conversion rate is say 1.1, then the
            output buffer is 1.1K, which may not be an acceptable
            buffer size for the audio driver (not a power of 2)
            */
            /* For now, punt and hope the rate distortion isn't great.
            */
#else
            if ( src_rate < dst_rate ) {
               m_rate_incr = (double)lo_rate/hi_rate;
               m_len_mult *= 2;
               m_len_ratio /= m_rate_incr;
            } else {
               m_rate_incr = (double)hi_rate/lo_rate;
               m_len_ratio *= m_rate_incr;
            }
            m_filters[m_filter_index++] = &audio_convert::RateSLOW;
#endif
         }
      }

      /* Set up the filter information */
      if (m_filter_index != 0) {
         m_needed = 1;
         m_src_format = src_format;
         m_dst_format = dst_format;
         m_len = 0;
         m_buf = NULL;
         m_filters[m_filter_index] = NULL;
      }

      return true;
   }

   bool convert_audio(uint8_t** dst, uint8_t* src, const uint32_t srclen, uint32_t* dstlen)
   {
      m_buf = *dst = (uint8_t*)av_malloc(srclen * m_len_mult);
      memcpy(m_buf, src, srclen);
      m_len = srclen;

      /* Make sure there's data to convert */
      if (m_buf == NULL) {
         printf("No buffer allocated for conversion!\n");
         return false;
      }

      /* Return okay if no conversion is necessary */
      m_len_cvt = m_len;
      if (m_filters[0] == NULL) {
         *dstlen = m_len_cvt;
         return true;
      }

      /* Set up the conversion and go! */
      m_filter_index = 0;
      ((this)->*m_filters[0])(m_src_format);
      *dstlen = m_len_cvt;

      return true;
   }

protected:

   /* Toggle endianness */
   void ConvertEndian(uint16_t format)
   {
      int i;
      uint8_t *data, tmp;

#ifdef DEBUG
      fprintf(stderr, "Converting audio endianness\n");
#endif

      data = m_buf;
      for (i = m_len_cvt / 2; i; --i) {
         tmp = data[0];
         data[0] = data[1];
         data[1] = tmp;
         data += 2;
      }
      format = (format ^ 0x1000);
      if (m_filters[++m_filter_index]) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Toggle signed/unsigned */
   void ConvertSign(uint16_t format)
   {
      int i;
      uint8_t *data;

#ifdef DEBUG
      fprintf(stderr, "Converting audio signedness\n");
#endif
      data = m_buf;
      if ((format & 0xFF) == 16) {
         if ((format & 0x1000) != 0x1000) { /* Little endian */
            ++data;
         }
         for (i = m_len_cvt / 2; i; --i) {
            *data ^= 0x80;
            data += 2;
         }
      } else {
         for (i = m_len_cvt; i; --i) {
            *data++ ^= 0x80;
         }
      }
      format = (format ^ 0x8000);
      if (m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert 16-bit to 8-bit */
   void Convert8(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting to 8-bit\n");
#endif
      src = m_buf;
      dst = m_buf;
      if ( (format & 0x1000) != 0x1000 ) { /* Little endian */
         ++src;
      }
      for (i = m_len_cvt / 2; i; --i) {
         *dst = *src;
         src += 2;
         dst += 1;
      }
      format = ((format & ~0x9010) | AUDIO_U8);
      m_len_cvt /= 2;
      if (m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert 8-bit to 16-bit - LSB */
   void Convert16LSB(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting to 16-bit LSB\n");
#endif
      src = m_buf + m_len_cvt;
      dst = m_buf + m_len_cvt * 2;
      for (i = m_len_cvt; i; --i) {
         src -= 1;
         dst -= 2;
         dst[1] = *src;
         dst[0] = 0;
      }
      format = ((format & ~0x0008) | AUDIO_U16LSB);
      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert 8-bit to 16-bit - MSB */
   void Convert16MSB(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting to 16-bit MSB\n");
#endif
      src = m_buf + m_len_cvt;
      dst = m_buf + m_len_cvt * 2;
      for (i = m_len_cvt; i; --i) {
         src -= 1;
         dst -= 2;
         dst[0] = *src;
         dst[1] = 0;
      }
      format = ((format & ~0x0008) | AUDIO_U16MSB);
      m_len_cvt *= 2;
      if (m_filters[++m_filter_index]) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Duplicate a mono channel to both stereo channels */
   void ConvertStereo(uint16_t format)
   {
      int i;

#ifdef DEBUG
      fprintf(stderr, "Converting to stereo\n");
#endif
      if ((format & 0xFF) == 16) {
         uint16_t *src, *dst;

         src = (uint16_t *)(m_buf + m_len_cvt);
         dst = (uint16_t *)(m_buf + m_len_cvt*2);
         for (i = m_len_cvt / 2; i; --i) {
            dst -= 2;
            src -= 1;
            dst[0] = src[0];
            dst[1] = src[0];
         }
      } else {
         uint8_t *src, *dst;

         src = m_buf + m_len_cvt;
         dst = m_buf + m_len_cvt*2;
         for (i = m_len_cvt; i; --i) {
            dst -= 2;
            src -= 1;
            dst[0] = src[0];
            dst[1] = src[0];
         }
      }
      m_len_cvt *= 2;
      if (m_filters[++m_filter_index]) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Duplicate a stereo channel to a pseudo-5.1 stream */
   void ConvertSurround(uint16_t format)
   {
      int i;

#ifdef DEBUG
      fprintf(stderr, "Converting stereo to surround\n");
#endif
      switch (format & 0x8018) {
      case AUDIO_U8: {
         uint8_t *src, *dst, lf, rf, ce;

         src = (uint8_t *)(m_buf+m_len_cvt);
         dst = (uint8_t *)(m_buf+m_len_cvt*3);
         for ( i=m_len_cvt; i; --i ) {
            dst -= 6;
            src -= 2;
            lf = src[0];
            rf = src[1];
            ce = (lf/2) + (rf/2);
            dst[0] = lf;
            dst[1] = rf;
            dst[2] = lf - ce;
            dst[3] = rf - ce;
            dst[4] = ce;
            dst[5] = ce;
         }
      }
      break;

      case AUDIO_S8: {
         int8_t *src, *dst, lf, rf, ce;

         src = (int8_t *)m_buf+m_len_cvt;
         dst = (int8_t *)m_buf+m_len_cvt*3;
         for ( i=m_len_cvt; i; --i ) {
            dst -= 6;
            src -= 2;
            lf = src[0];
            rf = src[1];
            ce = (lf/2) + (rf/2);
            dst[0] = lf;
            dst[1] = rf;
            dst[2] = lf - ce;
            dst[3] = rf - ce;
            dst[4] = ce;
            dst[5] = ce;
         }
      }
      break;

      case AUDIO_U16: {
         uint8_t *src, *dst;
         uint16_t lf, rf, ce, lr, rr;

         src = m_buf+m_len_cvt;
         dst = m_buf+m_len_cvt*3;

         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 12;
               src -= 4;
               lf = (uint16_t)((src[0]<<8)|src[1]);
               rf = (uint16_t)((src[2]<<8)|src[3]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[1] = (lf&0xFF);
               dst[0] = ((lf>>8)&0xFF);
               dst[3] = (rf&0xFF);
               dst[2] = ((rf>>8)&0xFF);

               dst[1+4] = (lr&0xFF);
               dst[0+4] = ((lr>>8)&0xFF);
               dst[3+4] = (rr&0xFF);
               dst[2+4] = ((rr>>8)&0xFF);

               dst[1+8] = (ce&0xFF);
               dst[0+8] = ((ce>>8)&0xFF);
               dst[3+8] = (ce&0xFF);
               dst[2+8] = ((ce>>8)&0xFF);
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 12;
               src -= 4;
               lf = (uint16_t)((src[1]<<8)|src[0]);
               rf = (uint16_t)((src[3]<<8)|src[2]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[0] = (lf&0xFF);
               dst[1] = ((lf>>8)&0xFF);
               dst[2] = (rf&0xFF);
               dst[3] = ((rf>>8)&0xFF);

               dst[0+4] = (lr&0xFF);
               dst[1+4] = ((lr>>8)&0xFF);
               dst[2+4] = (rr&0xFF);
               dst[3+4] = ((rr>>8)&0xFF);

               dst[0+8] = (ce&0xFF);
               dst[1+8] = ((ce>>8)&0xFF);
               dst[2+8] = (ce&0xFF);
               dst[3+8] = ((ce>>8)&0xFF);
            }
         }
      }
      break;

      case AUDIO_S16: {
         uint8_t *src, *dst;
         int16_t lf, rf, ce, lr, rr;

         src = m_buf+m_len_cvt;
         dst = m_buf+m_len_cvt*3;

         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 12;
               src -= 4;
               lf = (int16_t)((src[0]<<8)|src[1]);
               rf = (int16_t)((src[2]<<8)|src[3]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[1] = (lf&0xFF);
               dst[0] = ((lf>>8)&0xFF);
               dst[3] = (rf&0xFF);
               dst[2] = ((rf>>8)&0xFF);

               dst[1+4] = (lr&0xFF);
               dst[0+4] = ((lr>>8)&0xFF);
               dst[3+4] = (rr&0xFF);
               dst[2+4] = ((rr>>8)&0xFF);

               dst[1+8] = (ce&0xFF);
               dst[0+8] = ((ce>>8)&0xFF);
               dst[3+8] = (ce&0xFF);
               dst[2+8] = ((ce>>8)&0xFF);
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 12;
               src -= 4;
               lf = (int16_t)((src[1]<<8)|src[0]);
               rf = (int16_t)((src[3]<<8)|src[2]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[0] = (lf&0xFF);
               dst[1] = ((lf>>8)&0xFF);
               dst[2] = (rf&0xFF);
               dst[3] = ((rf>>8)&0xFF);

               dst[0+4] = (lr&0xFF);
               dst[1+4] = ((lr>>8)&0xFF);
               dst[2+4] = (rr&0xFF);
               dst[3+4] = ((rr>>8)&0xFF);

               dst[0+8] = (ce&0xFF);
               dst[1+8] = ((ce>>8)&0xFF);
               dst[2+8] = (ce&0xFF);
               dst[3+8] = ((ce>>8)&0xFF);
            }
         }
      }
      break;
      }

      m_len_cvt *= 3;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Duplicate a stereo channel to a pseudo-4.0 stream */
   void ConvertSurround_4(uint16_t format)
   {
      int i;

#ifdef DEBUG
      fprintf(stderr, "Converting stereo to quad\n");
#endif
      switch (format & 0x8018) {

      case AUDIO_U8: {
         uint8_t *src, *dst, lf, rf, ce;

         src = (uint8_t *)(m_buf+m_len_cvt);
         dst = (uint8_t *)(m_buf+m_len_cvt*2);
         for ( i=m_len_cvt; i; --i ) {
            dst -= 4;
            src -= 2;
            lf = src[0];
            rf = src[1];
            ce = (lf/2) + (rf/2);
            dst[0] = lf;
            dst[1] = rf;
            dst[2] = lf - ce;
            dst[3] = rf - ce;
         }
      }
      break;

      case AUDIO_S8: {
         int8_t *src, *dst, lf, rf, ce;

         src = (int8_t *)m_buf+m_len_cvt;
         dst = (int8_t *)m_buf+m_len_cvt*2;
         for ( i=m_len_cvt; i; --i ) {
            dst -= 4;
            src -= 2;
            lf = src[0];
            rf = src[1];
            ce = (lf/2) + (rf/2);
            dst[0] = lf;
            dst[1] = rf;
            dst[2] = lf - ce;
            dst[3] = rf - ce;
         }
      }
      break;

      case AUDIO_U16: {
         uint8_t *src, *dst;
         uint16_t lf, rf, ce, lr, rr;

         src = m_buf+m_len_cvt;
         dst = m_buf+m_len_cvt*2;

         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 8;
               src -= 4;
               lf = (uint16_t)((src[0]<<8)|src[1]);
               rf = (uint16_t)((src[2]<<8)|src[3]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[1] = (lf&0xFF);
               dst[0] = ((lf>>8)&0xFF);
               dst[3] = (rf&0xFF);
               dst[2] = ((rf>>8)&0xFF);

               dst[1+4] = (lr&0xFF);
               dst[0+4] = ((lr>>8)&0xFF);
               dst[3+4] = (rr&0xFF);
               dst[2+4] = ((rr>>8)&0xFF);
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 8;
               src -= 4;
               lf = (uint16_t)((src[1]<<8)|src[0]);
               rf = (uint16_t)((src[3]<<8)|src[2]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[0] = (lf&0xFF);
               dst[1] = ((lf>>8)&0xFF);
               dst[2] = (rf&0xFF);
               dst[3] = ((rf>>8)&0xFF);

               dst[0+4] = (lr&0xFF);
               dst[1+4] = ((lr>>8)&0xFF);
               dst[2+4] = (rr&0xFF);
               dst[3+4] = ((rr>>8)&0xFF);
            }
         }
      }
      break;

      case AUDIO_S16: {
         uint8_t *src, *dst;
         int16_t lf, rf, ce, lr, rr;

         src = m_buf+m_len_cvt;
         dst = m_buf+m_len_cvt*2;

         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 8;
               src -= 4;
               lf = (int16_t)((src[0]<<8)|src[1]);
               rf = (int16_t)((src[2]<<8)|src[3]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[1] = (lf&0xFF);
               dst[0] = ((lf>>8)&0xFF);
               dst[3] = (rf&0xFF);
               dst[2] = ((rf>>8)&0xFF);

               dst[1+4] = (lr&0xFF);
               dst[0+4] = ((lr>>8)&0xFF);
               dst[3+4] = (rr&0xFF);
               dst[2+4] = ((rr>>8)&0xFF);
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               dst -= 8;
               src -= 4;
               lf = (int16_t)((src[1]<<8)|src[0]);
               rf = (int16_t)((src[3]<<8)|src[2]);
               ce = (lf/2) + (rf/2);
               rr = lf - ce;
               lr = rf - ce;
               dst[0] = (lf&0xFF);
               dst[1] = ((lf>>8)&0xFF);
               dst[2] = (rf&0xFF);
               dst[3] = ((rf>>8)&0xFF);

               dst[0+4] = (lr&0xFF);
               dst[1+4] = ((lr>>8)&0xFF);
               dst[2+4] = (rr&0xFF);
               dst[3+4] = ((rr>>8)&0xFF);
            }
         }
      }
      break;
      }

      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   void ConvertStrip(uint16_t format)
   {
      int i;
      int32_t lsample, rsample;

#ifdef DEBUG
      fprintf(stderr, "Converting down to stereo\n");
#endif
      switch (format&0x8018) {

      case AUDIO_U8: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         for ( i=m_len_cvt/6; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 6;
            dst += 2;
         }
      }
      break;

      case AUDIO_S8: {
         int8_t *src, *dst;

         src = (int8_t *)m_buf;
         dst = (int8_t *)m_buf;
         for ( i=m_len_cvt/6; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 6;
            dst += 2;
         }
      }
      break;

      case AUDIO_U16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/12; i; --i ) {
               lsample = (uint16_t)((src[0]<<8)|src[1]);
               rsample = (uint16_t)((src[2]<<8)|src[3]);
               dst[1] = (lsample&0xFF);
               lsample >>= 8;
               dst[0] = (lsample&0xFF);
               dst[3] = (rsample&0xFF);
               rsample >>= 8;
               dst[2] = (rsample&0xFF);
               src += 12;
               dst += 4;
            }
         } else {
            for ( i=m_len_cvt/12; i; --i ) {
               lsample = (uint16_t)((src[1]<<8)|src[0]);
               rsample = (uint16_t)((src[3]<<8)|src[2]);
               dst[0] = (lsample&0xFF);
               lsample >>= 8;
               dst[1] = (lsample&0xFF);
               dst[2] = (rsample&0xFF);
               rsample >>= 8;
               dst[3] = (rsample&0xFF);
               src += 12;
               dst += 4;
            }
         }
      }
      break;

      case AUDIO_S16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/12; i; --i ) {
               lsample = (int16_t)((src[0]<<8)|src[1]);
               rsample = (int16_t)((src[2]<<8)|src[3]);
               dst[1] = (lsample&0xFF);
               lsample >>= 8;
               dst[0] = (lsample&0xFF);
               dst[3] = (rsample&0xFF);
               rsample >>= 8;
               dst[2] = (rsample&0xFF);
               src += 12;
               dst += 4;
            }
         } else {
            for ( i=m_len_cvt/12; i; --i ) {
               lsample = (int16_t)((src[1]<<8)|src[0]);
               rsample = (int16_t)((src[3]<<8)|src[2]);
               dst[0] = (lsample&0xFF);
               lsample >>= 8;
               dst[1] = (lsample&0xFF);
               dst[2] = (rsample&0xFF);
               rsample >>= 8;
               dst[3] = (rsample&0xFF);
               src += 12;
               dst += 4;
            }
         }
      }
      break;
      }
      m_len_cvt /= 3;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Discard top 2 channels of 6 */
   void ConvertStrip_2(uint16_t format)
   {
      int i;
      int32_t lsample, rsample;

#ifdef DEBUG
      fprintf(stderr, "Converting 6 down to quad\n");
#endif
      switch (format & 0x8018) {

      case AUDIO_U8: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         for ( i=m_len_cvt/4; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 4;
            dst += 2;
         }
      }
      break;

      case AUDIO_S8: {
         int8_t *src, *dst;

         src = (int8_t *)m_buf;
         dst = (int8_t *)m_buf;
         for ( i=m_len_cvt/4; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 4;
            dst += 2;
         }
      }
      break;

      case AUDIO_U16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/8; i; --i ) {
               lsample = (uint16_t)((src[0]<<8)|src[1]);
               rsample = (uint16_t)((src[2]<<8)|src[3]);
               dst[1] = (lsample&0xFF);
               lsample >>= 8;
               dst[0] = (lsample&0xFF);
               dst[3] = (rsample&0xFF);
               rsample >>= 8;
               dst[2] = (rsample&0xFF);
               src += 8;
               dst += 4;
            }
         } else {
            for ( i=m_len_cvt/8; i; --i ) {
               lsample = (uint16_t)((src[1]<<8)|src[0]);
               rsample = (uint16_t)((src[3]<<8)|src[2]);
               dst[0] = (lsample&0xFF);
               lsample >>= 8;
               dst[1] = (lsample&0xFF);
               dst[2] = (rsample&0xFF);
               rsample >>= 8;
               dst[3] = (rsample&0xFF);
               src += 8;
               dst += 4;
            }
         }
      }
      break;

      case AUDIO_S16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/8; i; --i ) {
               lsample = (int16_t)((src[0]<<8)|src[1]);
               rsample = (int16_t)((src[2]<<8)|src[3]);
               dst[1] = (lsample&0xFF);
               lsample >>= 8;
               dst[0] = (lsample&0xFF);
               dst[3] = (rsample&0xFF);
               rsample >>= 8;
               dst[2] = (rsample&0xFF);
               src += 8;
               dst += 4;
            }
         } else {
            for ( i=m_len_cvt/8; i; --i ) {
               lsample = (int16_t)((src[1]<<8)|src[0]);
               rsample = (int16_t)((src[3]<<8)|src[2]);
               dst[0] = (lsample&0xFF);
               lsample >>= 8;
               dst[1] = (lsample&0xFF);
               dst[2] = (rsample&0xFF);
               rsample >>= 8;
               dst[3] = (rsample&0xFF);
               src += 8;
               dst += 4;
            }
         }
      }
      break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Effectively mix right and left channels into a single channel */
   void ConvertMono(uint16_t format)
   {
      int i;
      int32_t sample;

#ifdef DEBUG
      fprintf(stderr, "Converting to mono\n");
#endif
      switch (format & 0x8018) {

      case AUDIO_U8: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         for ( i=m_len_cvt/2; i; --i ) {
            sample = src[0] + src[1];
            *dst = (uint8_t)(sample / 2);
            src += 2;
            dst += 1;
         }
      }
      break;

      case AUDIO_S8: {
         int8_t *src, *dst;

         src = (int8_t *)m_buf;
         dst = (int8_t *)m_buf;
         for ( i=m_len_cvt/2; i; --i ) {
            sample = src[0] + src[1];
            *dst = (int8_t)(sample / 2);
            src += 2;
            dst += 1;
         }
      }
      break;

      case AUDIO_U16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               sample = (uint16_t)((src[0]<<8)|src[1])+
                  (uint16_t)((src[2]<<8)|src[3]);
               sample /= 2;
               dst[1] = (sample&0xFF);
               sample >>= 8;
               dst[0] = (sample&0xFF);
               src += 4;
               dst += 2;
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               sample = (uint16_t)((src[1]<<8)|src[0])+
                  (uint16_t)((src[3]<<8)|src[2]);
               sample /= 2;
               dst[0] = (sample&0xFF);
               sample >>= 8;
               dst[1] = (sample&0xFF);
               src += 4;
               dst += 2;
            }
         }
      }
      break;

      case AUDIO_S16: {
         uint8_t *src, *dst;

         src = m_buf;
         dst = m_buf;
         if ( (format & 0x1000) == 0x1000 ) {
            for ( i=m_len_cvt/4; i; --i ) {
               sample = (int16_t)((src[0]<<8)|src[1])+
                  (int16_t)((src[2]<<8)|src[3]);
               sample /= 2;
               dst[1] = (sample&0xFF);
               sample >>= 8;
               dst[0] = (sample&0xFF);
               src += 4;
               dst += 2;
            }
         } else {
            for ( i=m_len_cvt/4; i; --i ) {
               sample = (int16_t)((src[1]<<8)|src[0])+
                  (int16_t)((src[3]<<8)|src[2]);
               sample /= 2;
               dst[0] = (sample&0xFF);
               sample >>= 8;
               dst[1] = (sample&0xFF);
               src += 4;
               dst += 2;
            }
         }
      }
      break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate down by multiple of 2 */
   void RateDIV2(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate / 2\n");
#endif
      src = m_buf;
      dst = m_buf;

      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/2; i; --i ) {
            dst[0] = src[0];
            src += 2;
            dst += 1;
         }
         break;
      case 16:
         for ( i=m_len_cvt/4; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 4;
            dst += 2;
         }
         break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate down by multiple of 2, for stereo */
   void RateDIV2_c2(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate / 2\n");
#endif
      src = m_buf;
      dst = m_buf;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/4; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            src += 4;
            dst += 2;
         }
         break;
      case 16:
         for ( i=m_len_cvt/8; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            src += 8;
            dst += 4;
         }
         break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }


   /* Convert rate down by multiple of 2, for quad */
   void RateDIV2_c4(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate / 2\n");
#endif
      src = m_buf;
      dst = m_buf;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/8; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            src += 8;
            dst += 4;
         }
         break;
      case 16:
         for ( i=m_len_cvt/16; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
            src += 16;
            dst += 8;
         }
         break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate down by multiple of 2, for 5.1 */
   void RateDIV2_c6(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate / 2\n");
#endif
      src = m_buf;
      dst = m_buf;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/12; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            src += 12;
            dst += 6;
         }
         break;
      case 16:
         for ( i=m_len_cvt/24; i; --i ) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
            dst[8] = src[8];
            dst[9] = src[9];
            dst[10] = src[10];
            dst[11] = src[11];
            src += 24;
            dst += 12;
         }
         break;
      }
      m_len_cvt /= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Very slow rate conversion routine */
   void RateSLOW(uint16_t format)
   {
      double ipos;
      int i, clen;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate * %4.4f\n", 1.0/m_rate_incr);
#endif

      clen = (int)((double)m_len_cvt / m_rate_incr);
      if ( m_rate_incr > 1.0 ) {
         switch (format & 0xFF) {
         case 8: {
            uint8_t *output;

            output = m_buf;
            ipos = 0.0;
            for ( i=clen; i; --i ) {
               *output = m_buf[(int)ipos];
               ipos += m_rate_incr;
               output += 1;
            }
         }
         break;

         case 16: {
            uint16_t *output;

            clen &= ~1;
            output = (uint16_t *)m_buf;
            ipos = 0.0;
            for ( i=clen/2; i; --i ) {
               *output=((uint16_t *)m_buf)[(int)ipos];
               ipos += m_rate_incr;
               output += 1;
            }
         }
         break;
         }
      } else {
         switch (format & 0xFF) {
         case 8: {
            uint8_t *output;

            output = m_buf+clen;
            ipos = (double)m_len_cvt;
            for ( i=clen; i; --i ) {
               ipos -= m_rate_incr;
               output -= 1;
               *output = m_buf[(int)ipos];
            }
         }
         break;

         case 16: {
            uint16_t *output;

            clen &= ~1;
            output = (uint16_t *)(m_buf+clen);
            ipos = (double)m_len_cvt/2;
            for ( i=clen/2; i; --i ) {
               ipos -= m_rate_incr;
               output -= 1;
               *output=((uint16_t *)m_buf)[(int)ipos];
            }
         }
         break;
         }
      }
      m_len_cvt = clen;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate up by multiple of 2 */
   void RateMUL2(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate * 2\n");
#endif

      src = m_buf+m_len_cvt;
      dst = m_buf+m_len_cvt*2;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt; i; --i ) {
            src -= 1;
            dst -= 2;
            dst[0] = src[0];
            dst[1] = src[0];
         }
         break;
      case 16:
         for ( i=m_len_cvt/2; i; --i ) {
            src -= 2;
            dst -= 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[1];
         }
         break;
      }
      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }


   /* Convert rate up by multiple of 2, for stereo */
   void RateMUL2_c2(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate * 2\n");
#endif
      src = m_buf+m_len_cvt;
      dst = m_buf+m_len_cvt*2;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/2; i; --i ) {
            src -= 2;
            dst -= 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[1];
         }
         break;
      case 16:
         for ( i=m_len_cvt/4; i; --i ) {
            src -= 4;
            dst -= 8;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[0];
            dst[5] = src[1];
            dst[6] = src[2];
            dst[7] = src[3];
         }
         break;
      }
      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate up by multiple of 2, for quad */
   void RateMUL2_c4(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate * 2\n");
#endif
      src = m_buf+m_len_cvt;
      dst = m_buf+m_len_cvt*2;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/4; i; --i ) {
            src -= 4;
            dst -= 8;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[0];
            dst[5] = src[1];
            dst[6] = src[2];
            dst[7] = src[3];
         }
         break;
      case 16:
         for ( i=m_len_cvt/8; i; --i ) {
            src -= 8;
            dst -= 16;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
            dst[8] = src[0];
            dst[9] = src[1];
            dst[10] = src[2];
            dst[11] = src[3];
            dst[12] = src[4];
            dst[13] = src[5];
            dst[14] = src[6];
            dst[15] = src[7];
         }
         break;
      }
      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }

   /* Convert rate up by multiple of 2, for 5.1 */
   void RateMUL2_c6(uint16_t format)
   {
      int i;
      uint8_t *src, *dst;

#ifdef DEBUG
      fprintf(stderr, "Converting audio rate * 2\n");
#endif

      src = m_buf+m_len_cvt;
      dst = m_buf+m_len_cvt*2;
      switch (format & 0xFF) {
      case 8:
         for ( i=m_len_cvt/6; i; --i ) {
            src -= 6;
            dst -= 12;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[0];
            dst[7] = src[1];
            dst[8] = src[2];
            dst[9] = src[3];
            dst[10] = src[4];
            dst[11] = src[5];
         }
         break;
      case 16:
         for ( i=m_len_cvt/12; i; --i ) {
            src -= 12;
            dst -= 24;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
            dst[8] = src[8];
            dst[9] = src[9];
            dst[10] = src[10];
            dst[11] = src[11];
            dst[12] = src[0];
            dst[13] = src[1];
            dst[14] = src[2];
            dst[15] = src[3];
            dst[16] = src[4];
            dst[17] = src[5];
            dst[18] = src[6];
            dst[19] = src[7];
            dst[20] = src[8];
            dst[21] = src[9];
            dst[22] = src[10];
            dst[23] = src[11];
         }
         break;
      }
      m_len_cvt *= 2;
      if ( m_filters[++m_filter_index] ) {
         ((this)->*m_filters[m_filter_index])(format);
      }
   }


private:
   int m_needed;              /**< Set to 1 if conversion possible */
   uint16_t m_src_format;		/**< Source audio format */
   uint16_t m_dst_format;		/**< Target audio format */
   double m_rate_incr;		   /**< Rate conversion increment */
   uint8_t* m_buf;			   /**< Buffer to hold entire audio data */
   int    m_len;			      /**< Length of original audio buffer */
   int    m_len_cvt;			   /**< Length of converted audio buffer */
   int    m_len_mult;		   /**< buffer must be len*len_mult big */
   double m_len_ratio; 	      /**< Given len, final size is len*len_ratio */
   void (audio_convert::*m_filters[10])(uint16_t format);
   int m_filter_index;		   /**< Current audio conversion function */
};

} // namespace libavplayer

#endif // __AUDIOCVT_H__
