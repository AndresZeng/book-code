/*--------------------------------------------------
      SHARE.C
      A library that dynamically allocates a single
      global buffer for all its clients to share.
  --------------------------------------------------*/

#include <windows.h>
#include <stdio.h>
#include "share.h"

#define BUFFER_SIZE           0x100000

PVOID SharedAlloc(DWORD dwBytes);
void SharedFree(PVOID pView);

/* shared variable defined in PUBLIC.C */
extern DWORD dwOffset;        /* position in buffer */

PSTR pBase;                   /* bottom of buffer */
HANDLE hFileMapping;          /* shared memory */
HANDLE hmxMappedFile;         /* protects buffer */

/* thread-local storage for client thread IDs */
__declspec(thread) DWORD dwThreadID;

/*--------------------------------------------------
      DLL MAIN
  --------------------------------------------------*/

BOOL WINAPI DllMain(HINSTANCE hinstDLL, 
   DWORD dwReason, LPVOID lpReserved)     
{
   switch (dwReason)
   {
      case DLL_PROCESS_ATTACH:
         /* get a pointer to the shared buffer */
         pBase = (PSTR)SharedAlloc(BUFFER_SIZE);
         if (!pBase) {
            return(FALSE);
         }
         /* fall through for primary thread */

      case DLL_THREAD_ATTACH:
         dwThreadID = GetCurrentThreadId( );
         break;
         
      case DLL_PROCESS_DETACH:
         SharedFree(pBase);
         break;
   }
   return(TRUE);
}

/*--------------------------------------------------
      ADD STRING
      Determine the next empty byte in the buffer
      and copy a string there. 
  --------------------------------------------------*/

BOOL AddString(PSTR pInput)
{
   PSTR pNext;       /* next empty space in buffer */
   char szDataString[256];    /* thread ID + input */
   UINT uLength;     /* byte count of szDataString */

   /* merge thread ID and data into one string */
   wsprintf(szDataString, "[0x%08lX] %s",
      dwThreadID, pInput);
   uLength = lstrlen(szDataString) + 1;      
      
   /* Wait in line to use the buffer. */
   WaitForSingleObject(hmxMappedFile, INFINITE);
   
   /* If the buffer is too small, stop. */
   if ((dwOffset + uLength) > BUFFER_SIZE) {
      ReleaseMutex(hmxMappedFile);
      return(FALSE);
   }
   
   /* Copy the string into the buffer. */
   pNext = pBase + dwOffset;
   lstrcpy(pNext, szDataString);
   
   /* Make dwOffset point to the next empty space. */
   dwOffset += uLength;
   
   ReleaseMutex(hmxMappedFile);
   return( TRUE );
}

/*--------------------------------------------------
      SHOW ALL STRINGS
      Dump all the strings in the buffer to stdout.
  --------------------------------------------------*/

void ShowAllStrings(void)
{
   PSTR pCurrent = pBase;
   PSTR pEnd = pBase + dwOffset;

   while (pCurrent < pEnd) {
      puts(pCurrent);
      pCurrent += lstrlen(pCurrent) + 1;
   }
}

/*--------------------------------------------------
      SHARED ALLOCATE
      Allocate a block of memory to share among
      several processes.
  --------------------------------------------------*/

PVOID SharedAlloc(DWORD dwBytes)
{
   PVOID pView;

   /* Create the file mapping object.  If it already */
   /* exists, this still returns a handle to it. */
   hFileMapping =
         CreateFileMapping((HANDLE)0xFFFFFFFF,
         (LPSECURITY_ATTRIBUTES)NULL, PAGE_READWRITE,
         0, dwBytes, "shared_buffer");

   /* Map the file object into the client's address */
   /* space and receive a pointer to the buffer. */
   pView = MapViewOfFile(hFileMapping,
      FILE_MAP_WRITE, 0, 0, dwBytes);

   /* Make a mutex to serialize buffer operations. */
   hmxMappedFile = CreateMutex(
      (LPSECURITY_ATTRIBUTES)NULL,
      FALSE, "buffer_mutex");

   return(pView);
}

/*--------------------------------------------------
      SHARED FREE
      Release a block of shared memory.
  --------------------------------------------------*/

void SharedFree(PVOID pView)
{
   UnmapViewOfFile(pView);
   CloseHandle(hFileMapping);
   CloseHandle(hmxMappedFile);
}
