#include <windows.h>

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
internal u64 GetLastWriteTime(string filename)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    u64 timeStamp = 0;
    if (GetFileAttributesExA((char *)(filename.str), GetFileExInfoStandard, &data))
    {
        timeStamp = ((u64)data.ftLastWriteTime.dwHighDateTime) | (data.ftLastWriteTime.dwLowDateTime);
    }
    return timeStamp;
}
internal void FreeFileMemory(void *memory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
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
                    FreeFileMemory(&output.str);
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

//
// Handles
//
internal OS_Handle OS_CreateOSHandle(HANDLE input)
{
    OS_Handle handle;
    handle.handle = (u64)input;
    return handle;
}

internal HANDLE OS_GetHandleFromOSHandle(OS_Handle input)
{
    HANDLE handle = (HANDLE *)input.handle;
    return handle;
}

//
// Semaphores
//
internal OS_Handle OS_CreateSemaphore(u32 maxCount)
{
    HANDLE handle    = CreateSemaphoreEx(0, 0, maxCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    OS_Handle result = OS_CreateOSHandle(handle);
    return result;
}

internal void OS_WaitOnSemaphore(OS_Handle input, u32 time = U32Max)
{
    HANDLE handle = OS_GetHandleFromOSHandle(input);
    WaitForSingleObjectEx(handle, time, 0);
}

internal void OS_ReleaseSemaphore(OS_Handle input, u32 count)
{
    HANDLE handle = OS_GetHandleFromOSHandle(input);
    ReleaseSemaphore(handle, count, 0);
}
