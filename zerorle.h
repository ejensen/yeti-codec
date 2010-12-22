#pragma once

unsigned int RLE2(const unsigned char* in, unsigned char* out, const unsigned int length);
unsigned int RLE3(const unsigned char* in, unsigned char* out, const unsigned int length);

unsigned int deRLE2(const unsigned char* in, unsigned char* out, const unsigned int length);
unsigned int deRLE3(const unsigned char* in, unsigned char* out, const unsigned int length);