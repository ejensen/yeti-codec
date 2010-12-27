#include "yeti.h"
#include "codec_inst.h"
#include "prediction.h"
#include "resolution.h"
#include "threading.h"

#include "huffyuv_a.h"
#include "convert_yuy2.h"
#include "convert_yv12.h"

DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if(m_started)
   {
      CompressEnd();
   }

   m_nullframes = GetPrivateProfileInt(SettingsHeading, "nullframes", FALSE, SettingsFile) > 0;
   m_deltaframes = GetPrivateProfileInt(SettingsHeading, "deltaframes", FALSE, SettingsFile) > 0;
   m_multithreading = GetPrivateProfileInt(SettingsHeading, "multithreading", FALSE, SettingsFile) > 0;

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

   int cb_size = m_width * m_height * 5/4;

   if (!m_compressWorker.InitCompressBuffers(cb_size))
   {
      return (DWORD)ICERR_MEMORY;
   }

   if (m_multithreading)
   {
      int code = InitThreads(true);
      if(code != ICERR_OK)
      {
         return code;
      }
   }

   m_started = true;

   return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 110% of image size + 4KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   return (DWORD)(EIGHTH(ALIGN_ROUND(lpbiIn->biWidth, 16) * lpbiIn->biHeight * lpbiIn->biBitCount) * 1.1 + 4096); // TODO: Optimize
}

// release resources when compression is done

DWORD CodecInst::CompressEnd()
{
   if(m_started)
   {
      if(m_multithreading)
      {
         EndThreads();
      }

      ALIGNED_FREE(m_buffer, "buffer");
      ALIGNED_FREE(m_buffer2, "buffer2");
      ALIGNED_FREE(m_deltaBuffer, "delta");
      ALIGNED_FREE(m_prevFrame, "prev");
      ALIGNED_FREE(m_colorTransBuffer, "colorT");
      m_compressWorker.FreeCompressBuffers();
   }

   m_started = false;
   return ICERR_OK;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings
DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize)
{
   m_out = (BYTE*)icinfo->lpOutput;
   m_in  = (BYTE*)icinfo->lpInput;

   if(icinfo->lFrameNum == 0 && !m_started)
   {
      DWORD error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
      if(error != ICERR_OK)
      {
         return error;
      }
   } 

   const BYTE* src = m_in;
   BYTE* dest;

   if(m_format >= RGB24)
   {
      dest = (m_compressFormat == YUY2) ? m_colorTransBuffer : m_buffer;

      if(m_format == RGB24)
      {
         mmx_ConvertRGB24toYUY2(m_in, dest, m_width * 3, DOUBLE(m_width), m_width, m_height);
      } 
      else if(m_format == RGB32)
      {
         mmx_ConvertRGB32toYUY2((const unsigned int*)m_in, (unsigned int*)dest, m_width, HALF(m_width), m_width, m_height);
      }
      src = m_buffer;
   }

   if(m_compressFormat == YUY2)
   {
      m_in = dest;
      return CompressYUY2(icinfo);
   }

   unsigned int dw = DOUBLE(m_width);
   BYTE * dst2 = m_colorTransBuffer;
   unsigned int yuy2_pitch = ALIGN_ROUND(dw, 16);
   unsigned int y_pitch = ALIGN_ROUND(m_width, 8);
   unsigned int uv_pitch = ALIGN_ROUND(HALF(m_width), 8);

   bool is_aligned = (m_width % 16) == 0;
   if(!is_aligned)
   {
      for(unsigned int h = 0; h < m_height; h++)
      {
         memcpy(dst2 + yuy2_pitch * h, src + dw * h, dw);
      }

      src = dst2;
      dst2 = m_buffer;
   }

   isse_yuy2_to_yv12(src, dw, yuy2_pitch, dst2, dst2 + y_pitch * m_height + HALF(uv_pitch * m_height), dst2 + y_pitch * m_height, y_pitch, uv_pitch, m_height);

   dest = m_colorTransBuffer;
   if(!is_aligned)
   {
      unsigned int h;

      for(h = 0; h < m_height; h++)
      {
         memcpy(dest + m_width * h, dst2 + y_pitch * h, m_width);
      }

      dst2 += y_pitch * m_height;
      dest += m_width * m_height;

      const unsigned int hw = HALF(m_width);
      for(h = 0 ; h < m_height; h++)
      {
         memcpy(dest + hw * h, dst2 + uv_pitch * h, hw);
      }
   }

   m_in = m_colorTransBuffer;

   //char buffer[11];
   //_itoa_s(m_compressFormat, buffer, 11, 10);
   //MessageBox(HWND_DESKTOP, buffer, "format", MB_OK);

   if(m_compressFormat == YV12)
   {
      return CompressYV12(icinfo);
   }
   else if(m_compressFormat == REDUCED)
   {
      return CompressReduced(icinfo);
   }

   return (DWORD)ICERR_BADFORMAT;
}

DWORD CodecInst::CompressYUY2(ICCOMPRESS* icinfo)
{
   const unsigned int mod		   = (m_SSE2 ? 15 : 7);
   const unsigned int hw         = HALF(m_width);
   const unsigned int pixels     = m_width * m_height;
   const unsigned int ay_stride  = ALIGN_ROUND(m_width, mod + 1);
   const unsigned int ac_stride  = ALIGN_ROUND(hw, mod + 1);
   const unsigned int ay_len     = ay_stride * m_height;
   const unsigned int ac_len     = ac_stride * m_height;
   const unsigned long len       = ay_len + DOUBLE(ac_len);

   BYTE frameType;
   BYTE* source = m_deltaBuffer;

   //TODO: Remove duplication.
   if(icinfo->dwFlags != ICCOMPRESS_KEYFRAME && icinfo->lFrameNum > 0) //TODO: Optimize
   {
      if(m_deltaframes)
      {
         unsigned long minDelta = len * 3;
         unsigned long bitCount = Fast_Sub_Count(m_deltaBuffer, m_in, m_prevFrame, len, minDelta);
         //unsigned long bitCount = Fast_XOR_Count(m_deltaBuffer, m_in, m_prevFrame, FOURTH(len), minDelta);

         if(bitCount == ULONG_MAX)
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
   BYTE* usrc  = ysrc + ay_len;
   BYTE* vsrc  = usrc + ac_len;

   if(!(hw & mod))
   {
      const unsigned int size = HALF(pixels);
      if(icinfo->lpbiInput->biCompression != FOURCC_UYVY)
      {
         for(unsigned int a = 0; a < size; a++)
         {
            const unsigned int a2 = DOUBLE(a);
            const unsigned int a4 = QUADRUPLE(a);

            ysrc[a2]    = source[a4    ];
            usrc[a]     = source[a4 + 1];
            ysrc[a2+1]  = source[a4 + 2];
            vsrc[a]     = source[a4 + 3];
         }
      } 
      else  // UYVY format
      {
         for(unsigned int a = 0; a < size; a++)
         {
            const unsigned int a2 = DOUBLE(a);
            const unsigned int a4 = QUADRUPLE(a);

            usrc[a]     = source[a4    ];
            ysrc[a2]    = source[a4 + 1];
            vsrc[a]     = source[a4 + 2];
            ysrc[a2+1]  = source[a4 + 3];
         }
      }
   } 
   else
   {
      const BYTE* src = source;
      BYTE* u = usrc;
      BYTE* v = vsrc;
      BYTE* y = ysrc;

      if(icinfo->lpbiInput->biCompression != FOURCC_UYVY)
      {
         for(unsigned int h = 0; h < m_height; h++)
         {
            unsigned int x = 0;
            for (; x < DOUBLE(m_width); x += 4)
            {
               y[0] = src[0];
               u[0] = src[1];
               y[1] = src[2];
               v[0] = src[3];
               y += 2;
               u++;
               v++;
               src += 4;
            }

            BYTE i, j, k;
            i = y[-1];
            j = u[-1];
            k = v[-1];

            for(unsigned int z = FOURTH(x); z < ac_stride; z++)
            {
               u[0] = j;
               v[0] = k;
               u++;
               v++;
            }
            for(x /= 2; x < ay_stride; x += 2)
            {
               y[0] = i;
               y[1] = i;
               y += 2;
            }
         }
      } 
      else // UYVY format
      {
         for(unsigned int h = 0; h < m_height; h++)
         {
            unsigned int x = 0;

            for(;x < DOUBLE(m_width); x += 4)
            {
               u[0] = src[0];
               y[0] = src[1];
               v[0] = src[2];
               y[1] = src[3];
               y += 2;
               u++;
               v++;
               src += 4;
            }

            BYTE i,j,k;
            i = y[-1];
            j = u[-1];
            k = v[-1];

            for(unsigned int z = FOURTH(x); z < ac_stride; z++)
            {
               u[0] = j;
               v[0] = k;
               u++;
               v++;
            }
            for(x /= 2; x < ay_stride; x += 2)
            {
               y[0] = i;
               y[1] = i;
               y += 2;
            }
         }
      }
   }

   BYTE* ydest = m_buffer2;
   BYTE* udest = ydest + ay_len;
   BYTE* vdest = udest + ac_len;

   size_t size;
   if (!m_multithreading)
   {
      if (m_SSE2)
      {
         SSE2_Predict_YUY2(ysrc, ydest, ay_stride, m_height, 1);
         SSE2_Predict_YUY2(usrc, udest, ac_stride, m_height, 0);
         SSE2_Predict_YUY2(vsrc, vdest, ac_stride, m_height, 0);
      } 
      else 
      {
         MMX_Predict_YUY2(ysrc, ydest, ay_stride, m_height, 1);
         MMX_Predict_YUY2(usrc, udest, ac_stride, m_height, 0);
         MMX_Predict_YUY2(vsrc, vdest, ac_stride, m_height, 0);
      }

      SWAP(ysrc, ydest);
      SWAP(usrc, udest);
      SWAP(vsrc, vdest);

      // remove alignment padding
      if (m_width & mod) // remove luminance padding 
      {
         for(unsigned int y = 0; y < m_height; y++)
         {
            memcpy(ydest + y * m_width, ysrc + y * ay_stride, m_width);
         }
         SWAP(ysrc, ydest);
      }
      if(hw & mod)  // remove chroma padding
      {
         unsigned int y;
         for(y = 0; y < m_height; y++)
         {
            memcpy(udest + HALF(y * m_width), usrc + y * ac_stride, hw);
         }
         for(y = 0; y < m_height; y++)
         {
            memcpy(vdest + HALF(y * m_width), vsrc + y * ac_stride, hw);
         }
         SWAP(usrc, udest);
         SWAP(vsrc, vdest);
      }

      size = m_compressWorker.Compact(ysrc, m_out + 9, pixels) + 9;
      *(UINT32*)(m_out + 1) = size;
      size += m_compressWorker.Compact(usrc, m_out + size, HALF(pixels));
      *(UINT32*)(m_out + 5) = size;
      size += m_compressWorker.Compact(vsrc, m_out + size, HALF(pixels));
   } 
   else
   { 
      m_info_a.m_source = ysrc;
      m_info_a.m_dest = m_out + 9;
      m_info_a.m_length = pixels;
      RESUME_THREAD(m_info_a.m_thread);
      m_info_b.m_source = usrc;
      m_info_b.m_dest = m_buffer2;
      m_info_b.m_length = HALF(pixels);
      RESUME_THREAD(m_info_b.m_thread);

      if(m_SSE2)
      {
         SSE2_Predict_YUY2(vsrc, vdest, ac_stride, m_height, 0);
      } 
      else 
      {
         MMX_Predict_YUY2(vsrc, vdest, ac_stride, m_height, 0);
      }


      vsrc  = ydest + ay_len + ac_len;
      vdest  = m_in + ay_len + ac_len;

      if(hw & mod)
      { // remove chroma padding
         for(unsigned int y = 0; y < m_height; y++)
         {
            memcpy(vdest + HALF(y * m_width), vsrc + y * ac_stride, hw);
         }
         SWAP(vsrc, vdest);
      }

      size = m_compressWorker.Compact(vsrc, m_deltaBuffer, HALF(pixels)); //TODO: change
      while(m_info_a.m_length)
      {
         Sleep(0);
      }

      int sizea = m_info_a.m_size;
      *(UINT32*)(m_out + 5) = sizea + 9;
      memcpy(m_out + sizea + 9, m_deltaBuffer, size);

      while(m_info_b.m_length)
      {
         Sleep(0);
      }

      int sizeb = m_info_b.m_size;
      *(UINT32*)(m_out + 1) = sizea + 9 + size;
      memcpy(m_out + sizea + size + 9, m_buffer2, sizeb);

      size += sizea + sizeb + 9;
   }

   m_out[0] = frameType;
   icinfo->lpbiOutput->biSizeImage = size;

   assert(*(__int64*)(m_out + *(UINT32*)(m_out + 1)) != 0);
   assert(*(__int64*)(m_out + *(UINT32*)(m_out + 5)) != 0);
   return ICERR_OK;
}

DWORD CodecInst::CompressYV12(ICCOMPRESS* icinfo)
{
   //TODO: Optimize for subsequent calls
   const unsigned int y_len	= m_width * m_height;
   const unsigned int c_len	= FOURTH(y_len);
   const unsigned int yu_len	= y_len + c_len;
   const unsigned int len		= yu_len + c_len;

   BYTE* source = m_deltaBuffer;
   BYTE frameType;

   //TODO: Remove duplication.
   if(icinfo->dwFlags != ICCOMPRESS_KEYFRAME && icinfo->lFrameNum > 0) //TODO: Optimize
   {
      if(m_deltaframes)
      {
         const unsigned long minDelta = len * 3;
         const unsigned long bitCount = Fast_Sub_Count(source, m_in, m_prevFrame, len, minDelta);
         //const unsigned long bitCount = Fast_XOR_Count(source, m_in, m_prevFrame, FOURTH(len), minDelta);

         if(bitCount == ULONG_MAX)
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

   //TODO: Optimize for subsequent calls
   const unsigned int mod = (m_SSE2 ? 16 : 8) - 1;

   const unsigned int hw = HALF(m_width);
   const unsigned int hh = HALF(m_height);

   const unsigned int y_stride = ALIGN_ROUND(m_width, mod + 1);
   const unsigned int c_stride = ALIGN_ROUND(hw, mod + 1);

   const unsigned int ay_len	= y_stride * m_height;
   const unsigned int ac_len	= HALF(c_stride * m_height);
   const unsigned int ayu_len	= ay_len + ac_len;

   const BYTE * ysrc;
   const BYTE * usrc;
   const BYTE * vsrc;

   const unsigned int in_aligned = !((int)m_in & mod);

   //note: chroma has only half the width of luminance, it may need to be aligned separately

   if(((hw) & mod) == 0)  // no alignment padding needed
   {
      if(in_aligned)  // no alignment needed
      {
         ysrc = source;
         usrc = source + y_len;
         vsrc = source + yu_len;
      } 
      else // data is naturally aligned,	input buffer is not
      {
         memcpy(m_buffer, source, len);
         ysrc = m_buffer;
         usrc = m_buffer + y_len;
         vsrc = m_buffer + yu_len;
      }
   } 
   else if((m_width & mod) == 0) // chroma needs alignment padding
   { 
      if (in_aligned)
      {
         ysrc = source;
      } 
      else 
      {
         memcpy(m_buffer, source, y_len);
         ysrc = m_buffer;
      }

      m_buffer += y_len; // step over luminance (not always needed...)

      for(unsigned int y = 0; y < DOUBLE(hh); y++) // TODO: Optimize?
      {
         memcpy(m_buffer + y * c_stride, source + y * hw + y_len, hw);
         BYTE val = m_buffer[y * c_stride + hw - 1];

         for(unsigned int x = hw; x < c_stride; x++)
         {
            m_buffer[y * c_stride + x] = val;
         }
      }

      usrc = m_buffer;
      vsrc = m_buffer + ac_len;

      m_buffer -= y_len;
   } 
   else  // both chroma and luminance need alignment padding
   {
      // align luminance
      unsigned int y;
      for(y = 0; y < m_height; y++)
      {
         memcpy(m_buffer + y * y_stride, source + y * m_width, m_width);
         BYTE val = m_buffer[y * y_stride + m_width - 1];

         for(unsigned int x = m_width; x < y_stride; x++)
         {
            m_buffer[y * y_stride + x]=val;
         }
      }

      ysrc = m_buffer;
      m_buffer += ay_len;

      for(y = 0; y < DOUBLE(hh); y++)//Optimize?
      {
         memcpy(m_buffer + y * c_stride, source + y * hw + y_len, hw);
         BYTE val = m_buffer[y * c_stride + hw - 1];

         for(unsigned int x = hw; x < c_stride; x++)
         {
            m_buffer[y * c_stride + x] = val;
         }
      }
      usrc = m_buffer;
      vsrc = m_buffer + c_stride * hh;

      m_buffer -= ay_len;
   }
   // done aligning, aligned data is in 'in' which may actually be prev or buffer 

   size_t size;
   if(!m_multithreading)
   {
      BYTE* buffer3 = (BYTE *)ALIGN_ROUND(m_out, 16);

      //set up dest buffers based on if alignment padding needs removal later
      BYTE* ydest;
      BYTE* udest;
      BYTE* vdest;

      ydest = (m_width & mod) ? buffer3 : m_buffer2; // TODO: needed?

      if (hw & mod)
      {
         udest = buffer3 + ay_len;
         vdest = buffer3 + ay_len + ac_len;
      }          
      else       
      {          
         udest = m_buffer2 + ay_len;
         vdest = m_buffer2 + ay_len + ac_len;
      }

      if(m_SSE2)
      {
         SSE2_BlockPredict(ysrc, ydest, y_stride, ay_len);
         SSE2_BlockPredict(usrc, udest, c_stride, ac_len);
         SSE2_BlockPredict(vsrc, vdest, c_stride, ac_len);
      } 
      else
      {
         MMX_BlockPredict(ysrc, ydest, y_stride, ay_len);
         MMX_BlockPredict(usrc, udest, c_stride, ac_len);
         MMX_BlockPredict(vsrc, vdest, c_stride, ac_len);
      }

      // remove alignment padding, all data will end up in buffer2
      if(m_width & mod) // remove luminance padding 
      {
         for(unsigned int y = 0; y < m_height; y++)
         {
            memcpy(m_buffer2 + y * m_width, ydest + y * y_stride, m_width);
         }
      }

      if(hw & mod)  // remove chroma padding
      {
         for(unsigned int y = 0 ; y< DOUBLE(hh); y++) // TODO: Optimize?
         {
            memcpy(m_buffer2 + y_len + y * hw, udest + y * c_stride, hw);
         }
      }

      size = 9;
      size += m_compressWorker.Compact(m_buffer2, m_out + size, y_len);
      *(UINT32*)(m_out + 1) = size;
      size += m_compressWorker.Compact(m_buffer2 + y_len, m_out + size, c_len);
      *(UINT32*)(m_out + 5) = size;
      size += m_compressWorker.Compact(m_buffer2 + yu_len, m_out + size , c_len);
   } 
   else 
   {

      m_info_a.m_source = ysrc;
      m_info_a.m_dest = m_out + 9;
      m_info_a.m_length = y_len;
      RESUME_THREAD(m_info_a.m_thread);
      m_info_b.m_source = usrc;
      m_info_b.m_dest = m_buffer2;
      m_info_b.m_length = c_len;
      RESUME_THREAD(m_info_b.m_thread);

      BYTE* vdest = m_buffer2 + ayu_len;

      if(m_SSE2)
      {
         SSE2_BlockPredict(vsrc, vdest, c_stride, ac_len);
      } 
      else 
      {
         MMX_BlockPredict(vsrc, vdest, c_stride, ac_len);
      }

      const bool keyframe = frameType & KEYFRAME;
      // remove alignment if needed
      if(hw & mod)  // remove chroma padding
      {
         vsrc = (keyframe) ? m_deltaBuffer : m_buffer;
         for(unsigned int y = 0; y < hh; y++)
         {
            memcpy((BYTE*)(int)vsrc + y * hw, vdest + y * c_stride, hw);
         }
      } 
      else
      {
         vsrc = vdest;
         vdest = (keyframe) ? m_deltaBuffer : m_buffer;
      }

      size = m_compressWorker.Compact(vsrc, vdest, c_len);

      while(m_info_a.m_length)
      {
         Sleep(0);
      }

      int sizea = m_info_a.m_size;
      *(UINT32*)(m_out + 5) = 9 + sizea;
      memcpy(m_out + sizea + 9, vdest, size);

      while(m_info_b.m_length)
      {
         Sleep(0);
      }

      int sizeb = m_info_b.m_size;
      *(UINT32*)(m_out + 1) = sizea + 9 + size;
      memcpy(m_out + sizea + 9 + size, m_buffer2, sizeb);

      size += sizea + sizeb + 9;
   }

   m_out[0] = frameType;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}

DWORD CodecInst::CompressReduced(ICCOMPRESS *icinfo)
{
   const unsigned int mod = (m_SSE2 ? 16 : 8);

   const unsigned int hh = HALF(m_height);
   const unsigned int hw = HALF(m_width);

   const unsigned int ry_stride = ALIGN_ROUND(hw, mod);
   const unsigned int rc_stride = ALIGN_ROUND(FOURTH(m_width), mod);
   const unsigned int ry_size   = ry_stride * hh;
   const unsigned int rc_size   = FOURTH(rc_stride * m_height);
   const unsigned int ryu_size  = ry_size + rc_size;
   const unsigned int y_size    = m_width * m_height;
   const unsigned int quarterSize = FOURTH(y_size);
   const unsigned int c_size    = quarterSize;
   const unsigned int yu_size   = y_size + c_size;
   const unsigned int ry_bytes  = quarterSize;
   const unsigned int rc_bytes  = FOURTH(quarterSize); 

   BYTE* ysrc = m_buffer;
   BYTE* usrc = m_buffer + ry_size;
   BYTE* vsrc = m_buffer + ryu_size;

   ReduceRes(m_in, ysrc, m_buffer2, m_width, m_height, m_SSE2);
   ReduceRes(m_in + y_size, usrc, m_buffer2, hw, hh, m_SSE2);
   ReduceRes(m_in + yu_size, vsrc, m_buffer2, hw, hh, m_SSE2);

   //TODO: Remove duplication.
   BYTE frameType;
   const unsigned int len = ry_size + DOUBLE(rc_size);
   if(icinfo->dwFlags != ICCOMPRESS_KEYFRAME && icinfo->lFrameNum > 0) //TODO: Optimize
   {
      if(m_deltaframes)
      {
         const unsigned long minDelta = len * 3;
         const unsigned long bitCount = Fast_Sub_Count(m_deltaBuffer, ysrc, m_prevFrame, len, minDelta);
         //const unsigned long bitCount = Fast_XOR_Count(m_deltaBuffer, ysrc, m_prevFrame, FOURTH(len), minDelta);

         if(bitCount == ULONG_MAX)
         {
            *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
            icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
            frameType = REDUCED_KEYFRAME;
         }
         else if(bitCount == 0 && m_nullframes)
         {
            icinfo->lpbiOutput->biSizeImage = 0;
            *icinfo->lpdwFlags = 0;
            return (DWORD)ICERR_OK;
         }
         else
         {
            ysrc = m_deltaBuffer;
            usrc = m_deltaBuffer + ry_size;
            vsrc = m_deltaBuffer + ryu_size;
            *icinfo->lpdwFlags |= AVIIF_LASTPART;
            frameType = REDUCED_DELTAFRAME;
         }
      }
      else
      {
         if(m_nullframes)
         {
            unsigned int pos = HALF(len) + 15;
            pos &= (~15);
            if(memcmp(ysrc + pos, m_prevFrame + pos, len - pos) == 0 && memcmp(ysrc, m_prevFrame, pos) == 0)
            {
               icinfo->lpbiOutput->biSizeImage = 0;
               *icinfo->lpdwFlags = 0;
               return (DWORD)ICERR_OK;
            }
         }

         *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
         icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
         frameType = REDUCED_KEYFRAME;
      }
   }
   else
   {
      *icinfo->lpdwFlags |= AVIIF_KEYFRAME;
      icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
      frameType = REDUCED_KEYFRAME;
   }

   if(m_nullframes || m_deltaframes)
   {
      memcpy(m_prevFrame, m_buffer, len);
   }

   size_t size;
   if(!m_multithreading)
   {
      BYTE* ydest = m_buffer2;
      BYTE* udest = m_buffer2 + ry_size;
      BYTE* vdest = m_buffer2 + ryu_size;

      if(m_SSE2)
      {
         SSE2_BlockPredict(ysrc, ydest, ry_stride, ry_size);
         SSE2_BlockPredict(usrc, udest, rc_stride, rc_size);
         SSE2_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      } 
      else
      {
         MMX_BlockPredict(ysrc, ydest, ry_stride, ry_size);
         MMX_BlockPredict(usrc, udest, rc_stride, rc_size);
         MMX_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      }

      if(hw % mod) // remove luminance padding
      {
         for(unsigned int y = 0; y < hh; y++)
         {
            memcpy(ysrc + y * hw, ydest + y * ry_stride, hw);
         }
      }
      else
      {
         ysrc = ydest;
      }

      unsigned int quarterWidth = FOURTH(m_width);
      unsigned int quarterHeight = FOURTH(m_height);

      if(rc_stride != quarterWidth)  // remove chroma padding
      {
         unsigned int y;
         for(y = 0; y < quarterHeight; y++) // TODO: Optimize
         {
            memcpy(usrc + y * quarterWidth, udest + y * rc_stride, quarterWidth);
         }
         for(y = 0; y < quarterHeight; y++)// TODO: Optimize
         {
            memcpy(vsrc + y * quarterWidth, vdest + y * rc_stride, quarterWidth);
         }
      } 
      else
      {
         usrc = udest;
         vsrc = vdest;
      }

      size = 9;
      size += m_compressWorker.Compact(ysrc, m_out + size, ry_bytes);
      *(UINT32*)(m_out + 1) = size;
      size += m_compressWorker.Compact(usrc, m_out + size, rc_bytes);
      *(UINT32*)(m_out + 5) = size;
      size += m_compressWorker.Compact(vsrc, m_out + size, rc_bytes);
   } 
   else
   {
      m_info_a.m_source = ysrc;
      m_info_a.m_dest = m_out + 9;
      m_info_a.m_length = ry_bytes;
      RESUME_THREAD(m_info_a.m_thread);

      m_info_b.m_source = usrc;
      m_info_b.m_dest = m_colorTransBuffer; //TODO: change
      m_info_b.m_length = rc_bytes;
      RESUME_THREAD(m_info_b.m_thread);

      BYTE *vdest = m_buffer2;

      if(m_SSE2)
      {
         SSE2_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      } 
      else
      {
         MMX_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      }

      // remove alignment if needed
      unsigned int quarterWidth = FOURTH(m_width);
      if(rc_stride != quarterWidth)  // remove chroma padding
      {
         const unsigned int quarterHeight = FOURTH(m_height);
         vsrc = vdest;
         vdest = m_buffer2 + rc_size;
         for (unsigned int y = 0; y< quarterHeight; y++)
         {
            memcpy(vdest + y * quarterWidth, vsrc + y * rc_stride, quarterWidth);
         }
      }
      vsrc = vdest;
      vdest = m_buffer2 + DOUBLE(rc_size);

      size = m_compressWorker.Compact(vsrc, vdest, rc_bytes);
      while(m_info_a.m_length)
      {
         Sleep(0);
      }

      int sizea = m_info_a.m_size;
      *(UINT32*)(m_out + 5) = 9 + sizea;
      memcpy(m_out + sizea + 9, vdest, size);

      while(m_info_b.m_length)
      {
         Sleep(0);
      }

      int sizeb = m_info_b.m_size;
      *(UINT32*)(m_out + 1)= sizea + 9 + size;
      memcpy(m_out + sizea + 9 + size, m_colorTransBuffer, sizeb);

      size += sizea + sizeb + 9;
   }

   m_out[0] = frameType;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}