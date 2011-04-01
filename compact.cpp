#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "yeti.h"
#include "compact.h"
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

   const double factor = temp / (double)length;
   double temp_array[256];

   int a;
   for ( a = 0; a < 256; a++ )
   {
      temp_array[a] = (int)(((int)m_probRanges[a+1])*factor);
   }
   for ( a = 0; a < 256; a++ )
   {
      m_probRanges[a+1] = (int)temp_array[a];
   }
   unsigned int newlen=0;
   for ( a = 1; a < 257; a++ )
   {
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
unsigned int CompressClass::ReadBitProbability(const BYTE* in)
{
   try 
   {
      unsigned int length = 0;
      //m_probRanges[0] = 0;
      ZeroMemory(m_probRanges, sizeof(m_probRanges));

      const unsigned int skip = GolombDecode(in, &m_probRanges[1], 256);

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
   }
}

// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
unsigned int CompressClass::CalcBitProbability(const BYTE* const in, const unsigned int length, BYTE* out)
{
   unsigned int table2[256];
   ZeroMemory(m_probRanges, 257 * sizeof(unsigned int));
   ZeroMemory(table2, sizeof(table2));

   unsigned int a=0;
   for (a = 0; a < (length&(~1)); a+=2)
   {
      m_probRanges[in[a]+1]++;
      table2[in[a+1]]++;
   }
   if ( a < length )
   {
      m_probRanges[in[a]+1]++;
   }
   for (a = 0; a < 256; a++){
      m_probRanges[a+1] += table2[a];
   }

   unsigned int size = 0;
   if ( out != NULL )
   {
      size = GolombEncode(&m_probRanges[1], out, 256);
   }

   ScaleBitProbability(length);

   return size;
}

unsigned int CompressClass::Compact(BYTE* in, BYTE* out, const unsigned int length)
{
   unsigned int bytes_used=0;

   BYTE* const buffer_1 = m_buffer;
   BYTE* const buffer_2 = m_buffer + ALIGN_ROUND(length * 3/2 + 16, 16);
   int rle = 0;
   unsigned int size = TestAndRLE(in, buffer_1, buffer_2, length, &rle);

   out[0] = rle;
   if ( rle )
   {
      if (rle == -1 )
      { // solid run of 0s, only 1st byte is needed
         out[0] = 0xFF;
         out[1] = in[0];
         bytes_used = 2;
      } 
      else 
      {
         BYTE* b2 = ( rle == 1 ) ? buffer_1 : buffer_2;

         *(UINT32*)(out+1)=size;
         unsigned int skip = CalcBitProbability(b2, size, out + 5);

         skip += RangeEncode(b2, out + 5 + skip, size) + 5;

         if ( size < skip ) { // RLE size is less than range compressed size
            out[0]+=4;
            memcpy(out+1,b2,size);
            skip=size+1;
         }
         bytes_used = skip;
      }
   } 
   else 
   {
      unsigned int skip = CalcBitProbability(in, length, out + 1);
      skip += RangeEncode(in,out+skip+1,length) +1;
      bytes_used=skip;
   }

   assert(bytes_used >= 2);
   assert(rle <=3 && rle >= -1);
   assert(out[0] == rle || rle == -1 || out[0]== rle + 4 );

   return bytes_used;
}

void CompressClass::Uncompact(const BYTE* in, BYTE* out, const unsigned int length)
{
   try
   {
      char rle = in[0];
      if(rle && ( rle < 8 || rle == 0xff ))
      {
         if(rle < 4)
         {
            unsigned int skip = ReadBitProbability(in + 5);

            if(!skip)
            {
               return;
            }

            Decode_And_DeRLE(in + 4 + skip + 1, out, length, in[0]);
         } 
         else  // RLE length is less than range compressed length
         {
            if ( rle == 0xff )
            { // solid run of 0s, only need to set 1 byte
               ZeroMemory(out, length);
               out[0] = in[1];
            } 
            else 
            { // RLE length is less than range compressed length
               rle-=4;
               if ( rle )
                  deRLE(in+1, out, length, rle);
               else // uncompressed length is smallest...
                  memcpy((void*)(in+1), out, length);
            }
         } 
      }
   }
   catch(...)
   {
   }
}

bool CompressClass::InitCompressBuffers(const unsigned int length)
{
   m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, length * 3/2 + length * 5/4 + 32, 8, "Compress::buffer");

   if (!m_buffer)
   {
      FreeCompressBuffers();
      return false;
   }
   return true;
}

void CompressClass::FreeCompressBuffers()
{	
   ALIGNED_FREE( m_buffer,"Compress::buffer");
}

CompressClass::CompressClass()
{
   m_buffer = NULL;
}

CompressClass::~CompressClass()
{
   FreeCompressBuffers();
}