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
         while(m_info_a.m_length || m_info_b.m_length) \
            Sleep(0); \
}

struct threadInfo
{
   HANDLE m_thread;
   CompressClass m_compressWorker;

   volatile const BYTE* m_source;
   volatile BYTE* m_dest;
   volatile BYTE* m_buffer;
   volatile size_t m_width;
   volatile size_t m_height;
   volatile size_t m_length;	// uncompressed data length
   volatile size_t m_size;		// compressed data length
   volatile unsigned int m_format;
};

DWORD WINAPI EncodeWorkerTread(LPVOID i);
DWORD WINAPI DecodeWorkerThread(LPVOID i);