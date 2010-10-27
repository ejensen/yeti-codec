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
#define TRY_CATCH_FUNC			// enables try/catch macro
#define bang(str) MessageBox(HWND_DESKTOP, str, "Error", MB_OK | MB_ICONEXCLAMATION)
#else
#define bang(str)
#endif

#ifdef TRY_CATCH_FUNC
#define try_catch( f ) \
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
#define try_catch( f ) f
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
#define aligned_free(ptr, str) { \
   if ( ptr ){ \
   try {\
   _aligned_free(ptr);\
} catch ( ... ){ } \
   } \
   ptr=NULL;\
}
#else
#define aligned_free(ptr, str) { \
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
#define align_round(x,y) ((((unsigned int)(x))+(y-1))&(~(y-1)))

#define Half(x) (x>>1)
#define Fourth(x) (x>>2)
#define Eighth(x) (x>>3)

#define Double(x) (x<<1)
#define Quadruple(x) (x<<2)

#include "resource.h"
#include "compact.h"

static const DWORD FOURCC_YETI = mmioFOURCC('Y','E','T','I');
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');
static const DWORD FOURCC_UYVY = mmioFOURCC('U','Y','V','Y');
static const DWORD FOURCC_YV12 = mmioFOURCC('Y','V','1','2');

static const char SettingsFile[] = "yeti.ini";

// possible frame flags
#define ARITH_YV12			1	// Standard YV12 frame.
#define REDUCED_RES        2	// Reduced Resolution frame.

// possible colorspaces
#define RGB24	24
#define RGB32	32
#define YUY2	16
#define YV12	12

struct threadinfo
{
   volatile const unsigned char * source;	// data source
   volatile unsigned char * dest;		// data destination
   unsigned char * buffer;	// buffer used for median prediction or restoration
   unsigned int SSE2;
   unsigned int SSE;
   unsigned int width;
   unsigned int height;
   unsigned int format;
   volatile unsigned int length;	// uncompressed data length
   volatile unsigned int size;		// compressed data length
   unsigned int lum;				// needed for YUY2 prediction
   HANDLE mutex;
   char mutex_name[12];
#ifdef _DEBUG
   char * name;
#endif
   HANDLE thread;
   CompressClass cObj;
};

class CodecInst
{
public:
   unsigned char * _pBuffer;
   unsigned char * _pPrev;
   const unsigned char * _pIn;
   unsigned char * _pOut;
   unsigned char * _pBuffer2;
   unsigned char * _pDelta;
   unsigned char * _pLossy_buffer;
   unsigned int _length;
   unsigned int _width;
   unsigned int _height;
   unsigned int _format;	//input format for compressing, output format for decompression. Also the bitdepth.
   bool _nullframes;
   bool _reduced;
   bool _multithreading;
   int _started;			//if the codec has been properly initialized yet
   threadinfo _info_a;
   threadinfo _info_b;
   threadinfo _info_c;
   CompressClass _cObj;

   int _SSE2;	
   int _SSE;
   int _MMX;

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

   void uncompact_macro( const unsigned char * _in, unsigned char * _out, unsigned int _length, unsigned int _width, unsigned int _height, threadinfo * _thread, int _format);
   int InitThreads( int encode);
   void EndThreads();

   int CompressYV12(ICCOMPRESS* icinfo);
   int CompressLossy(ICCOMPRESS* icinfo);
   int CompressReduced(ICCOMPRESS* icinfo);

   void ArithYV12Decompress();
   void ReduceResDecompress();
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);

#endif