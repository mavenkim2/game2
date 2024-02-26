// global HINSTANCE OS_W32_HINSTANCE;
// global bool SHOW_CURSOR;
// global bool RUNNING = true;

internal void Printf(char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    char printBuffer[1024];
    stbsp_vsprintf(printBuffer, fmt, va);
    va_end(va);
    OutputDebugStringA(printBuffer);
}

// NOTE: c string
internal u64 OS_GetLastWriteTime(string filename)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    u64 timeStamp = 0;
    if (GetFileAttributesExA((char *)(filename.str), GetFileExInfoStandard, &data))
    {
        timeStamp = ((u64)data.ftLastWriteTime.dwHighDateTime) | (data.ftLastWriteTime.dwLowDateTime);
    }
    return timeStamp;
}

internal string ReadEntireFile(string filename)
{
    string output     = {};
    HANDLE fileHandle = CreateFileA((char *)filename.str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSizeLargeInteger;
        if (GetFileSizeEx(fileHandle, &fileSizeLargeInteger))
        {
            u32 fileSize = (u32)fileSizeLargeInteger.QuadPart;
            output.str   = (u8 *)VirtualAlloc(NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (output.str)
            {
                DWORD bytesToRead;
                if (ReadFile(fileHandle, output.str, fileSize, &bytesToRead, NULL))
                {
                    output.size = fileSize;
                }
                else
                {
                    OS_Release(&output.str);
                    output.str = 0;
                }
            }
            else
            {
                Printf("Failed to commit memory for file\n");
            }
        }
        else
        {
        }
        CloseHandle(fileHandle);
    }
    else
    {
        Printf("Invalid file handle?\n");
    }
    return output;
}

internal b32 WriteFile(string filename, void *fileMemory, u32 fileSize)
{
    b32 result = false;
    HANDLE fileHandle =
        CreateFileA((char *)filename.str, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesToWrite;
        if (WriteFile(fileHandle, fileMemory, fileSize, &bytesToWrite, NULL))
        {
            result = (fileSize == bytesToWrite);
        }
        CloseHandle(fileHandle);
    }
    return result;
}

internal u64 OS_PageSize()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
}

internal void *OS_Alloc(u64 size)
{
    u64 pageSnappedSize = size;
    pageSnappedSize += OS_PageSize() - 1;
    pageSnappedSize -= pageSnappedSize % OS_PageSize();
    void *ptr = VirtualAlloc(0, pageSnappedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return ptr;
}

internal void OS_Release(void *memory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

//////////////////////////////
// Threads
//
global Arena *Win32_arena;
global Win32_Sync *Win32_freeSync;

internal Win32_Sync *Win32_SyncAlloc(Win32_SyncType type)
{
    Win32_Sync *sync = Win32_freeSync;
    if (sync)
    {
        StackPop(Win32_freeSync);
    }
    else
    {
        sync = PushStruct(Win32_arena, Win32_Sync);
    }
    Assert(sync != 0);
    sync->type = type;
    return sync;
}

internal void Win32_SyncFree(Win32_Sync *sync)
{
    sync->type = Win32_SyncType_Null;
    StackPush(Win32_freeSync, sync);
}

internal OS_Handle OS_ThreadStart(OS_ThreadFunction *func, void *ptr)
{
    // TODO Hack: Have proper OS initialization

    if (Win32_arena == 0)
    {
        Win32_arena = ArenaAllocDefault();
    }

    Win32_Sync *thread    = Win32_SyncAlloc(Win32_SyncType_Thread);
    thread->thread.func   = func;
    thread->thread.ptr    = ptr;
    thread->thread.handle = CreateThread(0, 0, Win32_ThreadProc, thread, 0, 0);
    OS_Handle handle      = {(u64)thread};
    return handle;
}

internal DWORD Win32_ThreadProc(void *p)
{
    Win32_Sync *thread      = (Win32_Sync *)p;
    OS_ThreadFunction *func = thread->thread.func;
    void *ptr               = thread->thread.ptr;

    ThreadContext tContext_ = {};
    ThreadContextInitialize(&tContext_);
    func(ptr);
    ThreadContextRelease();

    return 0;
}

//////////////////////////////
// Semaphores
//
internal OS_Handle OS_CreateSemaphore(u32 maxCount)
{
    HANDLE handle    = CreateSemaphoreEx(0, 0, maxCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    OS_Handle result = {(u64)handle};
    return result;
}

internal void OS_ReleaseSemaphore(OS_Handle input)
{
    HANDLE handle = (HANDLE *)input.handle;
    ReleaseSemaphore(handle, 1, 0);
}

internal void OS_ReleaseSemaphore(OS_Handle input, u32 count)
{
    HANDLE handle = (HANDLE *)input.handle;
    ReleaseSemaphore(handle, count, 0);
}

//////////////////////////////
// Mutexes
//
internal OS_Handle OS_CreateMutex()
{
    Win32_Sync *mutex = Win32_SyncAlloc(Win32_SyncType_Mutex);
    InitializeCriticalSection(&mutex->mutex);
    OS_Handle handle = {(u64)mutex};
    return handle;
}

internal void OS_DeleteMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    DeleteCriticalSection(&mutex->mutex);
    Win32_SyncFree(mutex);
}

internal void OS_TakeMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    EnterCriticalSection(&mutex->mutex);
}

internal void OS_DropMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    LeaveCriticalSection(&mutex->mutex);
}

internal OS_Handle OS_CreateRWMutex()
{
    Win32_Sync *mutex = Win32_SyncAlloc(Win32_SyncType_RWMutex);
    InitializeSRWLock(&mutex->rwMutex);
    OS_Handle handle = {(u64)mutex};
    return handle;
}

//////////////////////////////
// Read/Write Mutex
//
internal void OS_DeleteRWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    Win32_SyncFree(mutex);
}

internal void OS_TakeRMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    AcquireSRWLockShared(&mutex->rwMutex);
}

internal void OS_DropRMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    ReleaseSRWLockShared(&mutex->rwMutex);
}

internal void OS_TakeWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    AcquireSRWLockExclusive(&mutex->rwMutex);
}

internal void OS_DropWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    ReleaseSRWLockExclusive(&mutex->rwMutex);
}

//////////////////////////////
// Condition Variables
//
internal OS_Handle OS_CreateConditionVariable()
{
    Win32_Sync *cvar = Win32_SyncAlloc(Win32_SyncType_CVar);
    InitializeConditionVariable(&cvar->cv);
    OS_Handle handle = {(u64)cvar};
    return handle;
}
internal void OS_DeleteConditionVariable(OS_Handle input)
{
    Win32_Sync *cvar = (Win32_Sync *)input.handle;
    Win32_SyncFree(cvar);
}
// TODO: be able to input a wait time?
internal b32 OS_WaitConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result            = SleepConditionVariableCS(&cvar->cv, &mutex->mutex, INFINITE);
    return result;
}

internal b32 OS_WaitRConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result = SleepConditionVariableSRW(&cvar->cv, &mutex->rwMutex, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
    return result;
}

internal b32 OS_WaitRWConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result            = SleepConditionVariableSRW(&cvar->cv, &mutex->rwMutex, INFINITE, 0);
    return result;
}

internal void OS_SignalConditionVariable(OS_Handle cv)
{
    Win32_Sync *cvar = (Win32_Sync *)cv.handle;
    WakeConditionVariable(&cvar->cv);
}

internal void OS_BroadcastConditionVariable(OS_Handle cv)
{
    Win32_Sync *cvar = (Win32_Sync *)cv.handle;
    WakeAllConditionVariable(&cvar->cv);
}

//////////////////////////////
// Signals
//

// NOTE: this sets the event to auto-reset after a successful wait.
internal OS_Handle OS_CreateSignal()
{
    HANDLE handle    = CreateEvent(0, 0, 0, 0);
    OS_Handle result = {(u64)handle};
    return result;
}

internal b32 OS_SignalWait(OS_Handle input, u32 time = U32Max)
{
    HANDLE handle = (HANDLE *)input.handle;
    DWORD result  = WaitForSingleObject(handle, time);
    Assert(result == WAIT_OBJECT_0 || (time != U32Max && result == WAIT_TIMEOUT));
    return (result == WAIT_OBJECT_0);
}

internal void OS_RaiseSignal(OS_Handle input)
{
    HANDLE handle = (HANDLE *)input.handle;
    SetEvent(handle);
}

//
// System Info
//

internal u32 OS_NumProcessors()
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}
