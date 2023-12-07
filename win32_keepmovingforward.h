#ifndef WIN32_GAME_H

#include "keepmovingforward_types.h"
#include "keepmovingforward.h"
#include <windows.h>
#include <xaudio2.h>

#define XAUDIO2_CREATE(name)                                                                       \
    HRESULT name(IXAudio2 **ppXAudio2, UINT32 flags, XAUDIO2_PROCESSOR xAudio2Processor)
typedef XAUDIO2_CREATE(XAudio2CreateFunctionType);

struct Win32OffscreenBuffer
{
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

struct Win32WindowDimension
{
    int width;
    int height;
};

struct Win32State
{
    char executableFullPath[MAX_PATH];
    char executableDirectory[MAX_PATH];
};

// NOTE: if invalid, GameUpdateAndRender = 0
struct Win32GameCode
{
    HMODULE gameCodeDLL;
    FILETIME lastWriteTime;
    GameUpdateAndRenderFunctionType *GameUpdateAndRender;
};

#define WIN32_GAME_H
#endif
