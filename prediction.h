#pragma once

void SSE2_BlockPredict( const BYTE* source, BYTE* dest, const unsigned int stride, const unsigned int length);
void MMX_BlockPredict( const BYTE* source, BYTE* dest, const unsigned int stride, const unsigned int length);

void SSE2_Predict_YUY2(const BYTE* source, BYTE* dest, const unsigned int width, const unsigned height, const unsigned int lum);
void MMX_Predict_YUY2(const BYTE* source,BYTE* dest, const unsigned int width, const unsigned height, const unsigned int lum);

void ASM_BlockRestore(BYTE* source, unsigned int stride,unsigned int xlength, unsigned int mode);