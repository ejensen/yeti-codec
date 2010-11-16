#pragma once

void Reduce_Res(const unsigned char * src, unsigned char * dest, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2);
void Enlarge_Res(const unsigned char * src, unsigned char * dst, unsigned char * buffer, unsigned int width, unsigned int height, const bool SSE2);