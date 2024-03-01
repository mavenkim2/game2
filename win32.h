#include <windows.h>

enum Win32_SyncType
{
    Win32_SyncType_Null,
    Win32_SyncType_Thread,
    Win32_SyncType_Mutex,
    Win32_SyncType_RWMutex,
    Win32_SyncType_CVar,
    Win32_SyncType_Count,
};
struct Win32_Sync
{
    Win32_SyncType type;
    union
    {
        CRITICAL_SECTION mutex;
        struct
        {
            OS_ThreadFunction *func;
            void *ptr;
            DWORD id;
            HANDLE handle;
        } thread;
        SRWLOCK rwMutex;
        CONDITION_VARIABLE cv;
    };

    Win32_Sync *next;
};

///////////////////////////////////////
// Threads
//
internal Win32_Sync *Win32_SyncAlloc(Win32_SyncType type);
internal void Win32_SyncFree(Win32_Sync *sync);
internal DWORD Win32_ThreadProc(void *ptr);

//////////////////////////////
// File information
//
internal u64 Win32DenseTimeFromSystemtime(SYSTEMTIME *sysTime);
internal u64 Win32DenseTimeFromFileTime(FILETIME *filetime);
