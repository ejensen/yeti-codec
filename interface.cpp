#ifndef _YETI_GUI
#define _YETI_GUI

#include "yeti.h"
#include <commctrl.h>
#include <shellapi.h>
#include <Windowsx.h>

#define return_error() return ICERR_BADFORMAT;//{ char msg[256];sprintf(msg,"Returning error on line %d", __LINE__);MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION); return ICERR_BADFORMAT; }

CodecInst::CodecInst()
{
#ifdef _DEBUG
   if ( _started == 0x1337)
   {
      char msg[128];
      sprintf_s(msg,128,"Attempting to instantiate a codec instance that has not been destroyed");
      MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
   }
#endif

   _pBuffer=NULL;
   _pPrev=NULL;
   _pDelta=NULL;
   _pBuffer2=NULL;
   _pLossy_buffer=NULL;
   _length=0;
   _nullframes=false;
   _reduced=false;
   _started=0;
   _SSE2=0;
   _SSE=0;
   _MMX=0;
   _cObj._pProbRanges=NULL;
   _cObj._pBytecounts=NULL;
   _cObj._pBuffer=NULL;

   _multithreading=0;
   _info_a.source=NULL;
   _info_a.dest=NULL;
   _info_a.size=0;
   _info_a.length=0;
   _info_a.thread=0;
   _info_b=_info_a;
   _info_c=_info_a;

   memcpy((void*)&_info_a.cObj,&_cObj,sizeof(_cObj));
   memcpy((void*)&_info_b.cObj,&_cObj,sizeof(_cObj));
   memcpy((void*)&_info_c.cObj,&_cObj,sizeof(_cObj));
}

HMODULE hmoduleHuffYUY=0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID) 
{
   hmoduleHuffYUY = (HMODULE) hinstDLL;
   return TRUE;
}

HWND CreateTooltip(HWND hwnd)
{
   // initialize common controls
   INITCOMMONCONTROLSEX	iccex;		// struct specifying control classes to register
   iccex.dwICC		= ICC_WIN95_CLASSES;
   iccex.dwSize	= sizeof(INITCOMMONCONTROLSEX);
   InitCommonControlsEx(&iccex);

#ifdef X64_BUILD
   HINSTANCE	ghThisInstance=(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
#else 
   HINSTANCE	ghThisInstance=(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE);
#endif
   HWND		hwndTT;					// handle to the tooltip control

   // create a tooltip window
   hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      hwnd, NULL, ghThisInstance, NULL);

   SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

   // set some timeouts so tooltips appear fast and stay long (32767 seems to be a limit here)
   SendMessage(hwndTT, TTM_SETDELAYTIME, (WPARAM)(DWORD)TTDT_INITIAL, (LPARAM)500);
   SendMessage(hwndTT, TTM_SETDELAYTIME, (WPARAM)(DWORD)TTDT_AUTOPOP, (LPARAM)30*1000);

   return hwndTT;
}

struct { UINT item; UINT tip; } item2tip[] = 
{
   { IDC_NULLFRAMES,	IDS_TIP_NULLFRAMES },
   { IDC_MULTI,		IDS_TIP_MULTI	 }, 
   { IDC_REDUCED,	   IDS_TIP_REDUCED },
   { 0,0 }
};

int AddTooltip(HWND tooltip, HWND client, UINT stringid)
{
#ifdef X64_BUILD
   HINSTANCE ghThisInstance=(HINSTANCE)GetWindowLongPtr(client,GWLP_HINSTANCE);
#else
   HINSTANCE ghThisInstance=(HINSTANCE)GetWindowLong(client, GWL_HINSTANCE);
#endif

   TOOLINFO				ti;			// struct specifying info about tool in tooltip control
   static unsigned int		uid	= 0;	// for ti initialization
   RECT					rect;		// for client area coordinates
   TCHAR					buf[2000];	// a static buffer is sufficient, TTM_ADDTOOL seems to copy it

   // load the string manually, passing the id directly to TTM_ADDTOOL truncates the message :(
   if ( !LoadString(ghThisInstance, stringid, buf, 2000) ) return -1;

   // get coordinates of the main client area
   GetClientRect(client, &rect);

   // initialize members of the toolinfo structure
   ti.cbSize		= sizeof(TOOLINFO);
   ti.uFlags		= TTF_SUBCLASS;
   ti.hwnd			= client;
   ti.hinst		= ghThisInstance;		// not necessary if lpszText is not a resource id
   ti.uId			= uid;
   ti.lpszText		= buf;

   // Tooltip control will cover the whole window
   ti.rect.left	= rect.left;    
   ti.rect.top		= rect.top;
   ti.rect.right	= rect.right;
   ti.rect.bottom	= rect.bottom;

   // send a addtool message to the tooltip control window
   SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);	
   return uid++;
}

static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == WM_INITDIALOG)
   {
      CheckDlgButton(hwndDlg, IDC_NULLFRAMES,GetPrivateProfileInt("settings", "nullframes", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_MULTI,GetPrivateProfileInt("settings", "multithreading", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwndDlg, IDC_REDUCED,GetPrivateProfileInt("settings", "reduced", false, SettingsFile) ? BST_CHECKED : BST_UNCHECKED);
      HWND hwndTip = CreateTooltip(hwndDlg);
      for (int l=0; item2tip[l].item; l++ )
      {
         AddTooltip(hwndTip, GetDlgItem(hwndDlg, item2tip[l].item),	item2tip[l].tip);
      }
      SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, (LPARAM)(INT)350);	//TODO: Change
   } 
   else if (uMsg == WM_COMMAND) 
   {
      if (LOWORD(wParam)==IDC_OK)
      {
         WritePrivateProfileString("settings", "nullframes",(IsDlgButtonChecked(hwndDlg, IDC_NULLFRAMES) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString("settings", "multithreading",(IsDlgButtonChecked(hwndDlg, IDC_MULTI) == BST_CHECKED) ? "1" : NULL, SettingsFile);
         WritePrivateProfileString("settings", "reduced",(IsDlgButtonChecked(hwndDlg, IDC_REDUCED) == BST_CHECKED) ? "1" : NULL, SettingsFile);

         EndDialog(hwndDlg, 0);
      } 
      else if ( LOWORD(wParam)==IDC_CANCEL )
      {
         EndDialog(hwndDlg, 0);
      } 
   } 
   else if ( uMsg == WM_CLOSE )
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
   DialogBox(hmoduleHuffYUY, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, (DLGPROC)ConfigureDialogProc);
   return ICERR_OK;
}

CodecInst* Open(ICOPEN* icinfo) 
{
   if (icinfo && icinfo->fccType != ICTYPE_VIDEO)
   {
      return NULL;
   }

   CodecInst* pinst = new CodecInst();

   if (icinfo) 
   {
      icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;
   }

   return pinst;
}

CodecInst::~CodecInst()
{
   try
   {
      if ( _started == 0x1337 )
      {
         if (_pBuffer2)
         {
            CompressEnd();
         } 
         else
         {
            DecompressEnd();
         }
      }
      _started =0;
   } 
   catch ( ... ) {};
}

DWORD Close(CodecInst* pinst) 
{
   try 
   {
      if ( pinst && !IsBadWritePtr(pinst,sizeof(CodecInst)) )
      {
         delete pinst;
      }
   } 
   catch ( ... ){};
   return 1;
}

DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize)
{
   if ( pv == NULL )
   {
      return 3*sizeof(int);
   } 
   else if ( dwSize < 3*sizeof(int) )
   {
      return ICERR_BADSIZE;
   }

   int * state = (int*)pv;
   state[0]=GetPrivateProfileInt("settings", "nullframes", false, SettingsFile);
   state[1]=GetPrivateProfileInt("settings", "multithreading", false, SettingsFile);
   state[2]=GetPrivateProfileInt("settings", "reduced", false, SettingsFile);
   return 0;
}

DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) 
{
   if ( dwSize < 3*sizeof(int))
   {
      return 3*sizeof(int);
   }
   int * state = (int*)pv;
   char str[] = {0,0,0,0};
   str[0]='0'+state[0];
   WritePrivateProfileString("settings", "nullframes",str, SettingsFile);
   str[0]='0'+state[1];
   WritePrivateProfileString("settings", "multithreading",str, SettingsFile);
   str[0]='0'+state[2];
   WritePrivateProfileString("settings", "reduced",str, SettingsFile);
   return 3*sizeof(int);
}

// test for MMX, SSE, and SSE2 support

void __stdcall detectFlags( int * SSE2, int * SSE, int * MMX)
{
   int flags;
   __asm{
      mov			eax,1
         cpuid
         mov			flags,edx
   }
   if ( flags & (1 << 23) )
   {
      *MMX = true;
      if ( flags & (1 << 25) )
      {
         *SSE = true;
         if ( flags & (1 << 26) )
         {
            *SSE2 = true;
         }
      }
   }
}

// check if the codec can compress the given format to the desired format
DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{

   // check for valid format and bitdepth
   if ( lpbiIn->biCompression == 0) 
   {
      if ( lpbiIn->biBitCount != 24 && lpbiIn->biBitCount != 32)
      {
         return_error()
      }
   } 
   else if ( lpbiIn->biCompression == FOURCC_YUY2 || lpbiIn->biCompression == FOURCC_UYVY )
   {
      if ( lpbiIn->biBitCount != 16 ) 
      {
         return_error()
      }
   }
   else if ( lpbiIn->biCompression == FOURCC_YV12 )
   {
      if ( lpbiIn->biBitCount != 12 )
      {
         return_error()
      }
   } 
   else 
   {
      /*char msg[128];
      int x = lpbiIn->biCompression;
      char * y = (char*)&x;
      sprintf(msg,"Unknown format, %c%c%c%c",y[0],y[1],y[2],y[3]);
      MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);*/
      return_error()
   }

   if ( lpbiIn->biCompression == FOURCC_UYVY ) // down sampling UYUV is not supported
   { 
      return_error()
   }

   // Make sure width is mod 4 for YUV formats
   if ( lpbiIn->biWidth % 4 )
   {
      return_error()
   }

   // Make sure the height is acceptable for YV12 formats
   if ( lpbiIn->biHeight % 2)
   {
      return_error();
   }

   // See if the output format is acceptable if need be
   if ( lpbiOut )
   {
      if ( lpbiOut->biSize < sizeof(BITMAPINFOHEADER) )
         return_error()
         if ( lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 32 && lpbiOut->biBitCount != 16 && lpbiOut->biBitCount != 12 )
            return_error()
            if ( lpbiIn->biHeight != lpbiOut->biHeight )
               return_error()
               if ( lpbiIn->biWidth != lpbiOut->biWidth )
                  return_error()
                  if ( lpbiOut->biCompression != FOURCC_YETI )
                     return_error()
   }

   detectFlags(&_SSE2, &_SSE, &_MMX);
   if ( !_MMX )
   {
      return_error()
   }

   return (DWORD)ICERR_OK;
}

// return the intended compress format for the given input format
DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if ( !lpbiOut)
   {
      return sizeof(BITMAPINFOHEADER)+sizeof(UINT32);	
   }

   // make sure the input is an acceptable format
   if ( CompressQuery(lpbiIn, NULL) == ICERR_BADFORMAT )
   {
      return_error()
   }

   *lpbiOut = *lpbiIn;
   lpbiOut->biSize = sizeof(BITMAPINFOHEADER)+sizeof(UINT32);
   lpbiOut->biPlanes = 1;
   lpbiOut->biCompression = FOURCC_YETI;

   if ( lpbiIn->biBitCount != 24 ) // TODO: optimize
   {
      lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * lpbiIn->biBitCount/8;
   } 
   else 
   {
      lpbiOut->biSizeImage = align_round(lpbiIn->biWidth * lpbiIn->biBitCount/8,4)* lpbiIn->biHeight;
   }

   lpbiOut->biBitCount = lpbiIn->biBitCount;

   *(UINT32*)(&lpbiOut[1])= _reduced ? REDUCED_RES : ARITH_YV12; //Change?
   return (DWORD)ICERR_OK;
}

// return information about the codec

DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize)
{
   if (icinfo == NULL)
   {
      return sizeof(ICINFO);
   }

   if (dwSize < sizeof(ICINFO))
   {
      return 0;
   }

   icinfo->dwSize       = sizeof(ICINFO);
   icinfo->fccType      = ICTYPE_VIDEO;
   icinfo->fccHandler	= FOURCC_YETI;
   icinfo->dwFlags		= VIDCF_FASTTEMPORALC; //VIDCF_TEMPORAL;
   icinfo->dwVersion		= 0x00010000;
   icinfo->dwVersionICM	= ICVERSION;
   memcpy(icinfo->szName, L"Yeti", sizeof(L"Yeti"));
   memcpy(icinfo->szDescription, L"Yeti lossless codec", sizeof(L"Yeti lossless codec"));

   return sizeof(ICINFO);
}

// check if the codec can decompress the given format to the desired format
DWORD CodecInst::DecompressQuery(const LPBITMAPINFOHEADER lpbiIn, const LPBITMAPINFOHEADER lpbiOut)
{
   if ( lpbiIn->biCompression != FOURCC_YETI )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   if ( lpbiIn->biBitCount == 16 || lpbiIn->biBitCount == 12 )
   {
      if ( lpbiIn->biWidth%4 )
      {
         return (DWORD)ICERR_BADFORMAT;
      }
      if ( lpbiIn->biBitCount == 12 && lpbiIn->biHeight%2 )
      {
         return (DWORD)ICERR_BADFORMAT;
      }
   }

   // make sure the input bitdepth is valid
   if ( lpbiIn->biBitCount != 24 && lpbiIn->biBitCount != 32 && lpbiIn->biBitCount != 16 && lpbiIn->biBitCount != 12 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   detectFlags(&_SSE2, &_SSE, &_MMX);
   if ( !_MMX )
   {
      return_error()
   }

   if ( !lpbiOut )
   {
      return (DWORD)ICERR_OK;
   }

   //char msg[128];
   //char fcc[4];
   //*(unsigned int*)(&fcc[0])=lpbiOut->biCompression;
   //sprintf(msg,"Format = %d, BiComp= %c%c%c%c",lpbiOut->biBitCount,fcc[3],fcc[2],fcc[1],fcc[0] );
   //MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);

   // make sure the output format is one that can be decoded to
   if ( lpbiOut->biCompression != 0 && lpbiOut->biCompression != FOURCC_YV12 && lpbiOut->biCompression != FOURCC_YUY2 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   // make sure the output bitdepth is valid
   if ( lpbiOut->biBitCount != 32 && lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 16 && lpbiOut->biBitCount != 12 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }
   // make sure that no down sampling is being performed
   if ( lpbiOut->biBitCount < 12 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   // check for invalid widths/heights
   if ( lpbiIn->biWidth % 4 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }
   if ( lpbiIn->biHeight % 2 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   if ( lpbiOut->biCompression == 0 && lpbiOut->biBitCount != 24 && lpbiOut->biBitCount != 32)
   {
      return (DWORD)ICERR_BADFORMAT;
   }
   if ( lpbiOut->biCompression == FOURCC_YUY2 && lpbiOut->biBitCount != 16 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }
   if ( lpbiOut->biCompression == FOURCC_YV12 && lpbiOut->biBitCount != 12 )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   if ( lpbiIn->biHeight != lpbiOut->biHeight )
   {
      return (DWORD)ICERR_BADFORMAT;
   }
   if ( lpbiIn->biWidth != lpbiOut->biWidth )
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   return (DWORD)ICERR_OK;
}

// return the default decompress format for the given input format 
DWORD CodecInst::DecompressGetFormat(const LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut)
{
   if ( DecompressQuery(lpbiIn, NULL ) != ICERR_OK)
   {
      return (DWORD)ICERR_BADFORMAT;
   }

   if ( !lpbiOut)
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
   return_error()
}

#endif