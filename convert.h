#pragma once
void ConvertRGB24toYV16_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h);
void ConvertRGB32toYV16_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h);
void ConvertRGB32toYV12_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h);
void ConvertRGB24toYV12_SSE2(const unsigned char *src, unsigned char *ydst, unsigned char *udst, unsigned char *vdst, unsigned int w, unsigned int h);