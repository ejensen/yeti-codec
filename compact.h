#pragma once

#ifndef _YETI_COMPRESSION 
#define _YETI_COMPRESSION 

class CompressClass 
{
public:
	unsigned int * _pProbRanges;		// Byte probability range table
	unsigned int _scale;				// Used to replace some multiply/divides with binary shifts,
	// (1<<scale) is equal to the cumulative probability of all bytes
	unsigned int * _pBytecounts;		// Byte frequency table
	unsigned char * _pBuffer;			// buffer to perform RLE


	CompressClass();
	~CompressClass();

	bool InitCompressBuffers(const unsigned int length);
	void FreeCompressBuffers();

	unsigned int compact( const unsigned char * in, unsigned char * out, const unsigned int length, const bool yuy2_lum=false);
	void uncompact( const unsigned char * in, unsigned char * out, const unsigned int length);
	void calcprob(const unsigned char * in, const unsigned int length);
	void calcprob2(const unsigned char * in, const unsigned int length);
	void scaleprob(const unsigned int length);
	unsigned int readprob(const unsigned char * in);
	void readprob2(const unsigned char * in, const unsigned int length);
	unsigned int encode( const unsigned char * in, unsigned char * out, const unsigned int length);
	void decode(const unsigned char *in, unsigned char *out, const unsigned int length);
};

#endif