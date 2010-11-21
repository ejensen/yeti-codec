#include "bit_manager.h"

BitOutputManager::BitOutputManager(unsigned char* out_data)
{
   m_num_out_bytes = 0;
   m_current_byte = 0;
   m_output_mask = 0x80;
   m_out = out_data;
}

size_t BitOutputManager::Size()
{
   return m_num_out_bytes;
}

void BitOutputManager::OutputBit(const bool& bit)
{
   m_current_byte |= (bit ? (m_output_mask) : 0);

   // Shift mask to next bit in the output byte
   m_output_mask >>= 1; 

   if(m_output_mask == 0)
   { 
      // If a whole byte has been written, write out
      m_output_mask = 0x80;
      *(m_out++) = m_current_byte;
      m_current_byte = 0;
      ++m_num_out_bytes;
   }
}

void BitOutputManager::Flush()
{
   if(m_output_mask != 0x80)
   {
      *(m_out++) = m_current_byte;
      ++m_num_out_bytes;
   }
}

BitInputManager::BitInputManager(const unsigned char* in_data)
{
   m_num_bytes_read = 0;
   m_input_bits_left = 0;
   m_in = in_data;
}

bool BitInputManager::InputBit()
{
   if (m_input_bits_left == 0)
   {
      m_current_byte = *(m_in++);
      m_input_bits_left = 8;
      ++m_num_bytes_read;
   }

   m_input_bits_left--;

   return bool((m_current_byte >> m_input_bits_left) & 1);
}

size_t BitInputManager::ByteOffset()
{
   return m_num_bytes_read;
}