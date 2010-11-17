#pragma once
#include "yeti.h"

#define RESUME_THREAD(x) {\
	while( ResumeThread(x) != 1){\
		Sleep(0);\
	}\
}

#define WAIT_FOR_THREADS(threads) { \
	if ( m_multithreading ) { \
		if ( threads == 2) { \
			while ( m_info_a.m_length || m_info_b.m_length ) \
				Sleep(0); \
		} else if ( threads == 3 ){ \
			while ( m_info_a.m_length || m_info_b.m_length || m_info_c.m_length) \
				Sleep(0); \
		} \
	} \
}

DWORD WINAPI encode_worker_thread( LPVOID i );
DWORD WINAPI decode_worker_thread( LPVOID i );