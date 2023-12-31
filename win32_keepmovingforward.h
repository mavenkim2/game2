#ifndef WIN32_GAME_H

#include <windows.h>

#include "keepmovingforward_platform.h"
#include "keepmovingforward_string.h"
#include <gl/GL.h>
#include <xaudio2.h>

#define XAUDIO2_CREATE(name) HRESULT name(IXAudio2 **ppXAudio2, UINT32 flags, XAUDIO2_PROCESSOR xAudio2Processor)
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

struct Win32ReplayState
{
    HANDLE fileHandle;
    String8 filename;

    void *fileMemory;
    u64 totalSize;
};

struct Win32State
{
    Arena* arena;
    HANDLE recordingHandle;
    int currentRecordingIndex;

    HANDLE playbackHandle;
    int currentPlaybackIndex;

    Win32ReplayState replayStates[4];
    void *memory;

    String8 binaryDirectory;
};

// NOTE: if invalid, GameUpdateAndRender = 0
struct Win32GameCode
{
    HMODULE gameCodeDLL;
    FILETIME lastWriteTime;
    GameUpdateAndRenderFunctionType *GameUpdateAndRender;
};

inline LARGE_INTEGER Win32GetWallClock();

#define WIN32_GAME_H
#endif
