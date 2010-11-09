//This file contains functions that perform and undo median prediction.
#pragma once

void MMX_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int mode);
void SSE2_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int mode);

void ASM_BlockRestore(unsigned char * source, unsigned int stride,unsigned int xlength, unsigned int mode);

void reduce_res(const unsigned char * src, unsigned char * dest, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2);
void enlarge_res(const unsigned char * src, unsigned char * dst, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2);