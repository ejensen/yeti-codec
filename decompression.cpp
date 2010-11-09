#include "compact.h"
#include "yeti.h"
#include "interface.h"
#include "prediction.h"
#include "threading.h"
#include "convert.h"
#include "convert_yv12.h"
#include <float.h>
#include "huffyuv_a.h"
#include "convert_xvid.h"

// initialize the codec for decompression
DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if ( m_started )
   {
      DecompressEnd();
   }
   m_started = false;

   if ( int error = DecompressQuery(lpbiIn,lpbiOut) != ICERR_OK )
   {
      return error;
   }

   int buffer_size;

   m_width = lpbiIn->biWidth;
   m_height = lpbiIn->biHeight;

   detectFlags(&m_SSE2, &m_SSE);

   m_format = lpbiOut->biBitCount;

   m_length = EIGHTH(m_width * m_height * m_format);

   buffer_size = m_length + 2048;
   if ( m_format >= RGB24 )
   {
      buffer_size=ALIGN_ROUND(QUADRUPLE(m_width), 8) * m_height + 2048;
   }

   m_pBuffer = (unsigned char *)aligned_malloc( m_pBuffer, buffer_size, 16, "buffer");
   m_pPrev = (unsigned char *)aligned_malloc( m_pPrev, buffer_size, 16, "prev");
   ZeroMemory(m_pPrev, buffer_size);

   if ( !m_pBuffer || !m_pPrev )
   {
      return (DWORD)ICERR_MEMORY;
   }

   if ( !m_cObj.InitCompressBuffers( m_width * m_height * 5/4 ) )
   {
      return (DWORD)ICERR_MEMORY;
   }

   m_multithreading = GetPrivateProfileInt("settings", "multithreading", false, SettingsFile) > 0;
   if ( m_multithreading )
   {
      int code = InitThreads( false);
      if ( code != ICERR_OK )
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
   if ( m_started )
   {
      if ( m_multithreading )
      {
         EndThreads();
      }

      ALIGNED_FREE( m_pBuffer,"buffer");
      ALIGNED_FREE( m_pPrev, "prev");
      m_cObj.FreeCompressBuffers();
   }
   m_started=false;
   return ICERR_OK;
}

inline void CodecInst::uncompact_macro( const unsigned char * in, unsigned char * out, unsigned int length, unsigned int width, unsigned int height, threadinfo * thread, int format)
{
   if ( m_multithreading && thread )
   {
      thread->m_pSource = (unsigned char *)in;
      thread->m_pDest = out;
      thread->m_length = length;
      thread->m_width = width;
      thread->m_height = height;
      thread->m_format = format; 

      RESUME_THREAD(((threadinfo *)thread)->m_thread);
   } 
   else 
   {
      m_cObj.uncompact(in, out, length);
   }
}

// Decompress a YV12 keyframe
void CodecInst::YV12Decompress()
{
   unsigned char * dst = _pOut;
   unsigned char * dst2 = m_pBuffer;

   if ( m_format == YUY2 )
   {
      dst = m_pBuffer;
      dst2 = _pOut;
   }

   int size = *(unsigned int*)(m_pIn + 1);
   unsigned int hh = HALF(m_height);
   unsigned int hw = HALF(m_width);
   unsigned int wxh = m_width * m_height;
   unsigned int quarterArea = FOURTH(wxh);

   uncompact_macro(m_pIn + 9, dst, wxh, m_width, m_height, &m_info_a, YV12);
   uncompact_macro(m_pIn + size, dst + wxh, quarterArea, hw, hh, &m_info_b, YV12);

   size = *(unsigned int*)(m_pIn + 5);
   uncompact_macro(m_pIn + size, dst + wxh + quarterArea, quarterArea, hw, hh, NULL, YV12);

   ASM_BlockRestore(dst + wxh + quarterArea, hw, quarterArea, 0);
   WAIT_FOR_THREADS(2);

   if ( !m_multithreading )
   {
      ASM_BlockRestore(dst, m_width, wxh, 0);
      ASM_BlockRestore(dst + wxh, hw, quarterArea, 0);
   }

   unsigned int length = EIGHTH(wxh * YV12);

   Fast_XOR(dst, dst, m_pPrev, length);

   memcpy( m_pPrev, dst, length);

   if ( m_format == YV12 )
   {
      return;
   }

   //upsample if needed
   if ( m_SSE2 )
   {
      isse_yv12_to_yuy2(dst, dst + wxh + quarterArea, dst + wxh, m_width, m_width, hw, dst2, DOUBLE(m_width), m_height);
   } 
   else 
   {
      mmx_yv12_to_yuy2(dst, dst + wxh + quarterArea, dst + wxh, m_width, m_width, hw, dst2, DOUBLE(m_width), m_height);
   }

   if ( m_format == YUY2 )
   {
      return;
   }

   // upsample to RGB
   if ( m_format == RGB32 )
   {
      mmx_YUY2toRGB32(dst2, _pOut, dst2 + DOUBLE(wxh), DOUBLE(m_width));
   } 
   else 
   {
      mmx_YUY2toRGB24(dst2, _pOut, dst2 + DOUBLE(wxh), DOUBLE(m_width));
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

   unsigned char * dest = (m_format == YV12) ? m_pBuffer : _pOut;

   uncompact_macro(m_pIn + 9, dest, wxh, m_width, m_height, &m_info_a, YV12);
   uncompact_macro(m_pIn + size, dest + wxh, quarterSize, hw, hh, &m_info_b, YV12);
   size = *(unsigned int*)(m_pIn + 5);
   uncompact_macro(m_pIn + size, dest + wxh + quarterSize, quarterSize, hw, hh, NULL, YV12);

   ASM_BlockRestore(dest + m_width * m_height + quarterSize, hw, quarterSize, 0);

   WAIT_FOR_THREADS(2);

   if ( !m_multithreading )
   {
      ASM_BlockRestore(dest, m_width, wxh, 0);
      ASM_BlockRestore(dest + wxh, hw, quarterSize, 0);
   }

   m_width = DOUBLE(m_width);
   m_height = DOUBLE(m_height);
   wxh = m_width * m_height;

   unsigned char * source = dest;
   dest = (m_format == YV12) ? _pOut: m_pBuffer;

   unsigned char * ysrc = source;
   unsigned char * usrc = ysrc + FOURTH(wxh);
   unsigned char * vsrc = usrc + FOURTH(FOURTH(wxh));//TODO: optimize?
   unsigned char * ydest = dest;
   unsigned char * udest = ydest + wxh;
   unsigned char * vdest = udest + FOURTH(wxh);

   enlarge_res(ysrc, ydest, m_pBuffer2, HALF(m_width), HALF(m_height), m_SSE2); //TODO: optimize?
   enlarge_res(usrc, udest, m_pBuffer2, FOURTH(m_width), FOURTH(m_height), m_SSE2); //TODO: optimize?
   enlarge_res(vsrc, vdest, m_pBuffer2, FOURTH(m_width), FOURTH(m_height), m_SSE2); //TODO: optimize?

   ysrc = ydest;
   usrc = udest;
   vsrc = vdest;

   if ( m_format == RGB24)
   {
      yv12_to_rgb24_mmx(_pOut, m_width, ysrc, vsrc, usrc, m_width, HALF(m_width), m_width, -(int)m_height);
   } 
   else if ( m_format == RGB32 )
   {
      yv12_to_rgb32_mmx(_pOut, m_width, ysrc, vsrc, usrc, m_width, HALF(m_width), m_width, -(int)m_height);
   } 
   else if ( m_format == YUY2 )
   {
      yv12_to_yuyv_mmx(_pOut, m_width, ysrc, vsrc ,usrc, m_width, HALF(m_width), m_width, m_height);
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
   DWORD return_code=ICERR_OK;
   if ( !m_started )
   {
      DecompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
   }
   _pOut = (unsigned char *)icinfo->lpOutput;
   m_pIn  = (unsigned char *)icinfo->lpInput; 
   icinfo->lpbiOutput->biSizeImage = m_length;

   // according to the avi specs, the calling application is responsible for handling null frames.
   if ( icinfo->lpbiInput->biSizeImage == 0 )
   {
      return ICERR_OK;
   }

   // TMPGEnc (and probably some other programs) like to change the floating point
   // precision. This can occasionally produce rounding errors when scaling the probability
   // ranges to a power of 2, which in turn produces corrupted video. Here the code checks 
   // the FPU control word and sets the precision correctly if needed.
   // 
   unsigned int fpuword = _controlfp(0,0);
   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC) )
   {
      _controlfp(_PC_53 | _RC_NEAR,_MCW_PC | _MCW_RC);
#ifdef _DEBUG
      static bool firsttime = true;
      if ( firsttime )
      {
         firsttime=false;
         MessageBox (HWND_DESKTOP, "Floating point control word is not set correctly, correcting it", "Error", MB_OK | MB_ICONEXCLAMATION);
      }
#endif
   }

   switch ( m_pIn[0] ){
   case YV12_FRAME:
#ifdef _DEBUG
      if( ( icinfo->dwFlags & ICDECOMPRESS_HURRYUP ) == ICDECOMPRESS_HURRYUP )
      {
          MessageBox (HWND_DESKTOP, "Hurry Up!!!", "Info", MB_OK | MB_ICONEXCLAMATION);
      }
#endif
      YV12Decompress();
      break;
   case REDUCED_RES:
      ReduceResDecompress();
      break;
   default:
#ifdef _DEBUG
      char emsg[128];
      sprintf_s(emsg,128,"Unrecognized frame type: %d",m_pIn[0]);
      MessageBox (HWND_DESKTOP, emsg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      return_code = (DWORD)ICERR_ERROR;
      break;
   }

   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC))
   {
      _controlfp(fpuword,_MCW_PC | _MCW_RC);
   }

   return return_code;
#ifdef _DEBUG
   } 
   catch ( ... )
   {
   	MessageBox (HWND_DESKTOP, "Exception caught in decompress main", "Error", MB_OK | MB_ICONEXCLAMATION);
   	return (DWORD)ICERR_INTERNAL;
   }
#endif
}