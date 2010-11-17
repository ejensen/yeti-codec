// Much of this was based off of Ben Rudiak-Gould's huffyuv source code

#ifndef _MAIN_HEADER
#define _MAIN_HEADER

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
#define TRY_CATCH( f ) \
   try { f }\
   catch ( char * tc_cmsg){\
   char * tc_msg = (char *)malloc(strlen(tc_cmsg)+128);\
   sprintf_s(tc_msg,strlen(tc_cmsg)+128"Exception passed up to %s, line %d.\nOriginal exception: %s\n", __FILE__, __LINE__,tc_cmsg);\
   MessageBox (HWND_DESKTOP, tc_msg, "Error", MB_OK | MB_ICONEXCLAMATION);\
   throw tc_msg;\
}\
   catch (...){\
   char * tc_msg = (char*)malloc(128);\
   sprintf_s(tc_msg,128,"Exception caught in %s, line %d", __FILE__, __LINE__);\
   MessageBox (HWND_DESKTOP, tc_msg, "Error", MB_OK | MB_ICONEXCLAMATION); \
   throw tc_msg;\
}
#else
#define TRY_CATCH( f ) f
#endif

inline void * aligned_malloc( void *ptr, int size, int align, char *str ) 
{
   if ( ptr )
   {
      try
      {
#ifdef _DEBUG
         char msg[128];
         sprintf_s(msg,128,"Buffer '%s' is not null, attempting to free it...",str);
         MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
         _aligned_free(ptr);
      } 
      catch ( ... )
      {
#ifdef _DEBUG
         char msg[256];
         sprintf_s(msg,128,"An exception occurred when attempting to free non-null buffer '%s' in aligned_malloc",str);
         MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
      }
   }
   return _aligned_malloc(size, align);
}

#ifndef _DEBUG
#define ALIGNED_FREE(ptr, str) { \
   if ( ptr ){ \
   try {\
   _aligned_free(ptr);\
} catch ( ... ){ } \
   } \
   ptr=NULL;\
}
#else
#define ALIGNED_FREE(ptr, str) { \
   if ( ptr ){ \
   try { _aligned_free(ptr); } catch ( ... ){\
   char err_msg[256];\
   sprintf_s(err_msg,256,"Error when attempting to free pointer %s, value = 0x%X - file %s line %d",str,ptr,__FILE__, __LINE__);\
   MessageBox (HWND_DESKTOP, err_msg, "Error", MB_OK | MB_ICONEXCLAMATION);\
} \
   } \
   ptr=NULL;\
}
#endif


// y must be 2^n
#define ALIGN_ROUND(x,y) ((((unsigned int)(x))+(y-1))&(~(y-1)))

#define HALF(x) (x>>1)
#define FOURTH(x) (x>>2)
#define EIGHTH(x) (x>>3)

#define DOUBLE(x) (x<<1)
#define QUADRUPLE(x) (x<<2)

static const DWORD FOURCC_YETI = mmioFOURCC('Y','E','T','I');
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');
static const DWORD FOURCC_YV12 = mmioFOURCC('Y','V','1','2');

static const char SettingsFile[] = "yeti.ini";

// possible frame flags
#define DELTAFRAME            0x0
#define KEYFRAME              0x1

#define YV12_FRAME            0x10
#define YV12_DELTAFRAME       (YV12_FRAME | DELTAFRAME)
#define YV12_KEYFRAME         (YV12_FRAME | KEYFRAME)

#define REDUCED_FRAME         0x20
#define REDUCED_DELTAFRAME    (REDUCED_FRAME | DELTAFRAME)
#define REDUCED_KEYFRAME      (REDUCED_FRAME | KEYFRAME)

// possible colorspaces
#define RGB24	24
#define RGB32	32
#define YUY2	16
#define YV12	12

struct threadinfo
{
   volatile const unsigned char * m_pSource;	// data source
   volatile unsigned char * m_pDest;		// data destination
   unsigned char * m_pBuffer;	// buffer used for median prediction or restoration
   unsigned int m_width;
   unsigned int m_height;
   unsigned int m_format;
   bool m_keyframe;
   bool m_SSE2;
   volatile unsigned int m_length;	// uncompressed data length
   volatile unsigned int m_size;		// compressed data length
#ifdef _DEBUG
   char * m_strName;
#endif
   HANDLE m_thread;
   CompressClass m_cObj;
};

class CodecInst
{
public:
   unsigned char * m_pBuffer;
   unsigned char * m_pPrev;
   unsigned char * m_pIn;
   unsigned char * m_pOut;
   unsigned char * m_pBuffer2;
   unsigned char * m_pDelta;
   unsigned char * m_pColorTransformBuffer;
   unsigned int m_length;
   unsigned int m_width;
   unsigned int m_height;
   unsigned int m_format;	//input format for compressing, output format for decompression. Also the bitdepth.
   bool m_nullframes;
   bool m_deltaframes;
   bool m_reduced;
   bool m_multithreading;
   bool m_started;			//if the codec has been properly initialized yet
   threadinfo m_info_a;
   threadinfo m_info_b;
   threadinfo m_info_c;
   CompressClass m_cObj;

   bool m_SSE2;	
   bool m_SSE;

   CodecInst();
   ~CodecInst();

   inline unsigned int COUNT_BITS(unsigned int v)
   {
      v = v - ((v >> 1) & 0x55555555);
      v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
      return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
   }

   void Fast_XOR(void* dest, const void* src1, const void* src2, const unsigned int len ) 
   {
      unsigned* tempDest = (unsigned*)dest;
      unsigned* tempSrc1 = (unsigned*)src1;
      unsigned* tempSrc2 = (unsigned*)src2;
      for(unsigned i = 0; i < FOURTH(len); i++)
      {
         tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
      }
   }

   /*unsigned long Fast_XOR_Count(void* dest, const void* src1, const void* src2, const unsigned int len) 
   {
      unsigned* tempDest = (unsigned*)dest;
      unsigned* tempSrc1 = (unsigned*)src1;
      unsigned* tempSrc2 = (unsigned*)src2;

      unsigned long bitCount= 0;

      for(unsigned i = 0; i < FOURTH(len); i++)
      {
         tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
         bitCount += COUNT_BITS(tempDest[i]);
      }

      return bitCount;
   }*/

   const unsigned long Fast_XOR_Count(void* dest, const void* src1, const void* src2, const unsigned int len, const unsigned long max)
   {
      unsigned* tempDest = (unsigned*)dest;
      unsigned* tempSrc1 = (unsigned*)src1;
      unsigned* tempSrc2 = (unsigned*)src2;

      unsigned long bitCount = 0;

      for(unsigned int i = 0; bitCount < max && i < FOURTH(len); i++)
      {
         tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
         bitCount += COUNT_BITS(tempDest[i]);
      }

      return bitCount;
   }

   DWORD GetState(LPVOID pv, DWORD dwSize);
   DWORD SetState(LPVOID pv, DWORD dwSize);
   DWORD Configure(HWND hwnd);
   DWORD GetInfo(ICINFO* icinfo, DWORD dwSize);

   DWORD CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD Compress(ICCOMPRESS* icinfo, DWORD dwSize);
   DWORD CompressEnd();

   DWORD DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD Decompress(ICDECOMPRESS* icinfo, DWORD dwSize);
   DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD DecompressEnd();

   BOOL QueryConfigure();

   void InitDecompressionThreads(const unsigned char * in, unsigned char * out, unsigned int length, unsigned int width, unsigned int height, threadinfo * thread, int format, bool keyframe);
   DWORD InitThreads(int encode);
   void EndThreads();

   DWORD CompressYV12(ICCOMPRESS* icinfo);
   DWORD CompressLossy(ICCOMPRESS* icinfo);
   DWORD CompressReduced(ICCOMPRESS* icinfo);

   void YV12Decompress(bool keyframe);
   void ReduceResDecompress();
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);

#endif