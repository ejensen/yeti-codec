#pragma once
#include "common.h"

class BitOutputManager
{
public:
   BitOutputManager(BYTE* out_data);
   void OutputBit(const bool& bit);
   void Flush();
   size_t Size();
private:
   size_t m_num_out_bytes;
   BYTE* m_out;
   BYTE m_current_byte;
   unsigned int m_output_mask;
};

class BitInputManager
{
public:
   BitInputManager(const BYTE* in_data);
   bool InputBit();
   size_t ByteOffset();
private:
   size_t m_num_bytes_read;
   const BYTE* m_in;
   BYTE m_current_byte;
   unsigned int m_input_bits_left;
};