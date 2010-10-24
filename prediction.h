//This file contains functions that perform and undo median prediction.

void SSE2_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int mode);
void reduce_res(const unsigned char * src, unsigned char * dest, unsigned char * buffer, unsigned int width, unsigned int height, const int SSE2, const int SSE, const int MMX);
void enlarge_res(const unsigned char * src, unsigned char * dst, unsigned char * buffer, unsigned int width, unsigned int height, const int SSE2, const int SSE, const int MMX);
void ASM_BlockRestore(unsigned char * source, unsigned int stride,unsigned int xlength, unsigned int mode);

void MMX_BlockPredict( const unsigned char * source, unsigned char * dest, const unsigned int stride, const unsigned int length, int SSE, int mode);
void MMX_Restore32( unsigned char * out, const unsigned int stride, const unsigned int length, const int SSE, int mode);

void MMX_Predict_YUY2(const unsigned char * src,unsigned char * dest,const unsigned int width,const unsigned height,int SSE,int lum);

void SSE2_Predict_YUY2(const unsigned char * src,unsigned char * dest,const unsigned int width,const unsigned height,int lum);