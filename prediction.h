#pragma once

void SSE2_BlockPredict( const unsigned char* source, unsigned char* dest, const unsigned int stride, const unsigned int length);
void MMX_BlockPredict( const unsigned char* source, unsigned char* dest, const unsigned int stride, const unsigned int length);

void SSE2_Predict_YUY2(const unsigned char* source, unsigned char* dest, const unsigned int width, const unsigned height, int lum);
void MMX_Predict_YUY2(const unsigned char* source,unsigned char* dest, const unsigned int width, const unsigned height, int lum);

void ASM_BlockRestore(unsigned char* source, unsigned int stride,unsigned int xlength, unsigned int mode);