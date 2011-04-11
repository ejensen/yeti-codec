#include <stdint.h>
#include "yeti.h"
#include "codec_inst.h"
#include "threading.h"
#include "prediction.h"

DWORD WINAPI EncodeWorkerTread(LPVOID i)
{
   threadInfo* info = (threadInfo*)i;
   const size_t width = info->m_width;
   //const size_t height = info->m_height;
  // const size_t length = width * height;

   const BYTE* src = NULL;
   BYTE* dest = NULL;
   const unsigned int format=info->m_format;
   BYTE* const buffer = (BYTE*)info->m_buffer;
   assert(buffer != NULL);

   WaitForSingleObject(info->m_startEvent, INFINITE);
   while(true)
   {
      unsigned int length = info->m_length;
      if ( length == UINT32_MAX )
      {
         break;
      } 
      else if ( length > 0 )
      {
         src = (const BYTE*)info->m_source;
         dest = (BYTE*)info->m_dest;

         BYTE* dst = buffer + ((unsigned int)src&15);
         assert(dst != NULL);

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
      } 
      else
      {
         assert(false);
      }
      SignalObjectAndWait(info->m_doneEvent, info->m_startEvent, INFINITE, false);
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

   WaitForSingleObject(info->m_startEvent, INFINITE);
   while(true)
   {
      unsigned int length = info->m_length;
      if ( length == UINT32_MAX )
      {
         break;
      } 
      else if ( length > 0 )
      {
         src = (BYTE*)info->m_source;
         dest = (BYTE*)info->m_dest;

         assert(info->m_length>0);
         info->m_compressWorker.Uncompact(src, dest, info->m_length);
         info->m_length = 0;
      } 
      else
      {
         assert(false);
      }
      SignalObjectAndWait(info->m_doneEvent, info->m_startEvent, INFINITE, false);
   }

   info->m_compressWorker.FreeCompressBuffers();
   ALIGNED_FREE(info->m_buffer, "Thread Buffer");
   info->m_length = 0;
   return 0;
}

DWORD CodecInst::InitThreads(bool encode)
{
   m_info_a.m_length = m_info_b.m_length = 0;

   m_info_a.m_startEvent = CreateEvent(NULL ,false, false, NULL);
   m_info_a.m_doneEvent = CreateEvent(NULL, false, false, NULL);
   m_info_b.m_startEvent = CreateEvent(NULL, false, false, NULL);
   m_info_b.m_doneEvent = CreateEvent(NULL, false, false, NULL);

   unsigned int useFormat = (encode) ? m_compressFormat : m_format;

   m_info_a.m_width =  m_info_b.m_width = HALF(m_width);
   m_info_a.m_height = m_info_b.m_height = (useFormat != YV12) ? m_height : HALF(m_height); m_height;

   m_info_a.m_format =  m_info_b.m_format = useFormat;

   m_info_a.m_thread = m_info_b.m_thread = NULL;
   m_info_a.m_buffer = m_info_b.m_buffer = NULL;

   size_t buffer_size = ALIGN_ROUND(m_info_a.m_width, 16) * m_info_a.m_height + 256;

   bool memerror = false;
   bool interror = false;

   if ( encode ) 
   {
      if ( !m_info_a.m_compressWorker.InitCompressBuffers( buffer_size ) || !m_info_b.m_compressWorker.InitCompressBuffers( buffer_size )) {
         memerror = true;
      }
   }
   if ( !memerror )
   {
      LPTHREAD_START_ROUTINE threadDelegate = encode ? EncodeWorkerTread : DecodeWorkerThread;

      if ( !memerror )
      {
         if ( encode )
         {
            m_info_a.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_a, CREATE_SUSPENDED, NULL);
            m_info_b.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_b, CREATE_SUSPENDED, NULL);
         } 
         else
         {
            m_info_a.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_a, CREATE_SUSPENDED, NULL);
            m_info_b.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_b, CREATE_SUSPENDED, NULL);
         }
         if ( !m_info_a.m_thread || !m_info_b.m_thread )
         {
            interror = true;
         } 
         else 
         {
            SetThreadPriority(m_info_a.m_thread, THREAD_PRIORITY_BELOW_NORMAL);
            SetThreadPriority(m_info_b.m_thread, THREAD_PRIORITY_BELOW_NORMAL);
         }
      }
   }

   if ( !memerror && !interror && encode )
   {
      m_info_a.m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_info_a.m_buffer,buffer_size,16,"Info_a.buffer");
      m_info_b.m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_info_b.m_buffer,buffer_size,16,"Info_b.buffer");
      if (m_info_a.m_buffer == NULL || m_info_b.m_buffer == NULL ){
         memerror = true;
      }
   }

   if ( memerror || interror )
   {
      if ( encode )
      {
         ALIGNED_FREE(m_info_a.m_buffer,"Info_a.buffer");
         ALIGNED_FREE(m_info_b.m_buffer,"Info_b.buffer");
         m_info_a.m_compressWorker.FreeCompressBuffers();
         m_info_b.m_compressWorker.FreeCompressBuffers();
      }

      m_info_a.m_thread=NULL;
      m_info_b.m_thread=NULL;

      return (DWORD)((memerror) ? ICERR_MEMORY : ICERR_INTERNAL);
   } 
   else 
   {
      ResumeThread(m_info_a.m_thread);
      ResumeThread(m_info_b.m_thread);
      return (DWORD)ICERR_OK;
   }
}

void CodecInst::EndThreads()
{
   m_info_a.m_length = UINT32_MAX;
   m_info_b.m_length = UINT32_MAX;

   if(m_info_a.m_thread)
   {
      SetEvent(m_info_a.m_startEvent);
   }
   if(m_info_b.m_thread)
   {
      SetEvent(m_info_b.m_startEvent);
   }

   HANDLE threads[2];
   threads[0] = m_info_a.m_thread;
   threads[1] = m_info_b.m_thread;
   WaitForMultipleObjectsEx(2, &threads[0], true, 10000, true);

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

   m_info_a.m_thread = NULL;
   m_info_b.m_thread = NULL;
   m_info_a.m_length = 0;
   m_info_b.m_length = 0;
}