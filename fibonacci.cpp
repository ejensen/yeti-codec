#ifndef _FIBONACCI_CPP
#define _FIBONACCI_CPP

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <windows.h>
#include <assert.h>

// this file is used to compress the header without needing a header for the header...
// this is achieved due to the fact that the byte frequencies will be concentrated around
// 0 and fall off rapidly, resulting in the majority of the byte frequencies being single
// digit numbers or zero. Fibonacci coding is a variable bit method for encoding integers, with
// large numbers taking more bits than small numbers. Here, 0 takes two bits to encode,
// while a number like 200,000 takes about 28 bits. Additionally, if a frequency of zero is
// encountered, the number of zero frequencies that follows are encoded and the encode position
// moved accordingly. This is pretty much the same as the zero RLE done on the byte streams 
// themselves, except the run is encoded using Fibonacci coding.

#define writeBit(x) { \
	if ( x) \
	out[pos]|=bitpos;\
	bitpos>>=1;\
	if ( !bitpos){ \
	bitpos=0x80;\
	pos++;\
	out[pos]=0;\
	}\
}

unsigned int FibonacciEncode2(unsigned int * in, unsigned char * out, unsigned int length){
	unsigned char output[12];
	unsigned int pos=0;
	unsigned int bitpos=0x80;
	out[0]=0;
	unsigned int series[]={1,2,3,5,8,13,21,34};
	for ( unsigned int b =0; b < length; b++ ){
		memset(output,0,12);
		unsigned int num = in[b]+1; // value needs to be >= 1

		// calculate Fibonacci part
		unsigned int a;
		unsigned int bits =32;
		for( ; !(num & 0x80000000) && bits; bits-- )
			num<<=1;
		num<<=1;
		assert(bits);
		unsigned int bits2=bits;

		while(bits){
			for ( a =0; series[a]<= bits; a++ );
			a--;
			output[a]=1;
			bits-=series[a];
		}
		for ( a =7; !output[a]; a-- );
		a++;
		output[a]=1;
		for ( unsigned int x = 0; x <= a; x++ )
			writeBit( output[x] );
		bits2--;

		//write numerical part
		for (unsigned int place = 0x80000000 ; bits2; bits2--){
			writeBit( num&place);
			place>>=1;
		}
		if ( in[b]==0 ){
			unsigned int i;
			for ( i=1;i+b<length&&in[b+i]==0;i++);
			b+=i-1;
			memset(output,0,12);

			// calculate Fibonacci part
			bits =32;
			num=i;
			for( ; !(num & 0x80000000) && bits; bits-- )
				num<<=1;

			assert(bits > 0 );

			num<<=1;
			bits2=bits;
			while(bits){
				for ( a =0; series[a]<= bits; a++ );
				a--;
				output[a]=1;
				bits-=series[a];
			}
			for ( a =7; !output[a]; a-- );
			a++;
			output[a]=1;
			for ( unsigned int x = 0; x <= a; x++ )
				writeBit( output[x] );
			bits2--;

			//write numerical part
			for (unsigned int place = 0x80000000 ; bits2; bits2--){
				writeBit( num&place);
				place>>=1;
			}
		}
	}
	return pos+(bitpos!=0x80);
}

#define readBit() { \
	bit = bitpos & in[pos];\
	bitpos >>=1;\
	if ( !bitpos ){\
	pos++;\
	bitpos = 0x80;\
	}\
}

unsigned int FibonacciDecode(const unsigned char * in, int * out, int length){
	int pos =0;
	int bit;
	int bitpos = 0x80;
	int series[]={1,2,3,5,8,13,21,34};
	char input[32];
	memset(input,0,32);

	try {
		for ( int b =0; b < length; b++ ){
			bit=0;
			unsigned int prevbit=0;
			unsigned int bits=0;
			memset(input,0,32);
			unsigned int a;
			for ( a =0; !(prevbit && bit); a++ ){
				prevbit = bit;
				readBit();
				input[a]=bit;
				if ( bit && !prevbit )
					bits+=series[a];
			}
			bits--;
			int value=1;
			for ( a= 0;a < bits; a++ ){
				readBit();
				value<<=1;
				if ( bit )
					value++;
			}
			out[b]= value-1;
		}
	} catch (...){
		return 0;
	}
	return pos+1;
}

int FibonacciDecode2(const unsigned char * in, unsigned int * out){
	try{
		unsigned int pos =0;
		unsigned int bit;
		unsigned int bitpos = 0x80;
		unsigned int series[]={1,2,3,5,8,13,21,34};

		for ( unsigned int b =0; b < 256; b++ ){
			bit =0;
			unsigned int prevbit=0;
			unsigned int bits=0;
			unsigned int a=0;
			for ( ; !(prevbit && bit ); a++ ){

				prevbit = bit;
				readBit();
				if ( bit && !prevbit )
					bits+=series[a];
			}
			bits--;
			unsigned int value=1;
			for ( a= 0;a < bits; a++ ){
				readBit();
				value<<=1;
				if ( bit )
					value++;
			}
			out[b]= value-1;
			if ( out[b]==0){

				bit =0;
				prevbit=0;
				bits=0;
				for ( a =0; !(prevbit && bit); a++ ){
					prevbit = bit;
					readBit();

					if ( bit && !prevbit )
						bits+=series[a];
				}
				bits--;
				value=1;

				for ( a= 0;a < bits; a++ ){
					readBit();
					value<<=1;
					if ( bit )
						value++;
				}
				if ( b+value >= 257 )
					value = 256-b;
				for ( unsigned int x =0;x<value-1;x++){
					b++;
					out[b]=0;
				}

			}
		}
		return pos+(bitpos!=0x80);
	} catch (...){
		return 0;
	}
}
#endif