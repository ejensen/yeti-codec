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
   return _aligned_malloc(size,align);
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

#include "resource.h"
#include "compact.h"

static const DWORD FOURCC_YETI = mmioFOURCC('Y','E','T','I');
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');
static const DWORD FOURCC_YV12 = mmioFOURCC('Y','V','1','2');

static const char SettingsFile[] = "yeti.ini";

// possible frame flags
#define YV12_FRAME			1	// Standard YV12 frame.
#define REDUCED_RES        2	// Reduced Resolution frame.

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
   unsigned char * _pOut;
   unsigned char * m_pBuffer2;
   //unsigned char * m_pDelta;
   unsigned char * m_pLossy_buffer;
   unsigned int m_length;
   unsigned int m_width;
   unsigned int m_height;
   unsigned int m_format;	//input format for compressing, output format for decompression. Also the bitdepth.
   bool m_nullframes;
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

   void uncompact_macro( const unsigned char * in, unsigned char * out, unsigned int length, unsigned int width, unsigned int height, threadinfo * thread, int format);
   DWORD InitThreads( int encode);
   void EndThreads();

   DWORD CompressYV12(ICCOMPRESS* icinfo);

   inline void Fast_XOR(void* dest, const void* src1, const void* src2, const unsigned int len ) 
   {
      unsigned* tempDest = (unsigned*)dest;
      unsigned* tempSrc1 = (unsigned*)src1;
      unsigned* tempSrc2 = (unsigned*)src2;
      for(unsigned i = 0; i < FOURTH(len); i++)
      {
         tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
      }
   }

   DWORD CompressLossy(ICCOMPRESS* icinfo);
   DWORD CompressReduced(ICCOMPRESS* icinfo);

   void YV12Decompress();
   void ReduceResDecompress();
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);

#endif