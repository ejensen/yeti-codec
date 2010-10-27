#pragma once

namespace zero
{
   int testRLE(const unsigned char * in, const int length, const int minReduction );
   int RLE(const unsigned char * in, unsigned char * out, const unsigned int length, const int level);
   int deRLE(const unsigned char * in, unsigned char * out, const int length, const int level);
};