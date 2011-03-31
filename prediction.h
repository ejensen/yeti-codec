#pragma once

void SSE2_Block_Predict( const BYTE * __restrict source, BYTE * __restrict dest, const unsigned int width, const unsigned int length);
void Block_Predict( const BYTE * __restrict source, BYTE * __restrict dest, const unsigned int width, const unsigned int length);
void SSE2_Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const unsigned int width, const unsigned int length, const bool is_y);
void Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const unsigned int width, const unsigned int length, const bool is_y);
void SSE_Block_Predict(const BYTE * __restrict source, BYTE * __restrict dest, const unsigned int width, const unsigned int length);

void Split_YUY2(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const unsigned int width, const unsigned int height);
void Split_UYVY(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const unsigned int width, const unsigned int height);
void Interleave_And_Restore_YUY2( BYTE * __restrict out, BYTE * __restrict ysrc, const BYTE * __restrict usrc, const BYTE * __restrict vsrc, const unsigned int width, const unsigned int height);
void Restore_YV12( BYTE * __restrict ysrc, BYTE * __restrict usrc, BYTE * __restrict vsrc, const unsigned int width, const unsigned int height);