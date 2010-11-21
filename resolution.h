#pragma once

void ReduceRes(const unsigned char* src, unsigned char* dest, unsigned char* buffer, unsigned int width, unsigned int height, const bool SSE2);
void EnlargeRes(const unsigned char* src, unsigned char* dst, unsigned char* buffer, unsigned int width, unsigned int height, const bool SSE2);