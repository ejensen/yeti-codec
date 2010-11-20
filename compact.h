#pragma once

#ifndef _YETI_COMPRESSION 
#define _YETI_COMPRESSION 

class CompressClass 
{
public:
	unsigned int * m_probRanges;		// Byte probability range table
	unsigned int m_scale;				// Used to replace some multiply/divides with binary shifts,
	// (1<<scale) is equal to the cumulative probability of all bytes
	unsigned int * m_bytecounts;		// Byte frequency table
	unsigned char * m_buffer;			// buffer to perform RLE

	CompressClass();
	~CompressClass();

	bool InitCompressBuffers(const unsigned int length);
	void FreeCompressBuffers();

	unsigned int Compact( const unsigned char * in, unsigned char * out, const unsigned int length);
	void Uncompact( const unsigned char * in, unsigned char * out, const unsigned int length);
	void CalcBitProbability(const unsigned char * in, const unsigned int length);
	void ScaleBitProbability(const unsigned int length);
	unsigned int ReadBitProbability(const unsigned char * in);
	unsigned int __fastcall RangeEncode( const unsigned char * in, unsigned char * out, const unsigned int length);
	void __fastcall RangeDecode(const unsigned char *in, unsigned char *out, const unsigned int length);
};

#endif