#include <stdint.h>
#include "yeti.h"
#include "codec_inst.h"
#include "threading.h"
#include "prediction.h"

DWORD WINAPI EncodeWorkerTread(LPVOID i)
{
	ThreadData* threadData = (ThreadData *)i;

	WaitForSingleObject(threadData->m_startEvent, INFINITE);
	while(true)
	{
		unsigned int length = threadData->m_length;
		if ( length == UINT32_MAX )
		{
			break;
		} 
		else if ( length > 0 )
		{
			BYTE* src = (BYTE*)threadData->m_source;
			BYTE* dest = (BYTE*)threadData->m_dest;
			BYTE* dst = dest + ((unsigned int)src&15);

			if ( threadData->m_format == YUY2 )
			{
				Block_Predict_YUV16(src, dst, threadData->m_width, length, false);
			} 
			else
			{
				Block_Predict(src, dst, threadData->m_width, length );
			}

			threadData->m_size = threadData->m_compressWorker.Compact(dst, dest, length);
			assert( *(__int64*)dest != 0 );
			threadData->m_length = 0;
		} 
		else
		{
			assert(false);
		}
		SignalObjectAndWait(threadData->m_doneEvent, threadData->m_startEvent, INFINITE, false);
	}

	threadData->m_compressWorker.FreeCompressBuffers();
	ALIGNED_FREE(threadData->m_buffer, "Thread Buffer");
	threadData->m_length = 0;
	return 0;
}

DWORD WINAPI DecodeWorkerThread(LPVOID i)
{
	ThreadData* threadData = (ThreadData *)i;

	WaitForSingleObject(threadData->m_startEvent, INFINITE);
	while(true)
	{
		unsigned int length = threadData->m_length;
		if ( length == UINT32_MAX )
		{
			break;
		} 
		else if ( length > 0 )
		{
			BYTE* src = (BYTE*)threadData->m_source;
			BYTE* dest = (BYTE*)threadData->m_dest;

			threadData->m_compressWorker.Uncompact(src, dest, threadData->m_length);
			threadData->m_length = 0;
		} 
		else
		{
			assert(false);
		}
		SignalObjectAndWait(threadData->m_doneEvent, threadData->m_startEvent, INFINITE, false);
	}

	threadData->m_compressWorker.FreeCompressBuffers();
	ALIGNED_FREE(threadData->m_buffer, "Thread Buffer");
	threadData->m_length = 0;
	return 0;
}

DWORD CodecInst::InitThreads(bool encode)
{
	m_threads[0].m_length = m_threads[1].m_length = 0;

	m_threads[0].m_startEvent = CreateEvent(NULL ,false, false, NULL);
	m_threads[0].m_doneEvent = CreateEvent(NULL, false, false, NULL);
	m_threads[1].m_startEvent = CreateEvent(NULL, false, false, NULL);
	m_threads[1].m_doneEvent = CreateEvent(NULL, false, false, NULL);

	unsigned int useFormat = (encode) ? m_compressFormat : m_format;

	m_threads[0].m_width =   m_threads[1].m_width = HALF(m_width);
	m_threads[0].m_height =  m_threads[1].m_height = (useFormat != YV12) ? m_height : HALF(m_height); m_height;

	m_threads[0].m_format =   m_threads[1].m_format = useFormat;

	m_threads[0].m_thread =  m_threads[1].m_thread = NULL;
	m_threads[0].m_buffer =  m_threads[1].m_buffer = NULL;

	size_t buffer_size = ALIGN_ROUND(m_threads[0].m_width, 16) * m_threads[0].m_height + 256;

	bool memerror = false;
	bool interror = false;

	if (encode) 
	{
		if (!m_threads[0].m_compressWorker.InitCompressBuffers( buffer_size ) || ! m_threads[1].m_compressWorker.InitCompressBuffers( buffer_size )) 
		{
			memerror = true;
		}
	}
	if (!memerror)
	{
		LPTHREAD_START_ROUTINE threadDelegate = encode ? EncodeWorkerTread : DecodeWorkerThread;

		if (!memerror)
		{
			if (encode)
			{
				m_threads[0].m_thread = CreateThread(NULL, 0, threadDelegate, &m_threads[0], CREATE_SUSPENDED, NULL);
				m_threads[1].m_thread = CreateThread(NULL, 0, threadDelegate, &m_threads[1], CREATE_SUSPENDED, NULL);
			} 
			else
			{
				m_threads[0].m_thread = CreateThread(NULL, 0, threadDelegate, &m_threads[0], CREATE_SUSPENDED, NULL);
				m_threads[1].m_thread = CreateThread(NULL, 0, threadDelegate, &m_threads[1], CREATE_SUSPENDED, NULL);
			}

			if (!m_threads[0].m_thread || ! m_threads[1].m_thread)
			{
				interror = true;
			} 
			else 
			{
				SetThreadPriority( m_threads[1].m_thread, THREAD_PRIORITY_BELOW_NORMAL);
				SetThreadPriority( m_threads[1].m_thread, THREAD_PRIORITY_BELOW_NORMAL);
			}
		}
	}

	if (!memerror && !interror && encode)
	{
		m_threads[0].m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_threads[0].m_buffer, buffer_size, 16, "m_threads[0].buffer");
		m_threads[1].m_buffer = (BYTE *)ALIGNED_MALLOC((void *)m_threads[1].m_buffer, buffer_size, 16, "m_threads[1].buffer");
		if (m_threads[0].m_buffer == NULL || m_threads[1].m_buffer == NULL )
		{
			memerror = true;
		}
	}

	if (memerror || interror)
	{
		if (encode)
		{
			ALIGNED_FREE(m_threads[0].m_buffer,"m_threads[0].buffer");
			ALIGNED_FREE(m_threads[1].m_buffer,"m_threads[1].buffer");
			m_threads[0].m_compressWorker.FreeCompressBuffers();
			m_threads[1].m_compressWorker.FreeCompressBuffers();
		}

		m_threads[0].m_thread=NULL;
		m_threads[1].m_thread=NULL;

		return (DWORD)((memerror) ? ICERR_MEMORY : ICERR_INTERNAL);
	} 
	else 
	{
		ResumeThread(m_threads[0].m_thread);
		ResumeThread(m_threads[1].m_thread);
		return (DWORD)ICERR_OK;
	}
}

void CodecInst::EndThreads()
{
	m_threads[1].m_length = UINT32_MAX;
	m_threads[0].m_length = UINT32_MAX;

	HANDLE events[2];
	events[0]= m_threads[0].m_thread;
	events[1]= m_threads[1].m_thread;

	if(m_threads[0].m_thread)
	{
		SetEvent(m_threads[0].m_startEvent);
	}
	if(m_threads[1].m_thread)
	{
		SetEvent(m_threads[1].m_startEvent);
	}

	WaitForMultipleObjectsEx(2, &events[0], true, 10000, true);

	if(!CloseHandle(m_threads[0].m_thread))
	{
#ifdef _DEBUG
		MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0A", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
	}
	if(!CloseHandle(m_threads[1].m_thread))
	{
#ifdef _DEBUG
		MessageBox (HWND_DESKTOP, "CloseHandle failed for thread 0x0B", "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
	}

	m_threads[0].m_thread = NULL;
	m_threads[1].m_thread = NULL;
	m_threads[0].m_length = 0;
	m_threads[1].m_length = 0;
}