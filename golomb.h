#pragma once
#include "bit_manager.h"

// Code a value using unsigned exp-Golomb coding and output it
static void UnsignedGolombEncode(const unsigned int in, BitOutputManager& bitManger);
unsigned int GolombEncode(const unsigned int* in, unsigned char* out, unsigned int lengthh);

// Decode a value using unsigned exp-Golomb decoding and output it
static unsigned int UnsignedGolombDecode(BitInputManager& bitManger);
unsigned int GolombDecode(const unsigned char* in, unsigned int* out, unsigned int length);