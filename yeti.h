#pragma once

#include <process.h>
#include <malloc.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>
#include <malloc.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "resource.h"
#include "compact.h"

#ifdef _DEBUG
#define TRY_CATCH(f) \
   try { f }\
   catch(char * tc_cmsg){\
   char * tc_msg = (char*)malloc(strlen(tc_cmsg) + 128);\
   sprintf_s(tc_msg, strlen(tc_cmsg) + 128"Exception passed up to %s, line %d.\nOriginal exception: %s\n", __FILE__, __LINE__,tc_cmsg);\
   MessageBox(HWND_DESKTOP, tc_msg, "Error", MB_OK | MB_ICONEXCLAMATION);\
   throw tc_msg;\
}\
   catch (...){\
   char * tc_msg = (char*)malloc(128);\
   sprintf_s(tc_msg, 128,"Exception caught in %s, line %d", __FILE__, __LINE__);\
   MessageBox(HWND_DESKTOP, tc_msg, "Error", MB_OK | MB_ICONEXCLAMATION); \
   throw tc_msg;\
}
#else
#define TRY_CATCH(f) f
#endif

inline void* aligned_malloc(void *ptr, int size, int align, char *str) 
{
   if(ptr)
   {
      try
      {
#ifdef _DEBUG
         char msg[128];
         sprintf_s(msg, 128, "Buffer '%s' is not null, attempting to free it...", str);
         MessageBox(HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
         _aligned_free(ptr);
      } 
      catch ( ... )
      {
#ifdef _DEBUG
         char msg[256];
         sprintf_s(msg,128,"An exception occurred when attempting to free non-null buffer '%s' in aligned_malloc", str);
         MessageBox(HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      }
   }
   return _aligned_malloc(size, align);
}

#ifndef _DEBUG
#define ALIGNED_FREE(ptr, str) { \
   if (ptr){ \
   try {\
   _aligned_free(ptr);\
} catch ( ... ){ } \
   } \
   ptr = NULL;\
}
#else
#define ALIGNED_FREE(ptr, str) { \
   if(ptr){ \
   try { _aligned_free(ptr); } catch ( ... ){\
   char err_msg[256];\
   sprintf_s(err_msg, 256, "Error when attempting to free pointer %s, value = 0x%X - file %s line %d", str, ptr, __FILE__, __LINE__);\
   MessageBox(HWND_DESKTOP, err_msg, "Error", MB_OK | MB_ICONEXCLAMATION);\
} \
   } \
   ptr = NULL;\
}
#endif

#define SWAP(x, y) { unsigned int xchng = *(unsigned int*)(&x); x = y; *(unsigned int*)(&y) = xchng; }

// y must be 2^n
#define ALIGN_ROUND(x, y) ((((unsigned int)(x)) + (y - 1))&(~(y - 1)))

#define HALF(x) (x>>1)
#define FOURTH(x) (x>>2)
#define EIGHTH(x) (x>>3)

#define DOUBLE(x) (x<<1)
#define QUADRUPLE(x) (x<<2)

static const DWORD FOURCC_YETI = mmioFOURCC('Y','E','T','I');
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');
static const DWORD FOURCC_UYVY = mmioFOURCC('U','Y','V','Y');
static const DWORD FOURCC_YV12 = mmioFOURCC('Y','V','1','2');

static const char SettingsFile[] = "yeti.ini";
static const char SettingsHeading[] = "settings";

// possible frame flags
#define DELTAFRAME            0x0
#define KEYFRAME              0x1

#define YUY2_FRAME            0x00
#define YUY2_DELTAFRAME       (YUY2_FRAME | DELTAFRAME)
#define YUY2_KEYFRAME         (YUY2_FRAME | KEYFRAME)

#define YV12_FRAME            0x10
#define YV12_DELTAFRAME       (YV12_FRAME | DELTAFRAME)
#define YV12_KEYFRAME         (YV12_FRAME | KEYFRAME)

#define REDUCED_FRAME         0x20
#define REDUCED_DELTAFRAME    (REDUCED_FRAME | DELTAFRAME)
#define REDUCED_KEYFRAME      (REDUCED_FRAME | KEYFRAME)

// possible colorspaces
#define RGB24	   24
#define RGB32	   32
#define YUY2	   16
#define YV12	   12
#define REDUCED   6