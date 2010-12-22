#pragma once

#include "common.h"
#include "bit_manager.h"

static void UnsignedGolombEncode(const unsigned int in, BitOutputManager& bitManger);
unsigned int GolombEncode(const unsigned int* in, BYTE* out, unsigned int length);

static unsigned int UnsignedGolombDecode(BitInputManager& bitManger);
unsigned int GolombDecode(const BYTE* in, unsigned int* out, unsigned int length);