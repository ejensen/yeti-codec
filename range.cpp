#include "compact.h"
#include "yeti.h"
#include "reciprocal_table.h"
#include <intrin.h>

#define TOP_VALUE		0x80000000
#define BOTTOM_VALUE	0x00800000
#define SHIFT_BITS	23

const unsigned int dist_restore[]={0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,
	34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,
	86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,
	128,130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,160,162,164,
	166,168,170,172,174,176,178,180,182,184,186,188,190,192,194,196,198,200,202,
	204,206,208,210,212,214,216,218,220,222,224,226,228,230,232,234,236,238,240,
	242,244,246,248,250,252,254,255,253,251,249,247,245,243,241,239,237,235,233,
	231,229,227,225,223,221,219,217,215,213,211,209,207,205,203,201,199,197,195,
	193,191,189,187,185,183,181,179,177,175,173,171,169,167,165,163,161,159,157,
	155,153,151,149,147,145,143,141,139,137,135,133,131,129,127,125,123,121,119,
	117,115,113,111,109,107,105,103,101,99,97,95,93,91,89,87,85,83,81,79,77,75,73,
	71,69,67,65,63,61,59,57,55,53,51,49,47,45,43,41,39,37,35,33,31,29,27,25,23,21,
	19,17,15,13,11,9,7,5,3,1};

const unsigned int* dist_rest = &dist_restore[0];

// Compress a byte stream using range coding. The frequency table
// "prob_ranges" will previously have been set up by the calcProb function
size_t CompressClass::RangeEncode(const BYTE* in, BYTE* out, const size_t length)
{
	unsigned int low = 0;
	unsigned int range = TOP_VALUE;
	BYTE* const count = out;
	const BYTE* const ending = in + length;

	int range_table[256][2];
	for(int a=0;a<255;a++)
	{
		range_table[a][0] = m_probRanges[a];
		range_table[a][1] = m_probRanges[a+1] - m_probRanges[a];
	}
	range_table[255][0]= m_probRanges[255];
	range_table[255][1]= 0 - m_probRanges[255];

	unsigned int mask255[256];
	ZeroMemory(&mask255[0], 255*sizeof(mask255[0]));
	mask255[255] = 0xffffffff;

	do{
		int in_val = *in;
		++in;
		int r = range >> m_scale;

		// Set range to zero if the value is not 255
		range &= mask255[in_val];

		low += r * range_table[in_val][0];
		// For 255, range_table[255][1] is negative which results in
		// range being set to range-help*prob_ranges[255]. Other values
		// set range to help*(prob_ranges[in_val+1]-prob_ranges[in_val]).
		// This avoids a conditional that is mis-predicted about 5-25%
		// of the time for typical video.
		range += r * range_table[in_val][1];

		// If range has become too small, output 1-3 bytes
		if( range <= BOTTOM_VALUE )
		{
			// This is the overflow case - range got too small before the top bits
			// of low stabilized. The carry needs to be propagated back thought the
			// bytes that have already been written. The initial values of low and 
			// range seem to prevent this from happening before any values have been
			// written; at least I have not been able to cause it with intentionally
			// crafted video.
			if ( low & TOP_VALUE )
			{
				int pos=-1;
				while (out[pos]==255)
				{
					out[pos]=0;
					--pos;
				}
				++out[pos];
			}
			do {
				range <<= 8;
				*out = low>>SHIFT_BITS;
				low <<= 8;
				low &= (TOP_VALUE-1);
				++out;
			} while (range <= BOTTOM_VALUE);
		}
	} while ( in < ending );

	// flush the encoder
	if(low & TOP_VALUE)
	{
		int pos = -1;
		while(out[pos] == 255)
		{
			out[pos] = 0;
			--pos;
		}
		out[pos]++;
	} 

	out[0] = low>>23;
	out[1] = low>>15;
	out[2] = low>>7;
	out[3] = low<<1;
	out += 4;

	return (size_t)(out - count);
}

// This will loop at most 3 times (when range is < 0x81)
#define CheckForRead()\
while ( range <= BOTTOM_VALUE){\
	low <<= 8;\
	range <<= 8;\
	low += buffer;\
	buffer = ((in[0]&1)<<7) + (in[1]>>1);\
	++in;\
	assert(range>0);\
}

#define DecodeNonZero(isrun) \
if ( low < range_top*help ){\
	int shifter=hash_shift;\
	int rb = range_bottom;\
	unsigned int s = help;\
	while ( s >= 2048){\
		s+=3;\
		s>>=2;\
		rb+=rb;\
		rb+=rb;\
		shifter+=2;\
	}\
	int tmp = (int)(__emulu(low,reciprocal_table[s])>>32);\
	tmp -= rb;\
	if ( tmp < 0 )\
		tmp = 0;\
	tmp >>= shifter;\
	unsigned int x=range_hash[tmp];\
	while( (range = indexed_ranges[x+1][0]*help) <= low)\
		x=indexed_ranges[x+1][1];\
	range -= help * indexed_ranges[x][0];\
	low -= help * indexed_ranges[x][0];\
	if ( !(isrun) ){\
		*out++=x;\
	} else { \
		out+=dist_restore[x];\
	}\
} else {\
	range -= help * range_top;\
	low -= help * range_top;\
	if ( !(isrun) ){\
		*out++=255;\
	} else { \
		out+=dist_restore[255];\
	}\
}

#define IN_BYTE() ((unsigned int)(*in++))

/*
Decompress a byte stream that has had range coding applied to it.
The frequency table "prob_ranges" will have previously been set up using
the Readprob function. RLE is also undone if needed.
The simplified algorithm for the range decoder (without RLE decoding) is this:
	while ( out < ending ){
		CheckForRead();
		unsigned int help = range >> shift;
		unsigned int tmp = low/help;
		unsigned int x;
		for ( x=0;x<255 && prob_ranges[x+1]<=tmp;x++){};
		out[0]=x;
		out++;
		if (x < 255 ){
			range = (prob_ranges[x+1]-prob_ranges[x])*help;
		} else {
			range -= prob_ranges[x]*help;
		}
		low -= prob_ranges[x]*help;
	}

The no-RLE case below offers a fairly simple example of the optimized algorithm.
For the RLE cases, the output is initially set to zero, and then the output
pointer is advanced by the appropriate value when ever a run is detected in the
decoder.
*/
void CompressClass::Decode_And_DeRLE(const BYTE * __restrict in, BYTE * __restrict out, const size_t length, size_t level)
{
	__assume(length>=2);
	unsigned int low = (in[0]<<23)+(in[1]<<15)+(in[2]<<7)+(in[3]>>1);
	unsigned int buffer = ((in[3]&1)<<7)+(in[4]>>1);
	in += 4;
	unsigned int range = TOP_VALUE;
	const BYTE* ending = out + length;
	const unsigned int range_top = m_probRanges[255];
	const unsigned int range_bottom = m_probRanges[1];
	const unsigned int shift = m_scale;

	// The 2nd entry in indexed_ranges tells where the next 
	// prob_range > current is located. This allows the linear
	// search to skip unused entries. In testing, this reduced
	// the number of linear search lookups by 90%.
	unsigned int indexed_ranges[256][2];
	{
		int a=0;
		int next=0;
		for ( ;a<256;a++)
		{
			indexed_ranges[a][0] = m_probRanges[a];
			if ( next <= a)
			{
				++next;
				for ( ;m_probRanges[next] == m_probRanges[a]; next++){};
				--next;
			}
			indexed_ranges[a][1]=next;
		}
	}

	// When determining the probability range that a value falls into,
	// it is faster to use the top bits of the value as an index into
	// a table which tells where to begin a linear search, instead of
	// using a binary search with many conditional jumps. For the 
	// majority of the values, the indexed value will be the desired
	// value, so the linear search will terminate with only one
	// well-predictable conditional.
	BYTE range_hash[1<<12];
	unsigned int hs=0;
	unsigned int hash_range = range_top-range_bottom;
	while ( hash_range > ((unsigned int)1<<(12 + hs)) )
	{
		++hs;
	}

	const unsigned int hash_shift = hs;
	{
		unsigned int last = 0;
		for ( unsigned int prev=1;prev<255;prev++)
		{
			if ( indexed_ranges[prev+1][0] > indexed_ranges[prev][0])
			{
				unsigned int r = indexed_ranges[prev+1][0] - range_bottom;
				unsigned int curr = (r>>hash_shift) + ((r&((1<<hash_shift)-1))!=0);
				unsigned int size = curr-last;
				__assume(size >= 1);
				__assume(size <= (1<<12));
				memset(range_hash+last, prev, size);
				last = curr;
			}
		}
	}

	// The overhead for setting up range_hash and indexed_ranges is minimal,
	// the break-even point is roughly 1000-2000 decoded symbols (or about 
	// 300 bytes of typical compressed data). This is rare enough and fast
	// enough that it is not worth trying to optimize for the case where
	// decoding without lookup tables is faster.

	// Initialize out to zero so the decoder loop just updates the out pointer when 
	// zero runs are detected.
	if(level)
	{
		ZeroMemory(out, length);
	}

	if ( level == 0 )
	{
		// This is the simplest version of the optimized range decoder to understand,
		// since it does not have the RLE decoder rolled in.
		do {
			unsigned int help = range >> shift;
			if ( low < help*range_bottom )
			{ // Decode a zero value, most common case
				range = help * range_bottom;
				*out++=0;
			} 
			else
			 {
				if ( low < range_top*help ){ // The value is > 0 and < 255
					// Perform a reciprocal multiply to get tmp <= low/help.
					// The value must be <= for the linear search to work.
					int shifter=hash_shift;
					int rb = range_bottom;
					unsigned int s = help;
					// If the value is too large for the r_table,
					// do roughly (low/(help>>x))>>x, where x is how many
					// places help got shifted to make it fall in the range.
					while ( s >= 2048)
					{
						s+=3;// Round up divisor so result will be <= low/help.
						s>>=2; // Shifting 2 places gave best performance in testing.
						rb+=rb; // Shift up range_bottom so hash_shift can be merged with
						rb+=rb; // the reciprocal correction shift.
						shifter+=2;
					}
					// Perform a 64 bit reciprocal multiply, the top 32 bits are the desired result
					int tmp = (int)(__emulu(low,reciprocal_table[s])>>32);
					tmp -= rb;

					// rounding errors will sometimes cause tmp to be < range_bottom,
					// this is rare enough that a conditional is faster than masking.
					if ( tmp < 0 )
						tmp = 0;
					// Use the hash table to find where to start the linear search.
					tmp >>= shifter;
					unsigned int x=range_hash[tmp];

					// The linear search will correct for the error in the hash table
					// and for reciprocal multiply results less than low/help.
					while( (range = indexed_ranges[x+1][0]*help) <= low)
						x=indexed_ranges[x+1][1];

					// Update range and low based on decoded value; range has been
					// set to indexed_ranges[x+1][0]*help in the linear search.
					range -= help * indexed_ranges[x][0];
					low -= help * indexed_ranges[x][0];
					*out++=x;
				} 
				else
				{
					// A value of 255 is handled here due to range being updated differently and
					// because low can be greater than prob_ranges[256]*help in certain cases.
					range -= help * range_top;
					low -= help * range_top;
					*out++=255;
				}
			}
			// If range has become too small, read in more bytes
			CheckForRead();
		} while ( out < ending );
	} 
	else  if ( level == 3 )
	{
		unsigned int help = range >> shift;
		if ( low < help*range_bottom )
		{
			goto Level_3_Zero_Decode;
		} 
		else 
		{
			goto Level_3_Nonzero_Decode;
		}
		do 
		{
Level_3_Zero_Decode:
			range = help * range_bottom;
			++out;
			CheckForRead();
			help = range >> shift;
			if ( low < help*range_bottom )
			{
				range = help * range_bottom;
				++out;
				CheckForRead();
				help = range >> shift;
				if (low < help*range_bottom )
				{
					range = help * range_bottom;
					++out;
					CheckForRead();
					help = range >> shift;

					// a zero run was reached
					if ( low < help*range_bottom )
					{
						range = help * range_bottom;
					} 
					else
					{
						DecodeNonZero(true);
					}
					// see if there is another zero run following
					CheckForRead();
					help = range >> shift;
					if ( low < help*range_bottom && out < ending){
						goto Level_3_Zero_Decode;
					}
				}
			}
Level_3_Nonzero_Decode:
			if ( out >= ending )
			{
				break;
			}
			DecodeNonZero(false);
			CheckForRead();
			help = range >> shift;
			if ( low < help*range_bottom )
			{
				goto Level_3_Zero_Decode;
			}
			goto Level_3_Nonzero_Decode;
		} while ( true );
	} 
	else if ( level == 1 )
	{
		unsigned int help = range >> shift;
		if ( low < help*range_bottom )
		{
			goto Level_1_Zero_Decode;
		} 
		else 
		{
			goto Level_1_Nonzero_Decode;
		}
		do {
Level_1_Zero_Decode:
			range = help * range_bottom;
			++out;
			CheckForRead();
			help = range >> shift;
			// a zero run was reached
			if ( low < help*range_bottom )
			{
				range = help * range_bottom;
			} 
			else 
			{
				DecodeNonZero(true);
			}

			// see if there is another zero run following
			CheckForRead();
			help = range >> shift;
			if ( out < ending )
			{
				if ( low < help*range_bottom )
				{
					goto Level_1_Zero_Decode;
				}
			} 
			else 
			{
				break;
			}
Level_1_Nonzero_Decode:
			DecodeNonZero(false);
			CheckForRead();
			help = range >> shift;
			if ( out < ending )
			{
				if ( low < help*range_bottom )
				{
					goto Level_1_Zero_Decode;
				}
				goto Level_1_Nonzero_Decode;
			} 
			else 
			{
				break;
			}
		} while ( true );
	} 
	else if ( level == 2 )
	{ 
		// less optimized since it is not used in recent builds
		unsigned int run=0;
		do {
			CheckForRead();
			unsigned int help = range >> shift;
			if ( low < help*range_bottom )
			{
				range = help * range_bottom;
				if ( run < 2)
				{
					++out;
					++run;
				} 
				else 
				{
					run=0;
				}
			} 
			else
			 {
				DecodeNonZero( run==2 );
				run=0;
			}
		} while ( out < ending );
	}
}