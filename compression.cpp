#include "yeti.h"
#include "prediction.h"
#include "threading.h"
#include <float.h>

#include "huffyuv_a.h"
#include "convert_yuy2.h"
#include "convert_yv12.h"

// initialize the codec for compression
DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if ( m_started )
   {
      CompressEnd();
   }

   m_nullframes = GetPrivateProfileInt("settings", "nullframes", false, SettingsFile)>0;
   m_multithreading = GetPrivateProfileInt("settings", "multithreading", false, SettingsFile)>0;
   m_reduced = GetPrivateProfileInt("settings", "reduced", false, SettingsFile)>0;

   if ( int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK )
   {
      return error;
   }

   m_width = lpbiIn->biWidth;
   m_height = lpbiIn->biHeight;

   m_format = lpbiIn->biBitCount;
   m_length = EIGHTH(m_width * m_height * m_format);

   unsigned int buffer_size;
   if ( m_format < RGB24 )
   {
      buffer_size = EIGHTH(ALIGN_ROUND(m_width,32) * m_height * m_format) + 1024;
   } 
   else
   {
      buffer_size = QUADRUPLE(ALIGN_ROUND(m_width, 16) * m_height) + 1024;
   }

   m_pBuffer = (unsigned char *)aligned_malloc(m_pBuffer, buffer_size,16,"buffer");
   m_pBuffer2 = (unsigned char *)aligned_malloc(m_pBuffer2, buffer_size,16,"buffer2");
   m_pDelta = (unsigned char *)aligned_malloc(m_pDelta, buffer_size,16,"delta");
   m_pLossy_buffer = (unsigned char *)aligned_malloc(m_pLossy_buffer, buffer_size,16,"lossy");
   m_pPrev = (unsigned char *)aligned_malloc(m_pPrev, buffer_size,16,"prev");
   ZeroMemory(m_pPrev, buffer_size);

   if ( !m_pBuffer || !m_pBuffer2 || !m_pPrev || !m_pDelta || !m_pLossy_buffer)
   {
      ALIGNED_FREE(m_pBuffer,"buffer");
      ALIGNED_FREE(m_pBuffer2,"buffer2");
      ALIGNED_FREE(m_pDelta,"delta");
      ALIGNED_FREE(m_pLossy_buffer,"lossy");
      ALIGNED_FREE(m_pPrev,"prev");
      return (DWORD)ICERR_MEMORY;
   }

   int cb_size=m_width*m_height*5/4;

   if ( !m_cObj.InitCompressBuffers( cb_size ) )
   {
      return (DWORD)ICERR_MEMORY;
   }

   if ( m_multithreading )
   {
      int code = InitThreads( true );
      if ( code != ICERR_OK )
      {
         return code;
      }
   }

   m_started = true;

   return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 4KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   return (DWORD)( ALIGN_ROUND(lpbiIn->biWidth,16)*lpbiIn->biHeight*lpbiIn->biBitCount/8*1.05 + 4096); // TODO: Optimize
}

// release resources when compression is done

DWORD CodecInst::CompressEnd()
{
   if ( m_started )
   {
      if ( m_multithreading )
      {
         EndThreads();
      }

      ALIGNED_FREE( m_pBuffer, "buffer");
      ALIGNED_FREE( m_pBuffer2, "buffer2");
      ALIGNED_FREE( m_pDelta, "delta");
      ALIGNED_FREE( m_pPrev, "prev");
      ALIGNED_FREE( m_pLossy_buffer, "lossy_buffer");
      m_cObj.FreeCompressBuffers();
   }
   m_started = false;
   return ICERR_OK;
}

// Compress a YV12 keyframe
DWORD CodecInst::CompressYV12(ICCOMPRESS* icinfo)
{
   //TODO: Optimize for subsequent calls
   const unsigned int y_len	= m_width*m_height;
   const unsigned int c_len	= FOURTH(y_len);
   const unsigned int yu_len	= y_len+c_len;
   const unsigned int len		= yu_len+c_len;

   unsigned char* source;

   if(icinfo->lFrameNum > 0) //TODO: Optimize
   {
      if ( m_nullframes )
      {
         // compare in two parts, video is more likely to change in middle than at bottom
         unsigned int pos = HALF(len)+15;
         pos&=(~15);
         if (!memcmp(m_pIn+pos,m_pPrev+pos,len-pos) && !memcmp(m_pIn,m_pPrev,pos) )
         {
            icinfo->lpbiOutput->biSizeImage = 0;
            *icinfo->lpdwFlags = 0;
            return (DWORD)ICERR_OK;
         }
      }

      Fast_XOR(m_pDelta, m_pIn, m_pPrev, len);
   }
   else
   {
      memcpy(m_pDelta, m_pIn, len); //TODO Optimize
   }

   source = m_pDelta;

   //TODO: Optimize for subsequent calls
   const unsigned int mod = (m_SSE2?16:8) - 1;

   const unsigned int hw = HALF(m_width);
   const unsigned int hh = HALF(m_height);

   const unsigned int y_stride	= ALIGN_ROUND(m_width,mod+1);
   const unsigned int c_stride	= ALIGN_ROUND(hw,mod+1);

   const unsigned int ay_len	= y_stride*m_height;
   const unsigned int ac_len	= HALF(c_stride*m_height);
   const unsigned int ayu_len	= ay_len+ac_len;

   const unsigned char * ysrc;
   const unsigned char * usrc;
   const unsigned char * vsrc;

   const unsigned int in_aligned = !( (int)m_pIn & mod);
  

   //note: chroma has only half the width of luminance, it may need to be aligned separately

   if ( ((hw) & mod) == 0 )  // no alignment padding needed
   {
      if ( in_aligned )  // no alignment needed
      {
         ysrc = source;
         usrc = source+y_len;
         vsrc = source+yu_len;
      } 
      else // data is naturally aligned,	input buffer is not
      {
         memcpy(m_pBuffer, source, len);
         ysrc = m_pBuffer;
         usrc = m_pBuffer+y_len;
         vsrc = m_pBuffer+yu_len;
      }
   } 
   else if ( (m_width & mod) == 0 ) // chroma needs alignment padding
   { 
      if ( in_aligned )
      {
         ysrc = source;
      } 
      else 
      {
         memcpy(m_pBuffer,source,y_len);
         ysrc = m_pBuffer;
      }

      m_pBuffer += y_len; // step over luminance (not always needed...)

      for ( unsigned int y=0; y < DOUBLE(hh); y++) // TODO: Optimize?
      {
         memcpy(m_pBuffer + y * c_stride, source + y * hw + y_len, hw);
         unsigned char val = m_pBuffer[y * c_stride + hw - 1];

         for (unsigned int x = hw; x < c_stride; x++)
         {
            m_pBuffer[y * c_stride + x] = val;
         }
      }

      usrc = m_pBuffer;
      vsrc = m_pBuffer+ac_len;

      m_pBuffer -= y_len;
   } 
   else  // both chroma and luminance need alignment padding
   {
      // align luminance
      unsigned int y;
      for ( y = 0; y < m_height; y++)
      {
         memcpy(m_pBuffer + y * y_stride, source + y * m_width, m_width);
         unsigned char val = m_pBuffer[y * y_stride + m_width - 1];

         for (unsigned int x = m_width; x < y_stride; x++)
         {
            m_pBuffer[y * y_stride + x]=val;
         }
      }

      ysrc = m_pBuffer;
      m_pBuffer += ay_len;

      for ( y = 0; y < DOUBLE(hh); y++)//Optimize?
      {
         memcpy(m_pBuffer + y * c_stride,source + y * hw + y_len, hw);
         unsigned char val = m_pBuffer[y * c_stride + hw - 1];

         for ( unsigned int x = hw; x < c_stride; x++)
         {
            m_pBuffer[y * c_stride + x] = val;
         }
      }
      usrc = m_pBuffer;
      vsrc = m_pBuffer+c_stride*hh;

      m_pBuffer -= ay_len;
   }
   // done aligning, aligned data is in 'in' which may actually be prev or buffer 

   int size;
   if ( !m_multithreading )
   {
      unsigned char *buffer3 = (unsigned char *)ALIGN_ROUND(m_pOut,16);

      //set up dest buffers based on if alignment padding needs removal later
      unsigned char * ydest;
      unsigned char * udest;
      unsigned char * vdest;

      ydest = (m_width & mod) ? buffer3 : m_pBuffer2; // TODO: needed?


      if (hw & mod)
      {
         udest=buffer3 + ay_len;
         vdest=buffer3 + ay_len + ac_len;
      } 
      else 
      {
         udest=m_pBuffer2 + ay_len;
         vdest=m_pBuffer2 + ay_len + ac_len;
      }

      // perform prediction
      if ( m_SSE2 )
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

      memcpy(m_pBuffer2, m_pBuffer, len);

      // remove alignment padding, all data will end up in buffer2
      if (m_width & mod) // remove luminance padding 
      {
         for (unsigned int y = 0; y < m_height; y++)
         {
            memcpy(m_pBuffer2 + y * m_width, ydest + y * y_stride, m_width);
         }
      }

      if ( hw & mod )  // remove chroma padding
      {
         for ( unsigned int y = 0 ; y< DOUBLE(hh); y++) // TODO: Optimize?
         {
            memcpy(m_pBuffer2 + y_len + y * hw, udest + y * c_stride, hw);
         }
      }

      size = 9;
      size += m_cObj.compact(m_pBuffer2, m_pOut + size, y_len);
      *(UINT32*)(m_pOut + 1) = size;
      size += m_cObj.compact(m_pBuffer2 + y_len, m_pOut + size, c_len);
      *(UINT32*)(m_pOut + 5) = size;
      size += m_cObj.compact(m_pBuffer2 + yu_len, m_pOut + size , c_len);
   } 
   else 
   {
      m_info_a.m_pSource = ysrc;
      m_info_a.m_pDest = m_pOut+9;
      m_info_a.m_length = y_len;
      RESUME_THREAD(m_info_a.m_thread);
      m_info_b.m_pSource = usrc;
      m_info_b.m_pDest = m_pBuffer2;
      m_info_b.m_length = c_len;
      RESUME_THREAD(m_info_b.m_thread);

      unsigned char *vdest = m_pBuffer2 + ayu_len;

      if (m_SSE2)
      {
         SSE2_BlockPredict(vsrc, vdest, c_stride, ac_len);
      } 
      else 
      {
         MMX_BlockPredict(vsrc, vdest, c_stride, ac_len);
      }

      // remove alignment if needed
      if (hw & mod)  // remove chroma padding
      {
         for (unsigned int y = 0; y < hh; y++)
         {
            memcpy((unsigned char*)(int)vsrc + y * hw, vdest + y * c_stride, hw);
         }
      } 
      else
      {
         unsigned char * temp = (unsigned char*)(int)vsrc;
         vsrc = vdest;
         vdest = temp;
      }

      size = m_cObj.compact(vsrc, vdest, c_len);

      while (m_info_a.m_length)
      {
         Sleep(0);
      }

      int sizea = m_info_a.m_size;
      *(UINT32*)(m_pOut + 5)= 9 + sizea;
      memcpy(m_pOut + sizea + 9, vdest, size);

      while ( m_info_b.m_length )
      {
         Sleep(0);
      }

      int sizeb = m_info_b.m_size;
      *(UINT32*)(m_pOut + 1 ) = sizea + 9 + size;
      memcpy(m_pOut + sizea + 9 + size, m_pBuffer2, sizeb);

      size += sizea + sizeb + 9;
   }

   m_pOut[0] = YV12_FRAME;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}

DWORD CodecInst::CompressReduced(ICCOMPRESS *icinfo)
{
   const unsigned int mod = (m_SSE2?16:8);

   const unsigned int hh = HALF(m_height);
   const unsigned int hw = HALF(m_width);

   const unsigned int ry_stride = ALIGN_ROUND(hw, mod);
   const unsigned int rc_stride = ALIGN_ROUND(FOURTH(m_width), mod);
   const unsigned int ry_size   = ry_stride*hh;
   const unsigned int rc_size   = FOURTH(rc_stride * m_height);
   const unsigned int ryu_size  = ry_size+rc_size;
   const unsigned int y_size    = m_width*m_height;
   const unsigned int quarterSize = FOURTH(y_size);
   const unsigned int c_size    = quarterSize;
   const unsigned int yu_size   = y_size+c_size;
   const unsigned int ry_bytes  = quarterSize;
   const unsigned int rc_bytes  = FOURTH(quarterSize); 

   unsigned char * ysrc = m_pBuffer;
   unsigned char * usrc = m_pBuffer + ry_size;
   unsigned char * vsrc = m_pBuffer + ryu_size;

   reduce_res(m_pIn, ysrc, m_pBuffer2, m_width, m_height, m_SSE2);
   reduce_res(m_pIn + y_size, usrc, m_pBuffer2, hw, hh, m_SSE2);
   reduce_res(m_pIn + yu_size, vsrc, m_pBuffer2, hw, hh, m_SSE2);

   unsigned int size;

   if ( !m_multithreading )
   {
      unsigned char * ydest = m_pBuffer2;
      unsigned char * udest = m_pBuffer2 + ry_size;
      unsigned char * vdest = m_pBuffer2 + ryu_size;

      if ( m_SSE2 )
      {
         SSE2_BlockPredict(ysrc,ydest,ry_stride,ry_size);
         SSE2_BlockPredict(usrc,udest,rc_stride,rc_size);
         SSE2_BlockPredict(vsrc,vdest,rc_stride,rc_size);
      } 
      else
      {
         MMX_BlockPredict(ysrc,ydest,ry_stride,ry_size);
         MMX_BlockPredict(usrc,udest,rc_stride,rc_size);
         MMX_BlockPredict(vsrc,vdest,rc_stride,rc_size);
      }

      if ( hw % mod ) // remove luminance padding
      {
         for ( unsigned int y = 0; y < hh; y++)
         {
            memcpy(ysrc + y * hw, ydest + y * ry_stride, hw);
         }
      }
      else
      {
         ysrc = ydest;
      }

      unsigned int quarterWidth = FOURTH( m_width );
      unsigned int quarterHeight = FOURTH( m_height );

      if ( rc_stride != quarterWidth )  // remove chroma padding
      {
         unsigned int y;
         for (y = 0; y < quarterHeight; y++) // TODO: Optimize
         {
            memcpy(usrc + y * quarterWidth, udest + y * rc_stride, quarterWidth);
         }
         for (y = 0; y < quarterHeight; y++)// TODO: Optimize
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
      size += m_cObj.compact(ysrc, m_pOut + size, ry_bytes);
      *(UINT32*)(m_pOut+1)=size;
      size += m_cObj.compact(usrc, m_pOut + size, rc_bytes);
      *(UINT32*)(m_pOut+5)=size;
      size += m_cObj.compact(vsrc, m_pOut + size, rc_bytes);
   } 
   else
   {
      m_info_a.m_pSource = ysrc;
      m_info_a.m_pDest = m_pOut + 9;
      m_info_a.m_length = ry_bytes;
      RESUME_THREAD(m_info_a.m_thread);

      m_info_b.m_pSource = usrc;
      m_info_b.m_pDest = m_pPrev;
      m_info_b.m_length = rc_bytes;
      RESUME_THREAD(m_info_b.m_thread);

      unsigned char *vdest = m_pBuffer2;

      if ( m_SSE2 )
      {
         SSE2_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      } 
      else
      {
         MMX_BlockPredict(vsrc, vdest, rc_stride, rc_size);
      }

      // remove alignment if needed
      unsigned int quarterWidth = FOURTH( m_width );
      if ( rc_stride != quarterWidth )  // remove chroma padding
      {
         unsigned int quarterHeight = FOURTH( m_height );
         vsrc = vdest;
         vdest = m_pBuffer2 + rc_size;
         for (unsigned int y = 0; y< quarterHeight; y++)
         {
            memcpy(vdest + y * quarterWidth, vsrc + y * rc_stride, quarterWidth);
         }
      }
      vsrc = vdest;
      vdest = m_pBuffer2 + DOUBLE(rc_size);

      size=m_cObj.compact(vsrc, vdest, rc_bytes);
      while ( m_info_a.m_length )
      {
         Sleep(0);
      }

      int sizea = m_info_a.m_size;
      *(UINT32*)(m_pOut+5) = 9 + sizea;
      memcpy(m_pOut+sizea + 9, vdest, size);

      while ( m_info_b.m_length )
      {
         Sleep(0);
      }

      int sizeb = m_info_b.m_size;
      *(UINT32*)(m_pOut+1)= sizea + 9 + size;
      memcpy(m_pOut + sizea + 9 + size, m_pPrev, sizeb);

      size += sizea + sizeb + 9;
   }

   m_pOut[0] = REDUCED_RES;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}

// This downsamples the colorspace if the set encoding colorspace is lower than the
// colorspace of the input video
DWORD CodecInst::CompressLossy(ICCOMPRESS * icinfo )
{
   //unsigned char * lossy_buffer=_pPrev;

   //const unsigned char * const stored_in=_pIn;
   const unsigned char * src=0;
   DWORD ret_val=0;

   if ( m_format >= RGB24 )
   {
      if ( m_format == RGB24 )
      {
         mmx_ConvertRGB24toYUY2(m_pIn,m_pBuffer,m_width*3,DOUBLE(m_width),m_width,m_height);
      } 
      else if ( m_format == RGB32 )
      {
         mmx_ConvertRGB32toYUY2((const unsigned int *)m_pIn,(unsigned int *)m_pBuffer,m_width,HALF(m_width),m_width,m_height);
      }
      src = m_pBuffer;
   }
   else
   {
      src = m_pIn;
   }

   unsigned int dw = DOUBLE(m_width);
   unsigned char * dst2=m_pLossy_buffer;
   unsigned int yuy2_pitch = ALIGN_ROUND(dw,16);
   unsigned int y_pitch = ALIGN_ROUND(m_width,8);
   unsigned int uv_pitch = ALIGN_ROUND(HALF(m_width),8);

   bool is_aligned = (m_width%16)==0;
   if ( !is_aligned )
   {
      for (unsigned int h = 0; h < m_height; h++)
      {
         memcpy(dst2 + yuy2_pitch * h, src + dw * h, dw);
      }

      src = dst2;
      dst2 = m_pBuffer;
   }
   
   isse_yuy2_to_yv12(src, dw, yuy2_pitch, dst2, dst2+y_pitch*m_height+ HALF(uv_pitch * m_height), dst2+y_pitch*m_height, y_pitch,uv_pitch, m_height);

   unsigned char * dest = m_pLossy_buffer;
   if ( !is_aligned )
   {
      unsigned int h;

      for ( h=0;h < m_height;h++)
      {
         memcpy(dest + m_width * h, dst2 + y_pitch * h, m_width);
      }

      dst2 += y_pitch * m_height;
      dest += m_width * m_height;

      unsigned int hw = HALF(m_width);
      for ( h=0; h < m_height; h++)
      {
         memcpy(dest + hw * h, dst2 + uv_pitch * h, hw);
      }
   }

   m_pIn = m_pLossy_buffer;

   if ( m_reduced ) //TODO:Optimize
   {
      ret_val = CompressReduced(icinfo);
      if (ret_val == ICERR_OK )
      {
         *icinfo->lpdwFlags = AVIIF_KEYFRAME;
      }
   } 
   else
   {
      ret_val = CompressYV12(icinfo);
      if( ret_val == ICERR_OK )
      {
         if ( icinfo->dwFlags == 1 /*AVIIF_LIST*/ ) // TODO: Optimize
         {
            *icinfo->lpdwFlags = AVIIF_KEYFRAME;
         }
         else
         {
            *icinfo->lpdwFlags = AVIIF_LASTPART;
         }
      }
   }
   //_pIn=stored_in;
   return ret_val;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings

DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize)
{
   // TMPGEnc (and probably some other programs) like to change the floating point
   // precision. This can occasionally produce rounding errors when scaling the probability
   // ranges to a power of 2, which in turn produces corrupted video. Here the code checks 
   // the FPU control word and sets the precision correctly if needed.
   unsigned int fpuword = _controlfp(0,0);
   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC) )
   {
      _controlfp(_PC_53 | _RC_NEAR,_MCW_PC | _MCW_RC);
#ifdef _DEBUG
      static bool firsttime = true;
      if ( firsttime )
      {
         firsttime = false;
         MessageBox (HWND_DESKTOP, "Floating point control word is not set correctly, correcting it", "Error", MB_OK | MB_ICONEXCLAMATION);
      }
#endif
   }

   m_pOut = (unsigned char *)icinfo->lpOutput;
   m_pIn  = (unsigned char *)icinfo->lpInput;

   if ( icinfo->lFrameNum == 0 )
   {
      if ( !m_started )
      {
         if ( int error = CompressBegin(icinfo->lpbiInput,icinfo->lpbiOutput) != ICERR_OK )
            return error;
      }
   } 
   /*else if ( _nullframes ){
     // compare in two parts, video is more likely to change in middle than at bottom
     unsigned int pos = _length/2+15;
     pos&=(~15);
     if (!memcmp(_pIn+pos,_pPrev+pos,_length-pos) && !memcmp(_pIn,_pPrev,pos) ){
     icinfo->lpbiOutput->biSizeImage =0;
     *icinfo->lpdwFlags = 0;
     return ICERR_OK;
     }
     }*/
   if ( icinfo->lpckid )
   {
      *icinfo->lpckid = 'cd';
   }

   int ret_val = CompressLossy(icinfo);

   //if ( _nullframes ){
    memcpy( m_pPrev, m_pIn, m_length); // TODO: Optimize
   //}

   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC))
   {
      _controlfp(fpuword,_MCW_PC | _MCW_RC);
   }

   return (DWORD)ret_val;
}