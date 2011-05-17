#pragma once
#include "common.h"

size_t deRLE(const BYTE* in, BYTE* out, const size_t length, const BYTE level);
size_t TestAndRLE(BYTE* const __restrict in, BYTE* const __restrict out1, BYTE* const __restrict out3, const size_t length,  short& level);