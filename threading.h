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
      if(threads == 2) { \
         while(m_info_a.m_length || m_info_b.m_length) \
            Sleep(0); \
      } else if(threads == 3){ \
         while (m_info_a.m_length || m_info_b.m_length || m_info_c.m_length) \
            Sleep(0); \
      } \
}

struct threadInfo
{
   HANDLE m_thread;
   CompressClass m_compressWorker;

   volatile const BYTE* m_source;	// data source
   volatile BYTE* m_dest;		// data destination
   BYTE* m_buffer;
   volatile unsigned int m_width;
   volatile unsigned int m_height;
   volatile unsigned int m_format;
   volatile unsigned int m_length;	// uncompressed data length
   volatile unsigned int m_size;		// compressed data length
};

DWORD WINAPI EncodeWorkerTread(LPVOID i);
DWORD WINAPI DecodeWorkerThread(LPVOID i);