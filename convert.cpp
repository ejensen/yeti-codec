#include "convert.h"
#include "prediction.h"
#include <mmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

void ConvertRGB24toYV16_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h) 
{
	const __m128i fraction		= _mm_setr_epi32(0x84000,0x84000,0x84000,0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32			= _mm_setr_epi32(-32,-32,-32,-32);
	const __m128i y1y2_mult		= _mm_setr_epi32(0x4A85,0x4A85,0x4A85,0x4A85);
	const __m128i fpix_add		= _mm_setr_epi32(0x808000,0x808000,0x808000,0x808000);
	const __m128i fpix_mul		= _mm_setr_epi32(0x1fb,0x282,0x1fb,0x282);
	const __m128i cybgr_64		= _mm_setr_epi16(0,0x0c88,0x4087,0x20DE,0x0c88,0x4087,0x20DE,0);

	for ( unsigned int y=0;y<h;y++)
	{
		for ( unsigned int x=0;x<w;x+=4)
		{
			__m128i rgb0 = _mm_cvtsi32_si128(*(int*)&src[y*w*3+x*3]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w*3+x*3+4]);
			rgb0 = _mm_unpacklo_epi32(rgb0,rgb1);
			rgb0 = _mm_slli_si128(rgb0,1);
			rgb1 = _mm_srli_si128(rgb1,1);

			rgb0 = _mm_unpacklo_epi8(rgb0,_mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1,_mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0,cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1,cybgr_64);

			rgb0 = _mm_add_epi16(rgb0,_mm_srli_si128(rgb0,6));
			rgb1 = _mm_add_epi16(rgb1,_mm_srli_si128(rgb1,6));
			
			__m128i chroma = _mm_unpacklo_epi64(rgb0,rgb1);
			chroma = _mm_srli_epi32(chroma,16); // remove green channel

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma0 = _mm_srli_si128(luma0,4);
			luma1 = _mm_srli_si128(luma1,4);
			__m128i luma = _mm_unpacklo_epi64(luma0,luma1);

			luma = _mm_add_epi32(luma,fraction);
			luma = _mm_srli_epi32(luma,15);

			__m128i temp = _mm_add_epi32(luma,_mm_shuffle_epi32(luma,1+(0<<2)+(3<<4)+(2<<6)));
			temp = _mm_add_epi32(temp,neg32);
			temp = _mm_madd_epi16(temp,y1y2_mult);

			luma = _mm_packs_epi32(luma,luma);
			luma = _mm_packus_epi16(luma,luma);
			unsigned int flipped_pos = (h-y-1)*w+x;

			//if ( *(int *)&ydst[flipped_pos]!=_mm_cvtsi128_si32(luma)){
			//	__asm int 3;
			//}

			*(int *)&ydst[flipped_pos]=_mm_cvtsi128_si32(luma);


			chroma = _mm_slli_epi64(chroma,14);
			chroma = _mm_sub_epi32(chroma,temp);
			chroma = _mm_srli_epi32(chroma,9);
			chroma = _mm_madd_epi16(chroma,fpix_mul);
			chroma = _mm_add_epi32(chroma,fpix_add);
			chroma = _mm_packus_epi16(chroma,chroma);
			chroma = _mm_srli_epi16(chroma,8);
			chroma = _mm_shufflelo_epi16(chroma,0+(2<<2)+(1<<4)+(3<<6));
			chroma = _mm_packus_epi16(chroma,chroma);

			//if ( *(unsigned short *)&udst[flipped_pos/2]!=_mm_extract_epi16(chroma,0) ||
			//	*(unsigned short *)&vdst[flipped_pos/2]!=_mm_extract_epi16(chroma,1)){
			//	__asm int 3;
			//}

			*(unsigned short *)&udst[flipped_pos/2]=_mm_extract_epi16(chroma,0);
			*(unsigned short *)&vdst[flipped_pos/2]=_mm_extract_epi16(chroma,1);
		}
	}
}

void ConvertRGB32toYV16_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h) {
	const __m128i fraction		= _mm_setr_epi32(0x84000,0x84000,0x84000,0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32			= _mm_setr_epi32(-32,-32,-32,-32);
	const __m128i y1y2_mult		= _mm_setr_epi32(0x4A85,0x4A85,0x4A85,0x4A85);
	const __m128i fpix_add		= _mm_setr_epi32(0x808000,0x808000,0x808000,0x808000);
	const __m128i fpix_mul		= _mm_setr_epi32(0x1fb,0x282,0x1fb,0x282);
	const __m128i cybgr_64		= _mm_setr_epi16(0x0c88,0x4087,0x20DE,0,0x0c88,0x4087,0x20DE,0);

	for ( unsigned int y=0;y<h;y++){
		for ( unsigned int x=0;x<w;x+=4){
			__m128i rgb0 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4+8]);

			rgb0 = _mm_unpacklo_epi8(rgb0,_mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1,_mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0,cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1,cybgr_64);

			rgb0 = _mm_add_epi16(rgb0,_mm_shuffle_epi32(rgb0,(2<<0)+(3<<2)+(0<<4)+(1<<6)));
			rgb1 = _mm_add_epi16(rgb1,_mm_shuffle_epi32(rgb1,(2<<0)+(3<<2)+(0<<4)+(1<<6)));
			
			__m128i chroma = _mm_unpacklo_epi64(rgb0,rgb1);
			chroma = _mm_slli_epi32(chroma,16); // remove green channel

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma0 = _mm_srli_si128(luma0,4);
			luma1 = _mm_srli_si128(luma1,4);
			__m128i luma = _mm_unpacklo_epi64(luma0,luma1);

			luma = _mm_add_epi32(luma,fraction);
			luma = _mm_srli_epi32(luma,15);

			__m128i temp = _mm_add_epi32(luma,_mm_shuffle_epi32(luma,1+(0<<2)+(3<<4)+(2<<6)));
			temp = _mm_add_epi32(temp,neg32);
			temp = _mm_madd_epi16(temp,y1y2_mult);

			luma = _mm_packs_epi32(luma,luma);
			luma = _mm_packus_epi16(luma,luma);
			unsigned int flipped_pos = (h-y-1)*w+x;

			//if ( *(int *)&ydst[flipped_pos]!=_mm_cvtsi128_si32(luma)){
			//	__asm int 3;
			//}

			*(int *)&ydst[flipped_pos]=_mm_cvtsi128_si32(luma);

			chroma = _mm_srli_epi64(chroma,2);
			chroma = _mm_sub_epi32(chroma,temp);
			chroma = _mm_srli_epi32(chroma,9);
			chroma = _mm_madd_epi16(chroma,fpix_mul);
			chroma = _mm_add_epi32(chroma,fpix_add);
			chroma = _mm_packus_epi16(chroma,chroma);
			chroma = _mm_srli_epi16(chroma,8);
			chroma = _mm_shufflelo_epi16(chroma,0+(2<<2)+(1<<4)+(3<<6));
			chroma = _mm_packus_epi16(chroma,chroma);

			//if ( *(unsigned short *)&udst[flipped_pos/2]!=_mm_extract_epi16(chroma,0) ||
			//	*(unsigned short *)&vdst[flipped_pos/2]!=_mm_extract_epi16(chroma,1)){
			//	__asm int 3;
			//}
			*(unsigned short *)&udst[flipped_pos/2]=_mm_extract_epi16(chroma,0);
			*(unsigned short *)&vdst[flipped_pos/2]=_mm_extract_epi16(chroma,1);
		}
	}
}

void ConvertRGB32toYV12_SSE2(const unsigned char *src, unsigned char *ydest, unsigned char *udest, unsigned char *vdest, unsigned int w, unsigned int h) 
{
	const __m128i fraction		= _mm_setr_epi32(0x84000,0x84000,0x84000,0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32			= _mm_setr_epi32(-32,-32,-32,-32);
	const __m128i y1y2_mult		= _mm_setr_epi32(0x4A85,0x4A85,0x4A85,0x4A85);
	const __m128i fpix_add		= _mm_setr_epi32(0x808000,0x808000,0x808000,0x808000);
	const __m128i fpix_mul		= _mm_setr_epi32(0x1fb,0x282,0x1fb,0x282);
	const __m128i cybgr_64		= _mm_setr_epi16(0x0c88,0x4087,0x20DE,0,0x0c88,0x4087,0x20DE,0);

	for ( unsigned int y=0;y<h;y+=2)
	{
		unsigned char *ydst=ydest+(h-y-1)*w;
		unsigned char *udst=udest+(h-y-2)/2*w/2;
		unsigned char *vdst=vdest+(h-y-2)/2*w/2;
		for ( unsigned int x=0;x<w;x+=4){
			__m128i rgb0 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4+8]);
			__m128i rgb2 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4+w*4]);
			__m128i rgb3 = _mm_loadl_epi64((__m128i*)&src[y*w*4+x*4+8+w*4]);

			rgb0 = _mm_unpacklo_epi8(rgb0,_mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1,_mm_setzero_si128());
			rgb2 = _mm_unpacklo_epi8(rgb2,_mm_setzero_si128());
			rgb3 = _mm_unpacklo_epi8(rgb3,_mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0,cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1,cybgr_64);
			__m128i luma2 = _mm_madd_epi16(rgb2,cybgr_64);
			__m128i luma3 = _mm_madd_epi16(rgb3,cybgr_64);

			rgb0 = _mm_add_epi16(rgb0,_mm_shuffle_epi32(rgb0,(2<<0)+(3<<2)+(0<<4)+(1<<6)));
			rgb1 = _mm_add_epi16(rgb1,_mm_shuffle_epi32(rgb1,(2<<0)+(3<<2)+(0<<4)+(1<<6)));
			rgb2 = _mm_add_epi16(rgb2,_mm_shuffle_epi32(rgb2,(2<<0)+(3<<2)+(0<<4)+(1<<6)));
			rgb3 = _mm_add_epi16(rgb3,_mm_shuffle_epi32(rgb3,(2<<0)+(3<<2)+(0<<4)+(1<<6)));

			__m128i chroma0 = _mm_unpacklo_epi64(rgb0,rgb1);
			__m128i chroma1 = _mm_unpacklo_epi64(rgb2,rgb3);
			chroma0 = _mm_slli_epi32(chroma0,16); // remove green channel
			chroma1 = _mm_slli_epi32(chroma1,16);

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma2 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma3 = _mm_add_epi32(luma3, _mm_shuffle_epi32(luma3,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma0 = _mm_srli_si128(luma0,4);
			luma1 = _mm_srli_si128(luma1,4);
			luma2 = _mm_srli_si128(luma2,4);
			luma3 = _mm_srli_si128(luma3,4);
			luma0 = _mm_unpacklo_epi64(luma0,luma1);
			luma2 = _mm_unpacklo_epi64(luma2,luma3);

			luma0 = _mm_add_epi32(luma0,fraction);
			luma2 = _mm_add_epi32(luma2,fraction);
			luma0 = _mm_srli_epi32(luma0,15);
			luma2 = _mm_srli_epi32(luma2,15);

			__m128i temp0 = _mm_add_epi32(luma0,_mm_shuffle_epi32(luma0,1+(0<<2)+(3<<4)+(2<<6)));
			__m128i temp1 = _mm_add_epi32(luma2,_mm_shuffle_epi32(luma2,1+(0<<2)+(3<<4)+(2<<6)));
			temp0 = _mm_add_epi32(temp0,neg32);
			temp1 = _mm_add_epi32(temp1,neg32);
			temp0 = _mm_madd_epi16(temp0,y1y2_mult);
			temp1 = _mm_madd_epi16(temp1,y1y2_mult);

			luma0 = _mm_packs_epi32(luma0,luma0);
			luma2 = _mm_packs_epi32(luma2,luma2);
			luma0 = _mm_packus_epi16(luma0,luma0);
			luma2 = _mm_packus_epi16(luma2,luma2);

			//if ( *(int *)&ydst[x]!=_mm_cvtsi128_si32(luma0)||
			//	*(int *)&ydst[x-w]!=_mm_cvtsi128_si32(luma2)){
			//	__asm int 3;
			//}

			*(int *)&ydst[x]=_mm_cvtsi128_si32(luma0);
			*(int *)&ydst[x-w]=_mm_cvtsi128_si32(luma2);

			chroma0 = _mm_srli_epi64(chroma0,2);
			chroma1 = _mm_srli_epi64(chroma1,2);
			chroma0 = _mm_sub_epi32(chroma0,temp0);
			chroma1 = _mm_sub_epi32(chroma1,temp1);
			chroma0 = _mm_srli_epi32(chroma0,9);
			chroma1 = _mm_srli_epi32(chroma1,9);
			chroma0 = _mm_madd_epi16(chroma0,fpix_mul);
			chroma1 = _mm_madd_epi16(chroma1,fpix_mul);
			chroma0 = _mm_add_epi32(chroma0,fpix_add);
			chroma1 = _mm_add_epi32(chroma1,fpix_add);
			chroma0 = _mm_packus_epi16(chroma0,chroma0);
			chroma1 = _mm_packus_epi16(chroma1,chroma1);

			chroma0 = _mm_avg_epu8(chroma0,chroma1);

			chroma0 = _mm_srli_epi16(chroma0,8);
			chroma0 = _mm_shufflelo_epi16(chroma0,0+(2<<2)+(1<<4)+(3<<6));
			chroma0 = _mm_packus_epi16(chroma0,chroma0);
			
			//if ( *(unsigned short *)&udst[x/2]!=_mm_extract_epi16(chroma0,0) ||
			//	*(unsigned short *)&vdst[x/2]!=_mm_extract_epi16(chroma0,1)){
			//	__asm int 3;
			//}

			*(unsigned short *)&udst[x/2] = _mm_extract_epi16(chroma0,0);
			*(unsigned short *)&vdst[x/2] = _mm_extract_epi16(chroma0,1);
		}
	}
}

void ConvertRGB24toYV12_SSE2(const unsigned char *src, unsigned char *ydest, unsigned char *udest, unsigned char *vdest, unsigned int w, unsigned int h)
{
	const __m128i fraction		= _mm_setr_epi32(0x84000,0x84000,0x84000,0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32			= _mm_setr_epi32(-32,-32,-32,-32);
	const __m128i y1y2_mult		= _mm_setr_epi32(0x4A85,0x4A85,0x4A85,0x4A85);
	const __m128i fpix_add		= _mm_setr_epi32(0x808000,0x808000,0x808000,0x808000);
	const __m128i fpix_mul		= _mm_setr_epi32(0x1fb,0x282,0x1fb,0x282);
	const __m128i cybgr_64		= _mm_setr_epi16(0,0x0c88,0x4087,0x20DE,0x0c88,0x4087,0x20DE,0);

	for ( unsigned int y=0;y<h;y+=2)
	{
		unsigned char *ydst=ydest+(h-y-1)*w;
		unsigned char *udst=udest+(h-y-2)/2*w/2;
		unsigned char *vdst=vdest+(h-y-2)/2*w/2;
		for ( unsigned int x=0;x<w;x+=4){
			__m128i rgb0 = _mm_cvtsi32_si128(*(int*)&src[y*w*3+x*3]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w*3+x*3+4]);
			__m128i rgb2 = _mm_cvtsi32_si128(*(int*)&src[y*w*3+x*3+w*3]);
			__m128i rgb3 = _mm_loadl_epi64((__m128i*)&src[y*w*3+x*3+4+w*3]);
			rgb0 = _mm_unpacklo_epi32(rgb0,rgb1);
			rgb0 = _mm_slli_si128(rgb0,1);
			rgb1 = _mm_srli_si128(rgb1,1);

			rgb2 = _mm_unpacklo_epi32(rgb2,rgb3);
			rgb2 = _mm_slli_si128(rgb2,1);
			rgb3 = _mm_srli_si128(rgb3,1);

			rgb0 = _mm_unpacklo_epi8(rgb0,_mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1,_mm_setzero_si128());
			rgb2 = _mm_unpacklo_epi8(rgb2,_mm_setzero_si128());
			rgb3 = _mm_unpacklo_epi8(rgb3,_mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0,cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1,cybgr_64);
			__m128i luma2 = _mm_madd_epi16(rgb2,cybgr_64);
			__m128i luma3 = _mm_madd_epi16(rgb3,cybgr_64);

			rgb0 = _mm_add_epi16(rgb0,_mm_srli_si128(rgb0,6));
			rgb1 = _mm_add_epi16(rgb1,_mm_srli_si128(rgb1,6));
			rgb2 = _mm_add_epi16(rgb2,_mm_srli_si128(rgb2,6));
			rgb3 = _mm_add_epi16(rgb3,_mm_srli_si128(rgb3,6));
			
			__m128i chroma0 = _mm_unpacklo_epi64(rgb0,rgb1);
			__m128i chroma1 = _mm_unpacklo_epi64(rgb2,rgb3);
			chroma0 = _mm_srli_epi32(chroma0,16); // remove green channel
			chroma1 = _mm_srli_epi32(chroma1,16); // remove green channel

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma2 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma3 = _mm_add_epi32(luma3, _mm_shuffle_epi32(luma3,(1<<0)+(0<<2)+(3<<4)+(2<<6)));
			luma0 = _mm_srli_si128(luma0,4);
			luma1 = _mm_srli_si128(luma1,4);
			luma2 = _mm_srli_si128(luma2,4);
			luma3 = _mm_srli_si128(luma3,4);
			luma0 = _mm_unpacklo_epi64(luma0,luma1);
			luma2 = _mm_unpacklo_epi64(luma2,luma3); // luma1, luma3 no longer used

			luma0 = _mm_add_epi32(luma0,fraction);
			luma2 = _mm_add_epi32(luma2,fraction);
			luma0 = _mm_srli_epi32(luma0,15);
			luma2 = _mm_srli_epi32(luma2,15);

			__m128i temp0 = _mm_add_epi32(luma0,_mm_shuffle_epi32(luma0,1+(0<<2)+(3<<4)+(2<<6)));
			__m128i temp1 = _mm_add_epi32(luma2,_mm_shuffle_epi32(luma2,1+(0<<2)+(3<<4)+(2<<6)));
			temp0 = _mm_add_epi32(temp0,neg32);
			temp1 = _mm_add_epi32(temp1,neg32);
			temp0 = _mm_madd_epi16(temp0,y1y2_mult);
			temp1 = _mm_madd_epi16(temp1,y1y2_mult);

			luma0 = _mm_packs_epi32(luma0,luma0);
			luma2 = _mm_packs_epi32(luma2,luma2);
			luma0 = _mm_packus_epi16(luma0,luma0);
			luma2 = _mm_packus_epi16(luma2,luma2);
			//unsigned int flipped_pos = (h-y-1)*w+x;

			//if ( *(int *)&ydst[x]!=_mm_cvtsi128_si32(luma0)||
			//	*(int *)&ydst[x-w]!=_mm_cvtsi128_si32(luma2) ){
			//	__asm int 3;
			//}

			*(int *)&ydst[x]=_mm_cvtsi128_si32(luma0);
			*(int *)&ydst[x-w]=_mm_cvtsi128_si32(luma2);


			chroma0 = _mm_slli_epi64(chroma0,14);
			chroma1 = _mm_slli_epi64(chroma1,14);
			chroma0 = _mm_sub_epi32(chroma0,temp0);
			chroma1 = _mm_sub_epi32(chroma1,temp1);
			chroma0 = _mm_srli_epi32(chroma0,9);
			chroma1 = _mm_srli_epi32(chroma1,9);
			chroma0 = _mm_madd_epi16(chroma0,fpix_mul);
			chroma1 = _mm_madd_epi16(chroma1,fpix_mul);
			chroma0 = _mm_add_epi32(chroma0,fpix_add);
			chroma1 = _mm_add_epi32(chroma1,fpix_add);
			chroma0 = _mm_packus_epi16(chroma0,chroma0);
			chroma1 = _mm_packus_epi16(chroma1,chroma1);
			
			chroma0 = _mm_avg_epu8(chroma0,chroma1);

			chroma0 = _mm_srli_epi16(chroma0,8);
			chroma0 = _mm_shufflelo_epi16(chroma0,0+(2<<2)+(1<<4)+(3<<6));
			chroma0 = _mm_packus_epi16(chroma0,chroma0);

			//if ( *(unsigned short *)&udst[x/2]!=_mm_extract_epi16(chroma0,0) ||
			//	*(unsigned short *)&vdst[x/2]!=_mm_extract_epi16(chroma0,1)){
			//	__asm int 3;
			//}

			*(unsigned short *)&udst[x/2]=_mm_extract_epi16(chroma0,0);
			*(unsigned short *)&vdst[x/2]=_mm_extract_epi16(chroma0,1);
		}
	}
}