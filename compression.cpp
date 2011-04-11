#include "yeti.h"
#include "codec_inst.h"
#include "prediction.h"
#include "threading.h"
#include "convert.h"
#include "huffyuv_a.h"
#include "convert_yuy2.h"
#include "convert_yv12.h"

DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if(m_started == 0x1337)
   {
      CompressEnd();
   }
   m_started = 0;

   m_nullframes = GetPrivateProfileInt(SettingsHeading, "nullframes", FALSE, SettingsFile) > 0;
   m_deltaframes = GetPrivateProfileInt(SettingsHeading, "deltaframes", FALSE, SettingsFile) > 0;

   if (int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK)
   {
      return error;
   }

   m_width = lpbiIn->biWidth;
   m_height = lpbiIn->biHeight;

   m_format = lpbiIn->biBitCount;
   m_length = EIGHTH(m_width * m_height * m_format);

   size_t buffer_size;
   if(m_format < RGB24)
   {
      buffer_size = EIGHTH(ALIGN_ROUND(m_width, 32) * m_height * m_format) + 1024;
   } 
   else
   {
      buffer_size = QUADRUPLE(ALIGN_ROUND(m_width, 16) * m_height) + 1024;
   }

   m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, buffer_size, 16, "buffer");
   m_buffer2 = (BYTE*)ALIGNED_MALLOC(m_buffer2, buffer_size, 16, "buffer2");
   m_deltaBuffer = (BYTE*)ALIGNED_MALLOC(m_deltaBuffer, buffer_size, 16, "delta");
   m_colorTransBuffer = (BYTE*)ALIGNED_MALLOC(m_colorTransBuffer, buffer_size, 16, "colorT");
   m_prevFrame = (BYTE*)ALIGNED_MALLOC(m_prevFrame, buffer_size, 16, "prev");
   ZeroMemory(m_prevFrame, buffer_size);

   if(!m_buffer || !m_buffer2 || !m_prevFrame || !m_deltaBuffer || !m_colorTransBuffer)
   {
      ALIGNED_FREE(m_buffer, "buffer");
      ALIGNED_FREE(m_buffer2, "buffer2");
      ALIGNED_FREE(m_deltaBuffer, "delta");
      ALIGNED_FREE(m_colorTransBuffer, "colorT");
      ALIGNED_FREE(m_prevFrame, "prev");
      return (DWORD)ICERR_MEMORY;
   }

   if (!m_compressWorker.InitCompressBuffers(m_width * m_height))
   {
      return (DWORD)ICERR_MEMORY;
   }

   DWORD code = InitThreads(true);
   if(code != ICERR_OK)
   {
      return code;
   }
   m_started = 0x1337;

   return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 1KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   return (DWORD)(ALIGN_ROUND(lpbiIn->biWidth, 16) * lpbiIn->biHeight * lpbiIn->biBitCount / 8 * 1.05 + 1024); // TODO: Optimize
}

// release resources when compression is done

DWORD CodecInst::CompressEnd()
{
   if( m_started == 0x1337)
   {
      EndThreads();

      ALIGNED_FREE(m_buffer, "buffer");
      ALIGNED_FREE(m_buffer2, "buffer2");
      ALIGNED_FREE(m_deltaBuffer, "delta");
      ALIGNED_FREE(m_prevFrame, "prev");
      ALIGNED_FREE(m_colorTransBuffer, "colorT");
      m_compressWorker.FreeCompressBuffers();
   }

   m_started = 0;
   return ICERR_OK;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings
DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD /*dwSize*/)
{
   m_out = (BYTE*)icinfo->lpOutput;
   m_in  = (BYTE*)icinfo->lpInput;

   if(icinfo->lFrameNum == 0 && m_started != 0x1337)
   {
      DWORD error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
      if(error != ICERR_OK)
      {
         return error;
      }
   } 

   const size_t pixels = m_width * m_height;
   unsigned int upos = ALIGN_ROUND(pixels + 16, 16);
   unsigned int vpos = ALIGN_ROUND(pixels * 3/2 + 32, 16);

   if ( SSE2 )
   {
      if(m_compressFormat == YV12)
      {
         if(m_format == RGB24)
         {
            ConvertRGB24toYV12_SSE2(m_in, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_height);
         } 
         else if(m_format == RGB32)
         {
            ConvertRGB32toYV12_SSE2(m_in, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_height);
         }
         else 
         {
            isse_yuy2_to_yv12(m_in, m_width*2 , m_width*2, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_width/2, m_height);
         }
      }
      else 
      {
         if ( m_format == RGB24 )
         {
            ConvertRGB24toYV16_SSE2(m_in,m_colorTransBuffer,m_colorTransBuffer+upos,m_colorTransBuffer+vpos, m_width, m_height);
         } 
         else 
         {
            ConvertRGB32toYV16_SSE2(m_in,m_colorTransBuffer,m_colorTransBuffer+upos,m_colorTransBuffer+vpos, m_width, m_height);
         }
      }
   }
   else 
   {
      if(m_compressFormat == YV12)
      {
         if(m_format == RGB24)
         {
            mmx_ConvertRGB24toYUY2(m_in,m_buffer,m_width*3,m_width*2,m_width,m_height);
         } 
         else if(m_format == RGB32)
         {
            mmx_ConvertRGB32toYUY2(m_in,m_buffer,m_width*4,m_width*2,m_width,m_height);
         }
         const BYTE* src = (m_format>=RGB24)?m_buffer:m_in;
         isse_yuy2_to_yv12(src,m_width*2,m_width*2,m_colorTransBuffer,m_colorTransBuffer+upos,m_colorTransBuffer+vpos,m_width,m_width/2,m_height);
      } 
      else
      {
         if(m_format == RGB24)
         {
            mmx_ConvertRGB24toYUY2(m_in,m_buffer,m_width*3,m_width*2,m_width,m_height);
         } 
         else if(m_format == RGB32 )
         {
            mmx_ConvertRGB32toYUY2(m_in,m_buffer,m_width*4,m_width*2,m_width,m_height);
         }
         Split_YUY2(m_buffer, m_colorTransBuffer, m_colorTransBuffer+upos, m_colorTransBuffer+vpos,m_width,m_height);
      }
   }

   m_in = m_colorTransBuffer;

   return m_compressFormat == YV12 ? CompressYV12(icinfo) : CompressYUV16(icinfo);
}

DWORD CodecInst::CompressYUV16(ICCOMPRESS* icinfo)
{
   const size_t pixels    = m_width * m_height;
   const size_t y_stride  = ALIGN_ROUND(pixels + 16, 16);
   const size_t c_stride  = ALIGN_ROUND(HALF(pixels) + 32, 16);
   const size_t len       = y_stride + DOUBLE(c_stride);

   BYTE frameType;
   BYTE* source = m_deltaBuffer;

   //TODO: Remove duplication.
   if(icinfo->dwFlags != ICCOMPRESS_KEYFRAME && icinfo->lFrameNum > 0)
   {
      if(m_deltaframes)
      {
         const unsigned __int64 minDelta = len * 3;
         const unsigned __int64 bitCount = Fast_Sub_Count(m_deltaBuffer, m_in, m_prevFrame, len, minDelta);

         if(bitCount == ULLONG_MAX)
         {
            source = m_in;
            *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
            icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
            frameType = YUY2_KEYFRAME;
         }
         else if(bitCount == 0 && m_nullframes)
         {
            icinfo->lpbiOutput->biSizeImage = 0;
            *icinfo->lpdwFlags = 0;
            return (DWORD)ICERR_OK;
         }
         else
         {
            *icinfo->lpdwFlags |= AVIIF_LASTPART;
            frameType = YUY2_DELTAFRAME;
         }
      }
      else
      {
         if(m_nullframes)
         {
            size_t pos = HALF(len) + 15;
            pos &= (~15);
            if(memcmp(m_in + pos, m_prevFrame + pos, len - pos) == 0 && memcmp(m_in, m_prevFrame, pos) == 0)
            {
               icinfo->lpbiOutput->biSizeImage = 0;
               *icinfo->lpdwFlags = 0;
               return (DWORD)ICERR_OK;
            }
         }
         source = m_in;
         *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
         icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
         frameType = YUY2_KEYFRAME;
      }
   }
   else
   {
      source = m_in;
      *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
      icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
      frameType = YUY2_KEYFRAME;
   }

   if(m_nullframes || m_deltaframes)
   {
      memcpy(m_prevFrame, m_in, len);
   }

   BYTE* ysrc  = m_buffer;
   BYTE* usrc  = ysrc + y_stride;
   BYTE* vsrc  = usrc + c_stride;

   BYTE* ydest	= m_buffer2;
   BYTE* udest	= ydest + y_stride;
   BYTE* vdest	= udest + c_stride;

   BYTE *y, *u, *v;

   if ( icinfo->lpbiInput->biCompression == FOURCC_YV16 )
   {
      y = source;
      v = y + pixels;
      u = v + HALF(pixels);

      ydest += ((int)y)&15;
      udest += ((int)u)&15;
      vdest += ((int)v)&15;
   }
   else if ( icinfo->lpbiInput->biBitCount > YUY2 )
   {
      y = source;
      u = source + ALIGN_ROUND(m_width * m_height + 16, 16);
      v = source + ALIGN_ROUND(m_width * m_height * 3/2 + 32,16);
   } 
   else 
   {
      if ( icinfo->lpbiInput->biCompression == FOURCC_UYVY )
      {
         Split_UYVY(source, ysrc, usrc, vsrc, m_width, m_height);
      } 
      else
      {
         Split_YUY2(source, ysrc, usrc, vsrc, m_width, m_height);
      }
      y = ysrc;
      u = usrc;
      v = vsrc;
   }

   m_info_a.m_source = u;
   m_info_a.m_dest = udest;
   m_info_a.m_length = HALF(pixels);
   SetEvent(m_info_a.m_startEvent);
   m_info_b.m_source = v;
   m_info_b.m_dest = vdest;
   m_info_b.m_length =  HALF(pixels);
   SetEvent(m_info_b.m_startEvent);

   Block_Predict_YUV16(y, ydest, m_width, pixels, true);

   size_t size = m_compressWorker.Compact(ydest, m_out + 9, pixels);
   WaitForSingleObject(m_info_a.m_doneEvent,INFINITE);

   size_t sizea = m_info_a.m_size;
   *(UINT32*)(m_out+1) = size + 9;
   memcpy(m_out + size + 9, udest, sizea);

   WaitForSingleObject(m_info_b.m_doneEvent, INFINITE);

   size_t sizeb = m_info_b.m_size;
   *(UINT32*)(m_out+5) = sizea + 9 + size;
   memcpy(m_out + sizea + size + 9, vdest, sizeb);

   size += sizea + sizeb + 9;

   m_out[0] = frameType;
   icinfo->lpbiOutput->biSizeImage = size;
   assert( *(__int64*)(m_out+*(UINT32*)(m_out+1)) != 0 );
   assert( *(__int64*)(m_out+*(UINT32*)(m_out+5)) != 0 );
   return ICERR_OK;
}


DWORD CodecInst::CompressYV12(ICCOMPRESS* icinfo)
{
   const size_t y_len	= m_width * m_height;
   const size_t c_len	= FOURTH(y_len);
   const size_t yu_len	= y_len + c_len;
   const size_t len		= yu_len + c_len;

   BYTE* source = m_deltaBuffer;
   BYTE frameType;

   //TODO: Remove duplication.
   if(icinfo->dwFlags != ICCOMPRESS_KEYFRAME && icinfo->lFrameNum > 0)
   {
      if(m_deltaframes)
      {
         const unsigned __int64 minDelta = len * 3;
         const unsigned __int64 bitCount = Fast_Sub_Count(source, m_in, m_prevFrame, len, minDelta);

         if(bitCount == ULLONG_MAX)
         {
            source = m_in;
            *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
            icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
            frameType = YV12_KEYFRAME;
         }
         else if(bitCount == 0 && m_nullframes)
         {
            icinfo->lpbiOutput->biSizeImage = 0;
            *icinfo->lpdwFlags = 0;
            return (DWORD)ICERR_OK;
         }
         else
         {
            *icinfo->lpdwFlags |= AVIIF_LASTPART;
            frameType = YV12_DELTAFRAME;
         }
      }
      else
      {
         if(m_nullframes)
         {
            // compare in two parts, video is more likely to change in middle than at bottom
            unsigned int pos = HALF(len) + 15;
            pos &= (~15);
            if(memcmp(m_in + pos, m_prevFrame + pos, len - pos) == 0 && memcmp(m_in, m_prevFrame, pos) == 0)
            {
               icinfo->lpbiOutput->biSizeImage = 0;
               *icinfo->lpdwFlags = 0;
               return (DWORD)ICERR_OK;
            }
         }

         source = m_in;
         *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
         icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
         frameType = YV12_KEYFRAME;
      }
   }
   else
   {
      source = m_in;
      *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
      icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
      frameType = YV12_KEYFRAME;
   }

   if(m_nullframes || m_deltaframes)
   {
      BYTE* temp = m_prevFrame;
      m_prevFrame = m_colorTransBuffer;
      m_colorTransBuffer = temp;
   }

   const BYTE* ysrc = source;
   const BYTE* vsrc = ysrc + y_len;
   const BYTE* usrc = vsrc + c_len;

   if ( icinfo->lpbiInput->biBitCount != YV12 )
   {
      usrc = ysrc + ALIGN_ROUND(y_len + 16, 16);
      vsrc = ysrc + ALIGN_ROUND(y_len * 3/2 + 32, 16);
   }

   m_info_a.m_source = vsrc;
   m_info_a.m_dest = m_buffer2;
   m_info_a.m_length = c_len;
   SetEvent(m_info_a.m_startEvent);
   m_info_b.m_source = usrc;
   m_info_b.m_dest = m_buffer2 + HALF(y_len);
   m_info_b.m_length = c_len;
   SetEvent(m_info_b.m_startEvent);

   BYTE* ydest = m_buffer;
   ydest += (unsigned int)ysrc & 15;

   Block_Predict(ysrc, ydest, m_width, y_len);

   size_t size = m_compressWorker.Compact(ydest, m_out + 9, y_len);
   HANDLE events[2];
   events[0] = m_info_a.m_doneEvent;
   events[1] = m_info_b.m_doneEvent;

   DWORD object = WaitForMultipleObjects(2, &events[0], false, INFINITE);
   object -= WAIT_OBJECT_0;

   if (object == 0)
   {
      size_t sizea = m_info_a.m_size;
      *(UINT32*)(m_out + 1) = 9 + size;
      memcpy(m_out + size + 9, m_buffer2, sizea);
      WaitForSingleObject(m_info_b.m_doneEvent,INFINITE);
      size_t sizeb = m_info_b.m_size;
      *(UINT32*)(m_out + 5) = sizea + 9 + size;
      memcpy(m_out + sizea + 9 + size, m_buffer2 + HALF(y_len), sizeb);
      size += sizea + sizeb + 9;
   } 
   else 
   {
      size_t sizeb = m_info_b.m_size;
      *(UINT32*)(m_out + 5) = 9 + size;
      memcpy(m_out + 9 + size, m_buffer2 + HALF(y_len), sizeb);
      WaitForSingleObject(m_info_a.m_doneEvent,INFINITE);
      size_t sizea = m_info_a.m_size;
      *(UINT32*)(m_out + 1) = 9 + size + sizeb;
      memcpy(m_out + size + sizeb + 9, m_buffer2, sizea);
      size += sizea + sizeb + 9;
   }

   m_out[0] = frameType;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}