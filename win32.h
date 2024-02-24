#include <windows.h>

struct Win32_Thread
{
    OS_ThreadFunction *func;
    void *ptr;
    DWORD id;
    HANDLE handle;

    Win32_Thread* next;
};

///////////////////////////////////////
// Handles
//
internal OS_Handle Win32_CreateOSHandle(HANDLE input);
internal HANDLE Win32_GetHandleFromOSHandle(OS_Handle input);

///////////////////////////////////////
// Threads
//
internal Win32_Thread *Win32_ThreadAlloc();
internal DWORD Win32_ThreadProc(void *ptr);
