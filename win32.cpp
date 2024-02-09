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
    void* ptr = VirtualAlloc(0, pageSnappedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return ptr;
}

// LRESULT OS_W32_Callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     LRESULT result = 0;
//     switch (message)
//     {
//         case WM_SIZE:
//         {
//             break;
//         }
//         case WM_SETCURSOR:
//         {
//             if (SHOW_CURSOR)
//             {
//                 result = DefWindowProcW(window, message, wParam, lParam);
//             }
//             else
//             {
//                 SetCursor(0);
//             }
//             break;
//         }
//         case WM_ACTIVATEAPP:
//         {
//             break;
//         }
//         case WM_DESTROY:
//         {
//             RUNNING = false;
//             break;
//         }
//         case WM_CLOSE:
//         {
//             RUNNING = false;
//             break;
//         }
//         default:
//         {
//             result = DefWindowProcW(window, message, wParam, lParam);
//         }
//     }
//     return result;
// }
//
// internal b32 OS_Init()
// {
//     WNDCLASSW windowClass     = {};
//     windowClass.style         = CS_HREDRAW | CS_VREDRAW;
//     windowClass.lpfnWndProc   = OS_W32_Callback;
//     windowClass.hInstance     = OS_W32_HINSTANCE;
//     windowClass.lpszClassName = L"Keep Moving Forward";
//     windowClass.hCursor       = LoadCursorA(0, IDC_ARROW);
//
//     if (!RegisterClassW(&windowClass))
//     {
//         return 0;
//     }
//     HWND windowHandle =
//         CreateWindowExW(0, windowClass.lpszClassName, L"keep moving forward", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
//                         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, OS_W32_HINSTANCE, 0);
//     if (!windowHandle)
//     {
//         return 0;
//     }
//
//     return 1;
// }
