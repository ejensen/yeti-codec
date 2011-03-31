#include <stdint.h>
#include "yeti.h"
#include "codec_inst.h"
#include "threading.h"
#include "prediction.h"

DWORD WINAPI EncodeWorkerTread(LPVOID i)
{
   threadInfo* info = (threadInfo*)i;
   const unsigned int width = info->m_width;
   const unsigned int height = info->m_height;
   const unsigned int length = width * height;

   const BYTE* src = NULL;
   BYTE* dest = NULL;
   const unsigned int format=info->m_format;
   BYTE* const buffer = (BYTE*)info->m_buffer;

   while(info->m_length != UINT32_MAX)
   {
      src = (const BYTE*)info->m_source;
      dest = (BYTE*)info->m_dest;

      BYTE* dst = buffer + ((unsigned int)src&15);

      if ( format == YUY2 )
      {
         Block_Predict_YUV16(src, dst, width, length, false);
      } 
      else
      {
         Block_Predict(src, dst, width, length );
      }

      info->m_size = info->m_compressWorker.Compact(dst, dest, length);
      assert( *(__int64*)dest != 0 );
      info->m_length = 0;
      SuspendThread(info->m_thread);// go to sleep, main thread will resume it
   }

   info->m_compressWorker.FreeCompressBuffers();
   ALIGNED_FREE(info->m_buffer, "Thread Buffer");
   info->m_length = 0;
   return 0;
}

DWORD WINAPI DecodeWorkerThread(LPVOID i)
{
   threadInfo* info = (threadInfo*)i;
   BYTE* src = NULL;
   BYTE* dest = NULL;
   unsigned int length;

   while(info->m_length != UINT32_MAX) //TODO:Optimize
   {
      src = (BYTE*)info->m_source;
      dest = (BYTE*)info->m_dest;

      length = info->m_length;

      info->m_compressWorker.Uncompact(src, dest, length);
      info->m_length = 0;
      SuspendThread(info->m_thread);
   }

   info->m_compressWorker.FreeCompressBuffers();
   ALIGNED_FREE(info->m_buffer, "Thread Buffer");
   info->m_length = 0;
   return 0;
}

DWORD CodecInst::InitThreads(bool encode)
{
   m_info_a.m_length = 0;
   m_info_b.m_length = 0;
   m_info_c.m_length = 0;

   int useFormat = (encode) ? m_compressFormat : m_format;

   m_info_a.m_width = m_width;
   m_info_a.m_height = m_height;
   m_info_b.m_width = HALF(m_width);
   m_info_b.m_height = (useFormat == YUY2) ? m_height : HALF(m_height);

   m_info_c.m_width = m_width;
   m_info_c.m_height = m_height;

   m_info_a.m_format = useFormat;
   m_info_b.m_format = useFormat;
   m_info_c.m_format = useFormat;

   m_info_a.m_thread = NULL;
   m_info_b.m_thread = NULL;
   m_info_c.m_thread = NULL;
   m_info_a.m_buffer = NULL;
   m_info_b.m_buffer = NULL;
   m_info_c.m_buffer = NULL;

   size_t buffer_size = ALIGN_ROUND(m_info_a.m_width, 16) * m_info_a.m_height + 2048;

   bool memerror = false;
   bool interror = false;

   if ( encode ) {
      if ( !m_info_a.m_compressWorker.InitCompressBuffers( buffer_size ) || !m_info_b.m_compressWorker.InitCompressBuffers( buffer_size )) {
         memerror = true;
      }
   }
   if ( !memerror ){
      LPTHREAD_START_ROUTINE threadDelegate = encode ? EncodeWorkerTread : DecodeWorkerThread;

      if ( m_format == RGB32 ){
         if ( encode ){
            if ( !m_info_c.m_compressWorker.InitCompressBuffers( buffer_size )) {
               memerror = true;
            }
         }
         if ( !memerror ){
            if ( encode ){
               m_info_c.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_c, CREATE_SUSPENDED, NULL);
            } else {
               m_info_c.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_c, CREATE_SUSPENDED, NULL);
            }
         }
      }
      if ( !memerror ){
         if ( encode ){
            m_info_a.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_a, CREATE_SUSPENDED, NULL);
            m_info_b.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_b, CREATE_SUSPENDED, NULL);
         } else {
            m_info_a.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_a, CREATE_SUSPENDED, NULL);
            m_info_b.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_b, CREATE_SUSPENDED, NULL);
         }
         if ( !m_info_a.m_thread || !m_info_b.m_thread || (m_format==RGB32 && !m_info_c.m_thread )){
            interror = true;
         }
      }
   }

   if ( !memerror && !interror && encode ){
      m_info_a.m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_info_a.m_buffer,buffer_size,16,"Info_a.buffer");
      m_info_b.m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_info_b.m_buffer,buffer_size,16,"Info_b.buffer");
      if ( m_format == RGB32 ){
         m_info_c.m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_info_c.m_buffer,buffer_size,16,"Info_c.buffer");
         if ( m_info_c.m_buffer == NULL )
            memerror = true;
      }
      if (m_info_a.m_buffer == NULL || m_info_b.m_buffer == NULL ){
         memerror = true;
      }
   }

   if ( memerror || interror ){

      if ( encode ){
         ALIGNED_FREE(m_info_a.m_buffer,"Info_a.buffer");
         ALIGNED_FREE(m_info_b.m_buffer,"Info_b.buffer");
         m_info_a.m_compressWorker.FreeCompressBuffers();
         m_info_b.m_compressWorker.FreeCompressBuffers();
         m_info_c.m_compressWorker.FreeCompressBuffers();
         ALIGNED_FREE(m_info_c.m_buffer,"Info_c.buffer");
      }

      m_info_a.m_thread=NULL;
      m_info_b.m_thread=NULL;
      m_info_c.m_thread=NULL;
      if ( memerror ){
         return (DWORD)ICERR_MEMORY;
      } else {
         return (DWORD)ICERR_INTERNAL;
      }
   } else {
      return (DWORD)ICERR_OK;
   }
}

void CodecInst::EndThreads()
{
   m_info_a.m_length = UINT32_MAX;
   m_info_b.m_length = UINT32_MAX;
   m_info_c.m_length = UINT32_MAX;

   if(m_info_a.m_thread)
   {
      RESUME_THREAD(m_info_a.m_thread);
   }
   if(m_info_b.m_thread)
   {
      RESUME_THREAD(m_info_b.m_thread);
   }
   if(m_info_c.m_thread && m_format == RGB32)
   {
      RESUME_THREAD(m_info_c.m_thread);
   }

   while((m_info_a.m_length && m_info_a.m_thread) || (m_info_b.m_length && m_info_b.m_thread) || (m_info_c.m_length && m_format == RGB32 && m_info_c.m_thread))
   {
      Sleep(1);
   }

   if(!CloseHandle(m_info_a.m_thread))
   {
#ifdef _DEBUG
      MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0A", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }
   if(!CloseHandle(m_info_b.m_thread))
   {
#ifdef _DEBUG
      MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0B", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }
   if(m_info_c.m_thread && m_format == RGB32)
   {
      if(!CloseHandle(m_info_c.m_thread))
      {
#ifdef _DEBUG
         MessageBox (HWND_DESKTOP,"CloseHandle failed for thread 0x0C", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      }
   }

   m_info_a.m_thread = NULL;
   m_info_b.m_thread = NULL;
   m_info_c.m_thread = NULL;
   m_info_a.m_length = 0;
   m_info_b.m_length = 0;
   m_info_c.m_length = 0;
}