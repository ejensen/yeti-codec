#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <mmintrin.h>
#include <emmintrin.h>
#include <omp.h>
#include "yeti.h"
#include "compact.h"
#include "zerorle.h"
#include "golomb.h"

void InterframeDecode(BYTE* __restrict dest, const BYTE* __restrict src1, const BYTE* __restrict src2, const size_t len) 
{
	__m128i* mxSrc1 = (__m128i*) src1;
	__m128i* mxSrc2 = (__m128i*) src2;
	__m128i* mxDest = (__m128i*) dest;
	const int end = len / 16;

	#pragma omp parallel for
	for(int i = 0; i < end; i++)
	{
		mxDest[i] = _mm_add_epi8(mxSrc1[i], mxSrc2[i]);
	}
}

bool InterframeEncode(BYTE* __restrict dest, const BYTE* __restrict src1, const BYTE* __restrict src2, const size_t len, const unsigned __int64 minDelta)
{
	unsigned __int64 sad = 0;

	__m128i* mxDest = (__m128i*) dest;
	__m128i* mxSrc1 = (__m128i*) src1;
	__m128i* mxSrc2 = (__m128i*) src2;

	const int end = len / 16;

#pragma omp parallel for reduction(+: sad)
	for(int i = 0; i < end; i++)
	{
		__m128i mx1 = _mm_load_si128(mxSrc1 + i);
		__m128i mx2 = _mm_load_si128(mxSrc2 + i);
		__m128i mxs = _mm_sad_epu8(mx1,  mx2);
		sad += _mm_extract_epi16(mxs, 0) + _mm_extract_epi16(mxs, 4);
		mxDest[i] = _mm_sub_epi8(mx1, mx2);
	}

	return sad < minDelta;
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

	if ( temp != length )
	{
		const double factor = (int)temp / ((double)(int)length);
		unsigned int newlen = 0;

		for(int i = 1; i < 257; i++)
		{
			m_probRanges[i] = (unsigned int)(((int)m_probRanges[i]) * factor);
			newlen += m_probRanges[i];
		}
	
		newlen = temp - newlen;

		assert(newlen < 0x80000000);

		unsigned int b = 0;
		while(newlen)
		{
			if(m_probRanges[b+1])
			{
				m_probRanges[b+1]++;
				--newlen;
			}
			
			++b;
			b &= 0x7f;
		}
	}

	unsigned int a = 0;
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
	size_t length = 0;
	ZeroMemory(m_probRanges, sizeof(m_probRanges));

	const unsigned int skip = GolombDecode(in, &m_probRanges[1], 256);

	assert(skip);

	for(unsigned int a = 1; a < 257; a++)
	{
		length += m_probRanges[a];
	}

	assert(length);

	ScaleBitProbability(length);

	return skip;
}

// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
size_t CompressClass::CalcBitProbability(const BYTE* const in, size_t length, BYTE* out)
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

	for (a = 0; a < 256; a++)
	{
		m_probRanges[a+1] += table2[a];
	}

	// Clamp prob_ranges total to 1<<19 to ensure the range coder has enough precision to work with;
	// Larger totals reduce compression due to the reduced precision of the range variable, and
	// totals larger than 1<<22 can crash the range coder. Lower clamp values reduce the decoder speed
	// slightly on affected video.
	const int clamp_size = 1<<19;
	if ( out && length > clamp_size )
	{
		double factor = clamp_size;
		factor /= length;
		double temp[256];
		for (int a = 0; a < 256; a++)
		{
			temp[a] = ((int)m_probRanges[a+1]) * factor;
		}

		unsigned int total=0;
		for (int a = 0; a < 256; a++)
		{
			int scaled_val = (int)temp[a];
			scaled_val += (scaled_val == 0 && m_probRanges[a+1]);
			total += scaled_val;
			m_probRanges[a+1] = scaled_val;
		}

		int adjust = total < clamp_size? 1 : -1;
		int a = 0;
		while (total != clamp_size)
		{
			if (m_probRanges[a+1] > 1)
			{
				m_probRanges[a+1] += adjust;
				total += adjust;
			}
			++a;
			a&=255;
		}
		length = clamp_size;
	}

	size_t size = GolombEncode(&m_probRanges[1], out, 256);

	ScaleBitProbability(length);

	return size;
}

size_t CompressClass::Compact(BYTE* in, BYTE* out, const size_t length)
{
	size_t bytes_used = 0;

	BYTE* const buffer_1 = m_buffer;
	BYTE* const buffer_2 = m_buffer + ALIGN_ROUND(length + HALF(length) + 16, 16);
	short rle = 0;
	size_t size = TestAndRLE(in, buffer_1, buffer_2, length, rle);

	out[0] = rle;
	if ( rle )
	{
		if(rle == -1)
		{ // solid run of 0s, only 1st byte is needed
			out[0] = 0xFF;
			out[1] = in[0];
			bytes_used = 2;
		} 
		else 
		{
			BYTE* b2 = ( rle == 1 ) ? buffer_1 : buffer_2;

			*(UINT32*)(out+1)=size;
			size_t skip = CalcBitProbability(b2, size, out + 5);

			skip += RangeEncode(b2, out + 5 + skip, size) + 5;

			if ( size < skip )  // RLE size is less than range compressed size
			{
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
		skip += RangeEncode(in, out + skip + 1, length) +1;
		bytes_used=skip;
	}

	assert(bytes_used >= 2);
	assert(rle <=3 && rle >= -1);
	assert(out[0] == rle || rle == -1 || out[0]== rle + 4 );

	return bytes_used;
}

void CompressClass::Uncompact(const BYTE* in, BYTE* out, const size_t length)
{
#ifdef _DEBUG
	try
	{
#endif
		BYTE rle = in[0];
		if(rle && (rle < 8 || rle == 0xFF))
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
			else
			{
				if(rle == 0xFF)  // solid run of 0s, only need to set 1 byte
				{
					//MessageBox (HWND_DESKTOP, "ZerosOut", "Info", MB_OK);
					ZeroMemory(out, length);
					out[0] = in[1];
				}
				else 
				{
					rle -= 4;
					if (rle)
						deRLE(in+1, out, length, rle);
					else // uncompressed length is smallest...
						memcpy((void*)(in+1), out, length);
				}
			}
		}
		else
		{
			assert(*(int*)(in+1));
			size_t skip = ReadBitProbability(in + 1);
			assert(skip);
			Decode_And_DeRLE(in + skip + 1, out, length, 0);
		}
#ifdef _DEBUG
	}
	catch(...)
	{
		MessageBox(HWND_DESKTOP, "Uncompact Failed", "Error", MB_OK | MB_ICONEXCLAMATION);
	}
#endif
}

bool CompressClass::InitCompressBuffers(const size_t length)
{
	m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, length + HALF(length) + length * 5/4 + 32, 8, "Compress::buffer");

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