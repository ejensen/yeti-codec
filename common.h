#pragma once

typedef unsigned char BYTE;

#define SWAP(x, y) { BYTE xchng = *(BYTE*)(&x); x = y; *(BYTE*)(&y) = xchng; }

// y must be 2^n
#define ALIGN_ROUND(x, y) ((((unsigned int)(x)) + (y - 1))&(~(y - 1)))

#define HALF(x) (x>>1)
#define FOURTH(x) (x>>2)
#define EIGHTH(x) (x>>3)

#define DOUBLE(x) (x<<1)
#define QUADRUPLE(x) (x<<2)

extern bool SSE;
extern bool	SSE2;