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
   if ( _started  == 0x1337 )
   {
      CompressEnd();
   }
   _started = 0;

   _nullframes = GetPrivateProfileInt("settings", "nullframes", false, SettingsFile)>0;
   _multithreading = GetPrivateProfileInt("settings", "multithreading", false, SettingsFile)>0;
   _reduced = GetPrivateProfileInt("settings", "reduced", false, SettingsFile)>0;

   if ( int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK )
   {
      return error;
   }

   _width = lpbiIn->biWidth;
   _height = lpbiIn->biHeight;

   _format = lpbiIn->biBitCount;
   _length = Eighth(_width * _height * _format);

   unsigned int buffer_size;
   if ( _format < RGB24 )
   {
      buffer_size = Eighth(align_round(_width,32) * _height * _format) + 1024;
   } 
   else
   {
      buffer_size = Quadruple(align_round(_width, 16) * _height) + 1024;
   }

   _pBuffer = (unsigned char *)aligned_malloc(_pBuffer, buffer_size,16,"buffer");
   _pBuffer2 = (unsigned char *)aligned_malloc(_pBuffer2, buffer_size,16,"buffer2");
   _pDelta = (unsigned char *)aligned_malloc(_pDelta, buffer_size,16,"delta");
   _pLossy_buffer = (unsigned char *)aligned_malloc(_pLossy_buffer, buffer_size,16,"lossy");
   _pPrev = (unsigned char *)aligned_malloc(_pPrev, buffer_size,16,"prev");
   ZeroMemory(_pPrev, buffer_size);

   if ( !_pBuffer || !_pBuffer2 || !_pPrev || !_pDelta || !_pLossy_buffer)
   {
      aligned_free(_pBuffer,"buffer");
      aligned_free(_pBuffer2,"buffer2");
      aligned_free(_pDelta,"delta");
      aligned_free(_pLossy_buffer,"lossy");
      aligned_free(_pPrev,"prev");
      return (DWORD)ICERR_MEMORY;
   }

   int cb_size=_width*_height*5/4;

   if ( !_cObj.InitCompressBuffers( cb_size ) )
   {
      return (DWORD)ICERR_MEMORY;
   }

   if ( _multithreading )
   {
      int code = InitThreads( true );
      if ( code != ICERR_OK )
      {
         return code;
      }
   }

   _started = 0x1337;

   return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 4KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   return (DWORD)( align_round(lpbiIn->biWidth,16)*lpbiIn->biHeight*lpbiIn->biBitCount/8*1.05 + 4096); // TODO: Optimize
}

// release resources when compression is done

DWORD CodecInst::CompressEnd()
{
   if ( _started  == 0x1337 )
   {
      if ( _multithreading )
      {
         EndThreads();
      }

      aligned_free( _pBuffer, "buffer");
      aligned_free( _pBuffer2, "buffer2");
      aligned_free( _pDelta, "delta");
      aligned_free( _pPrev, "prev");
      aligned_free( _pLossy_buffer, "lossy_buffer");
      _cObj.FreeCompressBuffers();
   }
   _started = 0;
   return ICERR_OK;
}

// Compress a YV12 keyframe
DWORD CodecInst::CompressYV12(ICCOMPRESS* icinfo)
{
   const unsigned int mod = (_SSE2?16:8) - 1;

   const unsigned int hw = Half(_width);
   const unsigned int hh = Half(_height);

   const unsigned int y_len	= _width*_height;
   const unsigned int c_len	= Fourth(y_len);
   const unsigned int yu_len	= y_len+c_len;
   const unsigned int len		= yu_len+c_len;

   const unsigned int y_stride	= align_round(_width,mod+1);
   const unsigned int c_stride	= align_round(hw,mod+1);

   const unsigned int ay_len	= y_stride*_height;
   const unsigned int ac_len	= c_stride*hh;
   const unsigned int ayu_len	= ay_len+ac_len;

   const unsigned char * ysrc;
   const unsigned char * usrc;
   const unsigned char * vsrc;

   const unsigned int in_aligned = !((int)_pIn&mod);
   
   unsigned char* source;

   if(icinfo->lFrameNum > 0) //TODO: Optimize
   {
      if ( _nullframes )
      {
         // compare in two parts, video is more likely to change in middle than at bottom
         unsigned int pos = Half(len)+15;
         pos&=(~15);
         if (!memcmp(_pIn+pos,_pPrev+pos,len-pos) && !memcmp(_pIn,_pPrev,pos) )
         {
            icinfo->lpbiOutput->biSizeImage =0;
            *icinfo->lpdwFlags = NULL;
            return (DWORD)ICERR_OK;
         }
      }
      //DELTA!!!!
      unsigned int buffer_size = Eighth( align_round(_width, 32) * _height * _format )+1024; 
      for(unsigned int i = 0; i < buffer_size; i++)
      {
         _pDelta[i] = _pIn[i] ^ _pPrev[i];
      }

      source = _pDelta;
   }
   else
   {
      source = (unsigned char*)_pIn;
   }

   //note: chroma has only half the width of luminance, it may need to be aligned separately

   if ( ((hw)&mod) == 0 )  // no alignment padding needed
   {
      if ( in_aligned )  // no alignment needed
      {
         ysrc = source;
         usrc = source+y_len;
         vsrc = source+yu_len;
      } 
      else // data is naturally aligned,	input buffer is not
      {
         memcpy(_pBuffer,source,len);
         ysrc = _pBuffer;
         usrc = _pBuffer+y_len;
         vsrc = _pBuffer+yu_len;
      }
   } 
   else if ( (_width&mod) == 0 ) // chroma needs alignment padding
   { 
      if ( in_aligned )
      {
         ysrc = source;
      } 
      else 
      {
         memcpy(_pBuffer,source,y_len);
         ysrc = _pBuffer;
      }

      _pBuffer += y_len; // step over luminance (not always needed...)

      for ( unsigned int y=0;y<hh*2;y++) // TODO: Optimize
      {
         memcpy(_pBuffer + y * c_stride,source + y * hw + y_len, hw);
         unsigned char val = _pBuffer[y * c_stride + hw - 1];

         for (unsigned int x = hw; x < c_stride; x++)
         {
            _pBuffer[y * c_stride + x] = val;
         }
      }
      usrc = _pBuffer;
      vsrc = _pBuffer+ac_len;

      _pBuffer -= y_len;
   } 
   else  // both chroma and luminance need alignment padding
   {
      // align luminance
      unsigned int y;
      for ( y=0; y < _height; y++)
      {
         memcpy(_pBuffer + y * y_stride, source + y * _width, _width);
         unsigned char val = _pBuffer[y * y_stride + _width - 1];

         for (unsigned int x = _width; x < y_stride; x++)
         {
            _pBuffer[y * y_stride + x]=val;
         }
      }

      ysrc = _pBuffer;
      _pBuffer += ay_len;

      for ( y=0;y<hh*2;y++)//Optimize
      {
         memcpy(_pBuffer + y * c_stride,source + y * hw + y_len, hw);
         unsigned char val = _pBuffer[y * c_stride + hw - 1];

         for ( unsigned int x = hw; x < c_stride; x++)
         {
            _pBuffer[y * c_stride + x] = val;
         }
      }
      usrc = _pBuffer;
      vsrc = _pBuffer+c_stride*hh;

      _pBuffer -= ay_len;
   }
   // done aligning, aligned data is in 'in' (which may actually be prev) or buffer 

   int size;
   if ( !_multithreading )
   {
      unsigned char *buffer3=(unsigned char *)align_round(_pOut,16);

      //set up dest buffers based on if alignment padding needs removal later
      unsigned char * ydest;
      unsigned char * udest;
      unsigned char * vdest;

      ydest = (_width & mod) ? buffer3 : _pBuffer2; // TODO: needed?


      if (hw & mod)
      {
         udest=buffer3 + ay_len;
         vdest=buffer3 + ay_len + ac_len;
      } 
      else 
      {
         udest=_pBuffer2 + ay_len;
         vdest=_pBuffer2 + ay_len + ac_len;
      }

      // perform prediction
      if ( _SSE2 )
      {
         SSE2_BlockPredict(ysrc,ydest,y_stride,ay_len,0);
         SSE2_BlockPredict(usrc,udest,c_stride,ac_len,0);
         SSE2_BlockPredict(vsrc,vdest,c_stride,ac_len,0);
      } 
      else
      {
         MMX_BlockPredict(ysrc,ydest,y_stride,ay_len,_SSE,0);
         MMX_BlockPredict(usrc,udest,c_stride,ac_len,_SSE,0);
         MMX_BlockPredict(vsrc,vdest,c_stride,ac_len,_SSE,0);
      }

      // remove alignment padding, all data will end up in buffer2
      if (_width & mod) // remove luminance padding 
      {
         for (unsigned int y = 0; y < _height; y++)
         {
            memcpy(_pBuffer2 + y * _width, ydest + y * y_stride, _width);
         }
      }
      if ( hw & mod )  // remove chroma padding
      {
         for ( unsigned int y=0;y< hh*2;y++) // TODO: Optimize
         {
            memcpy(_pBuffer2 + y_len + y * hw, udest + y * c_stride, hw);
         }
      }

      size = 9;
      size += _cObj.compact(_pBuffer2, _pOut + size, y_len);
      *(UINT32*)(_pOut + 1) = size;
      size += _cObj.compact(_pBuffer2 + y_len, _pOut+size, c_len);
      *(UINT32*)(_pOut + 5) = size;
      size += _cObj.compact(_pBuffer2 + yu_len, _pOut + size ,c_len);
   } 
   else 
   {
      _info_a.source=ysrc;
      _info_a.dest=_pOut+9;
      _info_a.length=y_len;
      ForceResumeThread(_info_a.thread);
      _info_b.source=usrc;
      _info_b.dest=_pBuffer2;
      _info_b.length=c_len;
      ForceResumeThread(_info_b.thread);

      unsigned char *vdest=_pBuffer2+ayu_len;

      if ( _SSE2 )
      {
         SSE2_BlockPredict(vsrc,vdest,c_stride,ac_len,0);
      } 
      else 
      {
         MMX_BlockPredict(vsrc,vdest,c_stride,ac_len,_SSE,0);
      }

      // remove alignment if needed
      if ( hw & mod )  // remove chroma padding
      {
         for ( unsigned int y=0;y< hh;y++)
         {
            memcpy((unsigned char*)(int)vsrc+y*hw,vdest+y*c_stride,hw);
         }
      } 
      else
      {
         unsigned char * temp=(unsigned char*)(int)vsrc;
         vsrc = vdest;
         vdest = temp;
      }

      size=_cObj.compact(vsrc,vdest,c_len);

      while ( _info_a.length )
      {
         Sleep(0);
      }

      int sizea = _info_a.size;
      *(UINT32*)(_pOut+5)=9+sizea;
      memcpy(_pOut+sizea+9,vdest,size);

      while ( _info_b.length )
      {
         Sleep(0);
      }

      int sizeb = _info_b.size;
      *(UINT32*)(_pOut+1)=sizea+9+size;
      memcpy(_pOut+sizea+9+size,_pBuffer2,sizeb);

      size+=sizea+sizeb+9;
   }

   _pOut[0] = ARITH_YV12;
   icinfo->lpbiOutput->biSizeImage = size;
   return (DWORD)ICERR_OK;
}

DWORD CodecInst::CompressReduced(ICCOMPRESS *icinfo)
{
   const unsigned int mod = (_SSE2?16:8);

   const unsigned int hh = Half(_height);
   const unsigned int hw = Half(_width);

   const unsigned int ry_stride = align_round(hw, mod);
   const unsigned int rc_stride = align_round(Fourth(_width), mod);
   const unsigned int ry_size   = ry_stride*hh;
   const unsigned int rc_size   = Fourth(rc_stride * _height);
   const unsigned int ryu_size  = ry_size+rc_size;
   const unsigned int y_size    = _width*_height;
   const unsigned int quarterSize = Fourth(y_size);
   const unsigned int c_size    = quarterSize;
   const unsigned int yu_size   = y_size+c_size;
   const unsigned int ry_bytes  = quarterSize;
   const unsigned int rc_bytes  = Fourth(quarterSize); 

   unsigned char * ysrc = _pBuffer;
   unsigned char * usrc = _pBuffer + ry_size;
   unsigned char * vsrc = _pBuffer + ryu_size;

   reduce_res(_pIn, ysrc, _pBuffer2, _width, _height, _SSE2, _SSE);
   reduce_res(_pIn + y_size, usrc, _pBuffer2, hw, hh, _SSE2, _SSE);
   reduce_res(_pIn + yu_size, vsrc, _pBuffer2, hw, hh, _SSE2, _SSE);

   unsigned int size;

   if ( !_multithreading )
   {
      unsigned char * ydest = _pBuffer2;
      unsigned char * udest = _pBuffer2+ry_size;
      unsigned char * vdest = _pBuffer2+ryu_size;

      if ( _SSE2 )
      {
         SSE2_BlockPredict(ysrc,ydest,ry_stride,ry_size,0);
         SSE2_BlockPredict(usrc,udest,rc_stride,rc_size,0);
         SSE2_BlockPredict(vsrc,vdest,rc_stride,rc_size,0);
      } 
      else
      {
         MMX_BlockPredict(ysrc,ydest,ry_stride,ry_size,_SSE,0);
         MMX_BlockPredict(usrc,udest,rc_stride,rc_size,_SSE,0);
         MMX_BlockPredict(vsrc,vdest,rc_stride,rc_size,_SSE,0);
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

      unsigned int quarterWidth = Fourth( _width );
      unsigned int quarterHeight = Fourth( _height );

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
         usrc=udest;
         vsrc=vdest;
      }

      size = 9;
      size+=_cObj.compact(ysrc, _pOut + size, ry_bytes);
      *(UINT32*)(_pOut+1)=size;
      size+=_cObj.compact(usrc, _pOut + size, rc_bytes);
      *(UINT32*)(_pOut+5)=size;
      size+=_cObj.compact(vsrc, _pOut + size, rc_bytes);
   } 
   else
   {
      _info_a.source = ysrc;
      _info_a.dest =_pOut + 9;
      _info_a.length = ry_bytes;
      ForceResumeThread(_info_a.thread);
      _info_b.source = usrc;
      _info_b.dest = _pPrev;
      _info_b.length = rc_bytes;
      ForceResumeThread(_info_b.thread);

      unsigned char *vdest = _pBuffer2;

      if ( _SSE2 )
      {
         SSE2_BlockPredict(vsrc, vdest, rc_stride, rc_size, 0);
      } 
      else
      {
         MMX_BlockPredict(vsrc, vdest, rc_stride, rc_size ,_SSE, 0);
      }

      // remove alignment if needed
      unsigned int quarterWidth = Fourth( _width );
      if ( rc_stride != quarterWidth )  // remove chroma padding
      {
         unsigned int quarterHeight = Fourth( _height );
         vsrc = vdest;
         vdest = _pBuffer2 + rc_size;
         for (unsigned int y = 0; y< quarterHeight; y++)
         {
            memcpy(vdest + y * quarterWidth, vsrc + y * rc_stride, quarterWidth);
         }
      }
      vsrc = vdest;
      vdest = _pBuffer2 + Double(rc_size);

      size=_cObj.compact(vsrc, vdest, rc_bytes);
      while ( _info_a.length )
      {
         Sleep(0);
      }

      int sizea = _info_a.size;
      *(UINT32*)(_pOut+5) = 9 + sizea;
      memcpy(_pOut+sizea + 9, vdest, size);

      while ( _info_b.length )
      {
         Sleep(0);
      }

      int sizeb = _info_b.size;
      *(UINT32*)(_pOut+1)= sizea + 9 + size;
      memcpy(_pOut + sizea + 9 + size, _pPrev, sizeb);

      size += sizea + sizeb + 9;
   }

   _pOut[0] = REDUCED_RES;
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

   if ( _format >= RGB24 )
   {
      if ( _format == RGB24 )
      {
         mmx_ConvertRGB24toYUY2(_pIn,_pBuffer,_width*3,Double(_width),_width,_height);
      } 
      else if ( _format == RGB32 )
      {
         mmx_ConvertRGB32toYUY2((const unsigned int *)_pIn,(unsigned int *)_pBuffer,_width,Half(_width),_width,_height);
      }
      src = _pBuffer;
   }
   else
   {
      src = _pIn;
   }

   unsigned int dw = Double(_width);
   unsigned char * dst2=_pLossy_buffer;
   unsigned int yuy2_pitch = align_round(dw,16);
   unsigned int y_pitch = align_round(_width,8);
   unsigned int uv_pitch = align_round(Half(_width),8);

   bool is_aligned = (_width%16)==0;
   if ( !is_aligned )
   {
      for (unsigned int h = 0; h < _height; h++)
      {
         memcpy(dst2 + yuy2_pitch * h, src + dw * h, dw);
      }

      src = dst2;
      dst2 = _pBuffer;
   }

   if ( _SSE )
   {
      isse_yuy2_to_yv12(src, dw, yuy2_pitch, dst2, dst2+y_pitch*_height+ Half(uv_pitch * _height), dst2+y_pitch*_height, y_pitch,uv_pitch, _height);
   } 
   else 
   {
      mmx_yuy2_to_yv12( src, dw, yuy2_pitch, dst2, dst2+y_pitch*_height+ Half(uv_pitch * _height), dst2+y_pitch*_height, y_pitch,uv_pitch, _height);
   }

   unsigned char * dest = _pLossy_buffer;
   if ( !is_aligned )
   {
      unsigned int h;

      for ( h=0;h < _height;h++)
      {
         memcpy(dest + _width * h, dst2 + y_pitch * h, _width);
      }

      dst2 += y_pitch * _height;
      dest += _width * _height;

      unsigned int hw = Half(_width);
      for ( h=0; h < _height; h++)
      {
         memcpy(dest + hw * h, dst2 + uv_pitch * h, hw);
      }
   }

   _pIn = _pLossy_buffer;

   if ( _reduced ) //TODO:Optimize
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
      if ( icinfo->dwFlags == 1 /*AVIIF_LIST*/ && ret_val == ICERR_OK )
      {
         *icinfo->lpdwFlags = AVIIF_KEYFRAME;
      }
      else
      {
         *icinfo->lpdwFlags = AVIIF_LASTPART;
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

   _pOut = (unsigned char *)icinfo->lpOutput;
   _pIn  = (unsigned char *)icinfo->lpInput;

   if ( icinfo->lFrameNum == 0 )
   {
      if ( _started != 0x1337 )
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
    memcpy( _pPrev, _pIn, _length); // TODO: Optimize
   //}

   //if (ret_val == ICERR_OK )
   //{
   //   *icinfo->lpdwFlags = AVIIF_KEYFRAME; //TODO: Change for delta frames
   //}

   if ( !(fpuword & _PC_53) || (fpuword & _MCW_RC))
   {
      _controlfp(fpuword,_MCW_PC | _MCW_RC);
   }

   return (DWORD)ret_val;
}