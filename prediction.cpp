#include <emmintrin.h>

#include "yeti.h"
#include "prediction.h"

#pragma warning(disable:4731)

inline int median(int x, int y, int z) 
{
   int i = (x <= y) ? x : y;	//i = min(x,y);
   int j = (x >= y) ? x : y;	//j = max(x,y);
   i = (i >= z) ? i : z;      //i = max(i,z);
   return  (i <= j) ? i : j;	//j = min(i,j);
}

void SSE2_BlockPredict(const BYTE* source, BYTE* dest, const unsigned int stride, const unsigned int length)
{
   unsigned int a;
   __m128i t0;
   t0 = _mm_setzero_si128();
   for(a = 0; a < stride; a += 16)
   {
      __m128i x = _mm_slli_si128( *(__m128i*)(source + a), 1);
      x = _mm_or_si128(x, t0);
      t0 = *(__m128i*)(source + a);
      t0 = _mm_srli_si128(t0, 15);
      *(__m128i*)(dest + a) = _mm_sub_epi8(*(__m128i*)(source + a),x);
   }

   __m128i ta;
   __m128i tb;

   tb = *(__m128i *)(source + stride - 16); // x
   tb = _mm_srli_si128(tb, 15);

   // dest[stride] must equal source[stride]-source[stride-1] not source[0] due to a coding error
   // to achieve this, the initial values for z is set to y to make x the median
   ta=*(__m128i *)(source); // z
   ta = _mm_slli_si128(ta, 15);
   ta = _mm_srli_si128(ta, 15);

   for ( ;a < length; a += 16)
   {
      __m128i x=*(__m128i*)(source + a);
      x = _mm_slli_si128(x, 1);
      x = _mm_or_si128(x, tb);
      __m128i y=*(__m128i*)(source + a - stride);
      __m128i z=*(__m128i*)(source + a - stride);
      z = _mm_slli_si128(z, 1);
      z = _mm_or_si128(z, ta);

      __m128i u;
      __m128i v;
      __m128i w;
      __m128i t;

      t = _mm_setzero_si128();
      u = _mm_setzero_si128();
      v = _mm_setzero_si128();
      w = _mm_setzero_si128();

      t = _mm_unpacklo_epi8(x, t);
      u = _mm_unpacklo_epi8(y, u);
      v = _mm_unpacklo_epi8(z, v);
      t = _mm_add_epi16(t, u);
      t = _mm_sub_epi16(t ,v);

      u = _mm_setzero_si128();
      v = _mm_setzero_si128();

      u = _mm_unpackhi_epi8(x, u);
      v = _mm_unpackhi_epi8(y, v);
      w = _mm_unpackhi_epi8(z, w);

      u = _mm_add_epi16(u, v);
      u = _mm_sub_epi16(u, w); 

      z = _mm_packus_epi16(t, u); // z now equals x+y-z

      ta = _mm_srli_si128(y, 15);
      tb = _mm_srli_si128(*(__m128i*)(source + a), 15);

      __m128i i;
      __m128i j;

      i = _mm_min_epu8(x, y);
      j = _mm_max_epu8(x, y);
      i = _mm_max_epu8(i, z);
      j = _mm_min_epu8(i, j);

      *(__m128i*)(dest + a)=_mm_sub_epi8( *(__m128i*)(source + a), j);
   }
}

void MMX_BlockPredict(const BYTE* source, BYTE* dest, const unsigned int stride, const unsigned int length)
{
   unsigned int a;
   __m64 t0;
   t0 = _mm_setzero_si64();
   for(a = 0; a < stride; a+= 8)
   {
      __m64 x = _mm_slli_si64(*(__m64*)(source + a), 8);
      x = _mm_or_si64(x, t0);
      t0 = *(__m64*)(source + a);
      t0 = _mm_srli_si64(t0, 7 * 8);
      *(__m64*)(dest+a) = _mm_sub_pi8(*(__m64*)(source + a),x);
   }

   __m64 ta;
   __m64 tb;

   tb=*(__m64 *)(source + stride - 8); // x

   tb = _mm_srli_si64(tb, 7 * 8);

   // dest[stride] must equal source[stride]-source[stride-1] not source[0] due to a coding error
   // to achieve this, the initial values for z is set to y to make x the median
   ta=*(__m64 *)(source); // z
   ta = _mm_slli_si64(ta, 7 * 8);
   ta = _mm_srli_si64(ta, 7 * 8);

   for ( ;a<length;a+=8)
   {
      __m64 x=*(__m64*)(source + a);
      x = _mm_slli_si64(x, 8);
      x = _mm_or_si64(x, tb);
      __m64 y = *(__m64*)(source + a - stride);
      __m64 z = *(__m64*)(source + a - stride);
      z = _mm_slli_si64(z, 8);
      z = _mm_or_si64(z, ta);

      __m64 u;
      __m64 v;
      __m64 w;
      __m64 t;

      t = _mm_setzero_si64();
      u = _mm_setzero_si64();
      v = _mm_setzero_si64();
      w = _mm_setzero_si64();

      t = _mm_unpacklo_pi8(x, t);
      u = _mm_unpacklo_pi8(y, u);
      v = _mm_unpacklo_pi8(z, v);
      t = _mm_add_pi16(t, u);
      t = _mm_sub_pi16(t, v);

      u = _mm_setzero_si64();
      v = _mm_setzero_si64();

      u = _mm_unpackhi_pi8(x, u);
      v = _mm_unpackhi_pi8(y, v);
      w = _mm_unpackhi_pi8(z, w);

      u = _mm_add_pi16(u, v);
      u = _mm_sub_pi16(u, w); 

      z = _mm_packs_pu16(t, u); // z now equals x+y-z

      ta = _mm_srli_si64(y, 7 * 8);
      tb = _mm_srli_si64(*(__m64*)(source + a), 7 * 8);

      __m64 i;
      __m64 j;

      i = _mm_min_pu8(x, y);
      j = _mm_max_pu8(x, y);
      i = _mm_max_pu8(i, z);
      j = _mm_min_pu8(i, j);

      *(__m64*)(dest+a) = _mm_sub_pi8( *(__m64*)(source + a), j);
   }

   _mm_empty();
}

void SSE2_Predict_YUY2(const BYTE* source, BYTE* dest, const unsigned int width, const unsigned height, const unsigned int lum)
{
   dest[0] = source[0];

   unsigned int a = 0;
   __m128i ta = _mm_setzero_si128();

   if(lum)
   {
      dest[1]=source[1];
      for(a = 2; a < 16; a++)
      {
         dest[a] = source[a] - source[a - 1];
      }
      ta = *(__m128i*)(source);
      ta = _mm_srli_si128(ta, 15);
   }

   for (; a < width + 1; a += 16)
   { // dest bytes width+2 to width+16 will be overwritten with correct values
      __m128i x = *(__m128i *)(source + a);
      __m128i w = x;
      x = _mm_slli_si128(x, 1);
      x = _mm_or_si128(x, ta);
      ta = _mm_srli_si128(w, 15);
      w = _mm_sub_epi8(w, x);
      *(__m128i *)(dest + a) = w;
   }

   for(a = width + 2 + lum * 2; a < width + 16; a++)
   {
      BYTE t = source[a - 1];
      BYTE u = source[a - width];
      BYTE v = t;
      v += u;
      v -= source[a - width - 1];
      dest[a] = source[a] - median(t, u, v);
   }

   ta = *(__m128i *)(source + width);
   __m128i tb = *(__m128i *)(source);
   ta = _mm_srli_si128(ta, 15);
   tb = _mm_srli_si128(tb, 15);

   for(a = width + 16; a < width * height; a += 16)
   {
      __m128i x = *(__m128i *)(source + a);
      __m128i w = x;
      __m128i y = *(__m128i *)(source + a - width);
      __m128i z;

      x = _mm_slli_si128(x, 1);
      z = _mm_slli_si128(y, 1);

      x = _mm_or_si128(x, ta);
      z = _mm_or_si128(z, tb);

      ta = _mm_srli_si128(w, 15);
      tb = _mm_srli_si128(y, 15);

      z = _mm_sub_epi8(y, z);
      z = _mm_add_epi8(x, z);

      __m128i i;
      __m128i j;

      i = _mm_min_epu8(x, y);
      j = _mm_max_epu8(x, y);
      i = _mm_max_epu8(i, z);
      j = _mm_min_epu8(i, j);

      j = _mm_sub_epi8(w, j);


      *(__m128i *)(dest + a) = j;
   }
}

void MMX_Predict_YUY2(const BYTE* source, BYTE* dest, const unsigned int width, const unsigned height, const unsigned int lum)
{
   dest[0] = source[0];

   unsigned int a = 0;
   __m64 ta = _mm_setzero_si64();

   if (lum)
   {
      dest[1] = source[1];
      for(a = 2; a < 8; a++)
      {
         dest[a] = source[a] - source[a - 1];
      }
      ta = *(__m64*)(source);
      ta = _mm_srli_si64(ta, 7 * 8);
   }

   for ( ;a < width + 1; a += 8)
   { // dest bytes width+2 to width+16 will be overwritten with correct values
      __m64 x = *(__m64 *)(source + a);
      __m64 w = x;
      x = _mm_slli_si64(x, 8);
      x = _mm_or_si64(x, ta);
      ta = _mm_srli_si64(w, 7 * 8);
      w = _mm_sub_pi8(w, x);
      *(__m64 *)(dest + a) = w;
   }

   for(a = width + 2 + lum * 2; a < width + 8; a++)
   {
      BYTE t = source[a - 1];
      BYTE u = source[a - width];
      BYTE v = t;
      v += u;
      v -= source[a - width - 1];
      dest[a] = source[a] - median(t, u, v);
   }

   ta = *(__m64 *)(source + width);
   __m64 tb = *(__m64 *)(source);
   ta = _mm_srli_si64(ta, 7 * 8);
   tb = _mm_srli_si64(tb ,7 * 8);

   for(a = width + 8; a < width * height; a += 8)
   {
      __m64 x = *(__m64 *)(source + a);
      __m64 w = x;
      __m64 y = *(__m64 *)(source + a - width);
      __m64 z;

      x = _mm_slli_si64(x, 8);
      z = _mm_slli_si64(y, 8);

      x = _mm_or_si64(x, ta);
      z = _mm_or_si64(z, tb);

      ta = _mm_srli_si64(w, 7 * 8);
      tb = _mm_srli_si64(y, 7 * 8);

      z = _mm_sub_pi8(y, z);
      z = _mm_add_pi8(x, z);

      __m64 i;
      __m64 j;

      i = _mm_min_pu8(x, y);
      j = _mm_max_pu8(x, y);
      i = _mm_max_pu8(i, z);
      j = _mm_min_pu8(i, j);

      j = _mm_sub_pi8(w, j);


      *(__m64 *)(dest + a) = j;
   }
   _mm_empty();
}

void ASM_BlockRestore(BYTE* source, unsigned int stride, unsigned int xlength, unsigned int mode)
{
   for(unsigned int a = 1; a < stride + !mode; a++)
   {
      source[a] += source[a-1];
   }

   if(mode) //TODO: Optimize
   {
      source[stride] += source[0];
   }

   __asm{
      movd	mm1,ebx
         movd	mm6,esp
         movd	mm7,ebp
         mov		eax,source
         mov		ebx,xlength
         mov		ecx,stride

         mov		esi,eax

         add		ebx,esi

         dec		ebx

         mov		esp,ecx
         add		esi,esp
         neg		esp
         mov		ebp,ebx // ebp = ending
         movzx	eax,byte ptr[esi] // a-1
      movzx	ebx,byte ptr[esp + esi] // a-dis-1
      add		esp,esi
         inc		esp

median_restore_block:

      movzx	ecx,byte ptr[esp] // a-dis
      inc		esi
         inc		esp
         mov		edx,eax
         sub		eax,ebx
         add		eax,ecx
         mov		ebx,edx

         //		eax = z
         //		ebx = x
         //		ecx = y
         //		edx = x

         //i=min(x,y);
         //j=max(x,y);
         //i=max(i,z);
         //j=min(i,j);

         cmp		ebx,ecx
         cmovg	ebx,ecx
         cmp		edx,ecx
         cmovl	edx,ecx
         cmp		ebx,eax
         cmovl	ebx,eax
         movzx	eax,byte ptr[esi]
      cmp		ebx,edx
         cmovg	ebx,edx
         add		eax,ebx
         mov		ebx,ecx
         mov		byte ptr[esi],al
         and		eax,0xff
         cmp		esi,ebp
         jl		median_restore_block

         movd	ebx,mm1
         movd	esp,mm6
         movd	ebp,mm7
         emms
   }
}