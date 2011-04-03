#pragma once
#include "common.h"

void Fast_Add(BYTE* __restrict dest, const BYTE* __restrict src1, const BYTE* __restrict src2, const size_t len);
unsigned __int64 Fast_Sub_Count(BYTE* __restrict dest, const BYTE* __restrict src1, const BYTE* __restrict src2, const size_t len, const unsigned __int64 minDelta);

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