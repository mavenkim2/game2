#ifndef KEEPMOVINGFORWARD_PLATFORM_H
#define KEEPMOVINGFORWARD_PLATFORM_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "render/render.h"
#endif

#if INTERNAL
struct DebugPlatformHandle
{
    void *handle;
};

#define DEBUG_PLATFORM_GET_RESOLUTION(name) V2 name(DebugPlatformHandle handle)
typedef DEBUG_PLATFORM_GET_RESOLUTION(DebugPlatformGetResolutionFunctionType);
#endif

// Forward declarations
struct RenderState;

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

    r_allocate_texture_2D *R_AllocateTexture2D;
    r_delete_texture_2D *R_DeleteTexture2D;
    r_allocate_buffer *R_AllocateBuffer;
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

#define GAME_UPDATE_AND_RENDER(name)                                                                              \
    void name(GameMemory *memory, RenderState *renderState, GameSoundOutput *soundBuffer, GameInput *input, f32 dT)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);

#endif
