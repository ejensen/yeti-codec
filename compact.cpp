#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <mmintrin.h>
#include <emmintrin.h>
#include "yeti.h"
#include "compact.h"
#include "zerorle.h"
#include "golomb.h"

unsigned int COUNT_BITS(unsigned int v)
{
   v = v - ((v >> 1) & 0x55555555);
   v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
   return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

template <class T>
unsigned int COUNT_BITS(T v)
{
   v = v - ((v >> 1) & (T)~(T)0/3);
   v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);
   v = (v + (v >> 4)) & (T)~(T)0/255*15;
   return (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * CHAR_BIT; // count
}

void MMX_Fast_Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len) 
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

void SSE2_Fast_Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len) 
{
   __m128i* mxSrc1 = (__m128i*) src1;
   __m128i* mxSrc2 = (__m128i*) src2;
   __m128i* mxDest = (__m128i*) dest;
   const __m128i* end = mxDest + len / 16;

   _mm_empty();

   while(mxDest < end)
   {
      *mxDest++ = _mm_add_epi8 (*mxSrc1++, *mxSrc2++);
   }

   _mm_empty();
}

void Fast_Add(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len) 
{
   if(SSE2)
   {
      SSE2_Fast_Add(dest, src1, src2, len);
   }
   else
   {
      MMX_Fast_Add(dest, src1, src2, len);
   }
}

unsigned __int64 MMX_Fast_Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len, const unsigned __int64 minDelta)
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

unsigned __int64 SSE2_Fast_Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len, const unsigned __int64 minDelta)
{
   unsigned __int64 oldTotalBits = 0;
   unsigned __int64 totalBits = 0;

   unsigned int* intDest = (unsigned int*)dest;
   unsigned int* intSrc = (unsigned int*)src1;

   __m128i* mxDest = (__m128i*) dest;
   __m128i* mxSrc1 = (__m128i*) src1;
   __m128i* mxSrc2 = (__m128i*) src2;

   _mm_empty();

   //TODO: Optimize bit counting
   for(size_t i = 0; i < EIGHTH(len); i += 4)
   {
      oldTotalBits += COUNT_BITS(intSrc[i]) + COUNT_BITS(intSrc[i+1]) + COUNT_BITS(intSrc[i+3]) + COUNT_BITS(intSrc[i+4]);
      *mxDest++ = _mm_sub_epi8(*mxSrc1++, *mxSrc2++);
      totalBits += COUNT_BITS(intDest[i]) + COUNT_BITS(intDest[i+1]) +  COUNT_BITS(intDest[i+3]) + COUNT_BITS(intDest[i+4]);
   }

   _mm_empty();

   return (oldTotalBits - totalBits < minDelta) ? ULLONG_MAX : totalBits;
}

unsigned __int64 Fast_Sub_Count(BYTE* dest, const BYTE* src1, const BYTE* src2, const size_t len, const unsigned __int64 minDelta)
{
   return (SSE2) ? SSE2_Fast_Sub_Count(dest, src1, src2, len, minDelta) : MMX_Fast_Sub_Count(dest, src1, src2, len, minDelta);
}

// scale the byte probabilities so the cumulative
// probability is equal to a power of 2
void CompressClass::ScaleBitProbability(size_t length)
{
   assert(length > 0);
   assert(length < 0x80000000);

   unsigned int temp = 1;
   while(temp < length)
   {
      temp <<= 1;
   }

   const double factor = temp / (double)length;
   double temp_array[256];

   int a;
   for ( a = 0; a < 256; a++ )
   {
      temp_array[a] = (int)(((int)m_probRanges[a+1])*factor);
   }
   for ( a = 0; a < 256; a++ )
   {
      m_probRanges[a+1] = (int)temp_array[a];
   }
   unsigned int newlen=0;
   for ( a = 1; a < 257; a++ )
   {
      newlen += m_probRanges[a];
   }

   newlen = temp - newlen;

   assert(newlen < 0x80000000);

   a= 0;
   unsigned int b = 0;
   while(newlen)
   {
      if(m_probRanges[b+1])
      {
         m_probRanges[b+1]++;
         newlen--;
      }

      if((b & 0x80) == 0)
      {
         b =- (signed int) b;
         b &= 0xFF;
      } 
      else
      {
         b++;
         b &= 0x7f;
      }
   }

   for(a = 0; temp; a++)
   {
      temp >>= 1;
   }

   m_scale = a - 1;
   for(a = 1; a < 257; a++)
   {
      m_probRanges[a] += m_probRanges[a-1];
   }
}

// read the byte frequency header
size_t CompressClass::ReadBitProbability(const BYTE* in)
{
   try 
   {
      size_t length = 0;
      m_probRanges[0] = 0;
      //ZeroMemory(m_probRanges, sizeof(m_probRanges)); //TODO: change?

      const unsigned int skip = GolombDecode(in, &m_probRanges[1], 256);

      if(!skip)
      {
         return 0;
      }

      for(unsigned int a = 1; a < 257; a++)
      {
         length += m_probRanges[a];
      }

      if(!length)
      {
         return 0;
      }

      ScaleBitProbability(length);

      return skip;
   } 
   catch(...)
   {
   }
}

// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
size_t CompressClass::CalcBitProbability(const BYTE* const in, const size_t length, BYTE* out)
{
   unsigned int table2[256];
   ZeroMemory(m_probRanges, 257 * sizeof(unsigned int));
   ZeroMemory(table2, sizeof(table2));

   unsigned int a=0;
   for (a = 0; a < (length&(~1)); a+=2)
   {
      m_probRanges[in[a]+1]++;
      table2[in[a+1]]++;
   }
   if ( a < length )
   {
      m_probRanges[in[a]+1]++;
   }
   for (a = 0; a < 256; a++){
      m_probRanges[a+1] += table2[a];
   }

   size_t size = 0;
   if ( out != NULL )
   {
      size = GolombEncode(&m_probRanges[1], out, 256);
   }

   ScaleBitProbability(length);

   return size;
}

size_t CompressClass::Compact(BYTE* in, BYTE* out, const size_t length)
{
   size_t bytes_used = 0;

   BYTE* const buffer_1 = m_buffer;
   BYTE* const buffer_2 = m_buffer + ALIGN_ROUND(length * 3/2 + 16, 16);
   int rle = 0;
   size_t size = TestAndRLE(in, buffer_1, buffer_2, length, &rle);

   out[0] = rle;
   if ( rle )
   {
      if (rle == -1 )
      { // solid run of 0s, only 1st byte is needed
         out[1] = in[0];
         bytes_used = 2;
      } 
      else 
      {
         BYTE* b2 = ( rle == 1 ) ? buffer_1 : buffer_2;

         *(UINT32*)(out+1)=size;
         size_t skip = CalcBitProbability(b2, size, out + 5);

         skip += RangeEncode(b2, out + 5 + skip, size) + 5;

         if ( size < skip ) { // RLE size is less than range compressed size
            out[0]+=4;
            memcpy(out+1,b2,size);
            skip=size+1;
         }
         bytes_used = skip;
      }
   } 
   else 
   {
      size_t skip = CalcBitProbability(in, length, out + 1);
      skip += RangeEncode(in, out+skip+1, length) +1;
      bytes_used=skip;
   }

   assert(bytes_used >= 2);
   assert(rle <=3 && rle >= -1);
   assert(out[0] == rle || rle == -1 || out[0]== rle + 4 );

   return bytes_used;
}

void CompressClass::Uncompact(const BYTE* in, BYTE* out, const size_t length)
{
   try
   {
      BYTE rle = in[0];
      if(rle && ( rle < 8 || rle == 0xff ))
      {
         if(rle < 4)
         {
            size_t skip = ReadBitProbability(in + 5);

            if(!skip)
            {
               return;
            }

            Decode_And_DeRLE(in + 4 + skip + 1, out, length, in[0]);
         }
         else  // RLE length is less than range compressed length
         {
            if ( rle == 0xff )
            { // solid run of 0s, only need to set 1 byte
               ZeroMemory(out, length);
               out[0] = in[1];
            }
            else 
            { // RLE length is less than range compressed length
               rle -= 4;
               if ( rle )
                  deRLE(in+1, out, length, rle);
               else // uncompressed length is smallest...
                  memcpy((void*)(in+1), out, length);
            }
         }
      }
   }
   catch(...)
   {
   }
}

bool CompressClass::InitCompressBuffers(const size_t length)
{
   m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, length * 3/2 + length * 5/4 + 32, 8, "Compress::buffer");

   if (!m_buffer)
   {
      FreeCompressBuffers();
      return false;
   }
   return true;
}

void CompressClass::FreeCompressBuffers()
{	
   ALIGNED_FREE( m_buffer,"Compress::buffer");
}

CompressClass::CompressClass()
{
   m_buffer = NULL;
}

CompressClass::~CompressClass()
{
   FreeCompressBuffers();
}