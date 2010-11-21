#pragma once

static inline unsigned int COUNT_BITS(unsigned int v)
{
   v = v - ((v >> 1) & 0x55555555);
   v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
   return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static inline void Fast_XOR(void* dest, const void* src1, const void* src2, const unsigned int len) 
{
   unsigned* tempDest = (unsigned*)dest;
   unsigned* tempSrc1 = (unsigned*)src1;
   unsigned* tempSrc2 = (unsigned*)src2;
   for(unsigned i = 0; i < len; i++)
   {
      tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
   }
}

static const inline unsigned long Fast_XOR_Count(void* dest, const void* src1, const void* src2, const unsigned int len, const unsigned long max)
{
   unsigned* tempDest = (unsigned*)dest;
   unsigned* tempSrc1 = (unsigned*)src1;
   unsigned* tempSrc2 = (unsigned*)src2;

   unsigned long bitCount = 0;

   for(unsigned int i = 0; bitCount < max && i < len; i++)
   {
      tempDest[i] = tempSrc1[i] ^ tempSrc2[i];
      bitCount += COUNT_BITS(tempDest[i]);
   }

   return bitCount;
}

class CompressClass 
{
public:
   unsigned int * m_probRanges;		// Byte probability range table
   unsigned int m_scale;				// Used to replace some multiply/divides with binary shifts,
   // (1<<scale) is equal to the cumulative probability of all bytes
   unsigned int * m_bytecounts;		// Byte frequency table
   unsigned char * m_buffer;			// buffer to perform RLE

   CompressClass();
   ~CompressClass();

   bool InitCompressBuffers(const unsigned int length);
   void FreeCompressBuffers();

   unsigned int Compact( const unsigned char * in, unsigned char * out, const unsigned int length);
   void Uncompact( const unsigned char * in, unsigned char * out, const unsigned int length);
   void CalcBitProbability(const unsigned char * in, const unsigned int length);
   void ScaleBitProbability(const unsigned int length);
   unsigned int ReadBitProbability(const unsigned char * in);
   unsigned int __fastcall RangeEncode( const unsigned char * in, unsigned char * out, const unsigned int length);
   void __fastcall RangeDecode(const unsigned char *in, unsigned char *out, const unsigned int length);
};