#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "yeti.h"
#include "zerorle.h"
#include "golomb.h"

// scale the byte probabilities so the cumulative
// probability is equal to a power of 2
void CompressClass::ScaleBitProbability(unsigned int length)
{
   assert(length > 0);
   assert(length < 0x80000000);

   unsigned int temp = 1;
   while(temp < length)
   {
      temp <<= 1;
   }

   double factor = temp / (double)length;
   unsigned int newlen = 0;

   int a;
   for(a = 1; a < 257; a++)
   {
      m_probRanges[a] = (unsigned int)(m_probRanges[a]*factor);
      newlen += m_probRanges[a];
   }

   newlen = temp - newlen;

   assert(newlen < 0x80000000);

   a= 0;
   unsigned int b = 0;
   while(newlen)
   {
      if(m_probRanges[b+1])
      {
         m_probRanges[b+1]++;
         newlen--;
      }

      if((b & 0x80) == 0)
      {
      	b =- (signed int) b;
      	b &= 0xFF;
      } 
      else
      {
         b++;
         b &= 0x7f;
      }
   }

   for(a = 0; temp; a++)
   {
      temp >>= 1;
   }

   m_scale = a - 1;
   for(a = 1; a < 257; a++)
   {
      m_probRanges[a] += m_probRanges[a-1];
   }
}

// read the byte frequency header
unsigned int CompressClass::ReadBitProbability(const unsigned char * in)
{
   try 
   {
      unsigned int length = 0;
      m_probRanges[0] = 0;

      unsigned int skip = GolombDecode(in, &m_probRanges[1], 256);

      if(!skip)
      {
         return 0;
      }

      for(unsigned int a = 1; a < 257; a++)
      {
         length += m_probRanges[a];
      }

      if(!length)
      {
         return 0;
      }

      ScaleBitProbability(length);

      return skip;
   } 
   catch(...)
   {
      return 0;
   }
}

// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
void CompressClass::CalcBitProbability(const unsigned char * const in, const unsigned int length)
{
   m_probRanges[0] = 0;
   ZeroMemory(m_bytecounts, 256 * sizeof(unsigned int));

   for(unsigned int a = 0; a < length; a++)
   {
      m_bytecounts[in[a]]++;
   }

   memcpy(&m_probRanges[1], m_bytecounts, 256 * sizeof(unsigned int));

   ScaleBitProbability(length);
}

unsigned int CompressClass::Compact(const unsigned char * in, unsigned char * out, const unsigned int length)
{
   int bytes_used = 0;
   
   const unsigned char level = 2;
   unsigned int size = RLE2(in, m_buffer, length);
   out[0] = level;

   const unsigned char* source = m_buffer;

   if(size >= length) // RLE size is greater than uncompressed size
   {
      source = in;
      size = length;
      out[0] = 0;
   }

   *(UINT32*)(out + 1) = size;
   CalcBitProbability(source, size);

   unsigned int skip = GolombEncode(m_bytecounts, out + 5, 256);

   unsigned char tempc = out[4 + skip];
   unsigned int y = RangeEncode(source, out + 4 + skip, size);
   out[4 + skip] = tempc;
   skip += y + 5;

   if(size < skip)  // RLE size is less than range compressed size
   {
      //MessageBox(HWND_DESKTOP, "RLE Smaller", "Info", MB_OK);
      out[0] += 4;
      memcpy(out + 1, source, size);
      skip = size + 1;
   }

   bytes_used = skip;

   assert(bytes_used >= 2);
   assert(out[0] == level || out[0]== level + 4 || out[0] == 0);

   return bytes_used;
}

void CompressClass::Uncompact(const unsigned char * in, unsigned char * out, const unsigned int length)
{
   try
   {
      char rle = in[0];
      if(rle >= 0 && rle < 8)
      {
         if(rle < 4)
         {
            unsigned int size = *(UINT32*)(in + 1);
            unsigned int skip = ReadBitProbability(in + 5);
            
            if(!skip)
            {
               return;
            }

            unsigned char* dest = rle ? m_buffer : out;
            RangeDecode(in + 4 + skip, dest, size);

            if(rle == 2)
            {
               deRLE2(m_buffer, out, length);
            }
            else if( rle == 3)
            {
               deRLE3(m_buffer, out, length);
            }
         } 
         else  // RLE length is less than range compressed length
         {
            rle -= 4;
            if(rle == 2)
            {
               deRLE2(in + 1, out, length);
            }
            else if(rle == 3)
            {
               deRLE3(in + 1, out, length);
            }
         } 
      }
#ifdef _DEBUG
      else   // any other values may indicate a corrupted RLE level
      {
         char msg[128];
         sprintf_s(msg,128,"Error! in[0] = %d",in[0]);
         MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
         ZeroMemory(out, length);
         return;
      }
#endif
   } 
   catch(...){};
}

bool CompressClass::InitCompressBuffers(const unsigned int length)
{
   m_buffer = (unsigned char *)aligned_malloc(m_buffer, length, 32, "Compress::temp");
   m_probRanges = (unsigned int *)aligned_malloc(m_probRanges, 260 * sizeof(unsigned int), 64, "Compress::ranges");
   m_bytecounts = (unsigned int *)aligned_malloc(m_bytecounts, 260 * sizeof(unsigned int), 64, "Compress::bytecounts");
   if (!( m_buffer && m_probRanges && m_bytecounts))
   {
      FreeCompressBuffers();
      return false;
   }
   return true;
}

void CompressClass::FreeCompressBuffers()
{	
   ALIGNED_FREE( m_buffer,"Compress::buffer");
   ALIGNED_FREE( m_probRanges, "Compress::prob_ranges");
   ALIGNED_FREE( m_bytecounts, "Compress::bytecounts");
}

CompressClass::CompressClass()
{
   m_buffer = NULL;
   m_probRanges = NULL;
   m_bytecounts = NULL;
}

CompressClass::~CompressClass()
{
   FreeCompressBuffers();
}