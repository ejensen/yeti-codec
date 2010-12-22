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

/*static inline void Fast_XOR(void* dest, const void* src1, const void* src2, const unsigned int len) 
{
   unsigned int* tempDest = (unsigned int*)dest;
   unsigned int* tempSrc1 = (unsigned int*)src1;
   unsigned int* tempSrc2 = (unsigned int*)src2;
   for(unsigned i = 0; i < len; i++)
   {
      tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
   }
}*/

static inline void Fast_Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const unsigned int len) 
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

/*static inline void Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const unsigned int len) 
{
   for(unsigned i = 0; i < len; i++)
   {
      dest[i] = src1[i] + src2[i];
   }
}*/

static inline unsigned long Fast_Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const unsigned int len, const unsigned long minDelta)
{
   unsigned long oldTotalBits = 0;
   unsigned long totalBits = 0;

   unsigned int* intDest = (unsigned int*)dest;
   unsigned int* intSrc = (unsigned int*)src1;

   __m64* mxDest = (__m64*) dest;
   __m64* mxSrc1 = (__m64*) src1;
   __m64* mxSrc2 = (__m64*) src2;

   _mm_empty();

   for(unsigned int i = 0; i < FOURTH(len); i += 2)
   {
      oldTotalBits += COUNT_BITS(intSrc[i]) + COUNT_BITS(intSrc[i+1]);

      *mxDest++ = _mm_sub_pi8(*mxSrc1++, *mxSrc2++);

      totalBits += COUNT_BITS(intDest[i]) + COUNT_BITS(intDest[i+1]);
   }

   _mm_empty();

   return (oldTotalBits - totalBits < minDelta) ? ULONG_MAX : totalBits;
}

/*static inline unsigned long Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const unsigned int len, const unsigned long minDelta)
{
   unsigned int* intDest = (unsigned int*)dest;
   unsigned int* intSrc = (unsigned int*)src1;

   unsigned long oldTotalBits = 0;
   unsigned long totalBits = 0;

   for(unsigned int i = 0; i < len; i++)
   {
      oldTotalBits += COUNT_BITS(intSrc[i]);

      unsigned int j = QUADRUPLE(i);

      dest[j]     = src1[j]   - src2[j];
      dest[j+1]   = src1[j+1] - src2[j+1];
      dest[j+2]   = src1[j+2] - src2[j+2];
      dest[j+3]   = src1[j+3] - src2[j+3];

      totalBits += COUNT_BITS(intDest[i]);
   }

   return (oldTotalBits - totalBits < minDelta) ? ULONG_MAX : totalBits;
}*/

/*static inline unsigned long Fast_XOR_Count(void* dest, const void* src1, const void* src2, const unsigned int len, const unsigned long minDelta)
{
   unsigned int* tempDest = (unsigned int*)dest;
   unsigned int* tempSrc1 = (unsigned int*)src1;
   unsigned int* tempSrc2 = (unsigned int*)src2;

   unsigned long oldTotalBits = 0;
   unsigned long totalBits = 0;

   for(unsigned int i = 0; i < len; i++)
   {
      oldTotalBits += COUNT_BITS(tempSrc1[i]);
      tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
      totalBits += COUNT_BITS(tempDest[i]);
   }

   return (oldTotalBits - totalBits < minDelta) ? ULONG_MAX : totalBits;
}*/

class CompressClass 
{
public:
   unsigned int* m_probRanges;		// Byte probability range table
   unsigned int m_scale;				// Used to replace some multiply/divides with binary shifts,
   // (1<<scale) is equal to the cumulative probability of all bytes
   unsigned int* m_bytecounts;		// Byte frequency table
   BYTE* m_buffer;			// buffer to perform RLE

   CompressClass();
   ~CompressClass();

   bool InitCompressBuffers(const unsigned int length);
   void FreeCompressBuffers();

   unsigned int Compact(const BYTE* in, BYTE* out, const unsigned int length);
   void Uncompact(const BYTE* in, BYTE* out, const unsigned int length);
   void CalcBitProbability(const BYTE* in, const unsigned int length);
   void ScaleBitProbability(const unsigned int length);
   unsigned int ReadBitProbability(const BYTE* in);
   unsigned int RangeEncode( const BYTE* in, BYTE* out, const unsigned int length);
   void RangeDecode(const BYTE* in, BYTE* out, const unsigned int length);
};