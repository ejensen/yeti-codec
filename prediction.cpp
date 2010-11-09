//This file contains functions that perform and undo median prediction.

#include <mmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include <stdlib.h>
#include <memory.h>
#include "yeti.h"

#pragma warning(disable:4731)

#define align_round(x,y) ((((unsigned int)(x))+(y-1))&(~(y-1)))

inline int median(int x,int y,int z ) 
{
   int i = (x<=y)?x:y;	//i=min(x,y);
   int j = (x>=y)?x:y;	//j=max(x,y);
   i = (i>=z)?i:z;	   //i=max(i,z);
   return  (i<=j)?i:j;	//j=min(i,j);
}

void SSE2_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int mode)
{
   unsigned int a;
   __m128i t0;
   t0 = _mm_setzero_si128();
   for ( a=0;a<stride;a+=16)
   {
      __m128i x = _mm_slli_si128( *(__m128i*)(source+a), 1);
      x = _mm_or_si128(x,t0);
      t0 = *(__m128i*)(source+a);
      t0 = _mm_srli_si128(t0,15);
      *(__m128i*)(dest+a)=_mm_sub_epi8(*(__m128i*)(source+a),x);
   }

   __m128i ta;
   __m128i tb;

   tb=*(__m128i *)(source+stride-16); // x
   tb = _mm_srli_si128(tb,15);

   if ( !mode )
   {
      // dest[stride] must equal source[stride]-source[stride-1] not source[0] due to a coding error
      // to achieve this, the initial values for z is set to y to make x the median
      ta=*(__m128i *)(source); // z
      ta = _mm_slli_si128(ta,15);
      ta = _mm_srli_si128(ta,15);
   } 
   else  // otherwise, set z equal to x so y is the median
   {
      ta=tb;
   }

   for ( ;a<length;a+=16)
   {
      __m128i x=*(__m128i*)(source+a);
      x = _mm_slli_si128(x,1);
      x = _mm_or_si128(x,tb);
      __m128i y=*(__m128i*)(source+a-stride);
      __m128i z=*(__m128i*)(source+a-stride);
      z = _mm_slli_si128(z,1);
      z = _mm_or_si128(z,ta);

      __m128i u;
      __m128i v;
      __m128i w;
      __m128i t;

      t = _mm_setzero_si128();
      u = _mm_setzero_si128();
      v = _mm_setzero_si128();
      w = _mm_setzero_si128();

      t = _mm_unpacklo_epi8(x,t);
      u = _mm_unpacklo_epi8(y,u);
      v = _mm_unpacklo_epi8(z,v);
      t = _mm_add_epi16(t,u);
      t = _mm_sub_epi16(t,v);

      u = _mm_setzero_si128();
      v = _mm_setzero_si128();

      u = _mm_unpackhi_epi8(x,u);
      v = _mm_unpackhi_epi8(y,v);
      w = _mm_unpackhi_epi8(z,w);

      u = _mm_add_epi16(u,v);
      u = _mm_sub_epi16(u,w); 

      z = _mm_packus_epi16(t,u); // z now equals x+y-z

      ta = _mm_srli_si128(y,15);
      tb = _mm_srli_si128(*(__m128i*)(source+a),15);

      __m128i i;
      __m128i j;

      i = _mm_min_epu8(x,y);
      j = _mm_max_epu8(x,y);
      i = _mm_max_epu8(i,z);
      j = _mm_min_epu8(i,j);

      *(__m128i*)(dest+a)=_mm_sub_epi8( *(__m128i*)(source+a),j);
   }
}

void MMX_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int mode)
{
   unsigned int a;
   __m64 t0;
   t0 = _mm_setzero_si64();
   for ( a=0;a<stride;a+=8)
   {
      __m64 x = _mm_slli_si64( *(__m64*)(source+a), 8);
      x = _mm_or_si64(x,t0);
      t0 = *(__m64*)(source+a);
      t0 = _mm_srli_si64(t0,7*8);
      *(__m64*)(dest+a)=_mm_sub_pi8(*(__m64*)(source+a),x);
   }

   __m64 ta;
   __m64 tb;

   tb=*(__m64 *)(source+stride-8); // x

   tb = _mm_srli_si64(tb,7*8);

   if ( !mode )
   {
      // dest[stride] must equal source[stride]-source[stride-1] not source[0] due to a coding error
      // to achieve this, the initial values for z is set to y to make x the median
      ta=*(__m64 *)(source); // z
      ta = _mm_slli_si64(ta,7*8);
      ta = _mm_srli_si64(ta,7*8);
   } 
   else  // otherwise, set z equal to x so y is the median
   {
      ta=tb;
   }
   for ( ;a<length;a+=8)
   {
      __m64 x=*(__m64*)(source+a);
      x = _mm_slli_si64(x,8);
      x = _mm_or_si64(x,tb);
      __m64 y=*(__m64*)(source+a-stride);
      __m64 z=*(__m64*)(source+a-stride);
      z = _mm_slli_si64(z,8);
      z = _mm_or_si64(z,ta);

      __m64 u;
      __m64 v;
      __m64 w;
      __m64 t;

      t = _mm_setzero_si64();
      u = _mm_setzero_si64();
      v = _mm_setzero_si64();
      w = _mm_setzero_si64();

      t = _mm_unpacklo_pi8(x,t);
      u = _mm_unpacklo_pi8(y,u);
      v = _mm_unpacklo_pi8(z,v);
      t = _mm_add_pi16(t,u);
      t = _mm_sub_pi16(t,v);

      u = _mm_setzero_si64();
      v = _mm_setzero_si64();

      u = _mm_unpackhi_pi8(x,u);
      v = _mm_unpackhi_pi8(y,v);
      w = _mm_unpackhi_pi8(z,w);

      u = _mm_add_pi16(u,v);
      u = _mm_sub_pi16(u,w); 

      z = _mm_packs_pu16 (t,u); // z now equals x+y-z

      ta = _mm_srli_si64(y,7*8);
      tb = _mm_srli_si64(*(__m64*)(source+a),7*8);

      __m64 i;
      __m64 j;

      i = _mm_min_pu8(x,y);
      j = _mm_max_pu8(x,y);
      i = _mm_max_pu8(i,z);
      j = _mm_min_pu8(i,j);

      *(__m64*)(dest+a)=_mm_sub_pi8( *(__m64*)(source+a),j);
   }

   _mm_empty();
}

void ASM_BlockRestore(unsigned char * source, unsigned int stride,unsigned int xlength, unsigned int mode)
{
   for ( unsigned int a=1; a<stride+!mode; a++)
   {
      source[a] += source[a-1];
   }

   if ( mode )
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
      movzx	ebx,byte ptr[esp+esi] // a-dis-1
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

// halves the resolution and aligns the resulting data
void reduce_res(const unsigned char * src, unsigned char * dest, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2)
{
   const unsigned char * source;

   const unsigned int mod = (SSE2?32:16);

   const unsigned int stride = align_round(width,mod);

   if ( stride != width )
   {
      for ( unsigned int y=0;y<height;y++)
      {
         memcpy(buffer+y*stride,src+y*width,width);
         unsigned char u=buffer[y*stride+width-2];
         unsigned char v=buffer[y*stride+width-1];
         const unsigned int i=(u<<24)+(v<<16)+(u<<8)+v;
         for ( unsigned int x=width;x<stride;x+=4)
         {
            *(unsigned int*)(buffer+y*stride+x)=i;
         }
      }
      source=buffer;
   } 
   else if ( ((int)src)&(mod/2-1) )
   {
      memcpy(buffer,src,width*height);
      source=buffer;
   } 
   else
   {
      source=src;
   }

   if ( SSE2 )
   {
      __m128i mask = _mm_set1_epi16(0x00ff);
      for ( unsigned int y=0;y<height;y+=2)
      {
         for ( unsigned int x=0;x<stride;x+=32)
         {
            __m128i a = *(__m128i *)(source+stride*y+x);
            __m128i b = *(__m128i *)(source+stride*y+x+stride);

            __m128i c = *(__m128i *)(source+stride*y+x+16);
            __m128i d = *(__m128i *)(source+stride*y+x+stride+16);

            a = _mm_avg_epu8(a,b);
            c = _mm_avg_epu8(c,d);

            b = _mm_srli_si128(a,1);
            d = _mm_srli_si128(c,1);

            a = _mm_avg_epu8(a,b);
            c = _mm_avg_epu8(c,d);

            a = _mm_and_si128(a,mask);
            c = _mm_and_si128(c,mask);

            a = _mm_packus_epi16(a,c);
            *(__m128i *)(dest+y*stride/4+x/2)=a;
         }
      }
   }
   else
   {
      __m64 mask = _mm_set1_pi16(0x00ff);
      for ( unsigned int y=0;y<height;y+=2)
      {
         for ( unsigned int x=0;x<stride;x+=16)
         {
            __m64 a = *(__m64 *)(source+stride*y+x);
            __m64 b = *(__m64 *)(source+stride*y+x+stride);

            __m64 c = *(__m64 *)(source+stride*y+x+8);
            __m64 d = *(__m64 *)(source+stride*y+x+stride+8);

            a = _mm_avg_pu8(a,b);
            c = _mm_avg_pu8(c,d);

            b = _mm_srli_si64(a,8);
            d = _mm_srli_si64(c,8);

            a = _mm_avg_pu8(a,b);
            c = _mm_avg_pu8(c,d);

            a = _mm_and_si64(a,mask);
            c = _mm_and_si64(c,mask);

            a = _mm_packs_pu16(a,c);
            *(__m64 *)(dest+y*stride/4+x/2)=a;
         }
      }
      _mm_empty();
   }
}

// doubles the resolution
void enlarge_res(const unsigned char * src, unsigned char * dst, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2)
{
   const unsigned char * source;
   unsigned char * dest;

   const unsigned int mod = (SSE2?32:16);

   unsigned int stride = align_round(width,mod);

   if ( stride != width)
   {
      unsigned int y;
      for ( y=0;y<height;y++)
      {
         memcpy(dst+y*stride,src+y*width,width);
      }
      const unsigned char i = dst[y*stride+width-1];
      for ( unsigned int x=width;x<stride;x++)
      {
         dst[y*stride+x]=i;
      }
      source = dst;
      dest = buffer;
   } 
   else
   {
      source = src;
      dest = dst;
   }

   if ( SSE2 )
   {
      unsigned int y;
      for ( y=0;y<height;y++)
      {
         __m128i p = *(__m128i *)(source+y*stride);
         p = _mm_slli_si128(p,15);
         p = _mm_srli_si128(p,15);
         for ( unsigned int x=0;x<stride;x+=16)
         {
            __m128i a = *(__m128i *)(source+x+y*stride);
            __m128i b = _mm_slli_si128(a,1);

            b = _mm_or_si128(b,p);
            p = _mm_srli_si128(a,15);

            b = _mm_avg_epu8(a,b);

            *(__m128i*)(dest + DOUBLE(x) + QUADRUPLE(y*stride)) =  _mm_unpacklo_epi8(b,a);
            *(__m128i*)(dest + DOUBLE(x) + QUADRUPLE(y*stride) + 16) = _mm_unpackhi_epi8(b,a);
         }
      }

      height = DOUBLE(height);
      stride = DOUBLE(stride);
      width = DOUBLE(width);

      for ( y=0; y < height-2; y+=2)
      {
         for ( unsigned int x=0;x<stride;x+=32)
         {
            __m128i a = *(__m128i *)(dest+x+y*stride);
            __m128i b = *(__m128i *)(dest+x+y*stride + DOUBLE(stride));

            __m128i c = *(__m128i *)(dest+x+y*stride + 16);
            __m128i d = *(__m128i *)(dest+x+y*stride + DOUBLE(stride) + 16);

            a=_mm_avg_epu8(a,b);
            c=_mm_avg_epu8(c,d);
            *(__m128i*)(dest+x+y*stride+stride)=a;
            *(__m128i*)(dest+x+y*stride+stride+16)=c;
         }
      }	
   } 
   else 
   {
      unsigned int y;
      for ( y=0;y<height;y++)
      {
         __m64 p = *(__m64 *)(source+y*stride);
         p = _mm_slli_si64(p,7*8);
         p = _mm_srli_si64(p,7*8);
         for ( unsigned int x=0;x<stride;x+=8)
         {
            __m64 a = *(__m64 *)(source+x+y*stride);
            __m64 b = _mm_slli_si64(a,8);

            b = _mm_or_si64(b,p);
            p = _mm_srli_si64(a,8*7);

            b = _mm_avg_pu8(a,b);

            *(__m64*)(dest+DOUBLE(x)+QUADRUPLE(y*stride)) = _mm_unpacklo_pi8(b,a);
            *(__m64*)(dest+DOUBLE(x)+QUADRUPLE(y*stride)+8)=_mm_unpackhi_pi8(b,a);
         }
      }

      height = DOUBLE(height);
      stride = DOUBLE(stride);
      width = DOUBLE(width);

      for ( y=0;y<height-2;y+=2)
      {
         for ( unsigned int x=0;x<stride;x+=16)
         {
            __m64 a = *(__m64 *)(dest+x+y*stride);
            __m64 b = *(__m64 *)(dest+x+y*stride+DOUBLE(stride));

            __m64 c = *(__m64 *)(dest+x+y*stride+8);
            __m64 d = *(__m64 *)(dest+x+y*stride+DOUBLE(stride)+8);

            a=_mm_avg_pu8(a,b);
            c=_mm_avg_pu8(c,d);
            *(__m64*)(dest+x+y*stride+stride)=a;
            *(__m64*)(dest+x+y*stride+stride+8)=c;
         }
      }
      _mm_empty();
   }

   memcpy(dest+stride*(height-1),dest+stride*(height-2),stride);

   if ( stride != width )
   {
      for ( unsigned int y=0;y<height;y++)
      {
         memcpy(dst+y*width,dest+y*stride,width);
      }
   }
}