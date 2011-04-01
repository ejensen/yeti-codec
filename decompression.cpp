#include "compact.h"
#include "yeti.h"
#include "codec_inst.h"
#include "prediction.h"
#include "threading.h"

#include "convert_yuy2.h"
#include "convert_yv12.h"
#include "huffyuv_a.h"

// initialize the codec for decompression
DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
	if(m_started)
	{
		DecompressEnd();
	}

	if(int error = DecompressQuery(lpbiIn, lpbiOut) != ICERR_OK)
	{
		return error;
	}

	m_width = lpbiIn->biWidth;
	m_height = lpbiIn->biHeight;

	detectFlags(&SSE2, &SSE);

	m_format = lpbiOut->biBitCount;

	m_length = EIGHTH(m_width * m_height * m_format);

	const int buffer_size = (m_format >= RGB24) ? ALIGN_ROUND(QUADRUPLE(m_width), 8) * m_height + 2048 : m_length + 2048;

	m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, buffer_size, 16, "buffer");
	m_buffer2 = (BYTE*)ALIGNED_MALLOC(m_buffer2, buffer_size, 16, "buffer2");
	m_prevFrame = (BYTE*)ALIGNED_MALLOC(m_prevFrame, buffer_size, 16, "prev");

	if(!m_buffer || !m_buffer2 || !m_prevFrame)
	{
		return (DWORD)ICERR_MEMORY;
	}

	//if(!m_compressWorker.InitCompressBuffers( m_width * m_height))
	//{
	//	return (DWORD)ICERR_MEMORY;
	//}


	int code = InitThreads(false);
	if (code != ICERR_OK)
	{
		return code;
	}
	m_started = true;
	return ICERR_OK;
}

DWORD CodecInst::DecompressEnd()
{
	if(m_started)
	{
      EndThreads();

		ALIGNED_FREE(m_buffer,"buffer");
		ALIGNED_FREE(m_buffer2,"buffer2");
		ALIGNED_FREE(m_prevFrame, "prev");
		m_compressWorker.FreeCompressBuffers();
	}

	m_started = false;
	return ICERR_OK;
}

inline void CodecInst::InitDecompressionThreads(const BYTE* in, BYTE* out, size_t length, threadInfo* thread)
{
	if (thread)
	{
		thread->m_source = in;
		thread->m_dest = out;
		thread->m_length = length;
		while ( ResumeThread(thread->m_thread) > 1){
			Sleep(0);
		};
	} 
	else 
	{
		m_compressWorker.Uncompact(in, out, length);
	}
}

void CodecInst::YUY2Decompress(DWORD flags)
{
	BYTE* dst = m_out;
	BYTE* dst2 = m_buffer;

	if(m_format == YUY2)
	{
		dst = m_buffer;
		dst2 = m_out;
	}

	const size_t pixels = m_width * m_height;
	const size_t half = HALF(pixels);
	BYTE* y, *u, *v;
	y = dst;
	u = y + pixels;
	v = u + half;

	int size = *(UINT32*)(m_in + 1);
	InitDecompressionThreads(m_in + size, u, half, &m_info_a);
	size = *(UINT32*)(m_in+5);
	InitDecompressionThreads(m_in + size, v, half, &m_info_b);
	size=*(UINT32*)(m_in+1);
	InitDecompressionThreads(m_in + 9, y, pixels, NULL);

	// special case: RLE detected a solid Y value (compressed size = 2),
	// need to set 2nd Y value for restoration to work right
	if(size == 11) //TODO: Needed?
	{
		dst[1] = dst[0];
	}

	WAIT_FOR_THREADS(2);

	Interleave_And_Restore_YUY2(dst2, y, u, v, m_width, m_height);

	if((flags & ICDECOMPRESS_NOTKEYFRAME) == ICDECOMPRESS_NOTKEYFRAME)
	{
      //MessageBox(HWND_DESKTOP, "DeltaFrame", "Info", MB_OK);
		Fast_Add(dst2, dst2, m_prevFrame, DOUBLE(pixels));
	}

	memcpy(m_prevFrame, dst2, DOUBLE(pixels));

	if(m_format == YUY2 || (flags & ICDECOMPRESS_PREROLL) == ICDECOMPRESS_PREROLL)
	{
		return;
	}

	if(m_format == RGB24)
	{
		mmx_YUY2toRGB24(dst2, m_out, m_buffer + DOUBLE(pixels), DOUBLE(m_width));
	} 
	else 
	{
		mmx_YUY2toRGB32(dst2, m_out, m_buffer + DOUBLE(pixels), DOUBLE(m_width));
	}
}

void CodecInst::YV12Decompress(DWORD flags)
{
	BYTE* dst = m_out;
	BYTE* dst2 = m_buffer;

	if (m_format == YUY2)
	{
		dst = m_buffer;
		dst2 = m_out;
	}

	const unsigned int wxh = m_width * m_height;
	const unsigned int quarterArea = FOURTH(wxh);

	BYTE* usrc = dst + wxh;
	BYTE* vsrc = usrc + quarterArea;

	int size = *(UINT32*)(m_in+1);
	InitDecompressionThreads(m_in + size, usrc, quarterArea, &m_info_a);
	size = *(UINT32*)(m_in + 5);
	InitDecompressionThreads(m_in + size, vsrc, quarterArea, &m_info_b);
	InitDecompressionThreads(m_in + 9, dst, wxh, NULL );

	WAIT_FOR_THREADS(2);

	Restore_YV12(dst, usrc, vsrc, m_width, m_height);

	const size_t length = EIGHTH(wxh * YV12);

	if((flags & ICDECOMPRESS_NOTKEYFRAME) == ICDECOMPRESS_NOTKEYFRAME)
	{
		Fast_Add(dst, dst, m_prevFrame, length);
	}

	memcpy(m_prevFrame, dst, length);

	if(m_format == YV12 || (flags & ICDECOMPRESS_PREROLL) == ICDECOMPRESS_PREROLL)
	{
		return;
	}

	//upsample if needed
	isse_yv12_to_yuy2(dst, dst + wxh + quarterArea, dst + wxh, m_width, m_width, HALF(m_width), dst2, DOUBLE(m_width), m_height); 

	if(m_format == YUY2)
	{
		return;
	}

	// upsample to RGB
	if(m_format == RGB32)
	{
		mmx_YUY2toRGB32(dst2, m_out, dst2 + DOUBLE(wxh), DOUBLE(m_width));
	} 
	else 
	{
		mmx_YUY2toRGB24(dst2, m_out, dst2 + DOUBLE(wxh), DOUBLE(m_width));
	}
}

DWORD CodecInst::Decompress(ICDECOMPRESS* idcinfo) 
{
#ifdef _DEBUG
	try 
	{
#endif
		DWORD return_code = ICERR_OK;

		if(!m_started)
		{
			DecompressBegin(idcinfo->lpbiInput, idcinfo->lpbiOutput);
		}

		m_out = (BYTE*)idcinfo->lpOutput;
		m_in  = (BYTE*)idcinfo->lpInput; 
		idcinfo->lpbiOutput->biSizeImage = m_length;

		// according to the avi specs, the calling application is responsible for handling null frames.
		if(idcinfo->lpbiInput->biSizeImage == 0)
		{
#ifdef _DEBUG
         MessageBox (HWND_DESKTOP, "Received request to decode a null frame", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif     
			return ICERR_OK;
		}

		switch(m_in[0] & ~KEYFRAME)
		{
		case YUY2_FRAME:
			{
				YUY2Decompress(idcinfo->dwFlags);
				break;
			}
		case YV12_FRAME:
			{
				YV12Decompress(idcinfo->dwFlags);
				break;
			}
		default:
			{
#ifdef _DEBUG
				char emsg[128];
				sprintf_s(emsg, 128, "Unrecognized frame type: %d", m_in[0]);
				MessageBox (HWND_DESKTOP, emsg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
				return_code = (DWORD)ICERR_ERROR;
				break;
			}
		}

		return return_code;
#ifdef _DEBUG
	} 
	catch( ... )
	{
		MessageBox (HWND_DESKTOP, "Exception caught in decompress main", "Error", MB_OK | MB_ICONEXCLAMATION);
		return (DWORD)ICERR_INTERNAL;
	}
#endif
}