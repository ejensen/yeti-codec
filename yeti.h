#pragma once

#include <malloc.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>

#include "resource.h"

inline void* ALIGNED_MALLOC (void* ptr, size_t size, int align, char* str) 
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
         sprintf_s(msg,128,"An exception occurred when attempting to free non-null buffer '%s' in ALIGNED_MALLOC", str);
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
   _aligned_free((void*)ptr);\
} catch ( ... ){ } \
   } \
   ptr = NULL;\
}
#else
#define ALIGNED_FREE(ptr, str) { \
   if(ptr){ \
   try { _aligned_free((void*)ptr); } catch ( ... ){\
   char err_msg[256];\
   sprintf_s(err_msg, 256, "Error when attempting to free pointer %s, value = 0x%X - file %s line %d", str, ptr, __FILE__, __LINE__);\
   MessageBox(HWND_DESKTOP, err_msg, "Error", MB_OK | MB_ICONEXCLAMATION);\
} \
   } \
   ptr = NULL;\
}
#endif

static const DWORD FOURCC_YETI = mmioFOURCC('Y','E','T','I');
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');
static const DWORD FOURCC_UYVY = mmioFOURCC('U','Y','V','Y');
static const DWORD FOURCC_YV16 = mmioFOURCC('Y','V','1','6');
static const DWORD FOURCC_YV12 = mmioFOURCC('Y','V','1','2');

static const char SettingsFile[] = "yeti.ini";
static const char SettingsHeading[] = "settings";

// possible frame flags
#define DELTAFRAME            (BYTE)0x0
#define KEYFRAME              (BYTE)0x1

#define YUY2_FRAME            (BYTE)0x00
#define YUY2_DELTAFRAME       (YUY2_FRAME | DELTAFRAME)
#define YUY2_KEYFRAME         (YUY2_FRAME | KEYFRAME)

#define YV12_FRAME            (BYTE)0x10
#define YV12_DELTAFRAME       (YV12_FRAME | DELTAFRAME)
#define YV12_KEYFRAME         (YV12_FRAME | KEYFRAME)

// possible colorspaces
#define RGB24	   24
#define RGB32	   32
#define YUY2	   16
#define YV12	   12