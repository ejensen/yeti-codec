#pragma once

#define ForceResumeThread(x) {\
	while( ResumeThread(x) != 1){\
		Sleep(0);\
	}\
}

#define wait_for_threads(threads) { \
	if ( _multithreading ) { \
		if ( threads == 2) { \
			while ( _info_a.length || _info_b.length ) \
				Sleep(0); \
		} else if ( threads == 3 ){ \
			while ( _info_a.length || _info_b.length || _info_c.length) \
				Sleep(0); \
		} \
	} \
}

DWORD WINAPI encode_worker_thread( LPVOID i );
DWORD WINAPI decode_worker_thread( LPVOID i );