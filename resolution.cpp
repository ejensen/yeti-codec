#include <emmintrin.h>
#include <string.h>

#include "common.h"
#include "resolution.h"

// halves the resolution and aligns the resulting data
void ReduceRes(const BYTE* src, BYTE* dest, BYTE* buffer, const unsigned int width, const unsigned int height, const bool SSE2)
{
   const BYTE* source;
   const unsigned int mod = (SSE2 ? 32 : 16);
   const unsigned int stride = ALIGN_ROUND(width, mod);

   if(stride != width)
   {
      for(unsigned int y = 0;y < height; y++)
      {
         memcpy(buffer + y * stride, src + y * width, width);
         BYTE u = buffer[y * stride + width - 2]; //TODO: Optimize
         BYTE v = buffer[y * stride + width - 1];
         const unsigned int i = (u<<24 ) + (v<<16) + (u<<8) + v;

         for(unsigned int x = width; x < stride; x += 4)
         {
            *(unsigned int*)(buffer + y * stride + x) = i;
         }
      }
      source = buffer;
   } 
   else if(((int)src)&(HALF(mod) - 1))
   {
      memcpy(buffer, src, width * height);
      source = buffer;
   } 
   else
   {
      source = src;
   }

   if(SSE2)
   {
      __m128i mask = _mm_set1_epi16(0x00ff);
      for(unsigned int y = 0; y < height; y += 2)
      {
         for(unsigned int x = 0; x < stride; x += 32)
         {
            __m128i a = *(__m128i *)(source + stride * y + x);
            __m128i b = *(__m128i *)(source + stride * y + x + stride);

            __m128i c = *(__m128i *)(source + stride * y + x + 16);
            __m128i d = *(__m128i *)(source + stride * y + x + stride + 16);

            a = _mm_avg_epu8(a, b);
            c = _mm_avg_epu8(c, d);

            b = _mm_srli_si128(a, 1);
            d = _mm_srli_si128(c, 1);

            a = _mm_avg_epu8(a, b);
            c = _mm_avg_epu8(c, d);

            a = _mm_and_si128(a, mask);
            c = _mm_and_si128(c, mask);

            a = _mm_packus_epi16(a, c);
            *(__m128i *)(dest + y * stride/4 + x/2) = a;
         }
      }
   }
   else
   {
      __m64 mask = _mm_set1_pi16(0x00ff);
      for(unsigned int y = 0; y < height; y += 2)
      {
         for(unsigned int x = 0; x < stride; x += 16)
         {
            __m64 a = *(__m64*)(source + stride * y + x);
            __m64 b = *(__m64*)(source + stride * y + x + stride);

            __m64 c = *(__m64*)(source + stride * y + x + 8);
            __m64 d = *(__m64*)(source + stride * y + x + stride + 8);

            a = _mm_avg_pu8(a, b);
            c = _mm_avg_pu8(c, d);

            b = _mm_srli_si64(a, 8);
            d = _mm_srli_si64(c, 8);

            a = _mm_avg_pu8(a, b);
            c = _mm_avg_pu8(c, d);

            a = _mm_and_si64(a, mask);
            c = _mm_and_si64(c, mask);

            a = _mm_packs_pu16(a, c);
            *(__m64 *)(dest + y * stride/4 + x/2) = a;
         }
      }
      _mm_empty();
   }
}

// doubles the resolution
void EnlargeRes(const BYTE* src, BYTE* dst, BYTE* buffer, unsigned int width, unsigned int height, const bool SSE2)
{
   const BYTE* source;
   BYTE* dest;

   const unsigned int mod = (SSE2 ? 32 : 16);

   unsigned int stride = ALIGN_ROUND(width, mod);

   if(stride != width)
   {
      unsigned int y;
      for(y = 0; y < height; y++)
      {
         memcpy(dst + y * stride, src+ y * width, width);
      }

      const BYTE i = dst[y * stride + width - 1];
      for(unsigned int x = width; x < stride; x++)
      {
         dst[y * stride + x] = i;
      }
      source = dst;
      dest = buffer;
   } 
   else
   {
      source = src;
      dest = dst;
   }

   if (SSE2)
   {
      unsigned int y;
      for(y=0; y < height; y++)
      {
         __m128i p = *(__m128i*)(source + y * stride);
         p = _mm_slli_si128(p, 15);
         p = _mm_srli_si128(p, 15);
         for(unsigned int x = 0; x < stride; x += 16)
         {
            __m128i a = *(__m128i*)(source + x + y * stride);
            __m128i b = _mm_slli_si128(a, 1);

            b = _mm_or_si128(b,p);
            p = _mm_srli_si128(a, 15);

            b = _mm_avg_epu8(a,b);

            *(__m128i*)(dest + DOUBLE(x) + QUADRUPLE(y * stride)) =  _mm_unpacklo_epi8(b, a);
            *(__m128i*)(dest + DOUBLE(x) + QUADRUPLE(y * stride) + 16) = _mm_unpackhi_epi8(b, a);
         }
      }

      height = DOUBLE(height);
      stride = DOUBLE(stride);
      width = DOUBLE(width);

      for(y = 0; y < height - 2; y += 2)
      {
         for(unsigned int x = 0; x < stride; x += 32)
         {
            __m128i a = *(__m128i*)(dest + x + y * stride);
            __m128i b = *(__m128i*)(dest + x + y * stride + DOUBLE(stride));

            __m128i c = *(__m128i*)(dest + x + y * stride + 16);
            __m128i d = *(__m128i*)(dest + x + y * stride + DOUBLE(stride) + 16);

            a=_mm_avg_epu8(a, b);
            c=_mm_avg_epu8(c, d);
            *(__m128i*)(dest + x + y * stride + stride) = a;
            *(__m128i*)(dest + x + y * stride + stride + 16) = c; //TODO: Optimize
         }
      }	
   } 
   else 
   {
      unsigned int y;
      for(y = 0; y < height; y++)
      {
         __m64 p = *(__m64*)(source + y * stride);
         p = _mm_slli_si64(p, 7 * 8);
         p = _mm_srli_si64(p, 7 * 8);
         for(unsigned int x = 0; x < stride; x += 8)
         {
            __m64 a = *(__m64*)(source + x + y * stride);
            __m64 b = _mm_slli_si64(a ,8);

            b = _mm_or_si64(b, p);
            p = _mm_srli_si64(a , 8 * 7);

            b = _mm_avg_pu8(a, b);

            *(__m64*)(dest + DOUBLE(x) + QUADRUPLE(y * stride)) = _mm_unpacklo_pi8(b, a);
            *(__m64*)(dest + DOUBLE(x) + QUADRUPLE(y * stride) + 8) = _mm_unpackhi_pi8(b, a);
         }
      }

      height = DOUBLE(height);
      stride = DOUBLE(stride);
      width = DOUBLE(width);

      for(y = 0; y < height - 2; y += 2)
      {
         for(unsigned int x = 0; x < stride; x += 16)
         {
            __m64 a = *(__m64 *)(dest + x + y * stride);
            __m64 b = *(__m64 *)(dest + x + y * stride  +DOUBLE(stride));

            __m64 c = *(__m64 *)(dest + x + y * stride + 8);
            __m64 d = *(__m64 *)(dest + x + y * stride + DOUBLE(stride) + 8);

            a=_mm_avg_pu8(a, b);
            c=_mm_avg_pu8(c, d);
            *(__m64*)(dest + x + y * stride + stride) = a;
            *(__m64*)(dest + x + y * stride + stride + 8) = c;
         }
      }
      _mm_empty();
   }

   memcpy(dest + stride * (height - 1), dest + stride * (height - 2), stride);

   if(stride != width)
   {
      for(unsigned int y = 0; y < height; y++)
      {
         memcpy(dst + y * width, dest + y * stride, width);
      }
   }
}