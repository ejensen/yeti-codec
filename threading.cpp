#include <stdint.h>
#include "yeti.h"
#include "threading.h"
#include "prediction.h"

DWORD WINAPI EncodeWorkerTread(LPVOID i)
{
   threadInfo * info = (threadInfo*)i;
   const unsigned int width = info->m_width;
   const unsigned int height = info->m_height;

   const unsigned int SSE2 = info->m_SSE2;
   const unsigned int stride = ALIGN_ROUND(width, (SSE2 ? 16 : 8));

   const unsigned char * src = NULL;
   unsigned char * dest = NULL;
   unsigned char * const buffer = (unsigned char*)info->m_buffer;
//   const unsigned int format=info->m_format;

   while(info->m_length != UINT32_MAX) //TODO: Optimize
   {
      src = (const unsigned char*)info->m_source;
      dest = (unsigned char*)info->m_dest;

      unsigned char * dst = (width == stride) ? buffer : (unsigned char*)ALIGN_ROUND(dest, 16);

      //if( info->m_keyframe )
      //{
         if( SSE2 )
         {
            SSE2_BlockPredict(src, dst, stride, stride * height);
         } 
         else 
         {
            MMX_BlockPredict(src, dst, stride, stride * height);
         }

         if(width != stride)
         {
            unsigned char * padded = dst;
            unsigned char * stripped = buffer;
            for(unsigned int y = 0; y < height; y++)
            {
               memcpy(stripped + y * width, padded + y * stride, width);
            }
         }
      //}
      //else
      //{
      //   memcpy(buffer, src, stride * height);
      //}

      info->m_size = info->m_compressWorker.Compact(buffer, dest, width * height);

      assert(*(__int64*)dest != 0);

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
   threadInfo * info = (threadInfo*)i;
   unsigned int width;
   unsigned int height;
   unsigned char * src = NULL;
   unsigned char * dest = NULL;
   unsigned int format = YV12;
   unsigned int length;

   while(info->m_length != UINT32_MAX) //TODO:Optimize
   {
      src = (unsigned char*)info->m_source;
      dest = (unsigned char*)info->m_dest;

      length = info->m_length;
      //format = info->m_format;
      width = info->m_width;
      height = info->m_height;

      info->m_compressWorker.Uncompact(src, dest, length);

      //if(info->m_keyframe)
      //{
         ASM_BlockRestore(dest, width, width*height, format != YV12);
      //}

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

   m_info_a.m_width = m_width;
   m_info_b.m_width = HALF(m_width);
   m_info_c.m_width = m_width;

   m_info_a.m_height = m_height;
   m_info_b.m_height = HALF(m_height);
   m_info_c.m_height = m_height;

   if(m_reduced)
   {
      m_info_a.m_width = HALF(m_width);
      m_info_a.m_height = HALF(m_height);
      m_info_b.m_width = FOURTH(m_width);
      m_info_b.m_height = FOURTH(m_height);
   }

   //m_info_a.m_format = use_format;
   //m_info_b.m_format = use_format;
   //m_info_c.m_format = use_format;

   m_info_a.m_SSE2 = m_SSE2;
   m_info_b.m_SSE2 = m_SSE2;
   m_info_c.m_SSE2 = m_SSE2;

   m_info_a.m_thread = NULL;
   m_info_b.m_thread = NULL;
   m_info_c.m_thread = NULL;
   m_info_a.m_buffer = NULL;
   m_info_b.m_buffer = NULL;
   m_info_c.m_buffer = NULL;

   unsigned int buffer_a = DOUBLE(ALIGN_ROUND(m_info_a.m_width, 16) * m_info_a.m_height) + 2048;
   unsigned int buffer_b = DOUBLE(ALIGN_ROUND(m_info_b.m_width, 16) * m_info_b.m_height) + 2048;

   if(!m_info_a.m_compressWorker.InitCompressBuffers(buffer_a) || !m_info_b.m_compressWorker.InitCompressBuffers(buffer_b) 
      || !(m_info_a.m_buffer = (unsigned char*)aligned_malloc(m_info_a.m_buffer, buffer_a, 16, "Info_a.buffer"))
      || !(m_info_b.m_buffer = (unsigned char*)aligned_malloc(m_info_b.m_buffer, buffer_b, 16, "Info_b.buffer")))
   {
      m_info_a.m_compressWorker.FreeCompressBuffers();
      m_info_b.m_compressWorker.FreeCompressBuffers();
      ALIGNED_FREE(m_info_a.m_buffer, "Info_a.buffer");
      ALIGNED_FREE(m_info_b.m_buffer, "Info_b.buffer");
      m_info_a.m_thread = NULL;
      m_info_b.m_thread = NULL;
      return (DWORD)ICERR_MEMORY;
   } 
   else 
   { 
      LPTHREAD_START_ROUTINE threadDelegate = encode ? EncodeWorkerTread : DecodeWorkerThread;
      if(m_format == RGB32)
      {
         if(!m_info_c.m_compressWorker.InitCompressBuffers(buffer_a * 3) 
            || !(m_info_c.m_buffer = (unsigned char *)aligned_malloc(m_info_c.m_buffer, buffer_a, 16, "Info_c.buffer")))
         {
            m_info_a.m_compressWorker.FreeCompressBuffers();
            m_info_b.m_compressWorker.FreeCompressBuffers();
            m_info_c.m_compressWorker.FreeCompressBuffers();
            ALIGNED_FREE(m_info_a.m_buffer, "Info_a.buffer");
            ALIGNED_FREE(m_info_b.m_buffer, "Info_b.buffer");
            ALIGNED_FREE(m_info_c.m_buffer, "Info_c.buffer");
            m_info_a.m_thread = NULL;
            m_info_b.m_thread = NULL;
            m_info_c.m_thread = NULL;
            return (DWORD)ICERR_MEMORY;
         }

         m_info_c.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_c, CREATE_SUSPENDED, NULL);
      }

      m_info_a.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_a, CREATE_SUSPENDED, NULL);
      m_info_b.m_thread = CreateThread(NULL, 0, threadDelegate, &m_info_b, CREATE_SUSPENDED, NULL);

      if(!m_info_a.m_thread || !m_info_b.m_thread || (m_format == RGB32 && !m_info_c.m_thread))
      {
         m_info_a.m_compressWorker.FreeCompressBuffers();
         m_info_b.m_compressWorker.FreeCompressBuffers();
         if(m_format == RGB32)
         {
            m_info_c.m_compressWorker.FreeCompressBuffers();
            ALIGNED_FREE(m_info_c.m_buffer, "Info_c.buffer");
         }
         ALIGNED_FREE(m_info_a.m_buffer, "Info_a.buffer");
         ALIGNED_FREE(m_info_b.m_buffer, "Info_b.buffer");
         m_info_a.m_thread = NULL;
         m_info_b.m_thread = NULL;
         m_info_c.m_thread = NULL;
         return (DWORD)ICERR_INTERNAL;
      }
   }

   SetThreadPriority(m_info_a.m_thread, THREAD_PRIORITY_ABOVE_NORMAL); 

   return (DWORD)ICERR_OK;
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