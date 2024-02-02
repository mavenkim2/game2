#include <windows.h>
#include <gl/GL.h>

global HINSTANCE OS_W32_HINSTANCE;
global bool SHOW_CURSOR;
global bool RUNNING = true;

internal void Printf(char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    char printBuffer[1024];
    stbsp_vsprintf(printBuffer, fmt, va);
    va_end(va);
    OutputDebugStringA(printBuffer);
}

LRESULT OS_W32_Callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_SIZE:
        {
            break;
        }
        case WM_SETCURSOR:
        {
            if (SHOW_CURSOR)
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
        case WM_DESTROY:
        {
            RUNNING = false;
            break;
        }
        case WM_CLOSE:
        {
            RUNNING = false;
            break;
        }
        default:
        {
            result = DefWindowProcW(window, message, wParam, lParam);
        }
    }
    return result;
}

internal b32 OS_Init()
{
    WNDCLASSW windowClass     = {};
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = OS_W32_Callback;
    windowClass.hInstance     = OS_W32_HINSTANCE;
    windowClass.lpszClassName = L"Keep Moving Forward";
    windowClass.hCursor       = LoadCursorA(0, IDC_ARROW);

    if (!RegisterClassW(&windowClass))
    {
        return 0;
    }
    HWND windowHandle =
        CreateWindowExW(0, windowClass.lpszClassName, L"keep moving forward", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, OS_W32_HINSTANCE, 0);
    if (!windowHandle)
    {
        return 0;
    }

    return 1;
}
