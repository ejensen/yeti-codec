#include "golomb.h"

static void UnsignedGolombEncode(const unsigned int in, BitOutputManager& bitManger)
{
    unsigned int M = 0;
    unsigned int info;
    unsigned int val2 = in;

    val2++;
    while (val2>1) //get the log base 2 of val.
    {
        val2 >>= 1;
        M++;        
    }

    info = in - (1<<M) + 1;

    for(unsigned int i = 1; i <= M ; ++i)
    {
       bitManger.OutputBit(0);
    }

    bitManger.OutputBit(1);

    for(unsigned int i = 0 ; i < M; ++i)
    {
       bitManger.OutputBit((info & (1<<i)) != 0);
    }
}

unsigned int GolombEncode(const unsigned int* in, unsigned char* out, unsigned int length)
{
   BitOutputManager bitManager(out);
   for(unsigned int i = 0; i < length; i++)
   {
      UnsignedGolombEncode(*(in + i), bitManager);
   }

   bitManager.Flush();

   return bitManager.Size();
}

static unsigned int UnsignedGolombDecode(BitInputManager& bitManger)
{    
    unsigned int M = 0;
    unsigned int info = 0;
    bool bit = 0;

    do
    {
       bit = bitManger.InputBit();
       if(!bit)
       {
            ++M;
       }
    }
    while(!bit && M < 64);//terminate if the number is too big
  
    for(unsigned int i = 0 ; i < M; ++i)
    {
        if(bitManger.InputBit())
        {
            info |= (1<<i);
        }
    }

    return (1<<M) - 1 + info;
}

unsigned int GolombDecode(const unsigned char* in, unsigned int* out, unsigned int length)
{
   BitInputManager bitManager(in);
   for(unsigned int i = 0; i < length; i++)
   {
       *(out + i) = UnsignedGolombDecode(bitManager);
   }

   return bitManager.ByteOffset();
}