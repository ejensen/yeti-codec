#pragma once

#include "bit_manager.h"

static void UnsignedGolombEncode(const unsigned int in, BitOutputManager& bitManger);
unsigned int GolombEncode(const unsigned int* in, unsigned char* out, unsigned int length);

static unsigned int UnsignedGolombDecode(BitInputManager& bitManger);
unsigned int GolombDecode(const unsigned char* in, unsigned int* out, unsigned int length);