#include "yeti.h"
#include <commctrl.h>
#include <shellapi.h>
#include <Windowsx.h>

#define RETURN_ERROR() return (DWORD)ICERR_BADFORMAT;//{ char msg[256];sprintf(msg,"Returning error on line %d", __LINE__);MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION); return ICERR_BADFORMAT; }

CodecInst::CodecInst()
{
#ifdef _DEBUG
   if(m_started)
   {

   }
#endif

   m_compressWorker.m_probRanges = NULL;
   m_compressWorker.m_bytecounts = NULL;
   m_compressWorker.m_buffer = NULL;
   m_buffer = NULL;
   m_prevFrame = NULL;
   m_deltaBuffer = NULL;
   m_buffer2 = NULL;
   m_colorTransBuffer = NULL;
   m_length = 0;
   m_nullframes = false;
   m_deltaframes = false;
   m_reduced = false;
   m_started = false;
   m_SSE2 = 0;
   m_SSE = 0;

   m_multithreading = 0;
   m_info_a.m_source = NULL;
   m_info_a.m_dest = NULL;
   m_info_a.m_size = 0;
   m_info_a.m_length = 0;
   m_info_a.m_thread = 0;
   m_info_b = m_info_a;
   m_info_c = m_info_a;

   memcpy((void*)&m_info_a.m_compressWorker, &m_compressWorker, sizeof(m_compressWorker));
   memcpy((void*)&m_info_b.m_compressWorker, &m_compressWorker, sizeof(m_compressWorker));
   memcpy((void*)&m_info_c.m_compressWorker, &m_compressWorker, sizeof(m_compressWorker));
}

HMODULE hmoduleYeti = 0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID) 
{
   hmoduleYeti = (HMODULE) hinstDLL;
   return TRUE;
}

static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if(uMsg == WM_INITDIALOG)
   {
      CheckDlgButton(hwndDlg, IDC_NULLFRAMES,GetPrivateProfileInt("settings", "nullframes", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_MULTI,GetPrivateProfileInt("settings", "multithreading", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_REDUCED,GetPrivateProfileInt("settings", "reduced", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_DELTAFRAMES,GetPrivateProfileInt("settings", "deltaframes", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
   } 
   else if(uMsg == WM_COMMAND) 
   {
      if(LOWORD(wParam)==IDC_OK)
      {
         WritePrivateProfileString("settings", "nullframes", (IsDlgButtonChecked(hwndDlg, IDC_NULLFRAMES) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString("settings", "multithreading", (IsDlgButtonChecked(hwndDlg, IDC_MULTI) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString("settings", "reduced", (IsDlgButtonChecked(hwndDlg, IDC_REDUCED) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString("settings", "deltaframes", (IsDlgButtonChecked(hwndDlg, IDC_DELTAFRAMES) == BST_CHECKED) ? "1" : NULL, SettingsFile);

         EndDialog(hwndDlg, 0);
      } 
      else if(LOWORD(wParam)==IDC_CANCEL)
      {
         EndDialog(hwndDlg, 0);
      } 
   } 
   else if(uMsg == WM_CLOSE)
   {
      EndDialog(hwndDlg, 0);
   }
   return 0;
}

BOOL CodecInst::QueryConfigure() 
{
   return TRUE; 
}

DWORD CodecInst::Configure(HWND hwnd) 
{
   DialogBox(hmoduleYeti, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), hwnd, (DLGPROC)ConfigureDialogProc);
   return ICERR_OK;
}

CodecInst* Open(ICOPEN* icinfo) 
{
   if(icinfo && icinfo->fccType != ICTYPE_VIDEO)
   {
      return NULL;
   }

   CodecInst* pinst = new CodecInst();

   if(icinfo) 
   {
      icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;
   }

   return pinst;
}

CodecInst::~CodecInst()
{
   try
   {
      if(m_started)
      {
         if(m_colorTransBuffer)
         {
            CompressEnd();
         } 
         else
         {
            DecompressEnd();
         }
      }
      m_started = false;
   } 
   catch ( ... ) {};
}

DWORD Close(CodecInst* pinst) 
{
   try 
   {
      if(pinst && !IsBadWritePtr(pinst,sizeof(CodecInst)))
      {
         delete pinst;
      }
   } 
   catch ( ... ){};
   return 1;
}

DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize)
{
   if(pv == NULL)
   {
      return 4 * sizeof(int);
   } 
   else if(dwSize < 4 * sizeof(int))
   {
      return (DWORD)ICERR_BADSIZE;
   }

   int * state = (int*)pv;
   state[0] = GetPrivateProfileInt("settings", "nullframes", true, SettingsFile);
   state[1] = GetPrivateProfileInt("settings", "multithreading", true, SettingsFile);
   state[2] = GetPrivateProfileInt("settings", "reduced", false, SettingsFile);
   state[3] = GetPrivateProfileInt("settings", "deltaframes", true, SettingsFile);
   return 0;
}

DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) 
{
   if(dwSize < 4 * sizeof(int))
   {
      return 4 * sizeof(int);
   }
   int * state = (int*)pv;
   char str[] = {0,0,0,0};

   str[0]='0' + state[0];
   WritePrivateProfileString("settings", "nullframes",str, SettingsFile);
   str[0]='0' + state[1];
   WritePrivateProfileString("settings", "multithreading",str, SettingsFile);
   str[0]='0' + state[2];
   WritePrivateProfileString("settings", "reduced",str, SettingsFile);
   str[0]='0' + state[3];
   WritePrivateProfileString("settings", "deltaframes",str, SettingsFile);

   return 4 * sizeof(int);
}

// test for SSE, and SSE2 support
void __stdcall detectFlags(bool * SSE2, bool * SSE)
{
   int flags;
   __asm{
         mov			eax,1
         cpuid
         mov			flags,edx
   }
   if(flags & (1 << 25))
   {
      *SSE = true;
      if(flags & (1 << 26))
      {
         *SSE2 = true;
      }
   }
}

// check if the codec can compress the given format to the desired format
DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{

   // check for valid format and bitdepth
   if(lpbiIn->biCompression == 0) 
   {
      if(lpbiIn->biBitCount != RGB24 && lpbiIn->biBitCount != RGB32)
      {
         RETURN_ERROR();
      }
   } 
   else if(lpbiIn->biCompression == FOURCC_YUY2)
   {
      if(lpbiIn->biBitCount != YUY2) 
      {
         RETURN_ERROR();
      }
   }
   else if(lpbiIn->biCompression == FOURCC_YV12)
   {
      if(lpbiIn->biBitCount != YV12)
      {
         RETURN_ERROR();
      }
   } 
   else 
   {
      /*char msg[128];
      int x = lpbiIn->biCompression;
      char * y = (char*)&x;
      sprintf(msg,"Unknown format, %c%c%c%c",y[0],y[1],y[2],y[3]);
      MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);*/
      RETURN_ERROR();
   }

   // Make sure width is mod 4 for YUV formats
   if(lpbiIn->biWidth % 4)
      RETURN_ERROR();
   // Make sure the height is acceptable for YV12 formats
   if(lpbiIn->biHeight % 2)
      RETURN_ERROR();

   // See if the output format is acceptable if need be
   if(lpbiOut)
   {
      if(lpbiOut->biSize < sizeof(BITMAPINFOHEADER))
         RETURN_ERROR();
      if(lpbiOut->biBitCount != RGB24 && lpbiOut->biBitCount != RGB32 && lpbiOut->biBitCount != YUY2 && lpbiOut->biBitCount != YV12)
         RETURN_ERROR();
      if(lpbiIn->biHeight != lpbiOut->biHeight)
         RETURN_ERROR();
      if(lpbiIn->biWidth != lpbiOut->biWidth)
         RETURN_ERROR();
      if(lpbiOut->biCompression != FOURCC_YETI)
         RETURN_ERROR();
   }

   detectFlags(&m_SSE2, &m_SSE);
   if(!m_SSE)
   {
      RETURN_ERROR()
   }

   return (DWORD)ICERR_OK;
}

// return the intended compress format for the given input format
DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if(!lpbiOut)
   {
      return sizeof(BITMAPINFOHEADER)+sizeof(UINT32);	
   }

   // make sure the input is an acceptable format
   if(CompressQuery(lpbiIn, NULL) == ICERR_BADFORMAT)
   {
      RETURN_ERROR();
   }

   *lpbiOut = *lpbiIn;
   lpbiOut->biSize = sizeof(BITMAPINFOHEADER)+sizeof(UINT32);
   lpbiOut->biPlanes = 1;
   lpbiOut->biCompression = FOURCC_YETI;

   if(lpbiIn->biBitCount != RGB24)
   {
      lpbiOut->biSizeImage = EIGHTH(lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount);
      lpbiOut->biSizeImage = EIGHTH(lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount);
   } 
   else 
   {
      lpbiOut->biSizeImage = ALIGN_ROUND(EIGHTH(lpbiIn->biWidth * lpbiIn->biBitCount),4)* lpbiIn->biHeight;
   }

   lpbiOut->biBitCount = lpbiIn->biBitCount;

   return (DWORD)ICERR_OK;
}

// return information about the codec
DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize)
{
   if(icinfo == NULL)
   {
      return sizeof(ICINFO);
   }

   if(dwSize < sizeof(ICINFO))
   {
      return 0;
   }

   icinfo->dwSize       = sizeof(ICINFO);
   icinfo->fccType      = ICTYPE_VIDEO;
   icinfo->fccHandler	= FOURCC_YETI;
   icinfo->dwFlags		= VIDCF_FASTTEMPORALC | VIDCF_FASTTEMPORALD | VIDCF_TEMPORAL;
   icinfo->dwVersion		= 0x00000001;
   icinfo->dwVersionICM	= ICVERSION;
   memcpy(icinfo->szName, L"Yeti", sizeof(L"Yeti"));
   memcpy(icinfo->szDescription, L"Yeti Video Codec", sizeof(L"Yeti Video Codec"));

   return sizeof(ICINFO);
}

// check if the codec can decompress the given format to the desired format
DWORD CodecInst::DecompressQuery(const LPBITMAPINFOHEADER lpbiIn, const LPBITMAPINFOHEADER lpbiOut)
{
   if(lpbiIn->biCompression != FOURCC_YETI)
   {
      RETURN_ERROR();
   }

   if(lpbiIn->biBitCount == YUY2 || lpbiIn->biBitCount == YV12)
   {
      if(lpbiIn->biWidth % 4)
         RETURN_ERROR();
      if(lpbiIn->biBitCount == YV12 && lpbiIn->biHeight % 2)
         RETURN_ERROR();
   }

   // make sure the input bitdepth is valid
   if(lpbiIn->biBitCount != RGB24 && lpbiIn->biBitCount != RGB32 && lpbiIn->biBitCount != YUY2 && lpbiIn->biBitCount != YV12)
   {
      RETURN_ERROR();
   }

   detectFlags(&m_SSE2, &m_SSE);
   if(!m_SSE)
   {
      RETURN_ERROR();
   }

   if(!lpbiOut)
   {
      return (DWORD)ICERR_OK;
   }

   //char msg[128];
   //char fcc[4];
   //*(unsigned int*)(&fcc[0])=lpbiOut->biCompression;
   //sprintf(msg,"Format = %d, BiComp= %c%c%c%c",lpbiOut->biBitCount,fcc[3],fcc[2],fcc[1],fcc[0] );
   //MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);

   // make sure the output format is one that can be decoded to
   if(lpbiOut->biCompression != 0 && lpbiOut->biCompression != FOURCC_YV12 && lpbiOut->biCompression != FOURCC_YUY2)
      RETURN_ERROR();
   if(lpbiOut->biBitCount != RGB32 && lpbiOut->biBitCount != RGB24 && lpbiOut->biBitCount != YUY2 && lpbiOut->biBitCount != YV12)   // make sure the output bitdepth is valid
      RETURN_ERROR();
   if(lpbiIn->biWidth % 4) // check for invalid widths/heights
      RETURN_ERROR();
   if(lpbiIn->biHeight % 2)
      RETURN_ERROR();
   if(lpbiOut->biCompression == 0 && lpbiOut->biBitCount != RGB24 && lpbiOut->biBitCount != RGB32)
      RETURN_ERROR();
   if(lpbiOut->biCompression == FOURCC_YUY2 && lpbiOut->biBitCount != YUY2)
      RETURN_ERROR();
   if(lpbiOut->biCompression == FOURCC_YV12 && lpbiOut->biBitCount != YV12)
      RETURN_ERROR();
   if(lpbiIn->biHeight != lpbiOut->biHeight)
      RETURN_ERROR();
   if(lpbiIn->biWidth != lpbiOut->biWidth)
      RETURN_ERROR();

   return (DWORD)ICERR_OK;
}

// return the default decompress format for the given input format 
DWORD CodecInst::DecompressGetFormat(const LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if(DecompressQuery(lpbiIn, NULL) != ICERR_OK)
   {
      RETURN_ERROR();
   }

   if(!lpbiOut)
   {
      return sizeof(BITMAPINFOHEADER);
   }

   *lpbiOut = *lpbiIn;
   lpbiOut->biSize = sizeof(BITMAPINFOHEADER);
   lpbiOut->biPlanes = 1;

   lpbiOut->biBitCount=12;
   lpbiOut->biCompression = FOURCC_YV12;
   lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight + lpbiIn->biWidth * lpbiIn->biHeight/2;

   return (DWORD)ICERR_OK;
}

DWORD CodecInst::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   RETURN_ERROR()
}