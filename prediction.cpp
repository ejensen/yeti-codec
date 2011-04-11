#include <emmintrin.h>

#include "yeti.h"
#include "common.h"
#include "prediction.h"

#pragma warning(disable:4731)

bool SSE;
bool SSE2;

inline int median(int x, int y, int z) 
{
   int delta = x - y;
   delta &= delta >> 31;
   int i = y + delta;	//i=min(x,y);
   int j = x - delta;	//j=max(x,y);
   delta = i - z;
   delta &= delta >> 31;
   i = i - delta;	//i=max(i,z);
   delta = i - j;
   delta &= delta >> 31;
   return  j + delta;	//j=min(i,j);
}

void SSE2_Block_Predict(const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length)
{
   unsigned int align_shift = (16 - ((unsigned int)source&15))&15;

   // predict the bottom row
   unsigned int a;
   __m128i t0 = _mm_setzero_si128();
   if ( align_shift )
   {
      dest[0]=source[0];
      for(a = 1; a < align_shift; a++)
      {
         dest[a] = source[a]-source[a-1];
      }
      t0 = _mm_insert_epi16(t0, source[align_shift-1], 0);
   }
   for ( a = align_shift; a < width; a += 16){
      __m128i x = *(__m128i*)&source[a];
      t0 = _mm_or_si128(t0,_mm_slli_si128(x,1));
      *(__m128i*)&dest[a]=_mm_sub_epi8(x,t0);
      t0 = _mm_srli_si128(x,15);
   }

   if (width == length)
   {
      return;
   }

   __m128i z = _mm_setzero_si128();
   __m128i x = _mm_setzero_si128();

   x = _mm_insert_epi16(x,source[width-1],0);
   z = _mm_insert_epi16(z,source[0],0);

   const __m128i zero = _mm_setzero_si128();
   a = width;
   {
      // this block makes sure that source[a] is aligned to 16
      __m128i srcs = _mm_loadu_si128((__m128i *)&source[a]);
      __m128i y = _mm_loadu_si128((__m128i *)&source[a-width]);

      x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
      z = _mm_or_si128(z,_mm_slli_si128(y,1));

      __m128i tx = _mm_unpackhi_epi8(x,zero);
      __m128i ty = _mm_unpackhi_epi8(y,zero);
      __m128i tz = _mm_unpackhi_epi8(z,zero);

      tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

      tx = _mm_unpacklo_epi8(x,zero);
      ty = _mm_unpacklo_epi8(y,zero);
      z = _mm_unpacklo_epi8(z,zero);
      z = _mm_add_epi16(_mm_sub_epi16(tx,z),ty);
      z = _mm_packus_epi16(z,tz);

      __m128i i = _mm_min_epu8(x,y);
      x = _mm_max_epu8(x,y);
      i = _mm_max_epu8(i,z);
      x = _mm_min_epu8(x,i);

      srcs = _mm_sub_epi8(srcs,x);
      _mm_storeu_si128((__m128i*)&dest[a],srcs);
   }

   if ( align_shift )
   {
      a = ALIGN_ROUND(a, 16);
      a += align_shift;
      if ( a > width+16 )
      {
         a -= 16;
      }
   } 
   else 
   {
      a+=16;
      a&=(~15);
   }
   // source[a] is now aligned
   z = _mm_setzero_si128();
   x = _mm_setzero_si128();
   x = _mm_insert_epi16(x,source[a-1], 0);
   z = _mm_insert_epi16(z,source[a-width-1], 0);

   const unsigned int end = (length>=15)?length-15:0;

   // if width is a multiple of 16, use faster aligned reads
   // inside the prediction loop
   if ( width%16 == 0 )
   {
      for ( ;a<end;a+=16)
      {
         __m128i srcs = *(__m128i *)&source[a];
         __m128i y = *(__m128i *)&source[a-width];

         x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
         z = _mm_or_si128(z,_mm_slli_si128(y,1));

         __m128i tx = _mm_unpackhi_epi8(x,zero);
         __m128i ty = _mm_unpackhi_epi8(y,zero);
         __m128i tz = _mm_unpackhi_epi8(z,zero);

         tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

         tx = _mm_unpacklo_epi8(x,zero);
         ty = _mm_unpacklo_epi8(y,zero);
         z = _mm_unpacklo_epi8(z,zero);
         z = _mm_add_epi16(_mm_sub_epi16(tx,z),ty);
         z = _mm_packus_epi16(z,tz);

         __m128i i = _mm_min_epu8(x,y);
         x = _mm_max_epu8(x,y);
         i = _mm_max_epu8(i,z);
         x = _mm_min_epu8(x,i);

         i = _mm_srli_si128(srcs,15);

         srcs = _mm_sub_epi8(srcs,x);

         z = _mm_srli_si128(y,15);
         x = i;

         *(__m128i*)&dest[a] = srcs;
      }
   } 
   else 
   {
      // main prediction loop, source[a-width] is unaligned
      for ( ; a<end; a+=16)
      {
         __m128i srcs = *(__m128i *)&source[a];
         __m128i y = _mm_loadu_si128((__m128i *)&source[a-width]);

         x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
         z = _mm_or_si128(z,_mm_slli_si128(y,1));

         __m128i tx = _mm_unpackhi_epi8(x,zero);
         __m128i ty = _mm_unpackhi_epi8(y,zero);
         __m128i tz = _mm_unpackhi_epi8(z,zero);

         tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

         tx = _mm_unpacklo_epi8(x,zero);
         ty = _mm_unpacklo_epi8(y,zero);
         z = _mm_unpacklo_epi8(z,zero);
         z = _mm_add_epi16(_mm_sub_epi16(tx,z),ty);
         z = _mm_packus_epi16(z,tz);

         __m128i i = _mm_min_epu8(x,y);
         x = _mm_max_epu8(x,y);
         i = _mm_max_epu8(i,z);
         x = _mm_min_epu8(x,i);

         i = _mm_srli_si128(srcs,15);

         srcs = _mm_sub_epi8(srcs,x);

         z = _mm_srli_si128(y,15);
         x = i;

         *(__m128i*)&dest[a] = srcs;
      }
   }

   int x2, y2;
   for ( ; a<length; a++ )
   {
      x2 = source[a-1];
      y2 = source[a-width];
      dest[a] = source[a] - median(x2, y2, x2 + y2 - source[a - width - 1]);
   }
}

void SSE2_Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length, const bool is_y)
{
   unsigned int align_shift = (16 - ((int)source&15))&15;

   // predict the bottom row
   unsigned int a;
   __m128i t0 = _mm_setzero_si128();
   if ( align_shift )
   {
      dest[0]=source[0];
      for ( a=1;a<align_shift;a++)
      {
         dest[a] = source[a]-source[a-1];
      }
      t0 = _mm_insert_epi16(t0,source[align_shift-1],0);
   }
   for ( a=align_shift;a<width;a+=16)
   {
      __m128i x = *(__m128i*)&source[a];
      t0 = _mm_or_si128(t0,_mm_slli_si128(x,1));
      *(__m128i*)&dest[a]=_mm_sub_epi8(x,t0);
      t0 = _mm_srli_si128(x,15);
   }
   if ( width==length )
      return;

   __m128i z;
   __m128i x;

   /*const __m128i zero = */_mm_setzero_si128();
   a = width;
   {
      // this block makes sure that source[a] is aligned to 16
      __m128i srcs = _mm_loadu_si128((__m128i *)&source[a]);
      __m128i y = _mm_loadu_si128((__m128i *)&source[a-width]);

      x = _mm_slli_si128(srcs,1);
      z = _mm_slli_si128(y,1);
      z = _mm_add_epi8(_mm_sub_epi8(x,z),y);

      __m128i i = _mm_min_epu8(x,y);
      x = _mm_max_epu8(x,y);
      i = _mm_max_epu8(i,z);
      x = _mm_min_epu8(x,i);

      srcs = _mm_sub_epi8(srcs,x);
      _mm_storeu_si128((__m128i*)&dest[a],srcs);
   }

   if ( align_shift )
   {
      a = ALIGN_ROUND(a, 16);
      a += align_shift;
      if ( a > width+16 )
      {
         a -= 16;
      }
   } 
   else 
   {
      a+=16;
      a&=(~15);
   }
   // source[a] is now aligned
   z = _mm_setzero_si128();
   x = _mm_setzero_si128();
   x = _mm_insert_epi16(x,source[a-1],0);
   z = _mm_insert_epi16(z,source[a-width-1],0);


   const unsigned int end = (length>=15)?length-15:0;
   // if width is a multiple of 16, use faster aligned reads
   // inside the prediction loop
   if ( width%16 == 0 )
   {
      for ( ;a<end;a+=16)
      {
         __m128i srcs = *(__m128i *)&source[a];
         __m128i y = *(__m128i *)&source[a-width];

         x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
         z = _mm_or_si128(z,_mm_slli_si128(y,1));
         z = _mm_add_epi8(_mm_sub_epi8(x,z),y);

         __m128i i = _mm_min_epu8(x,y);
         x = _mm_max_epu8(x,y);
         i = _mm_max_epu8(i,z);
         x = _mm_min_epu8(x,i);

         i = _mm_srli_si128(srcs,15);

         srcs = _mm_sub_epi8(srcs,x);

         z = _mm_srli_si128(y,15);
         x = i;

         *(__m128i*)&dest[a] = srcs;
      }
   } 
   else 
   {
      // main prediction loop, source[a-width] is unaligned
      for ( ;a<end;a+=16)
      {
         __m128i srcs = *(__m128i *)&source[a];
         __m128i y = _mm_loadu_si128((__m128i *)&source[a-width]);

         x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
         z = _mm_or_si128(z,_mm_slli_si128(y,1));
         z = _mm_add_epi8(_mm_sub_epi8(x,z),y);

         __m128i i = _mm_min_epu8(x,y);
         x = _mm_max_epu8(x,y);
         i = _mm_max_epu8(i,z);
         x = _mm_min_epu8(x,i);

         i = _mm_srli_si128(srcs,15);

         srcs = _mm_sub_epi8(srcs,x);

         z = _mm_srli_si128(y,15);
         x = i;

         *(__m128i*)&dest[a] = srcs;
      }
   }

   int x2, y2;
   for ( ; a<length; a++ )
   {
      x2 = source[a-1];
      y2 = source[a-width];
      dest[a] = source[a]-median(x2, y2,(x2+y2-source[a-width-1])&255);
   }

   if ( is_y )
   {
      dest[1]=source[1];
   }
   dest[width] = source[width]-source[width-1];
   dest[width+1] = source[width+1]-source[width];

   if ( is_y )
   {
      dest[width+2] = source[width+2]-source[width+1];
      dest[width+3] = source[width+3]-source[width+2];
   }
}

void Split_YUY2(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const size_t width, const size_t height)
{
   const __m64 ymask = _mm_set_pi32(0x00FF00FF,0x00FF00FF);

   unsigned int a;
   const unsigned int end = (width*height)&(~7);
   for ( a=0;a<end;a+=8){
      __m64 y0 = *(__m64*)&src[a*2+0];
      __m64 y1 = *(__m64*)&src[a*2+8];

      __m64 u0 = _mm_srli_pi32(_mm_slli_pi32(y0,16),24);
      __m64 v0 = _mm_srli_pi32(y0,24);
      y0 = _mm_and_si64(y0,ymask);

      __m64 u1 = _mm_srli_pi32(_mm_slli_pi32(y1,16),24);
      __m64 v1 = _mm_srli_pi32(y1,24);
      y1 = _mm_and_si64(y1,ymask);

      y0 = _mm_packs_pu16(y0,y1);
      v0 = _mm_packs_pu16(v0,v1);
      u0 = _mm_packs_pu16(u0,u1);
      v0 = _mm_packs_pu16(v0,v0);
      u0 = _mm_packs_pu16(u0,u0);

      *(__m64*)&ydst[a] = y0;
      *(int*)&vdst[a/2] = _mm_cvtsi64_si32(v0);
      *(int*)&udst[a/2] = _mm_cvtsi64_si32(u0);
   }

   for ( ;a<height*width;a+=2)
   {
      ydst[a+0] = src[a*2+0];
      udst[a/2] = src[a*2+1];
      ydst[a+1] = src[a*2+2];
      vdst[a/2] = src[a*2+3];
   }

   _mm_empty();
}

void Split_UYVY(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const size_t width, const size_t height)
{
   const __m64 cmask = _mm_set_pi32(0x000000FF,0x000000FF);

   unsigned int a;
   const unsigned int end = (width*height)&(~7);
   for(a=0; a<end; a+=8)
   {
      __m64 u0 = *(__m64*)&src[a*2+0];
      __m64 u1 = *(__m64*)&src[a*2+8];

      __m64 y0 = _mm_srli_pi16(u0,8);
      __m64 v0 = _mm_and_si64(_mm_srli_si64(u0,16),cmask);
      u0 = _mm_and_si64(u0,cmask);

      __m64 y1 = _mm_srli_pi16(u1,8);
      __m64 v1 = _mm_and_si64(_mm_srli_si64(u1,16),cmask);
      u1 = _mm_and_si64(u1,cmask);

      y0 = _mm_packs_pu16(y0,y1);
      v0 = _mm_packs_pu16(v0,v1);
      u0 = _mm_packs_pu16(u0,u1);
      v0 = _mm_packs_pu16(v0,v0);
      u0 = _mm_packs_pu16(u0,u0);

      *(__m64*)&ydst[a] = y0;
      *(int*)&vdst[a/2] = _mm_cvtsi64_si32(v0);
      *(int*)&udst[a/2] = _mm_cvtsi64_si32(u0);
   }

   for ( ;a<height*width;a+=2)
   {
      udst[a/2] = src[a*2+0];
      ydst[a+0] = src[a*2+1];
      vdst[a/2] = src[a*2+2];
      ydst[a+1] = src[a*2+3];
   }

   _mm_empty();
}

void Block_Predict(const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length)
{
   if ( SSE2 ){
      SSE2_Block_Predict(source, dest, width, length);
   } else {
      SSE_Block_Predict(source, dest, width, length);
   }
}

void SSE_Block_Predict(const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length)
{
   unsigned int align_shift = (8 - ((unsigned int)source&7))&7;

   // predict the bottom row
   unsigned int a;
   __m64 t0 = _mm_setzero_si64();
   if ( align_shift )
   {
      dest[0]=source[0];
      for ( a=1;a<align_shift;a++)
      {
         dest[a] = source[a]-source[a-1];
      }
      t0 = _mm_insert_pi16(t0,source[align_shift-1],0);
   }
   for(a = align_shift ;a<width; a+=8)
   {
      __m64 x = *(__m64*)&source[a];
      t0 = _mm_or_si64(t0,_mm_slli_si64(x,8));
      *(__m64*)&dest[a]=_mm_sub_pi8(x,t0);
      t0 = _mm_srli_si64(x,7*8);
   }

   if(width == length)
   {
      _mm_empty();
      return;
   }

   __m64 z = _mm_cvtsi32_si64(source[0]);// load 1 byte
   __m64 x = _mm_cvtsi32_si64(source[width-1]);// load 1 byte

   // make sure that source[a] is a multiple of 8 so that only source[a-width]
   // will be unaligned if width is not a multiple of 8
   const __m64 zero = _mm_setzero_si64();
   a = width;
   {
      __m64 srcs = *(__m64 *)&source[a];
      __m64 y = *(__m64 *)&source[a-width];

      x = _mm_or_si64(x,_mm_slli_si64(srcs,8));
      z = _mm_or_si64(z,_mm_slli_si64(y,8));

      __m64 tx = _mm_unpackhi_pi8(x,zero);
      __m64 ty = _mm_unpackhi_pi8(y,zero);
      __m64 tz = _mm_unpackhi_pi8(z,zero);

      tz = _mm_add_pi16(_mm_sub_pi16(tx,tz),ty);

      tx = _mm_unpacklo_pi8(x,zero);
      ty = _mm_unpacklo_pi8(y,zero);
      z = _mm_unpacklo_pi8(z,zero);
      z = _mm_add_pi16(_mm_sub_pi16(tx,z),ty);
      z = _mm_packs_pu16(z,tz);

      __m64 i = _mm_min_pu8(x,y);
      x = _mm_max_pu8(x,y);
      i = _mm_max_pu8(i,z);
      x = _mm_min_pu8(x,i);

      srcs = _mm_sub_pi8(srcs,x);
      *(__m64*)&dest[a]=srcs;
   }

   if ( align_shift )
   {
      a = ALIGN_ROUND(a,8);
      a += align_shift;
      if ( a > width+8 ){
         a -= 8;
      }
   } 
   else 
   {
      a+=8;
      a&=(~7);
   }

   x = _mm_cvtsi32_si64(source[a-1]); // load 1 byte
   z = _mm_cvtsi32_si64(source[a-width-1]); // load 1 byte

   const unsigned int end = (length)&(~7);
   // main prediction loop, source[a] will be a multiple of 8
   for ( ; a<end; a+=8)
   {
      __m64 srcs = *(__m64 *)&source[a];
      __m64 y = *(__m64 *)&source[a-width];

      x = _mm_or_si64(x,_mm_slli_si64(srcs,8));
      z = _mm_or_si64(z,_mm_slli_si64(y,8));

      __m64 tx = _mm_unpackhi_pi8(x,zero);
      __m64 ty = _mm_unpackhi_pi8(y,zero);
      __m64 tz = _mm_unpackhi_pi8(z,zero);

      tz = _mm_add_pi16(_mm_sub_pi16(tx,tz),ty);

      tx = _mm_unpacklo_pi8(x,zero);
      ty = _mm_unpacklo_pi8(y,zero);
      z = _mm_unpacklo_pi8(z,zero);
      z = _mm_add_pi16(_mm_sub_pi16(tx,z),ty);
      z = _mm_packs_pu16(z,tz);

      __m64 i = _mm_min_pu8(x,y);
      x = _mm_max_pu8(x,y);
      i = _mm_max_pu8(i,z);
      x = _mm_min_pu8(x,i);

      i = _mm_srli_si64(srcs,7*8);

      srcs = _mm_sub_pi8(srcs,x);

      z = _mm_srli_si64(y,7*8);
      x = i;

      *(__m64*)&dest[a]=srcs;
   }
   _mm_empty();

   int x2, y2;
   for( ; a<length; a++ )
   {
      x2 = source[a-1];
      y2 = source[a-width];
      dest[a] = source[a]-median(x2, y2, x2+y2-source[a-width-1]);
   }
}

void SSE_Block_Predict_YUV16(const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length, const bool is_y)
{
   unsigned int align_shift = (8 - ((unsigned int)source&7))&7;

   // predict the bottom row
   unsigned int a;
   __m64 t0 = _mm_setzero_si64();
   if ( align_shift )
   {
      dest[0] = source[0];
      for (a=1; a<align_shift; a++)
      {
         dest[a] = source[a]-source[a-1];
      }
      t0 = _mm_cvtsi32_si64(source[align_shift-1]);
   }

   for(a=align_shift; a<width; a+=8)
   {
      __m64 x = *(__m64*)&source[a];
      t0 = _mm_or_si64(t0,_mm_slli_si64(x,8));
      *(__m64*)&dest[a]=_mm_sub_pi8(x,t0);
      t0 = _mm_srli_si64(x,7*8);
   }

   if ( width==length )
   {
      _mm_empty();
      return;
   }

   __m64 z = _mm_setzero_si64();
   __m64 x = _mm_setzero_si64();

   // make sure that source[a] is a multiple of 8 so that only source[a-width]
   // will be unaligned if width is not a multiple of 8
   a = width;
   {
      __m64 srcs = *(__m64 *)&source[a];
      __m64 y = *(__m64 *)&source[a-width];

      x = _mm_slli_si64(srcs,8);
      z = _mm_slli_si64(y,8);
      z = _mm_add_pi8(_mm_sub_pi8(x,z),y);

      __m64 i = _mm_min_pu8(x,y);
      x = _mm_max_pu8(x,y);
      i = _mm_max_pu8(i,z);
      x = _mm_min_pu8(x,i);

      srcs = _mm_sub_pi8(srcs, x);
      *(__m64*)&dest[a] = srcs;
   }

   if ( align_shift )
   {
      a = ALIGN_ROUND(a,8);
      a += align_shift;
      if(a > width + 8)
      {
         a -= 8;
      }
   } 
   else 
   {
      a+=8;
      a&=(~7);
   }

   x = _mm_cvtsi32_si64(source[a-1]);
   z = _mm_cvtsi32_si64(source[a-width-1]);

   const unsigned int end = (length)&(~7);

   // main prediction loop, source[a] will be a multiple of 8
   for ( ;a<end;a+=8)
   {
      __m64 srcs = *(__m64 *)&source[a];
      __m64 y = *(__m64 *)&source[a-width];

      x = _mm_or_si64(x,_mm_slli_si64(srcs, 8));
      z = _mm_or_si64(z,_mm_slli_si64(y, 8));
      z = _mm_add_pi8(_mm_sub_pi8(x, z), y);

      __m64 i = _mm_min_pu8(x, y);
      x = _mm_max_pu8(x,y);
      i = _mm_max_pu8(i,z);
      x = _mm_min_pu8(x,i);

      i = _mm_srli_si64(srcs, 7*8);

      srcs = _mm_sub_pi8(srcs, x);

      z = _mm_srli_si64(y,7*8);
      x = i;

      *(__m64*)&dest[a]=srcs;
   }
   _mm_empty();

   int x2, y2;
   for ( ; a<length; a++ )
   {
      x2 = source[a-1];
      y2 = source[a-width];
      dest[a] = source[a]-median(x2, y2, (x2+y2-source[a-width-1])&255);
   }

   if ( is_y )
   {
      dest[1]=source[1];
   }
   dest[width] = source[width]-source[width-1];
   dest[width+1] = source[width+1]-source[width];

   if ( is_y )
   {
      dest[width+2] = source[width+2]-source[width+1];
      dest[width+3] = source[width+3]-source[width+2];
   }
}

void Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length, const bool is_y)
{
   if ( SSE2 ){
      SSE2_Block_Predict_YUV16(source, dest, width, length, is_y);
   } else {
      SSE_Block_Predict_YUV16(source, dest, width, length, is_y);
   }
}

void Interleave_And_Restore_YUY2( unsigned char * __restrict output, unsigned char * __restrict ysrc, const unsigned char * __restrict usrc, const unsigned char * __restrict vsrc, const unsigned int width, const unsigned int height){

   output[0]=ysrc[0];
   output[1]=usrc[0];
   output[2]=ysrc[1];
   output[3]=vsrc[0];

   // restore the bottom row of pixels + 2 pixels
   for ( unsigned int a=1;a<width/2+2;a++){
      output[a*4+0]=output[a*4-2]+ysrc[a*2+0];
      output[a*4+1]=output[a*4-3]+usrc[a];
      output[a*4+2]=output[a*4-0]+ysrc[a*2+1];
      output[a*4+3]=output[a*4-1]+vsrc[a];
   }

   if ( height==1)
      return;

   const unsigned int stride = width*2;
   unsigned int a=width/2+2;
   __m64 x = _mm_setr_pi8(output[a*4-2],output[a*4-3],0,output[a*4-1],0,0,0,0);
   __m64 z = _mm_setr_pi8(output[a*4-3-stride],output[a*4-2-stride],output[a*4-1-stride],0,0,0,0,0);
   const __m64 mask = _mm_setr_pi8(0,0,0,0,-1,0,0,0);

   // restore all the remaining pixels using median prediction
   if ( SSE ){
      // 54
      // 50
      // 49
      const __m64 mask2 = _mm_setr_pi8(0,0,-1,0,0,-1,0,-1);
      for ( ; a<(height*width)/2; a+=2){
         __m64 srcs = _mm_cvtsi32_si64( *(unsigned int *)&ysrc[a*2]);
         __m64 temp0 = _mm_insert_pi16(_mm_setzero_si64(),*(int*)&usrc[a],0);
         __m64 temp1 = _mm_insert_pi16(_mm_setzero_si64(),*(int*)&vsrc[a],0);
         srcs = _mm_unpacklo_pi8(srcs,_mm_unpacklo_pi8(temp0,temp1));

         __m64 y = *(__m64 *)&output[a*4-stride];

         z = _mm_or_si64(z,_mm_slli_si64(y,24)); // z=uyvyuyvy
         z = _mm_or_si64(_mm_srli_pi16(z,8),_mm_slli_pi16(z,8));// z = yuyvyuyv
         z = _mm_sub_pi8(_mm_add_pi8(x,y),z);

         __m64 i = _mm_min_pu8(x,y);//min
         __m64 j = _mm_max_pu8(x,y);//max
         i = _mm_max_pu8(i,z);//max
         j = _mm_min_pu8(i,j);//min
         j = _mm_add_pi8(j,srcs); // yu-v----
         // mask and shift j (yuv)
         j = _mm_shuffle_pi16(j,(0<<0)+(0<<2)+(0<<4)+(1<<6));
         j = _mm_and_si64(j,mask2);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_min_pu8(x,y);//min
         j = _mm_max_pu8(x,y);//max
         i = _mm_max_pu8(i,z);//max
         j = _mm_min_pu8(i,j);//min
         j = _mm_add_pi8(j,srcs); // yuyv----

         // mask and shift j (y only)
         j = _mm_slli_si64(j,5*8);
         j = _mm_srli_pi32(j,3*8);

         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_min_pu8(x,y);//min
         j = _mm_max_pu8(x,y);//max
         i = _mm_max_pu8(i,z);//max
         j = _mm_min_pu8(i,j);//min
         j = _mm_add_pi8(j,srcs); // yuyvyu-v
         // mask and shift j (y only)
         j = _mm_and_si64(j,mask);
         j = _mm_slli_si64(j,2*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_min_pu8(x,y);//min
         x = _mm_max_pu8(x,y);//max
         i = _mm_max_pu8(i,z);//max
         x = _mm_min_pu8(i,x);//min
         x = _mm_add_pi8(x,srcs);
         *(__m64*)&output[a*4] = x;
         j = x;
         x = _mm_slli_si64(x,1*8);
         x = _mm_srli_si64(x,7*8);
         j = _mm_srli_pi16(j,1*8);
         j = _mm_srli_si64(j,3*8);
         x = _mm_or_si64(x,j);

         z = _mm_srli_si64(y,5*8);
      }
   } else {
      // 88
      // 64
      __declspec(align(8)) unsigned char temp[8]={0,0,0,0,0,0,0,0};
      for ( ; a<(height*width)/2; a+=2){
         __m64 srcs = _mm_cvtsi32_si64( *(unsigned int *)&ysrc[a*2]);
         temp[0]=usrc[a];
         temp[1]=vsrc[a];
         temp[2]=usrc[a+1];
         temp[3]=vsrc[a+1];
         srcs = _mm_unpacklo_pi8(srcs,_mm_cvtsi32_si64(*(int*)&temp[0]));

         __m64 y = *(__m64 *)&output[a*4-stride];

         z = _mm_or_si64(z,_mm_slli_si64(y,24)); // z=uyvyuyvy
         z = _mm_or_si64(_mm_srli_pi16(z,8),_mm_slli_pi16(z,8));// z = yuyvyuyv
         z = _mm_sub_pi8(_mm_add_pi8(x,y),z);

         __m64 i = _mm_sub_pi8(x,_mm_subs_pu8(x,y));//min
         __m64 j = _mm_add_pi8(y,_mm_subs_pu8(x,y));//max
         i = _mm_add_pi8(z,_mm_subs_pu8(i,z));//max
         j = _mm_sub_pi8(i,_mm_subs_pu8(i,j));//min
         j = _mm_add_pi8(j,srcs); // yu-v----
         // mask and shift j (y only)
         j = _mm_slli_si64(j,7*8);
         j = _mm_srli_si64(j,5*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_sub_pi8(x,_mm_subs_pu8(x,y));//min
         j = _mm_add_pi8(y,_mm_subs_pu8(x,y));//max
         i = _mm_add_pi8(z,_mm_subs_pu8(i,z));//max
         j = _mm_sub_pi8(i,_mm_subs_pu8(i,j));//min
         j = _mm_add_pi8(j,srcs); // yuyv----

         // mask and shift j (yuv)
         i = _mm_slli_si64(j,3*8);
         i = _mm_slli_pi16(i,1*8);
         j = _mm_slli_si64(j,5*8);
         j = _mm_srli_pi32(j,3*8);
         j = _mm_or_si64(i,j);

         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_sub_pi8(x,_mm_subs_pu8(x,y));//min
         j = _mm_add_pi8(y,_mm_subs_pu8(x,y));//max
         i = _mm_add_pi8(z,_mm_subs_pu8(i,z));//max
         j = _mm_sub_pi8(i,_mm_subs_pu8(i,j));//min
         j = _mm_add_pi8(j,srcs); // yuyvyu-v
         // mask and shift j (y only)
         j = _mm_and_si64(j,mask);
         j = _mm_slli_si64(j,2*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi8(z,j);

         i = _mm_sub_pi8(x,_mm_subs_pu8(x,y));//min
         x = _mm_add_pi8(y,_mm_subs_pu8(x,y));//max
         i = _mm_add_pi8(z,_mm_subs_pu8(i,z));//max
         x = _mm_sub_pi8(i,_mm_subs_pu8(i,x));//min
         x = _mm_add_pi8(x,srcs);
         *(__m64*)&output[a*4] = x;
         j = x;
         x = _mm_slli_si64(x,1*8);
         x = _mm_srli_si64(x,7*8);
         j = _mm_srli_pi16(j,1*8);
         j = _mm_srli_si64(j,3*8);
         x = _mm_or_si64(x,j);

         z = _mm_srli_si64(y,5*8);
      }
   }

   _mm_empty();

}

void Restore_YV12(BYTE * __restrict ysrc, BYTE * __restrict usrc, BYTE * __restrict vsrc, const size_t width, const size_t height){
   unsigned int a;
   for ( a=1;a<width/2;a++){
      usrc[a]+=usrc[a-1];
      vsrc[a]+=vsrc[a-1];
   }
   for ( a=1;a<width;a++){
      ysrc[a]+=ysrc[a-1];
   }

   {
      const __m64 mask = _mm_set_pi32(0x00ff00ff,0x00ff00ff);

      __m64 x = _mm_setr_pi16(ysrc[width-1],usrc[width/2-1],vsrc[width/2-1],0);
      __m64 z = _mm_setr_pi16(ysrc[width-1]-ysrc[0],usrc[width/2-1]-usrc[0],vsrc[width/2-1]-vsrc[0],0);

      for ( a=width; a<width*height/4 + width/2; a+=2){

         __m64 ys = _mm_setzero_si64();
         ys = _mm_insert_pi16(ys,*(int*)&ysrc[a-width],0);
         ys = _mm_insert_pi16(ys,*(int*)&usrc[a-width/2-width/2],1);
         ys = _mm_insert_pi16(ys,*(int*)&vsrc[a-width/2-width/2],2);

         __m64 s = _mm_setzero_si64();
         s = _mm_insert_pi16(s,*(int*)&ysrc[a],0);
         s = _mm_insert_pi16(s,*(int*)&usrc[a-width/2],1);
         s = _mm_insert_pi16(s,*(int*)&vsrc[a-width/2],2);

         __m64 y = _mm_and_si64(ys,mask);
         ys = _mm_srli_pi16(ys,8);
         z = _mm_add_pi16(z,y);

         __m64 srcs = _mm_and_si64(s,mask);
         s = _mm_srli_pi16(s,8);

         __m64 i = _mm_min_pi16(x,y);
         x = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         x = _mm_min_pi16(x,i);
         x = _mm_add_pi8(x,srcs);
         __m64 r = x;

         srcs = s;
         z = _mm_sub_pi16(x,y);
         y = ys;
         z = _mm_add_pi16(z,y);

         i = _mm_min_pi16(x,y);
         x = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         x = _mm_min_pi16(x,i);
         x = _mm_add_pi8(x,srcs);
         r = _mm_or_si64(r,_mm_slli_pi16(x,8));

         *(unsigned short *)&ysrc[a] = _mm_extract_pi16(r,0);
         *(unsigned short *)&usrc[a-width/2] = _mm_extract_pi16(r,1);
         *(unsigned short *)&vsrc[a-width/2] = _mm_extract_pi16(r,2);

         z = _mm_sub_pi16(x,y);
      }

      unsigned int align4 = (a+3)&(~3);
      for ( ;a<align4;a++){
         int x = ysrc[a-1];
         int y = ysrc[a-width];
         int z = x+y-ysrc[a-width-1];
         x = median(x,y,z);
         ysrc[a]+=x;
      }

      x = _mm_cvtsi32_si64(ysrc[a-1]);
      z = _mm_cvtsi32_si64(ysrc[a-width-1]);
      for ( ;a<((width*height)&(~3));a+=4){
         __m64 src = _mm_cvtsi32_si64(*(int*)&ysrc[a]);
         __m64 y = _mm_cvtsi32_si64(*(int*)&ysrc[a-width]);
         y = _mm_unpacklo_pi8(y,_mm_setzero_si64());
         src = _mm_unpacklo_pi8(src,_mm_setzero_si64());

         z = _mm_or_si64(z,_mm_slli_si64(y,2*8));

         z = _mm_sub_pi16(_mm_add_pi16(x,y),z);


         __m64 i = _mm_min_pi16(x,y);
         __m64 j = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         j = _mm_min_pi16(j,i);
         j = _mm_add_pi8(j,src); // 1 correct
         // mask and shift j
         j = _mm_slli_si64(j,6*8);
         j = _mm_srli_si64(j,4*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi16(z,j);

         i = _mm_min_pi16(x,y);
         j = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         j = _mm_min_pi16(j,i);
         j = _mm_add_pi8(j,src); // 2 correct
         // mask and shift j
         j = _mm_slli_si64(j,4*8);
         j = _mm_srli_pi32(j,2*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi16(z,j);

         i = _mm_min_pi16(x,y);
         j = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         j = _mm_min_pi16(j,i);
         j = _mm_add_pi8(j,src); // 3 correct
         // mask and shift j
         j = _mm_srli_si64(j,4*8);
         j = _mm_slli_si64(j,6*8);
         x = _mm_or_si64(x,j);
         z = _mm_add_pi16(z,j);

         i = _mm_min_pi16(x,y);
         x = _mm_max_pi16(x,y);
         i = _mm_max_pi16(i,z);
         x = _mm_min_pi16(x,i);
         x = _mm_add_pi8(x,src); // 4 correct

         *(int*)&ysrc[a] = _mm_cvtsi64_si32(_mm_packs_pu16(x,x));

         x = _mm_srli_si64(x,6*8);
         z = _mm_srli_si64(y,6*8);
      }
   }
   _mm_empty();

   for ( ;a<width*height;a++){
      int x = ysrc[a-1];
      int y = ysrc[a-width];
      int z = x+y-ysrc[a-width-1];
      ysrc[a] += median(x,y,z);
   }
}