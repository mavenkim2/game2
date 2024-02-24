// NOTE: I'm too lazy to put all the platform stuff in one file, so I'm just going to put
// platform specific stuff that the game calls into this file.
// TODO: header file with all deez

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

//
// Handles
//
internal OS_Handle Win32_CreateOSHandle(HANDLE input)
{
    OS_Handle handle;
    handle.handle = (u64)input;
    return handle;
}

internal HANDLE Win32_GetHandleFromOSHandle(OS_Handle input)
{
    HANDLE handle = (HANDLE *)input.handle;
    return handle;
}

//
// Threads
//

global Arena *Win32_arena;
global Win32_Thread *Win32_freeThread;

internal Win32_Thread *Win32_ThreadAlloc()
{
    Win32_Thread *thread = Win32_freeThread;
    if (thread)
    {
        StackPop(Win32_freeThread);
    }
    else
    {
        thread = PushStruct(Win32_arena, Win32_Thread);
    }
    Assert(thread != 0);
    return thread;
}

internal OS_Handle OS_ThreadAlloc(OS_ThreadFunction *func, void *ptr)
{
    // TODO Hack: Have proper OS initialization

    if (Win32_arena = 0)
    {
        Win32_arena = ArenaAllocDefault();
    }

    Win32_Thread *thread = Win32_ThreadAlloc();
    thread->func         = func;
    thread->ptr          = ptr;
    thread->handle       = CreateThread(0, 0, Win32_ThreadProc, thread, 0, 0);
    Win32_CreateOSHandle(thread->handle);
}

internal DWORD Win32_ThreadProc(void *p)
{
    Win32_Thread *thread = (Win32_Thread *)p;
    OS_ThreadFunction *func = thread->func;
    void *ptr               = thread->ptr;

    ThreadContext tContext_ = {};
    ThreadContextInitialize(&tContext_);
    func(ptr);
    ThreadContextRelease();
}

//
// Semaphores
//
internal OS_Handle OS_CreateSemaphore(u32 maxCount)
{
    HANDLE handle    = CreateSemaphoreEx(0, 0, maxCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    OS_Handle result = Win32_CreateOSHandle(handle);
    return result;
}

internal void OS_WaitOnSemaphore(OS_Handle input, u32 time = U32Max)
{
    HANDLE handle = Win32_GetHandleFromOSHandle(input);
    WaitForSingleObjectEx(handle, time, 0);
}

internal void OS_ReleaseSemaphore(OS_Handle input, u32 count)
{
    HANDLE handle = Win32_GetHandleFromOSHandle(input);
    ReleaseSemaphore(handle, count, 0);
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
