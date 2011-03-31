#include "yeti.h"
#include "codec_inst.h"
#include <commctrl.h>
#include <shellapi.h>
#include <Windowsx.h>

#define RETURN_ERROR() return (DWORD)ICERR_BADFORMAT;//{ char msg[256];sprintf(msg,"Returning error on line %d", __LINE__);MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION); return ICERR_BADFORMAT; }

HMODULE hmoduleYeti = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID) 
{
   hmoduleYeti = (HMODULE)hinstDLL;
   return TRUE;
}

static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if(uMsg == WM_INITDIALOG)
   {
      CheckDlgButton(hwndDlg, IDC_NULLFRAMES, GetPrivateProfileInt(SettingsHeading, "nullframes", FALSE, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_DELTAFRAMES, GetPrivateProfileInt(SettingsHeading, "deltaframes", FALSE, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_MULTI, GetPrivateProfileInt(SettingsHeading, "multithreading", FALSE, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);

      unsigned int compressFormat = GetPrivateProfileInt(SettingsHeading, "format", YUY2, SettingsFile);

      int id = IDC_YUY2;

      if(compressFormat == YV12)
         id = IDC_YV12;

      CheckRadioButton(hwndDlg, IDC_YUY2, IDC_REDUCED, id);
   } 
   else if(uMsg == WM_COMMAND) 
   {
      if(LOWORD(wParam) == IDC_OK)
      {
         WritePrivateProfileString(SettingsHeading, "nullframes", (IsDlgButtonChecked(hwndDlg, IDC_NULLFRAMES) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString(SettingsHeading, "deltaframes", (IsDlgButtonChecked(hwndDlg, IDC_DELTAFRAMES) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString(SettingsHeading, "multithreading", (IsDlgButtonChecked(hwndDlg, IDC_MULTI) == BST_CHECKED) ? "1" : NULL, SettingsFile);

         int format = YUY2;

         if(IsDlgButtonChecked(hwndDlg, IDC_YV12) == BST_CHECKED)
            format = YV12;

         char buffer[11];
         _itoa_s(format, buffer, 11, 10);
         WritePrivateProfileString(SettingsHeading, "format", buffer, SettingsFile);

         EndDialog(hwndDlg, 0);
      } 
      else if(LOWORD(wParam) == IDC_CANCEL)
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

CodecInst::CodecInst()
{
   m_compressWorker.m_buffer = NULL;
   m_buffer = NULL;
   m_prevFrame = NULL;
   m_deltaBuffer = NULL;
   m_buffer2 = NULL;
   m_colorTransBuffer = NULL;
   m_length = 0;
   m_nullframes = false;
   m_deltaframes = false;
   m_compressFormat = 0;
   m_started = false;

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

BOOL CodecInst::QueryConfigure() 
{
   return TRUE; 
}

DWORD CodecInst::Configure(HWND hwnd) 
{
   DialogBox(hmoduleYeti, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), hwnd, (DLGPROC)ConfigureDialogProc);
   return ICERR_OK;
}

DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize)
{
   return 0;
}

DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) 
{
   return 0;
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

   m_compressFormat = GetPrivateProfileInt(SettingsHeading, "format", YUY2, SettingsFile);

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

   detectFlags(&SSE2, &SSE);
   if(!SSE)
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
      return sizeof(BITMAPINFOHEADER) + sizeof(UINT32);	
   }

   // make sure the input is an acceptable format
   if(CompressQuery(lpbiIn, NULL) == ICERR_BADFORMAT)
   {
      RETURN_ERROR();
   }

   m_compressFormat = GetPrivateProfileInt(SettingsHeading, "format", YUY2, SettingsFile);

   *lpbiOut = *lpbiIn;
   lpbiOut->biSize = sizeof(BITMAPINFOHEADER) + sizeof(UINT32);
   lpbiOut->biPlanes = 1;
   lpbiOut->biCompression = FOURCC_YETI;

   if(lpbiIn->biBitCount != RGB24)
   {
      lpbiOut->biSizeImage = EIGHTH(lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount);
      lpbiOut->biSizeImage = EIGHTH(lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount);
   } 
   else 
   {
      lpbiOut->biSizeImage = ALIGN_ROUND(EIGHTH(lpbiIn->biWidth * lpbiIn->biBitCount), 4) * lpbiIn->biHeight;
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

   detectFlags(&SSE2, &SSE);
   if(!SSE)
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

   if ( lpbiIn->biBitCount == 16 )
   {
      lpbiOut->biBitCount = 16;
      lpbiOut->biCompression = FOURCC_YUY2;
      lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * 2;
   }
   else
   {
      lpbiOut->biBitCount = 12;
      lpbiOut->biCompression = FOURCC_YV12;
      lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight + HALF(lpbiIn->biWidth * lpbiIn->biHeight);
   }

   return (DWORD)ICERR_OK;
}

DWORD CodecInst::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   RETURN_ERROR()
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

void __stdcall detectFlags(bool* SSE2, bool* SSE)
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