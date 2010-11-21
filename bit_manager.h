#pragma once

class BitOutputManager
{
public:
   BitOutputManager(unsigned char* out_data);
   void OutputBit(const bool& bit);
   void Flush();
   size_t Size();
private:
   size_t m_num_out_bytes;
   unsigned char* m_out;
   unsigned char m_current_byte;
   unsigned int m_output_mask;
};

class BitInputManager
{
public:
   BitInputManager(const unsigned char* in_data);
   bool InputBit();
   size_t ByteOffset();
private:
   size_t m_num_bytes_read;
   const unsigned char* m_in;
   unsigned char m_current_byte;
   unsigned int m_input_bits_left;
};