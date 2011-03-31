#pragma once

// from from Ben Rudiak-Gould's Huffyuv source code
extern "C" 
{
   void __cdecl mmx_YUY2toRGB24(const BYTE* src, BYTE* dst, const BYTE* src_end, int stride);
   void __cdecl mmx_YUY2toRGB32(const BYTE* src, BYTE* dst, const BYTE* src_end, int stride);
}