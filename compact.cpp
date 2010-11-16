//	Lagarith v1.3.15, copyright 2008 by Ben Greenwood.
//	http://lags.leetcode.net/codec.html
//
//	This program is free software; you can redistribute it and/or
//	modify it under the terms of the GNU General Public License
//	as published by the Free Software Foundation; either version 2
//	of the License, or (at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef _COMPACT_CPP
#define _COMPACT_CPP

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include "yeti.h"
#include "fibonacci.h"
#include "zerorle.h"
#include "rle.h"

// scale the byte probabilities so the cumulative
// probability is equal to a power of 2
void CompressClass::ScaleBitProbability(unsigned int length)
{
   assert(length > 0);
   assert(length < 0x80000000);

   unsigned int temp = 1;
   while ( temp < length )
   {
      temp <<= 1;
   }

   double factor = temp / (double)length;
   unsigned int newlen=0;

   int a;
   for ( a = 1; a < 257; a++ )
   {
      m_pProbRanges[a]= (unsigned int)(m_pProbRanges[a]*factor);
      newlen+=m_pProbRanges[a];
   }

   newlen = temp-newlen;

   assert(newlen < 0x80000000);

   if((signed int)newlen < 0 )  // should never happen
   {
      m_pProbRanges[1] += newlen;
      newlen = 0;
#ifdef _DEBUG
      MessageBox (HWND_DESKTOP, "Newlen is less than 0", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }

   a=0;
   unsigned int b=0;
   while(newlen)
   {
      if ( m_pProbRanges[b+1] )
      {
         m_pProbRanges[b+1]++;
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

   for (a = 0; temp; a++)
   {
      temp>>=1;
   }

   m_scale = a - 1;
   for ( a = 1; a < 257; a++ )
   {
      m_pProbRanges[a] += m_pProbRanges[a-1];
   }
}

// read the byte frequency header
unsigned int CompressClass::ReadBitProbability(const unsigned char * in)
{
   try 
   {
      unsigned int length = 0;
      unsigned int skip;

      m_pProbRanges[0] = 0;

      skip = FibonacciDecode(in,&m_pProbRanges[1]);
      if ( !skip )
      {
         return 0;
      }

      for (unsigned int a = 1; a< 257; a++)
      {
         length+=m_pProbRanges[a];
      }

      if ( !length )
      {
         return 0;
      }

      ScaleBitProbability(length);

      return skip;
   } 
   catch (...)
   {
      return 0;
   }
}

// write the byte frequency header
#define WRITE_PROB( x ) (FibonacciEncode(m_pBytecounts, x, 256))


// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
void CompressClass::CalcBitProbability(const unsigned char * const in, const unsigned int length)
{
   m_pProbRanges[0] = 0;
   ZeroMemory(m_pBytecounts, 256 * sizeof(unsigned int));

   for (unsigned int a = 0; a < length; a++)
   {
      m_pBytecounts[in[a]]++;
   }

   memcpy(&m_pProbRanges[1], m_pBytecounts, 256 * sizeof(unsigned int));

   ScaleBitProbability(length);
}

// This function encapsulates compressing a byte stream.
// It applies a modified run length encoding if it saves enough bytes, 
// writes the frequency header, and range compresses the byte stream.
unsigned int CompressClass::Compact(const unsigned char * in, unsigned char * out, const unsigned int length)
{
   int min_size = length / 512;

   if (min_size < 16)
   {
      min_size = 16;
   }

   int bytes_used = 0;
   
   const unsigned char level = 2;
   unsigned int size = RLE2(in, m_pBuffer, length);
   out[0] = level;

   const unsigned char* source = m_pBuffer;

   if( size >= length ) // RLE size is greater than uncompressed size
   {
      source = in;
      size = length;
      out[0] = 0;
   }

   *(UINT32*)(out + 1) = size;
   CalcBitProbability(source, size);
   unsigned int skip = WRITE_PROB(out + 5);

   unsigned char tempc = out[4 + skip];
   unsigned int y = RangeEncode(source, out + 4 + skip, size);
   out[4 + skip] = tempc;
   skip += y + 5;

   if ( size < skip )  // RLE size is less than range compressed size
   {
      out[0] += 4;
      memcpy(out + 1, source, size);
      skip = size + 1;
   }

   bytes_used = skip;

   assert(bytes_used >= 2);
   assert(out[0] == level || out[0]== level + 4 || out[0] == 0 );

   return bytes_used;
}

// this function encapsulates decompressing a byte stream
void CompressClass::Uncompact(const unsigned char * in, unsigned char * out, const unsigned int length)
{
   try
   {
      char rle = in[0];
      if ( rle >= 0 && rle < 8 )
      {
         if ( rle < 4 )
         {
            unsigned int size = *(UINT32*)(in + 1);
            int skip;
            skip = ReadBitProbability(in+5);
            if( !skip )
            {
               return;
            }

            unsigned char* dest = rle ? m_pBuffer : out;
            RangeDecode(in + 4 + skip, dest, size);

            if(rle == 2)
            {
               deRLE2(m_pBuffer, out, length);
            }
            else if( rle == 3)
            {
               deRLE3(m_pBuffer, out, length);
            }
         } 
         else  // RLE length is less than range compressed length
         {
            rle -= 4;
            if(rle == 2)
            {
               deRLE2(in + 1, out, length);
            }
            else if( rle == 3)
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

// initialized the buffers used by RLE and range coding routines
bool CompressClass::InitCompressBuffers(const unsigned int length)
{
   m_pBuffer = (unsigned char *)aligned_malloc(m_pBuffer, length, 32, "Compress::temp");
   m_pProbRanges = (unsigned int *)aligned_malloc(m_pProbRanges, 260 * sizeof(unsigned int), 64, "Compress::ranges");
   m_pBytecounts = (unsigned int *)aligned_malloc(m_pBytecounts, 260 * sizeof(unsigned int), 64, "Compress::bytecounts");
   if (!( m_pBuffer && m_pProbRanges && m_pBytecounts ))
   {
      FreeCompressBuffers();
      return false;
   }
   return true;
}

// free the buffers used by RLE and range coding routines
void CompressClass::FreeCompressBuffers()
{	
   ALIGNED_FREE( m_pBuffer,"Compress::buffer");
   ALIGNED_FREE( m_pProbRanges, "Compress::prob_ranges");
   ALIGNED_FREE( m_pBytecounts, "Compress::bytecounts");
}

CompressClass::CompressClass()
{
   m_pBuffer = NULL;
   m_pProbRanges = NULL;
   m_pBytecounts = NULL;
}

CompressClass::~CompressClass()
{
   FreeCompressBuffers();
}

#endif