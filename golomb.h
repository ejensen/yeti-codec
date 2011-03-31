#pragma once

#include "common.h"
#include "bit_manager.h"

static void UnsignedGolombEncode(const unsigned int in, BitOutputManager& bitManger);
size_t GolombEncode(const unsigned int* in, BYTE* out, size_t length);

static size_t UnsignedGolombDecode(BitInputManager& bitManger);
size_t GolombDecode(const BYTE* in, unsigned int* out, size_t length);