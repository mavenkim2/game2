#ifndef WIN32_H
#define WIN32_H

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

struct Win32_FileIter
{
    HANDLE handle;
    WIN32_FIND_DATAA findData;
};

struct Win32_State
{
    Arena *mArena;
    Win32_Sync *mFreeSync;
    i64 mPerformanceFrequency;
    b32 mGranularSleep;

    string mBinaryDirectory;

    HINSTANCE mHInstance;

    b32 mShowCursor;

    Arena *mTempEventArena;
    OS_EventList mEventsList;

    TicketMutex mMutex;

    u64 mStartCounter;

    WINDOWPLACEMENT mWindowPosition = {sizeof(WINDOWPLACEMENT)};
};

void Win32_AddEvent(OS_Event event);
OS_Event Win32_CreateKeyEvent(OS_Key key, b32 isDown);
Win32_Sync *Win32_SyncAlloc(Win32_SyncType type);
void Win32_SyncFree(Win32_Sync *sync);

//////////////////////////////
// File information
//
internal u64 Win32DenseTimeFromSystemtime(SYSTEMTIME *sysTime);
internal u64 Win32DenseTimeFromFileTime(FILETIME *filetime);

#endif
