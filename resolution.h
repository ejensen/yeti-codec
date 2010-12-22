#pragma once

void ReduceRes(const BYTE* src, BYTE* dest, BYTE* buffer, const unsigned int width, const unsigned int height, const bool SSE2);
void EnlargeRes(const BYTE* src, BYTE* dst, BYTE* buffer, unsigned int width, unsigned int height, const bool SSE2);