#pragma once

#include <limits.h>
#include <mmintrin.h>
#include "common.h"


static inline unsigned int COUNT_BITS(unsigned int v)
{
   v = v - ((v >> 1) & 0x55555555);
   v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
   return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static inline void Fast_Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len) 
{
   __m64* mxSrc1 = (__m64*) src1;
   __m64* mxSrc2 = (__m64*) src2;
   __m64* mxDest = (__m64*) dest;
   const __m64* end = mxDest + EIGHTH(len);

   _mm_empty();

   while(mxDest < end)
   {
      *mxDest++ = _mm_add_pi8(*mxSrc1++, *mxSrc2++);
   }

   _mm_empty();
}

static inline unsigned __int64 Fast_Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len, const unsigned __int64 minDelta)
{
   unsigned __int64 oldTotalBits = 0;
   unsigned __int64 totalBits = 0;

   unsigned int* intDest = (unsigned int*)dest;
   unsigned int* intSrc = (unsigned int*)src1;

   __m64* mxDest = (__m64*) dest;
   __m64* mxSrc1 = (__m64*) src1;
   __m64* mxSrc2 = (__m64*) src2;

   _mm_empty();

   for(size_t i = 0; i < FOURTH(len); i += 2)
   {
      oldTotalBits += COUNT_BITS(intSrc[i]) + COUNT_BITS(intSrc[i+1]);
      *mxDest++ = _mm_sub_pi8(*mxSrc1++, *mxSrc2++);
      totalBits += COUNT_BITS(intDest[i]) + COUNT_BITS(intDest[i+1]);
   }

   _mm_empty();

   return (oldTotalBits - totalBits < minDelta) ? ULLONG_MAX : totalBits;
}

class CompressClass 
{
public:
   unsigned int m_probRanges[257];		// Byte probability range table
   unsigned int m_scale;				// Used to replace some multiply/divides with binary shifts,
   // (1<<scale) is equal to the cumulative probability of all bytes
   BYTE* m_buffer;

   CompressClass();
   ~CompressClass();

   bool InitCompressBuffers(const size_t length);
   void FreeCompressBuffers();

   size_t Compact(BYTE* __restrict in, BYTE* __restrict out, const size_t length);
   void Uncompact(const BYTE* __restrict in, BYTE* __restrict out, const size_t length);
   size_t CalcBitProbability(const BYTE* in, const size_t length, BYTE* out = 0);
   void ScaleBitProbability(const size_t length);
   size_t ReadBitProbability(const BYTE* in);
   size_t RangeEncode(const BYTE* in, BYTE* out, const size_t length);
   void Decode_And_DeRLE(const BYTE* __restrict in, BYTE* __restrict out, const size_t length, unsigned int level);
};