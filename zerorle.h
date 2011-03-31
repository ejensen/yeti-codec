#pragma once

int deRLE(const BYTE * in, BYTE * out, const int length, const int level);
unsigned int TestAndRLE(BYTE * const __restrict in, BYTE * const __restrict out1, BYTE * const __restrict out3, const unsigned int length, int * level);