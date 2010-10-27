#pragma once

void mmx_ConvertRGB32toYUY2(const unsigned int *src,unsigned int *dst,int src_pitch, int dst_pitch,int w, int h);
void mmx_ConvertRGB24toYUY2(const unsigned char *src,unsigned char *dst,int src_pitch, int dst_pitch,int w, int h);