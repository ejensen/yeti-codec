#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>

#include "threading.h"

class CodecInst
{
public:
   threadInfo m_info_a;
   threadInfo m_info_b;
   threadInfo m_info_c;
   CompressClass m_compressWorker;

   BYTE* m_buffer;
   BYTE* m_prevFrame;
   BYTE* m_in;
   BYTE* m_out;
   BYTE* m_buffer2;
   BYTE* m_deltaBuffer;
   BYTE* m_colorTransBuffer;
   unsigned int m_length;
   unsigned int m_width;
   unsigned int m_height;
   unsigned int m_format;	//input format for compressing, output format for decompression. Also the bitdepth.
   unsigned int m_compressFormat;
   bool m_nullframes;
   bool m_deltaframes;
   bool m_started;			//if the codec has been properly initialized yet

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
   DWORD Decompress(ICDECOMPRESS* idcinfo);
   DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
   DWORD DecompressEnd();

   BOOL QueryConfigure();

   void InitDecompressionThreads(const BYTE* in, BYTE* out, unsigned int length, threadInfo* thread);
   DWORD InitThreads(bool encode);
   void EndThreads();

   DWORD CompressYUV16(ICCOMPRESS* icinfo);
   DWORD CompressYV12(ICCOMPRESS* icinfo);
   DWORD CompressReduced(ICCOMPRESS* icinfo);

   void YUY2Decompress(DWORD flags);
   void YV12Decompress(DWORD flags);
   void ReduceResDecompress(DWORD flags);
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);

void __stdcall detectFlags(bool* SSE2, bool* SSE);