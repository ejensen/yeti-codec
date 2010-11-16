#ifndef _RANGE_CPP
#define _RANGE_CPP
#include <memory.h>

#include "compact.h"
#include "yeti.h"

#define TOP_VALUE		0x80000000
#define BOTTOM_VALUE	0x00800000
#define SHIFT_BITS		23

#define OUTPUT_BYTE( x ) *out++ = x;

// Compress a byte stream using range coding. The frequency table
// "prob_ranges" will previously have been set up by the calcProb function
unsigned int CompressClass::RangeEncode(const unsigned char * in, unsigned char * out, const unsigned int length)
{
   unsigned int low = 0;
   unsigned int range = TOP_VALUE;
   unsigned int buffer = 0;
   unsigned int help = 0;
   unsigned char * const count = out;
   const unsigned char * const ending = in + length;
   const unsigned int * const p_ranges = m_pProbRanges;

   while(in < ending)
   {
      // encode symbol
      unsigned int r = range >> m_scale;
      unsigned int tmp = r * p_ranges[*in];
      low += tmp;

      if( *in < 255 ) 
      {
         range = r * (p_ranges[*in+1] - p_ranges[*in]);
      }
      else 
      {
         range -= tmp;	
      }

      in++;

      while (range <= BOTTOM_VALUE) 
      {
         if((low >> SHIFT_BITS) < 255 )
         {
            OUTPUT_BYTE ( buffer );
            buffer = low >> SHIFT_BITS;
            for ( ; help != 0 ; help-- )
            {
               OUTPUT_BYTE ( 255 );
            }
         } 
         else 
         { 
            if(low & TOP_VALUE)
            {
               OUTPUT_BYTE( buffer + 1 );
               buffer = low >> SHIFT_BITS;
               for ( ; help; help--)
               {
                  OUTPUT_BYTE (0);
               }
            }
            else 
            {
               help++;
            }
         }
         range <<= 8;
         low <<= 8;
         low &= (TOP_VALUE-1);
      }
   }
   // flush the encoder
   if ( low >> SHIFT_BITS > 0xFF )
   {
      OUTPUT_BYTE( buffer+1 );
      while ( help-- )
      {
         *out++ = 0;
      }
   }
   else
   {
      OUTPUT_BYTE ( buffer );
      while ( help-- )
      {
         *out++ = 255;
      }
   }

   OUTPUT_BYTE( low >> 23 );
   OUTPUT_BYTE( low >> 15 );
   OUTPUT_BYTE( low >> 7 );
   OUTPUT_BYTE( low << 1 );

   return (unsigned int)(out-count);
}

#define IN_BYTE() ((unsigned int)(*in++))

// Decompress a byte stream that has had range coding applied to it.
// The frequency table "prob_ranges" will have previously been set up using
// the readProb function.
void CompressClass::RangeDecode( const unsigned char * in, unsigned char * out, const unsigned int length)
{
   in++;	// 1st byte is garbage
   unsigned int buffer = IN_BYTE();
   unsigned int low = buffer >> 1;
   buffer &= 1;
   unsigned int range = 0x80;
   const unsigned char * ending = out + length;
   const unsigned int range_top = m_pProbRanges[255];
   const unsigned int * const p_ranges = m_pProbRanges;
   const unsigned int shift = m_scale;
   unsigned int r_hash[256];

   // When determining the probability range that a value falls into,
   // it is faster to use the top 8 bits of the value as an index into
   // a table which tells where to begin a linear search, instead of
   // using a binary search with many more conditional jumps. For the 
   // majority of the values, the indexed value will be the desired
   // value, so the linear search will terminate with only 1 conditional.
   const unsigned int hash_shift = shift>8?(shift-8) : 0; 
   unsigned int prev = 0;
   for (unsigned int foo = 0; foo < 256; foo++)
   {
      unsigned int r = foo << hash_shift;
      for(; p_ranges[prev+1]<=r; prev++);
      r_hash[foo]= prev;
   }

   const unsigned int * const range_hash = r_hash;

   do 
   {
      while( range <= BOTTOM_VALUE )
      {
         low += low;	
         low |= buffer;
         low <<= 7;
         buffer = IN_BYTE();
         low |= ( buffer >> 1 );
         range <<= 8;
         buffer&=1;
      }
      unsigned int help = range >> shift;
      unsigned int tmp = low/help;
      if ( tmp < range_top )
      {
         // 'hash' the value and use that to determine
         // where to start the linear search
         unsigned int x = range_hash[tmp >> hash_shift];
         // use a linear search to find the decoded value
         for (; p_ranges[x+1] <= tmp; x++);

         *out++ = x; // output the decoded value

         low -= help * p_ranges[x];
         range = help * (p_ranges[x+1]-p_ranges[x]);

      } 
      else  // if tmp >= p_ranges[255], then x has to equal 255 
      {
         *out++ = 255;
         tmp = help * range_top;
         low -= tmp;
         range -= tmp;
      }
   } 
   while ( out < ending );
}
#endif