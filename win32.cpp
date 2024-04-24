// global HINSTANCE OS_W32_HINSTANCE;
// global bool SHOW_CURSOR;
// global bool RUNNING = true;
#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_math.h"
#include "platform.h"
#include "shared.h"
#endif

void Print(char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    char printBuffer[1024];
    stbsp_vsprintf(printBuffer, fmt, va);
    va_end(va);
    OutputDebugStringA(printBuffer);
}

global Win32_State *win32State;

//////////////////////////////
// Timing
//
f32 OS_GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    f32 wallClock = (f32)(result.QuadPart / win32State->mPerformanceFrequency);
    return wallClock;
}

f32 OS_NowSeconds()
{
    f32 result;

    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter))
    {
        result = (f32)(counter.QuadPart - win32State->mStartCounter) / (win32State->mPerformanceFrequency);
    }

    return result;
}

//////////////////////////////
// File Information
//

u64 Win32DenseTimeFromSystemtime(SYSTEMTIME *sysTime)
{
    u64 result = sysTime->wYear * 12 + sysTime->wMonth * 31 + sysTime->wDay * 24 + sysTime->wHour * 60 +
                 sysTime->wMinute * 60 + sysTime->wSecond * 1000 + sysTime->wMilliseconds;
    return result;
}

u64 Win32DenseTimeFromFileTime(FILETIME *filetime)
{
    SYSTEMTIME systime = {};
    FileTimeToSystemTime(filetime, &systime);
    u64 result = Win32DenseTimeFromSystemtime(&systime);
    return result;
}

OS_ATTRIBUTES_FROM_PATH(OS_AttributesFromPath)
{
    OS_FileAttributes result  = {};
    WIN32_FIND_DATAA findData = {};
    HANDLE handle             = FindFirstFileA((char *)path.str, &findData);
    if (handle != INVALID_HANDLE_VALUE)
    {
        u32 low             = findData.nFileSizeLow;
        u32 high            = findData.nFileSizeHigh;
        result.size         = (u64)low | (((u64)high) << 32);
        result.lastModified = Win32DenseTimeFromFileTime(&findData.ftLastWriteTime);
    }
    FindClose(handle);
    return result;
}

OS_GET_LAST_WRITE_TIME(OS_GetLastWriteTime)
{
    OS_FileAttributes attrib = OS_AttributesFromPath(path);
    u64 result               = attrib.lastModified;
    return result;
}

//////////////////////////////
// FILE I/O
//

OS_OPEN_FILE(OS_OpenFile)
{
    OS_Handle result          = {};
    DWORD accessFlags         = 0;
    DWORD shareMode           = 0;
    DWORD creationDisposition = OPEN_EXISTING;
    if (flags & OS_AccessFlag_Read) accessFlags |= GENERIC_READ;
    if (flags & OS_AccessFlag_Write) accessFlags |= GENERIC_WRITE;
    if (flags & OS_AccessFlag_ShareRead) shareMode |= FILE_SHARE_READ;
    if (flags & OS_AccessFlag_ShareWrite) shareMode |= FILE_SHARE_WRITE;
    if (flags & OS_AccessFlag_Write) creationDisposition = CREATE_ALWAYS;
    HANDLE file =
        CreateFileA((char *)path.str, accessFlags, shareMode, 0, creationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        result.handle = (u64)file;
    }
    else
    {
        Printf("Could not open file: %S\n", path);
        Assert(!"Temp");
    }
    return result;
}

OS_CLOSE_FILE(OS_CloseFile)
{
    if (input.handle != 0)
    {
        HANDLE handle = (HANDLE)input.handle;
        CloseHandle(handle);
    }
}

OS_ATTRIBUTE_FROM_FILE(OS_AttributesFromFile)
{
    OS_FileAttributes result = {};
    if (input.handle != 0)
    {
        HANDLE handle = (HANDLE)input.handle;
        BY_HANDLE_FILE_INFORMATION info;
        b32 good = GetFileInformationByHandle(handle, &info);
        if (good)
        {
            u32 low             = info.nFileSizeLow;
            u32 high            = info.nFileSizeHigh;
            result.size         = (u64)low | (((u64)high) << 32);
            result.lastModified = Win32DenseTimeFromFileTime(&info.ftLastWriteTime);
        }
    }
    return result;
}

// Reads part of a file, sequentially
u32 OS_ReadFile(OS_Handle handle, void *out, u64 offset, u32 size)
{
    u32 result  = 0;
    HANDLE file = (HANDLE)handle.handle;
    DWORD readSize;

    OVERLAPPED overlapped;
    overlapped.Offset     = (u32)((offset >> 0) & 0xffffffff);
    overlapped.OffsetHigh = (u32)((offset >> 32) & 0xffffffff);

    ReadFile(file, out, size, &readSize, &overlapped);
    result = readSize;
    return result;
}

OS_READ_FILE_HANDLE(OS_ReadEntireFile)
{
    u64 totalReadSize   = 0;
    LARGE_INTEGER start = {};
    HANDLE file         = (HANDLE)handle.handle;
    if (handle.handle != 0)
    {
        u64 size;
        GetFileSizeEx(file, (LARGE_INTEGER *)&size);

        for (totalReadSize = 0; totalReadSize < size;)
        {
            u64 readAmount = size - totalReadSize;
            u32 sizeToRead = (readAmount > U32Max) ? U32Max : (u32)readAmount;
            DWORD readSize = 0;
            if (!ReadFile(file, (u8 *)out + totalReadSize, sizeToRead, &readSize, 0)) break;
            totalReadSize += readSize;
            if (readSize != sizeToRead)
            {
                break;
            }
        }
    }

    return totalReadSize;
}

u64 OS_ReadEntireFile(string path, void *out)
{
    OS_Handle handle = OS_OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, path);
    u64 result       = OS_ReadEntireFile(handle, out);
    OS_CloseFile(handle);
    return result;
}

OS_READ_ENTIRE_FILE(OS_ReadEntireFile)
{
    OS_Handle handle = OS_OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, path);

    string result;
    result.size             = OS_AttributesFromPath(path).size;
    result.str              = PushArray(arena, u8, result.size + 1);
    result.str[result.size] = 0;

    u64 size = OS_ReadEntireFile(handle, result.str);
    Assert(size == result.size);
    OS_CloseFile(handle);

    return result;
}

#if 0
string ReadEntireFile(string filename)
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

#endif
OS_WRITE_FILE(OS_WriteFile)
{
    b32 result = false;
    HANDLE fileHandle =
        CreateFileA((char *)filename.str, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

//////////////////////////////
// File directory iteration
//
StaticAssert((sizeof(Win32_FileIter) <= sizeof(((OS_FileIter *)0)->memory)), fileIterSize);

string OS_GetCurrentWorkingDirectory()
{
    DWORD length = GetCurrentDirectoryA(0, 0);
    u8 *path     = PushArray(win32State->mArena, u8, length);
    length       = GetCurrentDirectoryA(length + 1, (char *)path);

    string result = Str8(path, length);

    return result;
}

OS_FileIter OS_DirectoryIterStart(string path, OS_FileIterFlags flags)
{
    OS_FileIter result;
    TempArena temp       = ScratchStart(0, 0);
    string search        = StrConcat(temp.arena, path, Str8Lit("\\*"));
    result.flags         = flags;
    Win32_FileIter *iter = (Win32_FileIter *)result.memory;
    iter->handle         = FindFirstFileA((char *)search.str, &iter->findData);
    return result;
}

b32 OS_DirectoryIterNext(Arena *arena, OS_FileIter *input, OS_FileProperties *out)
{
    b32 done               = 0;
    Win32_FileIter *iter   = (Win32_FileIter *)input->memory;
    OS_FileIterFlags flags = input->flags;
    if (!(input->flags & OS_FileIterFlag_Complete) && iter->handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            b32 skip         = 0;
            char *filename   = iter->findData.cFileName;
            DWORD attributes = iter->findData.dwFileAttributes;
            if (filename[0] == '.')
            {
                if (flags & OS_FileIterFlag_SkipHiddenFiles || filename[1] == 0 ||
                    (filename[1] == '.' && filename[2] == 0))
                {
                    skip = 1;
                }
            }
            if (attributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (flags & OS_FileIterFlag_SkipDirectories)
                {
                    skip = 1;
                }
            }
            else
            {
                if (flags & OS_FileIterFlag_SkipFiles)
                {
                    skip = 1;
                }
            }
            if (!skip)
            {
                out->size         = ((u64)iter->findData.nFileSizeLow) | (((u64)iter->findData.nFileSizeHigh) << 32);
                out->lastModified = Win32DenseTimeFromFileTime(&iter->findData.ftLastWriteTime);
                out->isDirectory  = (attributes & FILE_ATTRIBUTE_DIRECTORY);
                out->name         = PushStr8Copy(arena, Str8C(filename));
                done              = 1;
                if (!FindNextFileA(iter->handle, &iter->findData))
                {
                    input->flags |= OS_FileIterFlag_Complete;
                }
                break;
            }
        } while (FindNextFileA(iter->handle, &iter->findData));
    }
    return done;
}

void OS_DirectoryIterEnd(OS_FileIter *input)
{
    Win32_FileIter *iter = (Win32_FileIter *)input->memory;
    FindClose(iter->handle);
}

//////////////////////////////
// Memory
//
OS_PAGE_SIZE(OS_PageSize)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
}

OS_ALLOC(OS_Alloc)
{
    u64 pageSnappedSize = size;
    pageSnappedSize += OS_PageSize() - 1;
    pageSnappedSize -= pageSnappedSize % OS_PageSize();
    void *ptr = VirtualAlloc(0, pageSnappedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return ptr;
}

OS_RESERVE(OS_Reserve)
{
    void *ptr = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    return ptr;
}

OS_COMMIT(OS_Commit)
{
    b8 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
    return result;
}

OS_RELEASE(OS_Release)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

//////////////////////////////
// Initialization
//
void OS_Init()
{
    if (ThreadContextGet()->isMainThread)
    {
        Arena *arena       = ArenaAlloc();
        win32State         = PushStruct(arena, Win32_State);
        win32State->mArena = arena;

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        win32State->mPerformanceFrequency = frequency.QuadPart;

        win32State->mGranularSleep = (timeBeginPeriod(1) == TIMERR_NOERROR);

        LARGE_INTEGER counter;
        if (QueryPerformanceCounter(&counter))
        {
            win32State->mStartCounter = counter.QuadPart;
        }

        else
        {
            Assert(!"Timer not initializedD");
        }

        win32State->mShowCursor = 1;
        // Get binary directory
        {
            string binaryDirectory;
            TempArena temp               = ScratchStart(0, 0);
            DWORD size                   = kilobytes(4);
            binaryDirectory.str          = PushArray(temp.arena, u8, size);
            DWORD length                 = GetModuleFileNameA(0, (char *)binaryDirectory.str, size);
            binaryDirectory.size         = length;
            binaryDirectory              = Str8PathChopLastSlash(binaryDirectory);
            win32State->mBinaryDirectory = PushStr8Copy(arena, binaryDirectory);
            ScratchEnd(temp);
        }
        win32State = win32State;
    }
}

// TODO: this isn't quite right
void Win32_AddEvent(OS_Event event)
{
    OS_EventList *eventList = &win32State->mEventsList;
    OS_EventChunk *chunk    = eventList->last;
    if (eventList->last == 0 || eventList->last->count == eventList->last->cap)
    {
        chunk         = PushStruct(win32State->mTempEventArena, OS_EventChunk);
        chunk->cap    = 16;
        chunk->events = PushArray(win32State->mTempEventArena, OS_Event, chunk->cap);
        QueuePush(eventList->first, eventList->last, chunk);
    }
    chunk->events[chunk->count++] = event;
    eventList->numEvents++;
}

OS_Event Win32_CreateKeyEvent(OS_Key key, b32 isDown)
{
    OS_Event event;
    event.key  = key;
    event.type = isDown ? OS_EventType_KeyPressed : OS_EventType_KeyReleased;

    return event;
}

LRESULT Win32_Callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    b32 isRelease  = 0;
    switch (message)
    {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        {
            isRelease = 1;
        }
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDOWN:
        {
            OS_Key key;
            switch (message)
            {
                case WM_LBUTTONUP:
                case WM_LBUTTONDOWN: key = OS_Mouse_L; break;
                case WM_RBUTTONUP:
                case WM_RBUTTONDOWN: key = OS_Mouse_R; break;
            }

            OS_Event event = Win32_CreateKeyEvent(key, !isRelease);
            event.pos.x    = (f32)LOWORD(lParam);
            event.pos.y    = (f32)HIWORD(lParam);

            Win32_AddEvent(event);

            if (isRelease == 0) SetCapture(window);
            else ReleaseCapture();
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            u32 keyCode = (u32)wParam;
            Assert(keyCode < 256);

            static b32 uninitialized = 1;
            static OS_Key keyTable[256];
            if (uninitialized)
            {
                uninitialized = 0;
                for (u32 i = 0; i < 'Z' - 'A'; i++)
                {
                    keyTable[i + 'A'] = (OS_Key)(OS_Key_A + i);
                }
                keyTable[VK_SPACE] = OS_Key_Space;
                keyTable[VK_SHIFT] = OS_Key_Shift;
            }

            b32 wasDown    = !!(lParam & (1 << 30));
            b32 isDown     = !(lParam & (1 << 31));
            b32 altWasDown = !!(lParam & (1 << 29));

            OS_Event event   = Win32_CreateKeyEvent(keyTable[keyCode], isDown);
            event.transition = (isDown != wasDown);

            if (keyCode == VK_F4 && altWasDown)
            {
                event.type = OS_EventType_Quit;
            }
            Win32_AddEvent(event);
            // if (isDown && wasDown != isDown && keyCode == VK_RETURN && altWasDown)
            // {
            //     ToggleFullscreen(message.hwnd);
            // }
            // if (keyCode == 'L')
            // {
            //     if (isDown && isDown != wasDown)
            //     {
            //         if (state->currentPlaybackIndex)
            //         {
            //             Win32EndPlayback(state);
            //         }
            //         else if (!state->currentRecordingIndex)
            //         {
            //             Win32BeginRecording(state, 1);
            //         }
            //         else if (state->currentRecordingIndex && !state->currentPlaybackIndex)
            //         {
            //             Win32EndRecording(state);
            //             Win32BeginPlayback(state, 1);
            //         }
            //     }
            // }
            // break;
            break;
        }
        case WM_DESTROY:
        case WM_CLOSE:
        case WM_QUIT:
        {
            OS_Event event;
            event.type = OS_EventType_Quit;
            Win32_AddEvent(event);
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            EndPaint(window, &paint);
            break;
        }
        case WM_SIZE:
        {
            break;
        }
        case WM_KILLFOCUS:
        {
            OS_Event event;
            event.type = OS_EventType_LoseFocus;
            Win32_AddEvent(event);
            ReleaseCapture();
            break;
        }
        case WM_SETCURSOR:
        {
            if (win32State->mShowCursor)
            {
                result = DefWindowProcW(window, message, wParam, lParam);
            }
            else
            {
                SetCursor(0);
            }
            break;
        }
        case WM_ACTIVATEAPP:
        {
            break;
        }
        default:
        {
            result = DefWindowProcW(window, message, wParam, lParam);
        }
    }
    return result;
}

OS_Handle OS_WindowInit()
{
    OS_Handle result          = {};
    WNDCLASSW windowClass     = {};
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = Win32_Callback;
    windowClass.hInstance     = win32State->mHInstance;
    windowClass.lpszClassName = L"Keep Moving Forward";
    windowClass.hCursor       = LoadCursorA(0, IDC_ARROW);

    if (RegisterClassW(&windowClass))
    {
        HWND windowHandle = CreateWindowExW(0, windowClass.lpszClassName, L"keep moving forward",
                                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, windowClass.hInstance, 0);
        if (windowHandle)
        {
            result.handle = (u64)windowHandle;
        }
    }
    return result;
}

string OS_GetBinaryDirectory()
{
    string result = win32State->mBinaryDirectory;
    return result;
}

OS_GET_CENTER(OS_GetCenter)
{
    HWND windowHandle = (HWND)handle.handle;

    RECT windowRect;
    GetWindowRect(windowHandle, &windowRect);
    POINT centerPos = {(LONG)((windowRect.right + windowRect.left) / 2.f),
                       (LONG)((windowRect.bottom + windowRect.top) / 2.f)};

    if (screenToClient)
    {
        ScreenToClient(windowHandle, &centerPos);
    }

    V2 center = {(f32)centerPos.x, (f32)centerPos.y};
    return center;
}

V2 OS_GetWindowDimension(OS_Handle handle)
{
    HWND window = (HWND)handle.handle;
    V2 result;
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.x = (f32)(clientRect.right - clientRect.left);
    result.y = (f32)(clientRect.bottom - clientRect.top);
    return result;
}

OS_TOGGLE_CURSOR(OS_ToggleCursor)
{
    win32State->mShowCursor = on;
}

//////////////////////////////
// Windows
//

void OS_ToggleFullscreen(OS_Handle handle)
{
    HWND window = (HWND)handle.handle;
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(window, &win32State->mWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &win32State->mWindowPosition);
        SetWindowPos(window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

//////////////////////////////
// Keyboard/Mouse
//
OS_GET_EVENTS(OS_GetEvents)
{
    win32State->mTempEventArena       = arena;
    win32State->mEventsList.numEvents = 0;
    win32State->mEventsList.first     = 0;
    win32State->mEventsList.last      = 0;

    MSG message;
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    OS_Events events;
    events.numEvents = win32State->mEventsList.numEvents;
    events.events    = PushArrayNoZero(arena, OS_Event, win32State->mEventsList.numEvents);

    OS_Event *ptr = events.events;
    for (OS_EventChunk *node = win32State->mEventsList.first; node != 0; node = node->next)
    {
        MemoryCopy(ptr, node->events, sizeof(OS_Event) * node->count);
        ptr += node->count;
    }

    return events;
}

OS_GET_MOUSE_POS(OS_GetMousePos)
{
    POINT pos;
    V2 p = {};
    if (GetCursorPos(&pos))
    {
        HWND windowHandle = (HWND)handle.handle;
        ScreenToClient(windowHandle, &pos);
        p.x = (f32)pos.x;
        p.y = (f32)pos.y;
    }
    return p;
}

b32 OS_WindowIsFocused(OS_Handle handle)
{
    HWND window       = (HWND)handle.handle;
    HWND activeWindow = GetActiveWindow();
    return activeWindow == window;
}

OS_SET_MOUSE_POS(OS_SetMousePos)
{
    if (OS_WindowIsFocused(handle))
    {
        SetCursorPos((i32)pos.x, (i32)pos.y);
    }
}

//////////////////////////////
// Threads
//

Win32_Sync *Win32_SyncAlloc(Win32_SyncType type)
{
    Win32_Sync *sync = win32State->mFreeSync;
    while (sync && AtomicCompareExchangePtr(&win32State->mFreeSync, sync->next, sync) != sync)
    {
        sync = win32State->mFreeSync;
    }
    if (!sync)
    {
        BeginTicketMutex(&win32State->mMutex);
        Arena *arena = win32State->mArena;
        sync         = PushStruct(arena, Win32_Sync);
        EndTicketMutex(&win32State->mMutex);
    }
    Assert(sync != 0);
    sync->type = type;
    return sync;
}

void Win32_SyncFree(Win32_Sync *sync)
{
    sync->type = Win32_SyncType_Null;
    sync->next = win32State->mFreeSync;
    while (AtomicCompareExchangePtr(&win32State->mFreeSync, sync, sync->next) != sync->next)
    {
        sync->next = win32State->mFreeSync;
    }
}

DWORD Win32_ThreadProc(void *p)
{
    Win32_Sync *thread      = (Win32_Sync *)p;
    OS_ThreadFunction *func = thread->thread.func;
    void *ptr               = thread->thread.ptr;

    BaseThreadEntry(func, ptr);

    return 0;
}

OS_THREAD_START(OS_ThreadStart)
{
    // TODO Hack: Have proper OS initialization

    Win32_Sync *thread    = Win32_SyncAlloc(Win32_SyncType_Thread);
    thread->thread.func   = func;
    thread->thread.ptr    = ptr;
    thread->thread.handle = CreateThread(0, 0, Win32_ThreadProc, thread, 0, 0);
    OS_Handle handle      = {(u64)thread};
    return handle;
}

OS_THREAD_JOIN(OS_ThreadJoin)
{
    Win32_Sync *thread = (Win32_Sync *)handle.handle;
    if (thread && thread->type == Win32_SyncType_Thread && thread->thread.handle)
    {
        WaitForSingleObject(thread->thread.handle, INFINITE);
        CloseHandle(thread->thread.handle);
    }
    Win32_SyncFree(thread);
}

// TODO: set thread names using raise exception as well?
OS_SET_THREAD_NAME(OS_SetThreadName)
{
    TempArena scratch = ScratchStart(0, 0);

    u32 resultSize     = (u32)(2 * name.size);
    wchar_t *result    = (wchar_t *)PushArray(scratch.arena, u8, resultSize + 1);
    resultSize         = MultiByteToWideChar(CP_UTF8, 0, (char *)name.str, (i32)name.size, result, (i32)resultSize);
    result[resultSize] = 0;
    SetThreadDescription(GetCurrentThread(), result);

    ScratchEnd(scratch);
}

//////////////////////////////
// Semaphores
//
OS_CREATE_SEMAPHORE(OS_CreateSemaphore)
{
    HANDLE handle    = CreateSemaphoreEx(0, 0, maxCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    OS_Handle result = {(u64)handle};
    return result;
}

OS_RELEASE_SEMAPHORE(OS_ReleaseSemaphore)
{
    HANDLE handle = (HANDLE)input.handle;
    ReleaseSemaphore(handle, 1, 0);
}

OS_RELEASE_SEMAPHORES(OS_ReleaseSemaphores)
{
    HANDLE handle = (HANDLE)input.handle;
    ReleaseSemaphore(handle, count, 0);
}

OS_DELETE_SEMAPHORE(OS_DeleteSemaphore)
{
    HANDLE semaphore = (HANDLE)handle.handle;
    CloseHandle(semaphore);
}

//////////////////////////////
// Mutexes
//
OS_Handle OS_CreateMutex()
{
    Win32_Sync *mutex = Win32_SyncAlloc(Win32_SyncType_Mutex);
    InitializeCriticalSection(&mutex->mutex);
    OS_Handle handle = {(u64)mutex};
    return handle;
}

void OS_DeleteMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    DeleteCriticalSection(&mutex->mutex);
    Win32_SyncFree(mutex);
}

void OS_TakeMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    EnterCriticalSection(&mutex->mutex);
}

void OS_DropMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    LeaveCriticalSection(&mutex->mutex);
}

//////////////////////////////
// Read/Write Mutex
//
OS_Handle OS_CreateRWMutex()
{
    Win32_Sync *mutex = Win32_SyncAlloc(Win32_SyncType_RWMutex);
    InitializeSRWLock(&mutex->rwMutex);
    OS_Handle handle = {(u64)mutex};
    return handle;
}
void OS_DeleteRWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    Win32_SyncFree(mutex);
}

void OS_TakeRMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    AcquireSRWLockShared(&mutex->rwMutex);
}

void OS_DropRMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    ReleaseSRWLockShared(&mutex->rwMutex);
}

void OS_TakeWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    AcquireSRWLockExclusive(&mutex->rwMutex);
}

void OS_DropWMutex(OS_Handle input)
{
    Win32_Sync *mutex = (Win32_Sync *)input.handle;
    ReleaseSRWLockExclusive(&mutex->rwMutex);
}

//////////////////////////////
// Condition Variables
//
OS_Handle OS_CreateConditionVariable()
{
    Win32_Sync *cvar = Win32_SyncAlloc(Win32_SyncType_CVar);
    InitializeConditionVariable(&cvar->cv);
    OS_Handle handle = {(u64)cvar};
    return handle;
}
void OS_DeleteConditionVariable(OS_Handle input)
{
    Win32_Sync *cvar = (Win32_Sync *)input.handle;
    Win32_SyncFree(cvar);
}
// TODO: be able to input a wait time?
b32 OS_WaitConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result            = SleepConditionVariableCS(&cvar->cv, &mutex->mutex, INFINITE);
    return result;
}

b32 OS_WaitRConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result            = SleepConditionVariableSRW(&cvar->cv, &mutex->rwMutex, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
    return result;
}

b32 OS_WaitRWConditionVariable(OS_Handle cv, OS_Handle m)
{
    b32 result        = 0;
    Win32_Sync *cvar  = (Win32_Sync *)cv.handle;
    Win32_Sync *mutex = (Win32_Sync *)m.handle;
    result            = SleepConditionVariableSRW(&cvar->cv, &mutex->rwMutex, INFINITE, 0);
    return result;
}

void OS_SignalConditionVariable(OS_Handle cv)
{
    Win32_Sync *cvar = (Win32_Sync *)cv.handle;
    WakeConditionVariable(&cvar->cv);
}

void OS_BroadcastConditionVariable(OS_Handle cv)
{
    Win32_Sync *cvar = (Win32_Sync *)cv.handle;
    WakeAllConditionVariable(&cvar->cv);
}

//////////////////////////////
// Signals
//

// NOTE: this sets the event to auto-reset after a successful wait.
OS_Handle OS_CreateSignal()
{
    HANDLE handle    = CreateEvent(0, 0, 0, 0);
    OS_Handle result = {(u64)handle};
    return result;
}

OS_SIGNAL_WAIT(OS_SignalWait)
{
    HANDLE handle = (HANDLE)input.handle;
    DWORD result  = WaitForSingleObject(handle, U32Max);
    Assert(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT);
    return (result == WAIT_OBJECT_0);
}

void OS_RaiseSignal(OS_Handle input)
{
    HANDLE handle = (HANDLE)input.handle;
    SetEvent(handle);
}

//
// System Info
//

OS_NUM_PROCESSORS(OS_NumProcessors)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}

//////////////////////////////
// zzz
//
OS_SLEEP(OS_Sleep)
{
    Sleep(milliseconds);
}

//////////////////////////////
// DLL
//
struct OS_DLL
{
    OS_Handle mHandle;
    string mSource;
    string mLock;
    string mTemp;

    u64 mLastWriteTime;

    void **mFunctions;
    char **mFunctionNames;
    u32 mFunctionCount;
    b8 mValid;
};

void OS_LoadDLL(OS_DLL *dll)
{
    WIN32_FILE_ATTRIBUTE_DATA ignored;
    OS_Handle result = {};
    if (!GetFileAttributesExA((char *)(dll->mLock.str), GetFileExInfoStandard, &ignored))
    {
        CopyFileA((char *)(dll->mSource.str), (char *)(dll->mTemp.str), false);

        HMODULE gameCodeDLL = LoadLibraryA((char *)(dll->mTemp.str));
        dll->mHandle.handle = (u64)gameCodeDLL;
        dll->mLastWriteTime = OS_GetLastWriteTime(dll->mSource);
        dll->mValid         = true;

        if (gameCodeDLL)
        {
            for (u32 i = 0; i < dll->mFunctionCount; i++)
            {
                void *func = (void *)GetProcAddress(gameCodeDLL, dll->mFunctionNames[i]);
                if (func)
                {
                    dll->mFunctions[i] = func;
                }
                else
                {
                    OS_UnloadDLL(dll);
                    return;
                }
            }
        }
    }
}

void OS_UnloadDLL(OS_DLL *dll)
{
    if (OS_GetLastWriteTime(dll->mSource) != dll->mLastWriteTime)
    {
        FreeLibrary((HMODULE)dll->mHandle.handle);
        dll->mHandle.handle = 0;
        dll->mLastWriteTime = 0;
        dll->mValid         = false;
    }
}
