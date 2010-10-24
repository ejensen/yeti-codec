#include "compact.h"
#include "yeti.h"
#include "interface.h"
#include "prediction.h"
#include "threading.h"
#include "convert.h"
#include "convert_yv12.h"
#include <float.h>
#include "huffyuv_a.h"

// initialize the codec for decompression
DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if ( _started == 0x1337)
   {
      DecompressEnd();
   }
   _started = 0;

   if ( int error = DecompressQuery(lpbiIn,lpbiOut) != ICERR_OK )
   {
      return error;
   }

   int buffer_size;

   _width = lpbiIn->biWidth;
   _height = lpbiIn->biHeight;

   detectFlags(&_SSE2, &_SSE, &_MMX);

   _format = lpbiOut->biBitCount;

   _length = _width*_height*_format/8; //TODO: optimize

   buffer_size = _length+2048;
   if ( _format >= RGB24 )
   {
      buffer_size=align_round(_width*4,8)*_height+2048;
   }

   _pBuffer = (unsigned char *)lag_aligned_malloc( _pBuffer,buffer_size,16,"buffer");
   _pPrev = (unsigned char *)lag_aligned_malloc( _pPrev,buffer_size,16,"prev");
   ZeroMemory(_pPrev, buffer_size);

   if ( !_pBuffer || !_pPrev )
   {
      return ICERR_MEMORY;
   }

   if ( !_cObj.InitCompressBuffers( _width*_height*5/4 ) )
   {
      return ICERR_MEMORY;
   }

   _multithreading = GetPrivateProfileInt("settings", "multithreading", false, "yeti.ini")>0;
   if ( _multithreading )
   {
      int code = InitThreads( false);
      if ( code != ICERR_OK )
      {
         return code;
      }
   }
   _started = 0x1337;
   return ICERR_OK;
}

// release resources when decompression is done
DWORD CodecInst::DecompressEnd()
{
   if ( _started == 0x1337 )
   {
      if ( _multithreading )
      {
         EndThreads();
      }

      lag_aligned_free( _pBuffer,"buffer");
      lag_aligned_free( _pPrev, "prev");
      _cObj.FreeCompressBuffers();
   }
   _started=0;
   return ICERR_OK;
}

inline void CodecInst::uncompact_macro( const unsigned char * _in, unsigned char * _out, unsigned int _length, unsigned int _width, unsigned int _height, threadinfo * _thread, int _format)
{
   if ( _multithreading && _thread )
   {
      _thread->source=(unsigned char *)_in;
      _thread->dest=_out;
      _thread->length=_length;
      _thread->width=_width;
      _thread->height=_height;
      _thread->format=_format; 

      while ( ResumeThread(((threadinfo *)_thread)->thread) != 1)
      {
         Sleep(0);
      };
   } 
   else 
   {
      _cObj.uncompact(_in,_out,_length);
   }
}

// Decompress a YV12 keyframe

void CodecInst::ArithYV12Decompress()
{
   unsigned char * dst = _pOut;
   unsigned char * dst2 = _pBuffer;
   if ( _format == YUY2 )
   {
      dst = _pBuffer;
      dst2 = _pOut;
   }

   int size = *(UINT32*)(_pIn+1);
   uncompact_macro(_pIn+9,dst,_width*_height,_width,_height,&_info_a,YV12);
   uncompact_macro(_pIn+size,dst+_width*_height,_width*_height/4,_width/2,_height/2,&_info_b,YV12); //TODO: optimize
   size = *(UINT32*)(_pIn+5);
   uncompact_macro(_pIn+size,dst+_width*_height+_width*_height/4,_width*_height/4,_width/2,_height/2,NULL,YV12); //TODO: optimize

   ASM_BlockRestore(dst+_width*_height+_width*_height/4,_width/2,_width*_height/4,0); //TODO: optimize
   wait_for_threads(2);

   if ( !_multithreading )
   {
      ASM_BlockRestore(dst,_width,_width*_height,0);
      ASM_BlockRestore(dst+_width*_height,_width/2,_width*_height/4,0); //TODO: optimize
   }

   unsigned int length = _width * _height * YV12 / 8;
   for(unsigned int i = 0; i < length; i++)
   {
      dst[i] = dst[i] ^ _pPrev[i];
   }

   memcpy( _pPrev, dst, length);

   if ( _format == YV12 )
   {
      return;
   }

   //upsample if needed
   if ( _SSE2 )
   {
      isse_yv12_to_yuy2(dst,dst+_width*_height+_width*_height/4,dst+_width*_height,_width,_width,_width/2,dst2,_width*2,_height); //TODO: optimize
   } 
   else 
   {
      mmx_yv12_to_yuy2(dst,dst+_width*_height+_width*_height/4,dst+_width*_height,_width,_width,_width/2,dst2,_width*2,_height); //TODO: optimize
   }

   if ( _format == YUY2 )
   {
      return;
   }

   // upsample to RGB
   if ( _format == RGB32 )
   {
      mmx_YUY2toRGB32(dst2,_pOut,dst2+_width*_height*2,_width*2); //TODO: optimize
   } 
   else 
   {
      mmx_YUY2toRGB24(dst2,_pOut,dst2+_width*_height*2,_width*2); //TODO: optimize
   }
}

#include "convert_xvid.cpp"

void CodecInst::ReduceResDecompress()
{
   _width/=2;
   _height/=2;
   int size = *(UINT32*)(_pIn+1);
   unsigned char * dest = (_format==YV12)?_pBuffer:_pOut;
   uncompact_macro(_pIn+9,dest,_width*_height,_width,_height,&_info_a,YV12);
   uncompact_macro(_pIn+size,dest+_width*_height,_width*_height/4,_width/2,_height/2,&_info_b,YV12); //TODO: optimize
   size = *(UINT32*)(_pIn+5);
   uncompact_macro(_pIn+size,dest+_width*_height+_width*_height/4,_width*_height/4,_width/2,_height/2,NULL,YV12); //TODO: optimize

   ASM_BlockRestore(dest+_width*_height+_width*_height/4,_width/2,_width*_height/4,0); //TODO: optimize
   wait_for_threads(2);
   if ( !_multithreading )
   {
      ASM_BlockRestore(dest,_width,_width*_height,0);
      ASM_BlockRestore(dest+_width*_height,_width/2,_width*_height/4,0); //TODO: optimize
   }

   _width*=2;
   _height*=2;

   unsigned char * source=dest;
   dest=(_format==YV12)?_pOut:_pBuffer;

   unsigned char * ysrc = source;
   unsigned char * usrc = ysrc+_width*_height/4; //TODO: optimize
   unsigned char * vsrc = usrc+_width*_height/4/4; //TODO: optimize
   unsigned char * ydest = dest;
   unsigned char * udest = ydest+_width*_height;
   unsigned char * vdest = udest+_width*_height/4; //TODO: optimize

   enlarge_res(ysrc,ydest,_pBuffer2,_width/2,_height/2,_SSE2,_SSE,_MMX); //TODO: optimize
   enlarge_res(usrc,udest,_pBuffer2,_width/4,_height/4,_SSE2,_SSE,_MMX); //TODO: optimize
   enlarge_res(vsrc,vdest,_pBuffer2,_width/4,_height/4,_SSE2,_SSE,_MMX); //TODO: optimize

   ysrc = ydest;
   usrc = udest;
   vsrc = vdest;

   if ( _format == RGB24)
   {
      yv12_to_rgb24_mmx(_pOut,_width,ysrc,vsrc,usrc,_width,_width/2,_width,-(int)_height); //TODO: optimize
   } 
   else if ( _format == RGB32 ) 
   {
      yv12_to_rgb32_mmx(_pOut,_width,ysrc,vsrc,usrc,_width,_width/2,_width,-(int)_height); //TODO: optimize
   } 
   else if ( _format == YUY2 ) 
   {
      yv12_to_yuyv_mmx(_pOut,_width,ysrc,vsrc,usrc,_width,_width/2,_width,_height); //TODO: optimize
   }
}

// Called to decompress a frame, the actual decompression will be
// handed off to other functions based on the frame type.
DWORD CodecInst::Decompress(ICDECOMPRESS* icinfo, DWORD dwSize) 
{
#ifndef YETI_RELEASE
   try 
   {
#endif
   DWORD return_code=ICERR_OK;
   if ( _started != 0x1337 )
   {
      DecompressBegin(icinfo->lpbiInput,icinfo->lpbiOutput);
   }
   _pOut = (unsigned char *)icinfo->lpOutput;
   _pIn  = (unsigned char *)icinfo->lpInput; 
   icinfo->lpbiOutput->biSizeImage = _length;

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
#ifndef YETI_RELEASE
      static bool firsttime=true;
      if ( firsttime )
      {
         firsttime=false;
         MessageBox (HWND_DESKTOP, "Floating point control word is not set correctly, correcting it", "Error", MB_OK | MB_ICONEXCLAMATION);
      }
#endif
   }

   switch ( _pIn[0] ){
   case ARITH_YV12:
      ArithYV12Decompress();
      break;
   case REDUCED_RES:
      ReduceResDecompress();
      break;
   default:
#ifndef YETI_RELEASE
      char emsg[128];
      sprintf_s(emsg,128,"Unrecognized frame type: %d",_pIn[0]);
      MessageBox (HWND_DESKTOP, emsg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      return_code=ICERR_ERROR;
      break;
   }

   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC))
   {
      _controlfp(fpuword,_MCW_PC | _MCW_RC);
   }

   return return_code;
#ifndef YETI_RELEASE
   } 
   catch ( ... )
   {
   	MessageBox (HWND_DESKTOP, "Exception caught in decompress main", "Error", MB_OK | MB_ICONEXCLAMATION);
   	return ICERR_INTERNAL;
   }
#endif
}