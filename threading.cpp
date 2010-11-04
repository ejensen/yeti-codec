#include "yeti.h"
#include "prediction.h"
#include "threading.h"

DWORD WINAPI encode_worker_thread( LPVOID i )
{
   threadinfo * info = (threadinfo *)i;
   const unsigned int width = info->width;
   const unsigned int height = info->height;

   const unsigned int SSE2 = info->SSE2;
   const unsigned int SSE = info->SSE;
   const unsigned int stride = align_round(width,(SSE2?16:8));

   const unsigned char * src=NULL;
   unsigned char * dest=NULL;
   unsigned char * const buffer = (unsigned char *)info->buffer;
   const unsigned int format=info->format;

   while ( info->length != 0xFFFFFFFF )
   {
      src = (const unsigned char *)info->source;
      dest = (unsigned char *)info->dest;

      unsigned char * dst;
      if (  width != stride ) //TODO: Optimize
      {
         dst=(unsigned char *)align_round(dest,16);
      } 
      else
      {
         dst=buffer;
      }

      if ( SSE2 )
      {
         SSE2_BlockPredict(src,dst,stride,stride*height,(format!=YV12));
      } 
      else 
      {
         MMX_BlockPredict(src,dst,stride,stride*height,SSE,(format!=YV12));
      }

      if ( width!= stride )
      {
         unsigned char * padded = dst;
         unsigned char * stripped = buffer;
         for ( unsigned int y=0; y<height; y++)
         {
            memcpy(stripped+y*width,padded+y*stride,width);
         }
      }

      info->size=info->cObj.compact(buffer,dest,width*height);
      assert( *(__int64*)dest != 0 );
      info->length=0;
      SuspendThread(info->thread);// go to sleep, main thread will resume it
   }

   info->cObj.FreeCompressBuffers();
   aligned_free(info->buffer,"Thread Buffer");
   info->length=0;
   return 0;
}

DWORD WINAPI decode_worker_thread( LPVOID i )
{
   threadinfo * info = (threadinfo *)i;
   unsigned int width;
   unsigned int height;
   unsigned char * src=NULL;
   unsigned char * dest=NULL;
   unsigned int format;
   unsigned int length;
   while ( info->length != 0xFFFFFFFF )
   {
      src = (unsigned char *)info->source;
      dest = (unsigned char *)info->dest;

      length=info->length;
      format=info->format;
      width = info->width;
      height = info->height;

      info->cObj.uncompact(src,dest,length);

      ASM_BlockRestore(dest,width,width*height,format!=YV12);

      info->length=0;
      SuspendThread(info->thread);
   }

   info->cObj.FreeCompressBuffers();
   aligned_free(info->buffer,"Thread Buffer");
   info->length=0;
   return 0;
}

DWORD CodecInst::InitThreads( int encode )
{
   DWORD temp;
   _info_a.length=0;
   _info_b.length=0;
   _info_c.length=0;

   unsigned int use_format = YV12;

   _info_a.width = _width;
   _info_b.width = Half(_width);
   _info_c.width = _width;

   _info_a.height =_height;
   _info_b.height = Half(_height);
   _info_c.height = _height;

   if ( _reduced ) //TODO: Right?
   {
      _info_a.width = Half(_width);
      _info_a.height =  Half(_height);
      _info_b.width = Fourth(_width);
      _info_b.height = Fourth(_height);
   }

   _info_a.format = use_format;
   _info_b.format = use_format;
   _info_c.format = use_format;

   _info_a.SSE = _SSE;
   _info_b.SSE = _SSE;
   _info_c.SSE = _SSE;

   _info_a.SSE2 = _SSE2;
   _info_b.SSE2 = _SSE2;
   _info_c.SSE2 = _SSE2;

   _info_a.thread = NULL;
   _info_b.thread = NULL;
   _info_c.thread = NULL;
   _info_a.buffer = NULL;
   _info_b.buffer = NULL;
   _info_c.buffer = NULL;
#ifdef _DEBUG 
   _info_a.name = "Thread A";
   _info_b.name = "Thread B";
   _info_c.name = "Thread C";
#endif

   int buffer_a = align_round(_width,16)*(_height)+2048;
   int buffer_b = align_round(_info_b.width,16)*_info_b.height+2048;

   if ( !_info_a.cObj.InitCompressBuffers( buffer_a ) || !_info_b.cObj.InitCompressBuffers( buffer_b ) 
      || !(_info_a.buffer=(unsigned char *)aligned_malloc(_info_a.buffer,buffer_a,16,"Info_a.buffer"))
      || !(_info_b.buffer=(unsigned char *)aligned_malloc(_info_b.buffer,buffer_b,16,"Info_b.buffer")) )
   {
      _info_a.cObj.FreeCompressBuffers();
      _info_b.cObj.FreeCompressBuffers();
      aligned_free(_info_a.buffer,"Info_a.buffer");
      aligned_free(_info_b.buffer,"Info_b.buffer");
      _info_a.thread=NULL;
      _info_b.thread=NULL;
      return (DWORD)ICERR_MEMORY;
   } 
   else 
   { 
      if ( _format == RGB32 )
      {
         if ( !_info_c.cObj.InitCompressBuffers( buffer_a ) 
            || !(_info_c.buffer=(unsigned char *)aligned_malloc(_info_c.buffer,buffer_a,16,"Info_c.buffer")))
         {
            _info_a.cObj.FreeCompressBuffers();
            _info_b.cObj.FreeCompressBuffers();
            _info_c.cObj.FreeCompressBuffers();
            aligned_free(_info_a.buffer,"Info_a.buffer");
            aligned_free(_info_b.buffer,"Info_b.buffer");
            aligned_free(_info_c.buffer,"Info_c.buffer");
            _info_a.thread=NULL;
            _info_b.thread=NULL;
            _info_c.thread=NULL;
            return (DWORD)ICERR_MEMORY;
         }
         if ( encode )
         {
            _info_c.thread=CreateThread(NULL,0, encode_worker_thread, &_info_c,CREATE_SUSPENDED,&temp);
         } 
         else
         {
            _info_c.thread=CreateThread(NULL,0, decode_worker_thread, &_info_c,CREATE_SUSPENDED,&temp);
         }
      }
      if ( encode )
      {
         _info_a.thread=CreateThread(NULL,0, encode_worker_thread, &_info_a,CREATE_SUSPENDED,&temp);
         _info_b.thread=CreateThread(NULL,0, encode_worker_thread, &_info_b,CREATE_SUSPENDED,&temp);
      } 
      else 
      {
         _info_a.thread=CreateThread(NULL,0, decode_worker_thread, &_info_a,CREATE_SUSPENDED,&temp);
         _info_b.thread=CreateThread(NULL,0, decode_worker_thread, &_info_b,CREATE_SUSPENDED,&temp);
      }
      if ( !_info_a.thread || !_info_b.thread || (_format==RGB32 && !_info_c.thread ))
      {
         _info_a.cObj.FreeCompressBuffers();
         _info_b.cObj.FreeCompressBuffers();
         if ( _format==RGB32 )
         {
            _info_c.cObj.FreeCompressBuffers();
            aligned_free(_info_c.buffer,"Info_c.buffer");
         }
         aligned_free(_info_a.buffer,"Info_a.buffer");
         aligned_free(_info_b.buffer,"Info_b.buffer");
         _info_a.thread=NULL;
         _info_b.thread=NULL;
         _info_c.thread=NULL;
         return (DWORD)ICERR_INTERNAL;
      }
   }
   if ( use_format >= RGB24 )
   {
      SetThreadPriority(_info_b.thread,THREAD_PRIORITY_ABOVE_NORMAL);
   } 
   else 
   {
      SetThreadPriority(_info_a.thread,THREAD_PRIORITY_ABOVE_NORMAL);
   }
   return (DWORD)ICERR_OK;
}

void CodecInst::EndThreads()
{
   _info_a.length=0xFFFFFFFF;
   _info_b.length=0xFFFFFFFF;
   _info_c.length=0xFFFFFFFF;
   if ( _info_a.thread )
   {
      ForceResumeThread(_info_a.thread);
   }
   if ( _info_b.thread )
   {
      ForceResumeThread(_info_b.thread);
   }
   if ( _info_c.thread && _format == RGB32 )
   {
      ForceResumeThread(_info_c.thread);
   }
   while ( (_info_a.length && _info_a.thread) || (_info_b.length && _info_b.thread) || (_info_c.length && _format == RGB32 && _info_c.thread) )
   {
      Sleep(1);
   }

   if ( !CloseHandle(_info_a.thread) )
   {
#ifdef _DEBUG
      /*LPVOID lpMsgBuf;
      FormatMessage( 
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM | 
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      GetLastError(),
      0, // Default language
      (LPTSTR) &lpMsgBuf,
      0,
      NULL 
      );
      MessageBox( NULL, (char *)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
      LocalFree( lpMsgBuf );*/
      MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0A", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }
   if ( !CloseHandle(_info_b.thread) )
   {
#ifdef _DEBUG
      MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0B", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }
   if ( _info_c.thread && _format == RGB32 )
   {
      if ( !CloseHandle(_info_c.thread) )
      {
#ifdef _DEBUG
         MessageBox (HWND_DESKTOP,"CloseHandle failed for thread 0x0C", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      }
   }

   _info_a.thread=NULL;
   _info_b.thread=NULL;
   _info_c.thread=NULL;
   _info_a.length=0;
   _info_b.length=0;
   _info_c.length=0;
}