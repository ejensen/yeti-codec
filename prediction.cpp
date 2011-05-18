#include <emmintrin.h>

#include "yeti.h"
#include "common.h"
#include "prediction.h"

// this effectively performs a bubble sort to select the median:
// min(max(min(x,y),z),max(x,y))
// this assumes 32 bit ints, and that x and y are >=0 and <= 0x80000000
inline int median(int x,int y,int z ) 
{
	int delta = x-y;
	delta &= delta>>31;
	y+=delta; // min
	x-=delta; // max
	delta = y-z;
	delta &= delta>>31;
	y -=delta;	// max
	delta = y-x;
	delta &= delta>>31;
	return  x+delta;	// min
}

void Block_Predict(const BYTE* __restrict source, BYTE* __restrict dest, const size_t width, const size_t length)
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
		t0 = _mm_cvtsi32_si128(source[align_shift-1]);
	}
	for ( a = align_shift; a < width; a += 16){
		__m128i x = *(__m128i*)&source[a];
		t0 = _mm_or_si128(t0,_mm_slli_si128(x,1));
		*(__m128i*)&dest[a]=_mm_sub_epi8(x,t0);
		t0 = _mm_srli_si128(x,15);
	}

	if (width >= length)
	{
		return;
	}

	__m128i z;
	__m128i x;

	x = _mm_cvtsi32_si128(source[width-1]);
	z = _mm_cvtsi32_si128(source[0]);

	a = width;
	{
		// this block makes sure that source[a] is aligned to 16
		__m128i srcs = _mm_loadu_si128((__m128i *)&source[a]);
		__m128i y = _mm_loadu_si128((__m128i *)&source[a-width]);

		x = _mm_or_si128(x,_mm_slli_si128(srcs,1));
		z = _mm_or_si128(z,_mm_slli_si128(y,1));

		__m128i tx = _mm_unpackhi_epi8(x ,_mm_setzero_si128());
		__m128i ty = _mm_unpackhi_epi8(y, _mm_setzero_si128());
		__m128i tz = _mm_unpackhi_epi8(z, _mm_setzero_si128());

		tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

		tx = _mm_unpacklo_epi8(x, _mm_setzero_si128());
		ty = _mm_unpacklo_epi8(y, _mm_setzero_si128());
		z = _mm_unpacklo_epi8(z, _mm_setzero_si128());
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
	x = _mm_cvtsi32_si128(source[a-1]);
	z = _mm_cvtsi32_si128(source[a-width-1]);

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

			__m128i tx = _mm_unpackhi_epi8(x, _mm_setzero_si128());
			__m128i ty = _mm_unpackhi_epi8(y, _mm_setzero_si128());
			__m128i tz = _mm_unpackhi_epi8(z, _mm_setzero_si128());

			tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

			tx = _mm_unpacklo_epi8(x, _mm_setzero_si128());
			ty = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			z = _mm_unpacklo_epi8(z, _mm_setzero_si128());
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

			__m128i tx = _mm_unpackhi_epi8(x,_mm_setzero_si128());
			__m128i ty = _mm_unpackhi_epi8(y,_mm_setzero_si128());
			__m128i tz = _mm_unpackhi_epi8(z,_mm_setzero_si128());

			tz = _mm_add_epi16(_mm_sub_epi16(tx,tz),ty);

			tx = _mm_unpacklo_epi8(x,_mm_setzero_si128());
			ty = _mm_unpacklo_epi8(y,_mm_setzero_si128());
			z = _mm_unpacklo_epi8(z,_mm_setzero_si128());
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

	_mm_empty();

	int x2, y2;
	for ( ; a<length; a++ )
	{
		x2 = source[a-1];
		y2 = source[a-width];
		dest[a] = source[a] - median(x2, y2, x2 + y2 - source[a - width - 1]);
	}
}

void Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length, const bool is_y)
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
		t0 = _mm_cvtsi32_si128(source[align_shift-1]);
	}
	for ( a=align_shift;a<width;a+=16)
	{
		__m128i x = *(__m128i*)&source[a];
		t0 = _mm_or_si128(t0,_mm_slli_si128(x,1));
		*(__m128i*)&dest[a]=_mm_sub_epi8(x,t0);
		t0 = _mm_srli_si128(x,15);
	}
	if ( width>=length )
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
	x = _mm_cvtsi32_si128(source[a-1]);
	z = _mm_cvtsi32_si128(source[a-width-1]);

	const unsigned int end = length&(~15);
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

	_mm_empty();

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
	const __m128i ymask = _mm_set1_epi32(0x00FF00FF);
	const __m128i umask = _mm_set1_epi32(0x0000FF00);

	unsigned int a=0;
	unsigned int align = FOURTH((unsigned int)src & 15);

	for ( ;a<align;a+=2)
	{
		ydst[a]			= src[DOUBLE(a)];
		vdst[HALF(a)]	= src[DOUBLE(a)+1];
		ydst[a+1]		= src[DOUBLE(a)+2];
		udst[HALF(a)]	= src[DOUBLE(a)+3];
	}

	const unsigned int end = (width*height-align)&(~15);
	for ( ;a<end;a+=16)
	{
		__m128i y0 = *(__m128i*)&src[a*2+0];
		__m128i y1 = *(__m128i*)&src[a*2+16];

		__m128i u0 = _mm_srli_epi32(_mm_and_si128(y0,umask),8);
		__m128i v0 = _mm_srli_epi32(y0,24);
		y0 = _mm_and_si128(y0,ymask);

		__m128i u1 = _mm_srli_epi32(_mm_and_si128(y1,umask),8);
		__m128i v1 = _mm_srli_epi32(y1,24);
		y1 = _mm_and_si128(y1,ymask);

		y0 = _mm_packus_epi16(y0,y1);
		v0 = _mm_packus_epi16(v0,v1);
		u0 = _mm_packus_epi16(u0,u1);
		u0 = _mm_packus_epi16(u0,v0);

		*(__m128i*)&ydst[a] = y0;
		_mm_storel_epi64((__m128i*)&udst[a/2],u0);
		_mm_storel_epi64((__m128i*)&vdst[a/2],_mm_srli_si128(u0,8));
	}

	_mm_empty();

	for ( ;a<height*width;a+=2)
	{
		ydst[a]			= src[DOUBLE(a)];
		vdst[HALF(a)]	= src[DOUBLE(a)+1];
		ydst[a+1]		= src[DOUBLE(a)+2];
		udst[HALF(a)]	= src[DOUBLE(a)+3];
	}
}

void Split_UYVY(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const size_t width, const size_t height)
{
	unsigned int a=0;
	unsigned int align = FOURTH((unsigned int)src & 15);

	for ( ;a<align;a+=2)
	{
		vdst[HALF(a)]	= src[DOUBLE(a)];
		ydst[a]			= src[DOUBLE(a)+1];
		udst[HALF(a)]	= src[DOUBLE(a)+2];
		ydst[a+1]		= src[DOUBLE(a)+3];
	}

	const __m128i cmask = _mm_set_epi32(0x000000FF,0x000000FF,0x000000FF,0x000000FF);
	const unsigned int end = (width*height-align)&(~15);

	for ( ;a<end;a+=16)
	{
		__m128i u0 = *(__m128i*)&src[a*2+0];
		__m128i u1 = *(__m128i*)&src[a*2+16];

		__m128i y0 = _mm_srli_epi16(u0,8);
		__m128i v0 = _mm_and_si128(_mm_srli_si128(u0,2),cmask);
		u0 = _mm_and_si128(u0,cmask);

		__m128i y1 = _mm_srli_epi16(u1,8);
		__m128i v1 = _mm_and_si128(_mm_srli_si128(u1,2),cmask);
		u1 = _mm_and_si128(u1,cmask);

		y0 = _mm_packus_epi16(y0,y1);
		v0 = _mm_packus_epi16(v0,v1);
		u0 = _mm_packus_epi16(u0,u1);
		u0 = _mm_packus_epi16(u0,v0);

		*(__m128i*)&ydst[a] = y0;
		_mm_storel_epi64((__m128i*)&udst[a/2],u0);
		_mm_storel_epi64((__m128i*)&vdst[a/2],_mm_srli_si128(u0,8));
	}

	_mm_empty();

	for ( ;a<height*width;a+=2)
	{
		vdst[a/2]	= src[DOUBLE(a)];
		ydst[a]		= src[DOUBLE(a)+1];
		udst[a/2]	= src[DOUBLE(a)+2];
		ydst[a+1]	= src[DOUBLE(a)+3];
	}
}

void Interleave_And_Restore_YUY2(BYTE* __restrict output, BYTE* __restrict ysrc, const BYTE* __restrict usrc, const BYTE* __restrict vsrc, const unsigned int width, const unsigned int height)
{
	// restore the bottom row of pixels + 2 pixels
	{
		int y,u,v;
		output[0]=ysrc[0];
		output[1]=u=usrc[0];
		output[2]=y=ysrc[1];
		output[3]=v=vsrc[0];

		for ( unsigned int a=1;a<width/2+2;a++)
		{
			output[a*4]=y+=ysrc[DOUBLE(a)];
			output[a*4+1]=u+=usrc[a];
			output[a*4+2]=y+=ysrc[DOUBLE(a)+1];
			output[a*4+3]=v+=vsrc[a];
		}
	}

	if (height<=1)
		return;

	const unsigned int stride = DOUBLE(width);
	unsigned int a = HALF(width) + 2;

	// make sure output[a*4-stride] is aligned
	for( ;(a*4-stride)&15;a++ )
	{
		int x = output[a*4-2];
		int y = output[a*4-stride];
		int z = (x+y-output[a*4-stride-2])&255;
		output[a*4+0]=ysrc[a*2+0]+median(x,y,z);

		x = output[a*4-3];
		y = output[a*4-stride+1];
		z = (x+y-output[a*4-stride-3])&255;
		output[a*4+1]=usrc[a]+median(x,y,z);

		x = output[a*4];
		y = output[a*4-stride+2];
		z = (x+y-output[a*4-stride])&255;
		output[a*4+2]=ysrc[a*2+1]+median(x,y,z);

		x = output[a*4-1];
		y = output[a*4-stride+3];
		z = (x+y-output[a*4-stride-1])&255;
		output[a*4+3]=vsrc[a]+median(x,y,z);
	}

	{
		__m128i x = _mm_setr_epi8(output[a*4-2],output[a*4-3],0,output[a*4-1],0,0,0,0,0,0,0,0,0,0,0,0);
		__m128i z = _mm_setr_epi8(output[a*4-3-stride],output[a*4-2-stride],output[a*4-1-stride],0,0,0,0,0,0,0,0,0,0,0,0,0);
		const int ymask=255;
		const int cmask=~255;
		unsigned int ending = ((height*width)/2-3);

		// restore the majority of the pixels using SSE2 median prediction
		for ( ; a<ending; a+=4){
			__m128i srcs = _mm_loadl_epi64((__m128i *)&ysrc[a*2]);
			__m128i temp0 = _mm_cvtsi32_si128( *(int*)&usrc[a]);
			__m128i temp1 = _mm_cvtsi32_si128( *(int*)&vsrc[a]);
			srcs = _mm_unpacklo_epi8(srcs,_mm_unpacklo_epi8(temp0,temp1));

			__m128i y = _mm_load_si128((__m128i *)&output[a*4-stride]);

			z = _mm_or_si128(z,_mm_slli_si128(y,3)); // z=uyvyuyvy
			z = _mm_or_si128(_mm_srli_epi16(z,8),_mm_slli_epi16(z,8));// z = yuyvyuyv
			z = _mm_sub_epi8(_mm_add_epi8(x,y),z);

			__m128i i = _mm_min_epu8(x,y);//min
			__m128i j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yu-v------------
			// mask and shift j (yuv)
			j = _mm_shufflelo_epi16(j,(0<<0)+(0<<2)+(0<<4)+(1<<6));
			j = _mm_and_si128(j,_mm_setr_epi16(0,ymask,cmask,cmask,0,0,0,0));
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyv-u-v--------

			// mask and shift j (yuv)
			j = _mm_slli_si128(j,4);
			j = _mm_shufflelo_epi16(j,(3<<4));
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,ymask,0,cmask,cmask,0,0));

			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyvyu-v-u-v----
			// mask and shift j (y only)
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,ymask,0,0,0,0,0));
			j = _mm_slli_epi32(j,16);
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyvyuyv-u-v----
			// mask and shift j (yuv )
			j = _mm_slli_si128(j,4);
			j = _mm_shufflehi_epi16(j,(1<<0)+(1<<2)+(2<<4)+(3<<6));
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,0,0,ymask,0,cmask,cmask));
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyvyuyvyu-v-u-v
			// mask and shift j (y only )
			j = _mm_slli_si128(j,2);
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,0,0,0,ymask,0,0));
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyvyuyvyuyv-u-v
			// mask and shift j (y only )
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,0,0,0,ymask,0,0));
			j = _mm_slli_si128(j,2);
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			j = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			j = _mm_min_epu8(i,j);//min
			j = _mm_add_epi8(j,srcs); // yuyvyuyvyuyvyu-v
			// mask and shift j (y only )
			j = _mm_slli_si128(j,2);
			j = _mm_and_si128(j,_mm_setr_epi16(0,0,0,0,0,0,0,ymask));
			x = _mm_or_si128(x,j);
			z = _mm_add_epi8(z,j);

			i = _mm_min_epu8(x,y);//min
			x = _mm_max_epu8(x,y);//max
			i = _mm_max_epu8(i,z);//max
			x = _mm_min_epu8(i,x);//min
			x = _mm_add_epi8(x,srcs); // yuyvyuyvyuyvyuyv
			_mm_storeu_si128((__m128i*)&output[a*4],x); // will be aligned if width%8 is 0; but the speed change is negligible in my tests
			x = _mm_srli_epi64(x,40);
			x = _mm_unpackhi_epi8(x,_mm_setzero_si128());
			x = _mm_shufflelo_epi16(x,(1<<0)+(0<<2)+(3<<4)+(2<<6));
			x = _mm_packus_epi16(x,_mm_setzero_si128());

			z = _mm_srli_si128(y,13);

		}
	}

	_mm_empty();

	// finish off any remaining pixels
	for( ;a < width*height/2;a++ )
	{
		int x = output[a*4-2];
		int y = output[a*4-stride];
		int z = (x+y-output[a*4-stride-2])&255;
		output[a*4+0]=ysrc[a*2+0]+median(x,y,z);

		x = output[a*4-3];
		y = output[a*4-stride+1];
		z = (x+y-output[a*4-stride-3])&255;
		output[a*4+1]=usrc[a]+median(x,y,z);

		x = output[a*4];
		y = output[a*4-stride+2];
		z = (x+y-output[a*4-stride])&255;
		output[a*4+2]=ysrc[a*2+1]+median(x,y,z);

		x = output[a*4-1];
		y = output[a*4-stride+3];
		z = (x+y-output[a*4-stride-1])&255;
		output[a*4+3]=vsrc[a]+median(x,y,z);
	}

}

void Restore_YV12(BYTE * __restrict ysrc, BYTE * __restrict usrc, BYTE * __restrict vsrc, const size_t width, const size_t height)
{
	unsigned int a;
	{
		int y = 0;
		int u = 0;
		int v = 0;
		for ( a=0;a<width/2;a++)
		{
			usrc[a]=u+=usrc[a];
			vsrc[a]=v+=vsrc[a];
		}
		for ( a=0;a<width;a++)
		{
			ysrc[a]=y+=ysrc[a];
		}
	}

	//const __m128i mask = _mm_setr_epi32(0x00ff00ff,0x00ff00ff,0,0);

	__m128i x = _mm_setr_epi16(ysrc[width-1],usrc[width/2-1],vsrc[width/2-1],0,0,0,0,0);
	__m128i z = _mm_setr_epi16(ysrc[0],usrc[0],vsrc[0],0,0,0,0,0);

	for ( a=width; a<((width*height/4 + width/2)&(~3)); a+=4){

		__m128i y = _mm_cvtsi32_si128(*(int*)&ysrc[a-width]);
		__m128i y1 = _mm_cvtsi32_si128(*(int*)&usrc[a-width]);
		__m128i y2 = _mm_cvtsi32_si128(*(int*)&vsrc[a-width]);
		y = _mm_unpacklo_epi8(y,y1);
		y2 = _mm_unpacklo_epi8(y2,_mm_setzero_si128());
		y = _mm_unpacklo_epi16(y,y2);

		__m128i srcs =_mm_cvtsi32_si128(*(int*)&ysrc[a]);
		__m128i s1 = _mm_cvtsi32_si128(*(int*)&usrc[a-width/2]);
		__m128i s2 = _mm_cvtsi32_si128(*(int*)&vsrc[a-width/2]);

		srcs = _mm_unpacklo_epi8(srcs,s1);
		s2 = _mm_unpacklo_epi8(s2,_mm_setzero_si128());
		srcs = _mm_unpacklo_epi16(srcs,s2);

		__m128i temp = _mm_unpackhi_epi64(y,srcs);
		y = _mm_unpacklo_epi8(y,_mm_setzero_si128());
		srcs = _mm_unpacklo_epi8(srcs,_mm_setzero_si128());

		z = _mm_unpacklo_epi64(z,y);
		z = _mm_sub_epi16(_mm_add_epi16(x,y),z);

		__m128i i = _mm_min_epi16(x,y);
		__m128i j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,srcs);
		j = _mm_slli_si128(j,8);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		x = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		x = _mm_min_epi16(x,i);
		x = _mm_add_epi8(x,srcs);

		z = _mm_srli_si128(y,8);
		srcs = _mm_unpackhi_epi8(temp,_mm_setzero_si128());
		y = _mm_unpacklo_epi8(temp,_mm_setzero_si128());
		z = _mm_unpacklo_epi64(z,y);

		temp = x;
		x = _mm_srli_si128(x,8);

		z = _mm_sub_epi16(_mm_add_epi16(x,y),z);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,srcs);
		j = _mm_slli_si128(j,8);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		temp = _mm_or_si128(temp,_mm_srli_si128(temp,7));

		i = _mm_min_epi16(x,y);
		x = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		x = _mm_min_epi16(x,i);
		x = _mm_add_epi8(x,srcs);
		i = x;
		x = _mm_srli_si128(x,7);
		i = _mm_or_si128(i,x);
		temp = _mm_unpacklo_epi16(temp,i);

		*(int*)&ysrc[a]			= _mm_cvtsi128_si32(temp);
		*(int*)&usrc[a-width/2] = _mm_cvtsi128_si32(_mm_srli_si128(temp,4));
		*(int*)&vsrc[a-width/2] = _mm_cvtsi128_si32(_mm_srli_si128(temp,8));

		x = _mm_srli_epi16(x,8);
		z = _mm_srli_si128(y,8);
	}

	const size_t halfWidth = HALF(width);
	for ( ;a<width*height/4 + halfWidth;a++)
	{
		int i = ysrc[a-1];
		int j = ysrc[a-width];
		int k = i+j-ysrc[a-width-1];
		ysrc[a]+=median(i,j,k);

		i = usrc[a-halfWidth-1];
		j = usrc[a-halfWidth-halfWidth];
		k = i+j-usrc[a-halfWidth-halfWidth-1];
		usrc[a-width/2]+=median(i,j,k);

		i = vsrc[a-halfWidth-1];
		j = vsrc[a-halfWidth-halfWidth];
		k = i+j-vsrc[a-halfWidth-halfWidth-1];
		vsrc[a-halfWidth]+=median(i,j,k);
	}

	for ( ;a&7;a++){
		int x = ysrc[a-1];
		int y = ysrc[a-width];
		int z = x+y-ysrc[a-width-1];
		x = median(x,y,z);
		ysrc[a]+=x;
	}

	x = _mm_cvtsi32_si128(ysrc[a-1]);
	z = _mm_cvtsi32_si128(ysrc[a-width-1]);

	__m128i shift_mask = _mm_cvtsi32_si128(255);

	for ( ;a<((width*height)&(~7));a+=8){
		__m128i src = _mm_loadl_epi64((__m128i*)&ysrc[a]);
		__m128i y = _mm_loadl_epi64((__m128i*)&ysrc[a-width]);
		y = _mm_unpacklo_epi8(y,_mm_setzero_si128());
		src = _mm_unpacklo_epi8(src,_mm_setzero_si128());

		z = _mm_or_si128(z,_mm_slli_si128(y,2));

		z = _mm_sub_epi16(_mm_add_epi16(x,y),z);

		__m128i i = _mm_min_epi16(x,y);
		__m128i j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 1 correct
		// mask and shift j
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 2 correct
		// mask and shift j
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 3 correct
		// mask and shift j
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 4 correct
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 5 correct
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 6 correct
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_slli_si128(shift_mask,2);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		j = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		j = _mm_min_epi16(j,i);
		j = _mm_add_epi8(j,src); // 7 correct
		j = _mm_and_si128(j,shift_mask);
		j = _mm_slli_si128(j,2);
		shift_mask = _mm_srli_si128(shift_mask,12);
		x = _mm_or_si128(x,j);
		z = _mm_add_epi16(z,j);

		i = _mm_min_epi16(x,y);
		x = _mm_max_epi16(x,y);
		i = _mm_max_epi16(i,z);
		x = _mm_min_epi16(x,i);
		x = _mm_add_epi8(x,src); // 8 correct

		_mm_storel_epi64((__m128i*)&ysrc[a], _mm_packus_epi16(x,x));

		x = _mm_srli_si128(x,14);
		z = _mm_srli_si128(y,14);
	}

	_mm_empty();

	for ( ;a<width*height;a++)
	{
		int x = ysrc[a-1];
		int y = ysrc[a-width];
		int z = x+y-ysrc[a-width-1];
		x = median(x,y,z);
		ysrc[a]+=x;
	}
}