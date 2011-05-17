#include "yeti.h"
#include "codec_inst.h"
#include "prediction.h"
#include "threading.h"
#include "convert.h"
#include "huffyuv_a.h"
#include "convert_yv12.h"

DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
	if(m_started == 0x1337)
	{
		CompressEnd();
	}
	m_started = 0;

	m_nullframes = GetPrivateProfileInt(SettingsHeading, "nullframes", FALSE, SettingsFile) > 0;
	m_deltaframes = GetPrivateProfileInt(SettingsHeading, "deltaframes", FALSE, SettingsFile) > 0;

	if (int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK)
	{
		return error;
	}

	m_width = lpbiIn->biWidth;
	m_height = lpbiIn->biHeight;

	m_format = lpbiIn->biBitCount;
	m_length = EIGHTH(m_width * m_height * m_format);

	size_t buffer_size;
	if(m_format < RGB24)
	{
		buffer_size = EIGHTH(ALIGN_ROUND(m_width, 32) * m_height * m_format) + 1024;
	} 
	else
	{
		buffer_size = QUADRUPLE(ALIGN_ROUND(m_width, 16) * m_height) + 1024;
	}

	m_buffer = (BYTE*)ALIGNED_MALLOC(m_buffer, buffer_size, 16, "buffer");
	m_buffer2 = (BYTE*)ALIGNED_MALLOC(m_buffer2, buffer_size, 16, "buffer2");
	m_deltaBuffer = (BYTE*)ALIGNED_MALLOC(m_deltaBuffer, buffer_size, 16, "delta");
	m_colorTransBuffer = (BYTE*)ALIGNED_MALLOC(m_colorTransBuffer, buffer_size, 16, "colorT");
	m_prevFrame = (BYTE*)ALIGNED_MALLOC(m_prevFrame, buffer_size, 16, "prev");
	ZeroMemory(m_prevFrame, buffer_size);

	if(!m_buffer || !m_buffer2 || !m_prevFrame || !m_deltaBuffer || !m_colorTransBuffer)
	{
		ALIGNED_FREE(m_buffer, "buffer");
		ALIGNED_FREE(m_buffer2, "buffer2");
		ALIGNED_FREE(m_deltaBuffer, "delta");
		ALIGNED_FREE(m_colorTransBuffer, "colorT");
		ALIGNED_FREE(m_prevFrame, "prev");
		return (DWORD)ICERR_MEMORY;
	}

	if (!m_compressWorker.InitCompressBuffers(m_width * m_height))
	{
		return (DWORD)ICERR_MEMORY;
	}

	DWORD code = InitThreads(true);
	if(code != ICERR_OK)
	{
		return code;
	}
	m_started = 0x1337;

	return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 1KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
	return (DWORD)(ALIGN_ROUND(lpbiIn->biWidth, 16) * lpbiIn->biHeight * lpbiIn->biBitCount / 8 * 1.05 + 1024); // TODO: Correct?
}

// release resources when compression is done

DWORD CodecInst::CompressEnd()
{
	if( m_started == 0x1337)
	{
		EndThreads();

		ALIGNED_FREE(m_buffer, "buffer");
		ALIGNED_FREE(m_buffer2, "buffer2");
		ALIGNED_FREE(m_deltaBuffer, "delta");
		ALIGNED_FREE(m_prevFrame, "prev");
		ALIGNED_FREE(m_colorTransBuffer, "colorT");
		m_compressWorker.FreeCompressBuffers();
	}

	m_started = 0;
	return ICERR_OK;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings
DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD /*dwSize*/)
{
	m_out = (BYTE*)icinfo->lpOutput;
	m_in  = (BYTE*)icinfo->lpInput;

	if(icinfo->lFrameNum == 0 && m_started != 0x1337)
	{
		DWORD error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
		if(error != ICERR_OK)
		{
			return error;
		}
	} 

	const size_t pixels = m_width * m_height;
	unsigned int upos = ALIGN_ROUND(pixels + 16, 16);
	unsigned int vpos = ALIGN_ROUND(pixels + HALF(pixels) + 32, 16);

	if(m_compressFormat == YV12)
	{
		if(m_format == RGB24)
		{
			ConvertRGB24toYV12_SSE2(m_in, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_height);
			m_in = m_colorTransBuffer;
		} 
		else if(m_format == RGB32)
		{
			ConvertRGB32toYV12_SSE2(m_in, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_height);
			m_in = m_colorTransBuffer;
		}
		else if (m_format == YUY2)
		{
			isse_yuy2_to_yv12(m_in, m_width*2 , m_width*2, m_colorTransBuffer, m_colorTransBuffer + upos, m_colorTransBuffer + vpos, m_width, m_width/2, m_height);
			m_in = m_colorTransBuffer;
		}
	}
	else 
	{
		if(m_format == RGB24)
		{
			ConvertRGB24toYV16_SSE2(m_in,m_colorTransBuffer,m_colorTransBuffer+upos,m_colorTransBuffer+vpos, m_width, m_height);
			m_in = m_colorTransBuffer;
		} 
		else if(m_format == RGB32)
		{
			ConvertRGB32toYV16_SSE2(m_in,m_colorTransBuffer,m_colorTransBuffer+upos,m_colorTransBuffer+vpos, m_width, m_height);
			m_in = m_colorTransBuffer;
		}
	}

	return m_compressFormat == YV12 ? CompressYV12(icinfo) : CompressYUV16(icinfo);
}

unsigned int CodecInst::HandleTwoCompressionThreads(unsigned int chan_size)
{
	unsigned int channel_sizes[3];
	channel_sizes[0] = chan_size;

	unsigned int current_size = chan_size+9;

	HANDLE events[2];
	events[0] = m_threads[0].m_doneEvent;
	events[1] = m_threads[1].m_doneEvent;

	int pos = WaitForMultipleObjects(2, &events[0], false, INFINITE);
	pos -= WAIT_OBJECT_0;

	ThreadData * finished = &m_threads[pos];
	ThreadData * running = &m_threads[!pos];

	((DWORD*)(m_out+1))[pos] = current_size;
	channel_sizes[pos+1] = finished->m_size;
	memcpy(m_out+current_size, (unsigned char*)finished->m_dest, channel_sizes[pos+1]);
	current_size += finished->m_size;

	pos = !pos;
	((DWORD*)(m_out+1))[pos] = current_size;

	WaitForSingleObject(running->m_doneEvent,INFINITE);

	finished = running;
	channel_sizes[pos+1] = finished->m_size;

	memcpy(m_out+current_size, (unsigned char*)finished->m_dest, channel_sizes[pos+1]);
	current_size += finished->m_size;

	if (channel_sizes[0] >= channel_sizes[1] && channel_sizes[0] >= channel_sizes[2] && m_threads[0].m_priority != THREAD_PRIORITY_BELOW_NORMAL) 
	{
		SetThreadPriority(m_threads[0].m_thread,THREAD_PRIORITY_BELOW_NORMAL);
		SetThreadPriority(m_threads[1].m_thread,THREAD_PRIORITY_BELOW_NORMAL);
		m_threads[0].m_priority=THREAD_PRIORITY_BELOW_NORMAL;
		m_threads[1].m_priority=THREAD_PRIORITY_BELOW_NORMAL;
	} 
	else if (channel_sizes[1] >= channel_sizes[2] &&  m_threads[0].m_priority != THREAD_PRIORITY_ABOVE_NORMAL)
	{
		SetThreadPriority(m_threads[0].m_thread,THREAD_PRIORITY_ABOVE_NORMAL);
		SetThreadPriority(m_threads[1].m_thread,THREAD_PRIORITY_NORMAL);
		m_threads[0].m_priority=THREAD_PRIORITY_ABOVE_NORMAL;
		m_threads[0].m_priority=THREAD_PRIORITY_NORMAL;
	}
	else if (m_threads[1].m_priority != THREAD_PRIORITY_ABOVE_NORMAL)
	{
		SetThreadPriority(m_threads[0].m_thread,THREAD_PRIORITY_NORMAL);
		SetThreadPriority(m_threads[1].m_thread,THREAD_PRIORITY_ABOVE_NORMAL);
		m_threads[0].m_priority=THREAD_PRIORITY_NORMAL;
		m_threads[0].m_priority=THREAD_PRIORITY_ABOVE_NORMAL;
	}
	return current_size;
}

DWORD CodecInst::CompressYUV16(ICCOMPRESS* icinfo)
{
	const size_t pixels    = m_width * m_height;
	const size_t y_stride  = ALIGN_ROUND(pixels + 16, 16);
	const size_t c_stride  = ALIGN_ROUND(HALF(pixels) + 32, 16);
	const size_t len       = y_stride + DOUBLE(c_stride);

	if(m_nullframes)
	{
		size_t pos = HALF(len) + 15;
		pos &= (~15);
		if(memcmp(m_in + pos, m_prevFrame + pos, len - pos) == 0 && memcmp(m_in, m_prevFrame, pos) == 0)
		{
			icinfo->lpbiOutput->biSizeImage = 0;
			*icinfo->lpdwFlags = 0;
			return (DWORD)ICERR_OK;
		}
	}

	BYTE frameType;
	BYTE* source = m_in;

	if(m_deltaframes 
		&& (icinfo->dwFlags & ICCOMPRESS_KEYFRAME) != ICCOMPRESS_KEYFRAME
		&& icinfo->lFrameNum > 0 
		&& InterframeEncode(m_deltaBuffer, m_in, m_prevFrame, len, DOUBLE(len)))
	{
		source = m_deltaBuffer;
		*icinfo->lpdwFlags |= AVIIF_LASTPART;
		frameType = YUY2_DELTAFRAME;
	}
	else
	{
		*icinfo->lpdwFlags |= AVIIF_KEYFRAME;
		icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
		frameType = YUY2_KEYFRAME;
	}

	if(m_nullframes || m_deltaframes)
	{
		memcpy(m_prevFrame, m_in, len);
	}

	BYTE* ysrc  = (source != m_deltaBuffer) ? m_deltaBuffer :m_colorTransBuffer;
	BYTE* usrc  = ysrc + y_stride;
	BYTE* vsrc  = usrc + c_stride;

	BYTE* ydest	= m_buffer2;
	BYTE* udest	= ydest + y_stride;
	BYTE* vdest	= udest + c_stride;

	BYTE *y, *u, *v;

	if (icinfo->lpbiInput->biCompression == FOURCC_YV16)
	{
		y = source;
		v = y + pixels;
		u = v + HALF(pixels);

		ydest += ((int)y)&15;
		udest += ((int)u)&15;
		vdest += ((int)v)&15;
	}
	else if (icinfo->lpbiInput->biBitCount > YUY2)
	{
		y = source;
		u = source + ALIGN_ROUND(m_width * m_height + 16, 16);
		v = source + ALIGN_ROUND(m_width * m_height * 3/2 + 32, 16);
	} 
	else 
	{
		if (icinfo->lpbiInput->biCompression == FOURCC_YUY2)
		{
			Split_YUY2(source, ysrc, usrc, vsrc, m_width, m_height);
		} 
		else
		{
			Split_UYVY(source, ysrc, usrc, vsrc, m_width, m_height);
		}
		y = ysrc;
		u = usrc;
		v = vsrc;
	}

	BYTE* ucomp = m_buffer;
	BYTE* vcomp = m_buffer + ALIGN_ROUND(pixels + 64, 16);

	m_threads[0].m_source = u;
	m_threads[0].m_dest = ucomp;
	m_threads[0].m_length = HALF(pixels);
	SetEvent(m_threads[0].m_startEvent);

	m_threads[1].m_source = v;
	m_threads[1].m_dest = vcomp;
	m_threads[1].m_length = HALF(pixels);
	SetEvent(m_threads[1].m_startEvent);

	Block_Predict_YUV16(y, ydest, m_width, pixels, true);

	size_t y_size = m_compressWorker.Compact(ydest, m_out + 9, pixels);
	size_t size = HandleTwoCompressionThreads(y_size);

	m_out[0] = frameType;
	icinfo->lpbiOutput->biSizeImage = size;
	assert( *(__int64*)(m_out+*(UINT32*)(m_out+1)) != 0 );
	assert( *(__int64*)(m_out+*(UINT32*)(m_out+5)) != 0 );
	return ICERR_OK;
}

DWORD CodecInst::CompressYV12(ICCOMPRESS* icinfo)
{
	const size_t pixels	= m_width * m_height;
	const size_t c_len	= FOURTH(pixels);
	const size_t len		= (icinfo->lpbiInput->biBitCount != YV12) ? ALIGN_ROUND(pixels + HALF(pixels) + 32, 16) + c_len : pixels + HALF(pixels);

	if(m_nullframes)
	{
		// compare in two parts, video is more likely to change in middle than at bottom
		unsigned int pos = HALF(len) + 15;
		pos &= (~15);
		if(memcmp(m_in + pos, m_prevFrame + pos, len - pos) == 0 && memcmp(m_in, m_prevFrame, pos) == 0)
		{
			icinfo->lpbiOutput->biSizeImage = 0;
			*icinfo->lpdwFlags = 0;
			return (DWORD)ICERR_OK;
		}
	}

	BYTE* source = m_in;
	BYTE frameType;

	if(m_deltaframes 
		&& (icinfo->dwFlags & ICCOMPRESS_KEYFRAME) != ICCOMPRESS_KEYFRAME
		&& icinfo->lFrameNum > 0 
		&& InterframeEncode(m_deltaBuffer, m_in, m_prevFrame, len, DOUBLE(len)))
	{
		source = m_deltaBuffer;
		*icinfo->lpdwFlags |= AVIIF_LASTPART;
		frameType = YV12_DELTAFRAME;
	}
	else
	{
		*icinfo->lpdwFlags |= AVIIF_KEYFRAME;
		icinfo->dwFlags |= ICCOMPRESS_KEYFRAME;
		frameType = YV12_KEYFRAME;
	}

	if(m_nullframes || m_deltaframes)
	{
		memcpy(m_prevFrame, m_in, len);
	}

	const BYTE* ysrc = source;
	const BYTE* vsrc = ysrc + pixels;
	const BYTE* usrc = vsrc + c_len;

	if(icinfo->lpbiInput->biBitCount != YV12)
	{
		usrc = ysrc + ALIGN_ROUND(pixels + 16, 16);
		vsrc = ysrc + ALIGN_ROUND(pixels + HALF(pixels) + 32, 16);
	}

	BYTE* ucomp = m_buffer2;
	BYTE* vcomp = m_buffer2 + ALIGN_ROUND(HALF(pixels) + 64, 16);

	m_threads[0].m_source = vsrc;
	m_threads[0].m_dest = vcomp;
	m_threads[0].m_length = c_len;
	SetEvent(m_threads[0].m_startEvent);

	m_threads[1].m_source = usrc;
	m_threads[1].m_dest = ucomp;
	m_threads[1].m_length = c_len;
	SetEvent(m_threads[1].m_startEvent);

	BYTE* ydest = m_buffer;
	ydest += (unsigned int)ysrc & 15;

	Block_Predict(ysrc, ydest, m_width, pixels);

	size_t y_size = m_compressWorker.Compact(ydest, m_out + 9, pixels);
	size_t size = HandleTwoCompressionThreads(y_size);

	m_out[0] = frameType;
	icinfo->lpbiOutput->biSizeImage = size;
	return (DWORD)ICERR_OK;
}