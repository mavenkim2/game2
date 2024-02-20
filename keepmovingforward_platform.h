#ifndef KEEPMOVINGFORWARD_PLATFORM_H
#define KEEPMOVINGFORWARD_PLATFORM_H

#if INTERNAL
struct DebugPlatformHandle
{
    void *handle;
};

#define DEBUG_PLATFORM_GET_RESOLUTION(name) V2 name(DebugPlatformHandle handle)
typedef DEBUG_PLATFORM_GET_RESOLUTION(DebugPlatformGetResolutionFunctionType);
#endif

// Forward declarations
struct OS_JobQueue;
struct RenderState;

typedef void PlatformToggleCursorFunctionType(b32 value);
typedef void OS_JobCallback(void *data);
typedef void os_queue_job(OS_JobQueue *queue, OS_JobCallback *callback, void *data);

void PlatformToggleCursor(b32 value);

struct GameMemory
{
    b32 isInitialized;

    u64 PersistentStorageSize;
    void *PersistentStorageMemory;

    u64 TransientStorageSize;
    void *TransientStorageMemory;

    PlatformToggleCursorFunctionType *PlatformToggleCursor;
    os_queue_job *OS_QueueJob;

    OS_JobQueue *highPriorityQueue;
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
    char *scanDest         = destination;
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

#define GAME_UPDATE_AND_RENDER(name)                                                                              \
    void name(GameMemory *memory, RenderState *renderState, GameSoundOutput *soundBuffer, GameInput *input, f32 dT)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);

#endif
