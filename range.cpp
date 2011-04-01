#include "compact.h"
#include "yeti.h"

#define TOP_VALUE		0x80000000
#define BOTTOM_VALUE	0x00800000
#define SHIFT_BITS		23

const unsigned int dist_restore[]={0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,
   34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,
   86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,
   128,130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,160,162,164,
   166,168,170,172,174,176,178,180,182,184,186,188,190,192,194,196,198,200,202,
   204,206,208,210,212,214,216,218,220,222,224,226,228,230,232,234,236,238,240,
   242,244,246,248,250,252,254,255,253,251,249,247,245,243,241,239,237,235,233,
   231,229,227,225,223,221,219,217,215,213,211,209,207,205,203,201,199,197,195,
   193,191,189,187,185,183,181,179,177,175,173,171,169,167,165,163,161,159,157,
   155,153,151,149,147,145,143,141,139,137,135,133,131,129,127,125,123,121,119,
   117,115,113,111,109,107,105,103,101,99,97,95,93,91,89,87,85,83,81,79,77,75,73,
   71,69,67,65,63,61,59,57,55,53,51,49,47,45,43,41,39,37,35,33,31,29,27,25,23,21,
   19,17,15,13,11,9,7,5,3,1};

const unsigned int* dist_rest = &dist_restore[0];

// Compress a byte stream using range coding. The frequency table
// "prob_ranges" will previously have been set up by the calcProb function
size_t CompressClass::RangeEncode(const BYTE* in, BYTE* out, const size_t length)
{
   unsigned int low = 0;
   unsigned int range = TOP_VALUE;
   unsigned int help = 0;
   BYTE* const count = out;
   const BYTE* const ending = in + length;

   while(in < ending)
   {
      unsigned int r = range >> m_scale;
      __assume(r<=0x40000000);
      __assume(r>=1);

      unsigned int tmp = r * m_probRanges[*in];
      low += tmp;

      range = (*in < 255) ? (r * (m_probRanges[*in+1] - m_probRanges[*in])) : (range - tmp);

      in++;

      while(range <= BOTTOM_VALUE) 
      {
         unsigned int val = (low & TOP_VALUE)>0;
         unsigned int shifted = low>>SHIFT_BITS;
         range <<= 8;
         low <<= 8;
         low &= (TOP_VALUE-1);

         if ( shifted == 255 )
         {
            help++;
         } 
         else 
         {
            out[-1]+=val;
            for ( ; help != 0 ; help-- )
            {
               *out = val-1;
               out++;
            }
            *out = shifted;
            out++;
         }
      }
   }

   // flush the encoder
   if ( (low >> 23) > 255 )
   {
      out[-1]++;
      while ( help-- )
         *out++=0;
   } 
   else 
   {
      while ( help-- )
         *out++=255;
   }

   out[0] = low>>23;
   out[1] = low>>15;
   out[2] = low>>7;
   out[3] = low<<1;
   out += 4;

   return (size_t)(out - count);
}

#define IN_BYTE() ((unsigned int)(*in++))

// Decompress a byte stream that has had range coding applied to it.
// The frequency table "prob_ranges" will have previously been set up using
// the Readprob function. RLE is also undone if needed.
void CompressClass::Decode_And_DeRLE( const BYTE * __restrict in, BYTE * __restrict out, const size_t length, size_t level)
{
   __assume(length>=1);
   unsigned int buffer = in[0];
   unsigned int low = buffer >> 1;
   buffer = ((in[0]&1)<<7) + (in[1]>>1);
   in++;
   unsigned int range = 0x80;
   const BYTE * ending = out+length;
   const unsigned int range_top = m_probRanges[255];
   const unsigned int range_bottom = m_probRanges[1];
   const unsigned int * const __restrict p_ranges = m_probRanges;
   const unsigned int shift = m_scale;
   BYTE range_hash[1<<12];

   // When determining the probability range that a value falls into,
   // it is faster to use the top bits of the value as an index into
   // a table which tells where to begin a linear search, instead of
   // using a binary search with many conditional jumps. For the 
   // majority of the values, the indexed value will be the desired
   // value, so the linear search will terminate with only one
   // well-predictable conditional.
   unsigned int hs=0;
   unsigned int hash_range = range_top-range_bottom;
   while ( hash_range > ((unsigned int)1<<(12 + hs)) ){
      hs++;
   }
   const unsigned int hash_shift = hs;
   {
      unsigned int last = 0;
      for ( unsigned int prev=1;prev<255;prev++){
         if ( p_ranges[prev+1] > p_ranges[prev]){
            unsigned int r = p_ranges[prev+1] - range_bottom;
            unsigned int curr = (r>>hash_shift) + ((r&((1<<hash_shift)-1))!=0);
            unsigned int size = curr-last;
            __assume(size >= 1);
            __assume(size <= (1<<12));
            memset(range_hash+last,prev,size);
            last = curr;
         }
      }
   }

   /*unsigned int prev =0;
   unsigned int pos=0;
   for ( unsigned int foo =0; prev<255; foo += (1<<hash_shift)){
   while (p_ranges[prev+1]<=foo)
   prev++;
   range_hash[pos++]= prev;
   }*/

   low <<= 8;
   range <<= 8;
   low += buffer;
   buffer = ((in[0]&1)<<7) + (in[1]>>1);
   in++;
   low <<= 8;
   range <<= 8;
   low += buffer;
   buffer = ((in[0]&1)<<7) + (in[1]>>1);
   in++;
   low <<= 8;
   range <<= 8;
   low += buffer;
   buffer = ((in[0]&1)<<7) + (in[1]>>1);
   in++;

   if ( level == 0 )
   {
      // handle range coding with no RLE
      do {
         while ( range <= BOTTOM_VALUE){
            low <<= 8;
            range <<= 8;
            low += buffer;
            buffer = ((in[0]&1)<<7) + (in[1]>>1);
            in++;
         }
         unsigned int help = range >> shift;
         __assume(help <= 512);
         __assume(help >= 1);
         if ( low < range_bottom*help )
         {
            range = help * range_bottom;
            *out++=0;
         } 
         else 
         {
            if ( low < range_top*help )
            {
               unsigned int tmp = low/help;

               // 'hash' the value and use that to determine
               // where to start the linear search
               unsigned int x=range_hash[(tmp-range_bottom)>>hash_shift];

               // use a linear search to find the decoded value
               while( p_ranges[x+1] <= tmp)
                  x++;

               range = help * (p_ranges[x+1]-p_ranges[x]);
               low -= help * p_ranges[x];
               *out++=x;

            } 
            else 
            { // if tmp >= p_ranges[255], then x has to equal 255 
               range -= help * range_top;
               low -= help * range_top;
               *out++=255;
            }
         }
      } 
      while ( out < ending );
   } 
   else 
   {
      // handle range coding and RLE
      ZeroMemory(out, length);
      unsigned int run=0;
      do {
         while ( range <= BOTTOM_VALUE)
         {
            low <<= 8;
            range <<= 8;
            low += buffer;
            buffer = ((in[0]&1)<<7) + (in[1]>>1);
            in++;
         }
         unsigned int help = range >> shift;
         __assume(help <= 512);
         __assume(help >= 1);
         if ( low < help*range_bottom )
         { // value is 0
            range = help * range_bottom;
            if ( run < level){
               out++;
               run++;
            } else {
               run=0;
            }
         } 
         else
         {
            if ( low < range_top*help )
            {
               unsigned int tmp = low/help;

               // 'hash' the value and use that to determine
               // where to start the linear search
               unsigned int x=range_hash[(tmp-range_bottom)>>hash_shift];

               // use a linear search to find the decoded value
               while( p_ranges[x+1] <= tmp)
                  x++;

               range = help * (p_ranges[x+1]-p_ranges[x]);
               low -= help * p_ranges[x];
               if ( run!=level ){
                  *out++=x;
               } else {
                  out += dist_restore[x];
               }
               run = 0;
            } else { // if tmp >= p_ranges[255], then x has to equal 255 
               range -= help * range_top;
               low -= help * range_top;
               if ( run < level ){
                  *out++=255;
               } else {
                  *out++=0;
               }
               run = 0;
            }
         }
      } while ( out < ending );
   }
}