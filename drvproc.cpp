// From Ben Rudiak-Gould's huffyuv

#include "yeti.h"

/***************************************************************************
* DriverProc  -  The entry point for an installable driver.
*
* PARAMETERS
* dwDriverId:  For most messages, <dwDriverId> is the DWORD
*     value that the driver returns in response to a <DRV_OPEN> message.
*     Each time that the driver is opened, through the <DrvOpen> API,
*     the driver receives a <DRV_OPEN> message and can return an
*     arbitrary, non-zero value. The installable driver interface
*     saves this value and returns a unique driver handle to the
*     application. Whenever the application sends a message to the
*     driver using the driver handle, the interface routes the message
*     to this entry point and passes the corresponding <dwDriverId>.
*     This mechanism allows the driver to use the same or different
*     identifiers for multiple opens but ensures that driver handles
*     are unique at the application interface layer.
*
*     The following messages are not related to a particular open
*     instance of the driver. For these messages, the dwDriverId
*     will always be zero.
*
*         DRV_LOAD, DRV_FREE, DRV_ENABLE, DRV_DISABLE, DRV_OPEN
*
* hDriver: This is the handle returned to the application by the
*    driver interface.
*
* uiMessage: The requested action to be performed. Message
*     values below <DRV_RESERVED> are used for globally defined messages.
*     Message values from <DRV_RESERVED> to <DRV_USER> are used for
*     defined driver protocols. Messages above <DRV_USER> are used
*     for driver specific messages.
*
* lParam1: Data for this message.  Defined separately for
*     each message
*
* lParam2: Data for this message.  Defined separately for
*     each message
*
* RETURNS
*   Defined separately for each message.
*
***************************************************************************/
LRESULT WINAPI DriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) 
{
   CodecInst* pCodecInst = (CodecInst*)(UINT)dwDriverID;

   switch (uiMessage) 
   {
   case DRV_LOAD:
         return (LRESULT)1L;
   case DRV_FREE:
         return (LRESULT)1L;
   case DRV_OPEN:
         return (LRESULT)(DWORD)(UINT) Open((ICOPEN*) lParam2);
   case DRV_CLOSE:
      {
         if (pCodecInst) 
         {
            Close(pCodecInst);
         }
         return (LRESULT)1L;
      }

   /*********************************************************************

   compression messages

   *********************************************************************/

   case ICM_COMPRESS_QUERY:
         return pCodecInst->CompressQuery((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_COMPRESS_BEGIN:
         return pCodecInst->CompressBegin((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_COMPRESS_GET_FORMAT:
         return pCodecInst->CompressGetFormat((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_COMPRESS_GET_SIZE:
         return pCodecInst->CompressGetSize((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_COMPRESS:
         return pCodecInst->Compress((ICCOMPRESS*)lParam1, (DWORD)lParam2);               
   case ICM_COMPRESS_END:
         return pCodecInst->CompressEnd();

   /*********************************************************************

   decompress messages

   *********************************************************************/

   case ICM_DECOMPRESS_QUERY:
         return pCodecInst->DecompressQuery((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_DECOMPRESS_BEGIN:
         return  pCodecInst->DecompressBegin((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_DECOMPRESS_GET_FORMAT:
         return pCodecInst->DecompressGetFormat((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_DECOMPRESS_GET_PALETTE:
         return pCodecInst->DecompressGetPalette((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
   case ICM_DECOMPRESS:
         return pCodecInst->Decompress((ICDECOMPRESS*)lParam1, (DWORD)lParam2);
   case ICM_DECOMPRESS_END:
         return pCodecInst->DecompressEnd();

   /*********************************************************************

   state messages

   *********************************************************************/

   case DRV_QUERYCONFIGURE:
         return (LRESULT)1L;
   case DRV_CONFIGURE:
      {
         pCodecInst->Configure((HWND)lParam1);
         return DRV_OK;
      }
   case ICM_CONFIGURE:
         return (lParam1 == -1) ? ICERR_OK : pCodecInst->Configure((HWND)lParam1);
   case ICM_ABOUT:
         return ICERR_UNSUPPORTED;
   case ICM_GETSTATE:
         return pCodecInst->GetState((LPVOID)lParam1, (DWORD)lParam2);
   case ICM_SETSTATE:
         return pCodecInst->SetState((LPVOID)lParam1, (DWORD)lParam2);
   case ICM_GETINFO:
         return pCodecInst->GetInfo((ICINFO*)lParam1, (DWORD)lParam2);
   case ICM_GETDEFAULTQUALITY: 
      {
         if (lParam1) 
         {
            *((LPDWORD)lParam1) = ICQUALITY_HIGH;
            return ICERR_OK;
         }
         break;
      }

   /*********************************************************************

   standard driver messages

   *********************************************************************/

   case DRV_DISABLE:
   case DRV_ENABLE:
         return (LRESULT)1L;

   case DRV_INSTALL:
   case DRV_REMOVE:
         return (LRESULT)DRV_OK;
   }

#ifndef PROFILER
   if (uiMessage < DRV_USER)
   {
      return DefDriverProc(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
   }
   else
   {
      return ICERR_UNSUPPORTED;
#else
      return ICERR_UNSUPPORTED;
#endif
   }
}

void WINAPI ShowConfiguration(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
   CodecInst codecInst;
   codecInst.Configure(hwnd);
}