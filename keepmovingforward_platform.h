#ifndef KEEPMOVINGFORWARD_PLATFORM_H
#define KEEPMOVINGFORWARD_PLATFORM_H

#if INTERNAL
struct DebugReadFileOutput
{
    u32 fileSize;
    void *contents;
};
struct DebugPlatformHandle
{
    void *handle;
};

#define DEBUG_PLATFORM_FREE_FILE(name) void name(void *fileMemory)
typedef DEBUG_PLATFORM_FREE_FILE(DebugPlatformFreeFileFunctionType);

#define DEBUG_PLATFORM_READ_FILE(name) DebugReadFileOutput name(const char *fileName)
typedef DEBUG_PLATFORM_READ_FILE(DebugPlatformReadFileFunctionType);

#define DEBUG_PLATFORM_WRITE_FILE(name) b32 name(const char *fileName, u32 fileSize, void *fileMemory)
typedef DEBUG_PLATFORM_WRITE_FILE(DebugPlatformWriteFileFunctionType);

#define DEBUG_PLATFORM_GET_RESOLUTION(name) V2 name(DebugPlatformHandle handle)
typedef DEBUG_PLATFORM_GET_RESOLUTION(DebugPlatformGetResolutionFunctionType);
#endif

typedef void PlatformToggleCursorFunctionType(b32 value);

void PlatformToggleCursor(b32 value);

struct GameMemory
{
    b32 isInitialized;

    u64 PersistentStorageSize;
    void *PersistentStorageMemory;

    u64 TransientStorageSize;
    void *TransientStorageMemory;

    PlatformToggleCursorFunctionType *PlatformToggleCursor;
#if 0
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
    i32 halfTransitionCount;
    b32 keyDown;
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
    V2 lastMousePos;
    V2 deltaMouse;
    f32 dT;
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

#define GAME_UPDATE_AND_RENDER(name)                                                                                   \
    void name(GameMemory *memory, OpenGL *openGL, GameSoundOutput *soundBuffer, GameInput *input, f32 dT)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);

#endif
