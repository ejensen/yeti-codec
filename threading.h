#pragma once
#include "compact.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define RESUME_THREAD(x) {\
   while(ResumeThread(x) != 1){\
      Sleep(0);\
   }\
}

#define WAIT_FOR_THREADS(threads) { \
   if (m_multithreading) { \
      if(threads == 2) { \
         while(m_info_a.m_length || m_info_b.m_length) \
            Sleep(0); \
      } else if(threads == 3){ \
         while (m_info_a.m_length || m_info_b.m_length || m_info_c.m_length) \
            Sleep(0); \
      } \
   } \
}

struct threadInfo
{
   HANDLE m_thread;
   CompressClass m_compressWorker;

   volatile const unsigned char* m_source;	// data source
   volatile unsigned char* m_dest;		// data destination
   unsigned char* m_buffer;
   unsigned int m_width;
   unsigned int m_height;
   unsigned int m_format;
   unsigned int m_lum;	         // needed for YUY2 prediction
   volatile unsigned int m_length;	// uncompressed data length
   volatile unsigned int m_size;		// compressed data length
   bool m_SSE2;
};

DWORD WINAPI EncodeWorkerTread(LPVOID i);
DWORD WINAPI DecodeWorkerThread(LPVOID i);