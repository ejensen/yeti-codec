#include <string.h>
#include <mmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include "zerorle.h"

// this lookup table is used for encoding run lengths so that
// the run byte distribution should roughly match the data
// distribution, improving compression.
#pragma warning( push )
#pragma warning( disable : 4245 )
static const BYTE dist_match[]={ 0,0,0,-1,1,-2,2,-3,3,-4,4,-5,5,-6,6,-7,7,-8,8,-9,9,
   -10,10,-11,11,-12,12,-13,13,-14,14,-15,15,-16,16,-17,17,-18,18,-19,19,-20,20,
   -21,21,-22,22,-23,23,-24,24,-25,25,-26,26,-27,27,-28,28,-29,29,-30,30,-31,31,
   -32,32,-33,33,-34,34,-35,35,-36,36,-37,37,-38,38,-39,39,-40,40,-41,41,-42,42,
   -43,43,-44,44,-45,45,-46,46,-47,47,-48,48,-49,49,-50,50,-51,51,-52,52,-53,53,
   -54,54,-55,55,-56,56,-57,57,-58,58,-59,59,-60,60,-61,61,-62,62,-63,63,-64,64,
   -65,65,-66,66,-67,67,-68,68,-69,69,-70,70,-71,71,-72,72,-73,73,-74,74,-75,75,
   -76,76,-77,77,-78,78,-79,79,-80,80,-81,81,-82,82,-83,83,-84,84,-85,85,-86,86,
   -87,87,-88,88,-89,89,-90,90,-91,91,-92,92,-93,93,-94,94,-95,95,-96,96,-97,97,
   -98,98,-99,99,-100,100,-101,101,-102,102,-103,103,-104,104,-105,105,-106,106,
   -107,107,-108,108,-109,109,-110,110,-111,111,-112,112,-113,113,-114,114,-115,
   115,-116,116,-117,117,-118,118,-119,119,-120,120,-121,121,-122,122,-123,123,
   -124,124,-125,125,-126,126,-127,127,-128};
#pragma warning( pop )

extern const unsigned int* dist_rest;

#define dist_match_max 0x80

// this table is used in performing RLE; it is used to look up how
// many leading zeros or non-zeros were found in an eight-byte block
static const BYTE countlookup[] =  {8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,
   0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,
   1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,
   0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
   4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,
   0,1,0,2,0,1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,
   1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,8};

// This function encodes zero runs longer than 256 bytes;
// This is rare and takes a variable amount of bytes per level
void Encode_Long_Run( BYTE ** l1, BYTE ** l3, size_t count)
{
   BYTE* lvl1 = l1[0];
   BYTE* lvl3 = l3[0];
   size_t x = count;
   do {
      lvl1[0]=0;
      lvl1[1]=dist_match_max;
      x-=256;
      lvl1+=2;
   } while (x>256);

   lvl1[0]=0;
   lvl1[1]=dist_match[x+1];
   lvl1+=2;

   while (count>258)
   {
      lvl3[0]=0;
      lvl3[1]=0;
      lvl3[2]=0;
      lvl3[3]=dist_match_max;
      count-=258;
      lvl3+=4;
   }
   if ( count == 1)
   {
      lvl3[0]=0;
      lvl3++;
   } 
   else if ( count == 2 )
   {
      lvl3[0]=0;
      lvl3[1]=0;
      lvl3+=2;
   } 
   else
   {
      lvl3[0]=0;
      lvl3[1]=0;
      lvl3[2]=0;
      lvl3[3]=dist_match[count-1];
      lvl3+=4;
   }
   l1[0]=lvl1;
   l3[0]=lvl3;
}

/*
This function takes the input data and performs RLE on runs of zeros.
After performing the encoding, the function looks at the length of the
resulting byte sequences, and tries to select the RLE level that gives
the best compression & speed. The possible levels are 0 (no RLE
compression), 1 (each zero is followed by a byte indicating how many
additional zeros follow), and 3 (three sequential zero values are
followed by a byte indicating how many additional zeros follow).
There is also a special case for data in which every byte after the
first is zero, in this case the function will return -1, and only
the first byte needs to be saved.
*/

size_t TestAndRLE(BYTE * const __restrict in, BYTE * const __restrict out1, BYTE * const __restrict out3, const size_t length, char& level)
{
   BYTE * lvl1 = out1;
   BYTE * lvl3 = out3;

   unsigned int a=0;

   // end marker values to prevent overrunning
   in[length] = 255;
   in[length+1] = 0;
   in[length+2] = 255;

   const __m64 zero = _mm_setzero_si64();
   if ( in[0] == 0 )
   {
      goto SSE_RLE_Start_With_0;
   }
   // Perform RLE on the data using runs of length 1 and 3 using SSE
   while(a<length)
   {
      {
         unsigned int step;
         do {
            // copy non-zero bytes until a zero is found
            __m64 s = *(__m64*)&in[a];
            *(__m64*)lvl1 = s;
            *(__m64*)lvl3 = s;
            s = _mm_cmpeq_pi8(s,zero);
            step = _mm_movemask_pi8(s);
            step = countlookup[step]; // step now equals the number of sequential non-zeros
            lvl1+=step;
            lvl3+=step;
            a+=step;
         } while (step == 8);
      }
SSE_RLE_Start_With_0:
      unsigned int count=0;
      {
         unsigned int step;
         do {
            // count the number of zeros in the current run
            __m64 s = *(__m64*)&in[a];
            s = _mm_cmpeq_pi8(s,zero);
            step = _mm_movemask_pi8(s);
            step = countlookup[step+1]; // step now equals the number of sequential zeros
            a+=step;
            count+=step;
         } while ( step == 8);
      }

      // encode the run of zeros
      if ( count <= 256 )
      {
         lvl1[0]=0;
         lvl3[0]=0;
         lvl3[1]=0;
         lvl3[2]=0;
         lvl1[1]=dist_match[count+1];
         lvl3[3]=dist_match[count-1];
         lvl1+=2;
         lvl3+=(count>=3)?4:count;
      } 
      else 
      {
         Encode_Long_Run(&lvl1,&lvl3,count);
      }
   }
   _mm_empty();

   unsigned int len1 = (int)(lvl1-out1);
   unsigned int len3 = (int)(lvl3-out3);

   // trim off end markers if needed
   if ( a > length )
   {
      len1-=3;
      len3-=2;
   }

   // check if the data was one long run of zeros
   if ( len1 <= (length/256)*2 + 3)
   {
      a = (in[0]>0);
      if ( ((len1 - a)%2)==0 )
      {
         bool solid = true;
         for ( ;a<len1-2;a+=2)
         {
            if ( out1[a] || out1[a+1]!=128)
            {
               solid = false;
               break;
            }
         }
         if ( solid && out1[len1-2]==0 )
         {
            level = -1;
            return 2;
         }
      }
   }

   // Level selection method:
   // None typically gives the best compression if level 3 cannot reduce the size
   // by at least %1.5. Level 1 typically gives better compression if it is
   // significantly smaller than level 3 (98% of level 1) and level 3 is much smaller than
   // none (32% of none). For very simple data (level 1 < 10% of none), none typically
   // will compress a few bytes less than level 1, but the speed tradeoff isn't
   // worth it in my opinion.

   // 197/200 = 98.5%
   if ( length*197/200 <= len3 )
   {
      level = 0;
      return length;
   }

   if ( len1 < len3*98/100 && len3*100/length <= 32 )
   {
      level = 1;
      return len1;
   }

   level = 3;
   return len3;

}

// This undoes the modified Run Length Encoding on a byte stream.
// The 'level' parameter tells how many 0's must be read before it
// is considered a 'run', either 1, 2, or 3. Once 'level' zeros have
// been read in the byte stream, the following byte tells how many more
// 0 bytes to output. This routine is only used in the rare occurrence
// where RLE compression is better than header + range coding

size_t deRLE(const BYTE * in, BYTE * out, const size_t length, const BYTE level)
{
   unsigned int a = 0;
   unsigned int b = 0;
   memset(out, 0, length);
   if( level == 1 )
   {
      while ( b < length)
      {
         if ( in[a] ){
            out[b++] = in[a++];
         } 
         else
         {
            b += dist_rest[in[a+1]]+1;
            a += 2;
         }
      }
   } 
   else if ( level == 2 )
   {
      while ( b < length-1)
      {
         if ( in[a] ) {
            out[b++]=in[a++];
         } 
         else if ( in[a+1] )
         {
            out[b]=0;
            out[b+1]=in[a+1];
            b+=2;
            a+=2;
         } 
         else 
         {
            b+=dist_rest[in[a+2]]+2;
            a+=3;
         }
      }
      if (b < length )
      {
         out[b]=in[a];
      }
   } 
   else if (level==3)
   {
      while ( b < length-2)
      {
         if ( in[a] ) 
         {
            out[b++]=in[a++];
         } 
         else if ( in[a+1] )
         {
            out[b]=0;
            out[b+1]=in[a+1];
            b+=2;
            a+=2;
         }
         else if ( in[a+2] )
         {
            out[b]=0;
            out[b+1]=0;
            out[b+2]=in[a+2];
            b+=3;
            a+=3;
         } 
         else
         {
            b+=dist_rest[in[a+3]]+3;
            a+=4;
         }
      }
      if (b < length )
      {
         out[b]=in[a];
         if (b < length-1 )
         {
            out[b+1]=in[a+1];
         }
      }
   }
   return a;
}