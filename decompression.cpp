#include "compact.h"
#include "yeti.h"
#include "interface.h"
#include "prediction.h"
#include "resolution.h"
#include "threading.h"

#include "convert.h"
#include "convert_yv12.h"
#include "huffyuv_a.h"
#include "convert_xvid.h"

// initialize the codec for decompression
DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if(m_started)
   {
      DecompressEnd();
   }

   if(int error = DecompressQuery(lpbiIn, lpbiOut) != ICERR_OK)
   {
      return error;
   }

   int buffer_size;

   m_width = lpbiIn->biWidth;
   m_height = lpbiIn->biHeight;

   detectFlags(&m_SSE2, &m_SSE);

   m_format = lpbiOut->biBitCount;

   m_length = EIGHTH(m_width * m_height * m_format);


   if(m_format >= RGB24)
   {
      buffer_size = ALIGN_ROUND(QUADRUPLE(m_width), 8) * m_height + 2048;
   }
   else
   {
      buffer_size = m_length + 2048;
   }

   m_buffer = (unsigned char*)aligned_malloc(m_buffer, buffer_size, 16, "buffer");
   m_buffer2 = (unsigned char*)aligned_malloc(m_buffer2, buffer_size, 16, "buffer2");
   m_prevFrame = (unsigned char*)aligned_malloc(m_prevFrame, buffer_size, 16, "prev");
   ZeroMemory(m_prevFrame, buffer_size);

   if(!m_buffer || !m_buffer2 || !m_prevFrame)
   {
      return (DWORD)ICERR_MEMORY;
   }

   if(!m_cObj.InitCompressBuffers( m_width * m_height * 5/4 ))
   {
      return (DWORD)ICERR_MEMORY;
   }

   m_multithreading = GetPrivateProfileInt("settings", "multithreading", false, SettingsFile) > 0;
   if(m_multithreading)
   {
      int code = InitThreads(false);
      if (code != ICERR_OK)
      {
         return code;
      }
   }
   m_started = true;
   return ICERR_OK;
}

// release resources when decompression is done
DWORD CodecInst::DecompressEnd()
{
   if(m_started)
   {
      if(m_multithreading)
      {
         EndThreads();
      }

      ALIGNED_FREE(m_buffer,"buffer");
      ALIGNED_FREE(m_buffer2,"buffer2");
      ALIGNED_FREE(m_prevFrame, "prev");
      m_cObj.FreeCompressBuffers();
   }

   m_started = false;
   return ICERR_OK;
}

inline void CodecInst::InitDecompressionThreads(const unsigned char * in, unsigned char * out, unsigned int length, unsigned int width, unsigned int height, threadinfo * thread, int format, bool keyframe)
{
   if (m_multithreading && thread)
   {
      thread->m_source = in;
      thread->m_dest = out;
      thread->m_length = length;
      thread->m_width = width;
      thread->m_height = height;
      thread->m_format = format;
      thread->m_keyframe = keyframe;

      RESUME_THREAD(((threadinfo *)thread)->m_thread);
   } 
   else 
   {
      m_cObj.Uncompact(in, out, length);
   }
}

// Decompress a YV12 keyframe
void CodecInst::YV12Decompress(bool keyframe)
{
   unsigned char * dst = m_out;
   unsigned char * dst2 = m_buffer;

   if (m_format == YUY2)
   {
      dst = m_buffer;
      dst2 = m_out;
   }

   int size = *(unsigned int*)(m_pIn + 1);
   unsigned int hh = HALF(m_height);
   unsigned int hw = HALF(m_width);
   unsigned int wxh = m_width * m_height;
   unsigned int quarterArea = FOURTH(wxh);

   InitDecompressionThreads(m_pIn + 9, dst, wxh, m_width, m_height, &m_info_a, YV12, keyframe);
   InitDecompressionThreads(m_pIn + size, dst + wxh, quarterArea, hw, hh, &m_info_b, YV12, keyframe);

   size = *(unsigned int*)(m_pIn + 5);
   InitDecompressionThreads(m_pIn + size, dst + wxh + quarterArea, quarterArea, hw, hh, NULL, YV12, keyframe);

   //if(keyframe)
   //{
      ASM_BlockRestore(dst + wxh + quarterArea, hw, quarterArea, 0);
   //}

   WAIT_FOR_THREADS(2);

   if ( /*keyframe &&*/ !m_multithreading )
   {
      ASM_BlockRestore(dst, m_width, wxh, 0);
      ASM_BlockRestore(dst + wxh, hw, quarterArea, 0);
   }

   unsigned int length = EIGHTH(wxh * YV12);

   if(!keyframe)
   {
      Fast_XOR(dst, dst, m_prevFrame, length);
   }
   //else
   //{
   //   MessageBox (HWND_DESKTOP, "Keyframe", "info", MB_OK);
   //}

   memcpy(m_prevFrame, dst, length);

   if(m_format == YV12)
   {
      return;
   }

   //upsample if needed
   isse_yv12_to_yuy2(dst, dst + wxh + quarterArea, dst + wxh, m_width, m_width, hw, dst2, DOUBLE(m_width), m_height); 

   if(m_format == YUY2)
   {
      return;
   }

   // upsample to RGB
   if(m_format == RGB32)
   {
      mmx_YUY2toRGB32(dst2, m_out, dst2 + DOUBLE(wxh), DOUBLE(m_width));
   } 
   else 
   {
      mmx_YUY2toRGB24(dst2, m_out, dst2 + DOUBLE(wxh), DOUBLE(m_width));
   }
}

void CodecInst::ReduceResDecompress()
{
   m_width = HALF(m_width);
   m_height = HALF(m_height);

   int size = *(unsigned int*)(m_pIn + 1);
   unsigned int hh = HALF(m_height);
   unsigned int hw = HALF(m_width);
   unsigned int wxh = m_width * m_height;
   unsigned int quarterSize = FOURTH(wxh);

   unsigned char * dest = (m_format == YV12) ? m_buffer : m_out;

   InitDecompressionThreads(m_pIn + 9, dest, wxh, m_width, m_height, &m_info_a, YV12, true);
   InitDecompressionThreads(m_pIn + size, dest + wxh, quarterSize, hw, hh, &m_info_b, YV12, true);
   size = *(unsigned int*)(m_pIn + 5);
   InitDecompressionThreads(m_pIn + size, dest + wxh + quarterSize, quarterSize, hw, hh, NULL, YV12, true);

   ASM_BlockRestore(dest + m_width * m_height + quarterSize, hw, quarterSize, 0);

   WAIT_FOR_THREADS(2);

   if(!m_multithreading)
   {
      ASM_BlockRestore(dest, m_width, wxh, 0);
      ASM_BlockRestore(dest + wxh, hw, quarterSize, 0);
   }

   m_width = DOUBLE(m_width);
   m_height = DOUBLE(m_height);
   wxh = m_width * m_height;

   unsigned char* source = dest;
   dest = (m_format == YV12) ? m_out: m_buffer;

   unsigned char* ysrc = source;
   unsigned char* usrc = ysrc + FOURTH(wxh);
   unsigned char* vsrc = usrc + FOURTH(FOURTH(wxh));
   unsigned char* ydest = dest;
   unsigned char* udest = ydest + wxh;
   unsigned char* vdest = udest + FOURTH(wxh);

   Enlarge_Res(ysrc, ydest, m_buffer2, HALF(m_width), HALF(m_height), m_SSE2);
   Enlarge_Res(usrc, udest, m_buffer2, FOURTH(m_width), FOURTH(m_height), m_SSE2);
   Enlarge_Res(vsrc, vdest, m_buffer2, FOURTH(m_width), FOURTH(m_height), m_SSE2);

   ysrc = ydest;
   usrc = udest;
   vsrc = vdest;

   if(m_format == RGB24)
   {
      yv12_to_rgb24_mmx(m_out, m_width, ysrc, vsrc, usrc, m_width, HALF(m_width), m_width, -(int)m_height);
   } 
   else if(m_format == RGB32)
   {
      yv12_to_rgb32_mmx(m_out, m_width, ysrc, vsrc, usrc, m_width, HALF(m_width), m_width, -(int)m_height);
   } 
   else if(m_format == YUY2)
   {
      yv12_to_yuyv_mmx(m_out, m_width, ysrc, vsrc ,usrc, m_width, HALF(m_width), m_width, m_height);
   }
}

// Called to decompress a frame, the actual decompression will be
// handed off to other functions based on the frame type.
DWORD CodecInst::Decompress(ICDECOMPRESS* icinfo, DWORD dwSize) 
{
#ifdef _DEBUG
   try 
   {
#endif
      DWORD return_code = ICERR_OK;

      if(!m_started)
      {
         DecompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
      }

      m_out = (unsigned char *)icinfo->lpOutput;
      m_pIn  = (unsigned char *)icinfo->lpInput; 
      icinfo->lpbiOutput->biSizeImage = m_length;

      // according to the avi specs, the calling application is responsible for handling null frames.
      if(icinfo->lpbiInput->biSizeImage == 0)
      {
         return ICERR_OK;
      }

      switch(m_pIn[0])
      {
      case YV12_DELTAFRAME:
      case YV12_KEYFRAME:
         {
            YV12Decompress( m_pIn[0] & KEYFRAME );
            break;
         }
      case REDUCED_DELTAFRAME:
      case REDUCED_KEYFRAME:
         {
            ReduceResDecompress();
            break;
         }
      default:
         {
#ifdef _DEBUG
            char emsg[128];
            sprintf_s(emsg,128,"Unrecognized frame type: %d",m_pIn[0]);
            MessageBox (HWND_DESKTOP, emsg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
            return_code = (DWORD)ICERR_ERROR;
            break;
         }
      }

      return return_code;
#ifdef _DEBUG
   } 
   catch( ... )
   {
      MessageBox (HWND_DESKTOP, "Exception caught in decompress main", "Error", MB_OK | MB_ICONEXCLAMATION);
      return (DWORD)ICERR_INTERNAL;
   }
#endif
}