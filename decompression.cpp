#include "compact.h"
#include "yeti.h"
#include "codec_inst.h"
#include "prediction.h"
#include "threading.h"

#include "convert_yv12.h"
#include "huffyuv_a.h"

// initialize the codec for decompression
DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
	if(m_started == 0x1337)
	{
		DecompressEnd();
	}

	m_started = 0;

	if(int error = DecompressQuery(lpbiIn, lpbiOut) != ICERR_OK)
	{
		return error;
	}

	m_width = lpbiIn->biWidth;
	m_height = lpbiIn->biHeight;

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

	int code = InitThreads(false);
	if (code != ICERR_OK)
	{
		return code;
	}
	m_started = 0x1337;
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

	m_started = 0;
	return ICERR_OK;
}

void CodecInst::Decode3Channels(BYTE* dst1, unsigned int len1, BYTE* dst2, unsigned int len2, BYTE* dst3, unsigned int len3)
{
	const BYTE* src1 = m_in + 9;
	const BYTE* src2 = m_in + *(UINT32*)(m_in+1);
	const BYTE* src3 = m_in + *(UINT32*)(m_in+5);

	int size1 = *(UINT32*)(m_in+1);
	int size2 = *(UINT32*)(m_in+5);
	int size3 = m_compressed_size - size2;
	size2 -= size1;
	size1 -= 9;

	// Compressed size should approximate decoding time.
	// The priority of the largest channel is boosted to improve
	// load balancing when there are fewer cores than channels
	if (size1 >= size2 && size1 >= size3 && m_threads[0].m_priority != THREAD_PRIORITY_BELOW_NORMAL)
	{
		SetThreadPriority(m_threads[0].m_thread, THREAD_PRIORITY_BELOW_NORMAL);
		SetThreadPriority(m_threads[1].m_thread, THREAD_PRIORITY_BELOW_NORMAL);
		m_threads[0].m_priority = THREAD_PRIORITY_BELOW_NORMAL;
		m_threads[1].m_priority = THREAD_PRIORITY_BELOW_NORMAL;
	} 
	else if (size2 >= size3 && m_threads[0].m_priority != THREAD_PRIORITY_ABOVE_NORMAL)
	{
		SetThreadPriority(m_threads[0].m_thread, THREAD_PRIORITY_ABOVE_NORMAL);
		SetThreadPriority(m_threads[1].m_thread, THREAD_PRIORITY_NORMAL);
		m_threads[0].m_priority = THREAD_PRIORITY_ABOVE_NORMAL;
		m_threads[1].m_priority = THREAD_PRIORITY_NORMAL;
	}
	else if(m_threads[1].m_priority != THREAD_PRIORITY_ABOVE_NORMAL)
	{
		SetThreadPriority(m_threads[0].m_thread, THREAD_PRIORITY_NORMAL);
		SetThreadPriority(m_threads[1].m_thread, THREAD_PRIORITY_ABOVE_NORMAL);
		m_threads[0].m_priority = THREAD_PRIORITY_NORMAL;
		m_threads[1].m_priority = THREAD_PRIORITY_ABOVE_NORMAL;
	}

	m_threads[0].m_source = src2;
	m_threads[0].m_dest = dst2;
	m_threads[0].m_length = len2;
	SetEvent(m_threads[0].m_startEvent);

	m_threads[1].m_source = src3;
	m_threads[1].m_dest = dst3;
	m_threads[1].m_length = len3;
	SetEvent(m_threads[1].m_startEvent);

	m_compressWorker.Uncompact(src1, dst1, len1);

	WAIT_FOR_THREADS(2);
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
	BYTE* ydst = dst;
	BYTE* udst = ydst + pixels;
	BYTE* vdst = udst + half;

	Decode3Channels(ydst, pixels, udst, half, vdst, half);

	// special case: RLE detected a solid Y value (compressed size = 2),
	// need to set 2nd Y value for restoration to work right
	if(*(UINT32*)(m_in+1) == 11) //TODO: Needed?
	{
		dst[1] = dst[0];
	}

	Interleave_And_Restore_YUY2(dst2, ydst, udst, vdst, m_width, m_height);

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

	const size_t pixels = m_width * m_height;
	const size_t fourth = FOURTH(pixels);
	BYTE* ydst = dst;
	BYTE* udst = ydst + pixels;
	BYTE* vdst = udst + fourth;

	Decode3Channels(ydst, pixels, udst, fourth, vdst, fourth);

	Restore_YV12(ydst, udst, vdst, m_width, m_height);

	const size_t length = EIGHTH(pixels * YV12);
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
	isse_yv12_to_yuy2(dst, dst + pixels + fourth, dst + pixels, m_width, m_width, HALF(m_width), dst2, DOUBLE(m_width), m_height); 

	if(m_format == YUY2)
	{
		return;
	}
	// upsample to RGB
	if(m_format == RGB32)
	{
		mmx_YUY2toRGB32(dst2, m_out, dst2 + DOUBLE(pixels), DOUBLE(m_width));
	} 
	else 
	{
		mmx_YUY2toRGB24(dst2, m_out, dst2 + DOUBLE(pixels), DOUBLE(m_width));
	}
}

DWORD CodecInst::Decompress(ICDECOMPRESS* idcinfo) 
{
#ifdef _DEBUG
	try 
	{
#endif
		DWORD return_code = ICERR_OK;

		if(m_started != 0x1337)
		{
			DecompressBegin(idcinfo->lpbiInput, idcinfo->lpbiOutput);
		}

		m_out = (BYTE*)idcinfo->lpOutput;
		m_in  = (BYTE*)idcinfo->lpInput; 
		idcinfo->lpbiOutput->biSizeImage = m_length;

		m_compressed_size = idcinfo->lpbiInput->biSizeImage;
		// according to the avi specs, the calling application is responsible for handling null frames.
		if(m_compressed_size == 0)
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