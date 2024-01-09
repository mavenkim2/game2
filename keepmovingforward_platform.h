#ifndef KEEPMOVINGFORWARD_PLATFORM_H
#define KEEPMOVINGFORWARD_PLATFORM_H

#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.h"

#if INTERNAL
struct DebugReadFileOutput
{
    u32 fileSize;
    void *contents;
};
struct DebugPlatformHandle {
    void* handle;
};

#define DEBUG_PLATFORM_FREE_FILE(name) void name(void *fileMemory)
typedef DEBUG_PLATFORM_FREE_FILE(DebugPlatformFreeFileFunctionType);

#define DEBUG_PLATFORM_READ_FILE(name) DebugReadFileOutput name(const char *fileName)
typedef DEBUG_PLATFORM_READ_FILE(DebugPlatformReadFileFunctionType);

#define DEBUG_PLATFORM_WRITE_FILE(name)                                                                 \
    bool name(const char *fileName, u32 fileSize, void *fileMemory)
typedef DEBUG_PLATFORM_WRITE_FILE(DebugPlatformWriteFileFunctionType);

#define DEBUG_PLATFORM_GET_RESOLUTION(name) V2 name(DebugPlatformHandle handle) 
typedef DEBUG_PLATFORM_GET_RESOLUTION(DebugPlatformGetResolutionFunctionType);
#endif

struct GameMemory
{
    bool isInitialized;

    u64 PersistentStorageSize;
    void *PersistentStorageMemory;

    u64 TransientStorageSize;
    void *TransientStorageMemory;

#if INTERNAL
    DebugPlatformFreeFileFunctionType *DebugPlatformFreeFile;
    DebugPlatformReadFileFunctionType *DebugPlatformReadFile;
    DebugPlatformWriteFileFunctionType *DebugPlatformWriteFile;
    DebugPlatformGetResolutionFunctionType *DebugPlatformGetResolution;
    DebugPlatformHandle handle;
#endif
};

struct GameOffscreenBuffer
{
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

// TODO: do we need half transition count if we're just polling once a frame?
// can't we just use keydown, keypressed, key released?
struct GameButtonState
{
    int halfTransitionCount;
    bool keyDown;
};

struct GameInput
{
    union
    {
        GameButtonState buttons[10];
        struct
        {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
            GameButtonState jump;
            GameButtonState shift;
            GameButtonState swap;
            GameButtonState soul;
            GameButtonState leftClick;
            GameButtonState rightClick;
        };
    };
    V2 mousePos; 
    float dT;
};


struct GameSoundOutput
{
    i16 *samples;
    int samplesPerSecond;
    int sampleCount;
};

inline void CopyString_(char *destination, const char *source)
{
    char *scanDest = destination;
    const char *scanSource = source;
    while (*scanSource)
    {
        *scanDest++ = *scanSource++;
    }
}

// NOTE: assume destination can hold source
// overwrrites \0 from destination, adds source to dest
inline void AddStrings_(char *destination, const char *source)
{
    char *scanDest = destination;
    while (*scanDest)
    {
        scanDest++;
    }

    const char *scanSource = source;
    while (*scanSource)
    {
        *scanDest++ = *scanSource++;
    }
};

#define GAME_UPDATE_AND_RENDER(name)                                                                         \
    void name(GameMemory *memory, GameOffscreenBuffer *buffer, GameSoundOutput *soundBuffer,        \
              GameInput *input)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);

#endif
