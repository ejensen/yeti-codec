#ifndef _ZERO_RLE
#define _ZERO_RLE

#include "yeti.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <math.h>

#include "zerorle.h"

static const char dist_match[]={ 0,0,-1,1,-2,2,-3,3,-4,4,-5,5,-6,6,-7,7,-8,8,-9,9,
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

static const unsigned char dist_restore[]={0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,
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

#define distribution_match(x) (dist_match[x])
#define distribution_restore(x) (dist_restore[x])

namespace zero {

   // This performs a modified Run Length Encoding on a byte stream.
   // Only runs of 0 values are encoded, all other values are ignored.
   // The 'level' parameter tells how many 0's must be read before it
   // is considered a 'run', either 1, 2, or 3. Once 'level' zeros have
   // been read in the byte stream, the number of zeros following are counted.
   // This number is then modified so that the run value distribution matches 
   // the byte distribution for the image data, and this number then put into
   // the out byte stream.
   int RLE(const unsigned char * in, unsigned char * out, const unsigned int length, const int level)
   {
      unsigned int a=0;
      int b =0;
      // these loops are spit into 2 parts, one which handles the bulk of the encoding, and
      // one that handles the last couple bytes and thus needs to do bounds checking

      if (level == 1 )
      {
         if ( length > 300 )
         {
            while (a < length-300)
            {
               if ( in[a] )
               {
                  out[b++]=in[a++];
               } 
               else
               {
                  int d=1;
                  for ( ; !in[a+d] && d <=255; d++);
                  a+=d;
                  out[b]=0;
                  out[b+1]=distribution_match(d);
                  b+=2;
               }
            }
         }
         while (a < length)
         {
            if ( in[a] )
            {
               out[b++]=in[a++];
            } 
            else 
            {
               int d=1;
               for ( ;a+d < length && !in[a+d] && d <=255; d++);
               a+=d;
               out[b]=0;
               out[b+1]=distribution_match(d);
               b+=2;
            }
         }
      } else if( level == 2 )
      {
         if ( length > 300 )
         {
            while ( a < length-300)
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
               else 
               {
                  int d=2;
                  for ( ; !in[a+d] && d<257; d++);
                  a+=d;
                  d--;
                  out[b]=0;
                  out[b+1]=0;
                  out[b+2]=distribution_match(d);
                  b+=3;
               }
            }
         }
         while ( a < length)
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
            else 
            {
               int d=2;
               for ( ;a+d < length && !in[a+d]&& d < 257; d++);
               a+=d;
               d--;
               out[b]=0;
               out[b+1]=0;
               out[b+2]=distribution_match(d);
               b+=3;
            }
         }
      } 
      else if ( level == 3 )
      {
         if ( length > 300)
         {
            while ( a < length-300)
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
                  int d=3;
                  for ( ; !in[a+d]&& d < 258; d++);
                  a+=d;
                  d-=2;
                  out[b]=0;
                  out[b+1]=0;
                  out[b+2]=0;
                  out[b+3]=distribution_match(d);
                  b+=4;
               }
            }
         }
         while ( a < length)
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
               int d=3;
               for ( ;  a+d < length && !in[a+d]&& d < 258; d++);
               a+=d;
               d-=2;
               out[b]=0;
               out[b+1]=0;
               out[b+2]=0;
               out[b+3]=distribution_match(d);
               b+=4;
            }
         }
      }
      return b;
   }

   // Estimate which 0 run length will result in the shortest byte stream
   int testRLE(const unsigned char * in, const int length, const int minReduction )
   {
      int lvl1=0;
      int lvl2=0;
      int lvl3=0;

      int l2low[]={0,1,3,3};
      int l3low[]={0,1,2,4};

      for ( int a =0; a < length; a++ )
      {
         if ( !in[a] )
         {
            int b=a;
            a++;
            for ( ; !in[a] && a < length;a++);
            b=a-b;
            if ( b <= 256 )
            {
               lvl1+=2;
               if ( b < 4)
               {
                  lvl2+=l2low[b];
                  lvl3+=l3low[b];
               }
               else
               {
                  lvl2+=3;
                  lvl3+=4;
               }
            } 
            else
            {
               lvl1+=(b/256+1)*2;
               lvl2+=((b-b/256)/256+1)*3;
               lvl3+=((b-(2*b/256))/256+1)*4;
            }
         }
         lvl1++;
         lvl2++;
         lvl3++;
      }

      // if RLE compressed extremely well, see if all but the 1st byte are 0 
      if ( lvl1 < length/128+4 )
      {
         unsigned int val=*(unsigned int*)(in);
         val-=in[0];
         val|=*(unsigned int*)(in+4);
         val|=*(unsigned int*)(in+8);
         val|=*(unsigned int*)(in+12);
         val|=*(unsigned int*)(in+16);
         val|=*(unsigned int*)(in+20);
         val|=*(unsigned int*)(in+24);
         val|=*(unsigned int*)(in+28);
         if ( !val )
         {
            if ( !memcmp(in+16,in+32,length-32))
            {
               return -1;
            }
         }
      }

      lvl1=length-lvl1;
      lvl2=length-lvl2;
      lvl3=length-lvl3;


      if ( lvl1 >= minReduction && lvl1 >= lvl2 && lvl1 >= lvl3 )
         return 1;
      if ( lvl2 >= minReduction && lvl2 >= lvl3 )
         return 2;
      if ( lvl3 >= minReduction )
         return 3;
      return 0;
   }

   // This undoes the modified Run Length Encoding on a byte stream.
   // The 'level' parameter tells how many 0's must be read before it
   // is considered a 'run', either 1, 2, or 3. Once 'level' zeros have
   // been read in the byte stream, the following byte tells how many more
   // 0 bytes to output.
   int deRLE(const unsigned char * in, unsigned char * out, const int length, const int level)
   {
      int a=0;
      int b=0;
      memset(out,0,length);
      if ( level == 1 )
      {
         while ( b < length)
         {
            if ( in[a] )
            {
               out[b++]=in[a++];
            } 
            else 
            {
               b+=distribution_restore(in[a+1])+1;
               a+=2;
            }
         }
      } 
      else if ( level == 2 )
      {
         while ( b < length-1)
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
            else
            {
               b+=distribution_restore(in[a+2])+2;
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
               b+=distribution_restore(in[a+3])+3;
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
}
#endif