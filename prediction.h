//This file contains functions that perform and undo median prediction.
#pragma once

void MMX_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length);
void SSE2_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length);

void ASM_BlockRestore(unsigned char * source, unsigned int stride,unsigned int xlength, unsigned int mode);