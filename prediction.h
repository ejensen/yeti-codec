#pragma once
#include "common.h"

void Block_Predict( const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length);
void Block_Predict_YUV16( const BYTE * __restrict source, BYTE * __restrict dest, const size_t width, const size_t length, const bool is_y);

void Split_YUY2(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const size_t width, const size_t height);
void Split_UYVY(const BYTE * __restrict src, BYTE * __restrict ydst, BYTE * __restrict udst, BYTE * __restrict vdst, const size_t width, const size_t height);
void Interleave_And_Restore_YUY2( BYTE * __restrict out, BYTE * __restrict ysrc, const BYTE * __restrict usrc, const BYTE * __restrict vsrc, const size_t width, const size_t height);
void Restore_YV12( BYTE * __restrict ysrc, BYTE * __restrict usrc, BYTE * __restrict vsrc, const size_t width, const size_t height);