#pragma once

unsigned int RLE2(const BYTE* in, BYTE* out, const unsigned int length);
unsigned int RLE3(const BYTE* in, BYTE* out, const unsigned int length);

unsigned int deRLE2(const BYTE* in, BYTE* out, const unsigned int length);
unsigned int deRLE3(const BYTE* in, BYTE* out, const unsigned int length);