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
#include <windows.h>
#include <math.h>
#include "yeti.h"
#include "fibonacci.h"
#include "zerorle.h"

// scale the byte probabilities so the cumulative
// probability is equal to a power of 2
void CompressClass::scaleprob(unsigned int length)
{
   assert(length > 0);
   assert(length < 0x80000000);

   unsigned int temp = 1;
   while ( temp < length )
   {
      temp<<=1;
   }

   double factor = temp/(double)length;
   unsigned int newlen=0;

   int a;
   for ( a = 1; a < 257; a++ )
   {
      m_pProbRanges[a]= (unsigned int)(m_pProbRanges[a]*factor);
      newlen+=m_pProbRanges[a];
   }

   newlen = temp-newlen;
   assert(newlen < 0x80000000);

   if ( (signed int)newlen < 0 )  // should never happen
   {
      m_pProbRanges[1]+=newlen;
      newlen=0;
#ifdef _DEBUG
      MessageBox (HWND_DESKTOP, "Newlen is less than 0", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
   }

   a =0;
   unsigned int b=0;
   while ( newlen  )
   {
      if ( m_pProbRanges[b+1] )
      {
         m_pProbRanges[b+1]++;
         newlen--;
      }
      //TODO: change?
      //if ( b&0x80==0 ){	// order of operations is 'wrong'; it has been left this way
      //					// since the compression change is negligible and fixing it
      //					// breaks backwards compatibility
      //	b=-(signed int)b;
      //	b&=0xFF;
      //} else{
      b++;
      b&=0x7f;
      //}

   }

   for ( a =0; temp; a++)
   {
      temp>>=1;
   }

   m_scale =a-1;
   for ( a = 1; a < 257; a++ )
   {
      m_pProbRanges[a]+=m_pProbRanges[a-1];
   }
}

// read the byte frequency header
unsigned int CompressClass::readprob(const unsigned char * in)
{
   try 
   {
      unsigned int length=0;
      unsigned int skip;

      m_pProbRanges[0]=0;

      skip = FibonacciDecode2(in,&m_pProbRanges[1]);
      if ( !skip )
      {
         return 0;
      }

      for (unsigned int a =1;a< 257; a++)
      {
         length+=m_pProbRanges[a];
      }

      if ( !length )
      {
         return 0;
      }

      scaleprob(length);

      return skip;
   } 
   catch (...)
   {
      return 0;
   }
}

// write the byte frequency header
#define WRITE_PROB( x ) ( FibonacciEncode2(m_pBytecounts,x,256))


// Determine the frequency of each byte in a byte stream; the frequencies are then scaled
// so the total is a power of 2. This allows binary shifts to be used instead of some
// multiply and divides in the compression/decompression routines
void CompressClass::calcprob(const unsigned char * const in, const unsigned int length)
{
   m_pProbRanges[0]=0;
   memset(m_pBytecounts,0,256*sizeof(unsigned int));
   unsigned int a;
   for (a=0; a < length; a++ )
   {
      m_pBytecounts[in[a]]++;
   }

   memcpy(&m_pProbRanges[1],m_pBytecounts,256*sizeof(unsigned int));

   scaleprob(length);
}

// This function encapsulates compressing a byte stream.
// It applies a modified run length encoding if it saves enough bytes, 
// writes the frequency header, and range compresses the byte stream.
unsigned int CompressClass::compact( const unsigned char * in, unsigned char * out, const unsigned int length)
{
   int min_size = length / 512;

   if ( min_size < 16 )
   {
      min_size = 16;
   }

   int bytes_used=0;

   //int rle = best_compresssion(in,out,(int*)bytecounts,length,1);
   int rle;
   rle = zero::testRLE(in,length,min_size);

   out[0] = rle;
   if ( rle )
   {
      if (rle == -1 )  // solid run of 0s, only 1st byte is needed
      {
         out[0]=0xFF;
         out[1]=in[0];
         bytes_used= 2;
      } 
      else 
      {
         int size = zero::RLE(in,m_pBuffer,length,rle);

         *(UINT32*)(out+1)=size;
         calcprob(m_pBuffer,size);
         int skip = WRITE_PROB(out+5);

         int tempc = out[4+skip];
         int y = encode(m_pBuffer,out+4+skip,size);
         out[4+skip]=tempc;
         skip+=y+5;
         if ( size < skip )  // RLE size is less than range compressed size
         {
            out[0]+=4;
            memcpy(out+1,m_pBuffer,size);
            skip=size+1;
         }
         bytes_used= skip;
      }
   } 
   else
   {
      calcprob(in, length);
      int skip = WRITE_PROB(out+1);

      int tempc = out[skip];
      int ret = encode(in, out+skip,length)+1+skip;
      out[skip]=tempc;
      bytes_used=ret;
   }
   assert(bytes_used>=2);
   assert(rle <=3 && rle >= -1);
   assert(out[0] == rle || rle == -1 || out[0]==rle+4 );

   return bytes_used;
}

// this function encapsulates decompressing a byte stream
void CompressClass::uncompact( const unsigned char * in, unsigned char * out, const unsigned int length)
{
   try
   {
      int rle = in[0];
      if ( rle && ( rle < 8 || rle == 0xff ) )  // any other values may indicate a corrupted RLE level from 1.3.0 or 1.3.1	
      {
         if ( rle < 4 ){
            unsigned int size = *( UINT32 *)(in+1);
            //if ( size >= length )  // should not happen, most likely a corrupted 1.3.x encoding
            //{
            //   int skip;
            //   skip = readprob(in+1);
            //   if ( !skip )
            //      return;
            //   decode(in+skip,out,length);
            //} 
            //else 
            //{	
               int skip;
               skip = readprob(in+5);
               if ( !skip )
                  return;
               decode(in+4+skip,m_pBuffer,size);
               zero::deRLE(m_pBuffer,out,length,in[0]);
            //}
         } 
         else
         {
            if ( rle == 0xff )
            { // solid run of 0s, only need to set 1 byte
               ZeroMemory(out, length);
               out[0]=in[1];
            } 
            else 
            { // RLE length is less than range compressed length
               rle-=4;
               if ( rle )
               {
                  zero::deRLE(in+1,out,length,rle);
               }
               else// uncompressed length is smallest...
               {
                  memcpy((void*)(in+1),out,length);
               }
            }
         } 
      } 
      else 
      {
#ifdef _DEBUG
         if ( in[0] )
         {
            char msg[128];
            sprintf_s(msg,128,"Error! in[0] = %d",in[0]);
            MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
            ZeroMemory(out, length);
            return;
         }
#endif

         int skip = *(int*)(in+1);
         // Adobe Premiere Pro tends to zero out the data when multi-threading is enabled...
         // This will help avoid errors
         if ( !skip )
         {
            return;
         }
         skip = readprob(in+1);
         if ( !skip )
         {
            return;
         }
         decode(in+skip,out,length);
      }
   } 
   catch(...){};
}

// initialized the buffers used by RLE and range coding routines
bool CompressClass::InitCompressBuffers(const unsigned int length)
{
   m_pBuffer = (unsigned char *)aligned_malloc(m_pBuffer,length,32,"Compress::temp");
   m_pProbRanges = (unsigned int *)aligned_malloc(m_pProbRanges,260*sizeof(unsigned int),64,"Compress::ranges");
   m_pBytecounts = (unsigned int *)aligned_malloc(m_pBytecounts,260*sizeof(unsigned int),64,"Compress::bytecounts");
   if ( !( m_pBuffer && m_pProbRanges && m_pBytecounts ) )
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
   m_pBuffer=NULL;
   m_pProbRanges=NULL;
   m_pBytecounts=NULL;
}

CompressClass::~CompressClass()
{
   FreeCompressBuffers();
}

#endif