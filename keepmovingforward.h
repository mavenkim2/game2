#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_types.h"

#if INTERNAL
struct DebugReadFileOutput
{
    uint32 fileSize;
    void *contents;
};

#define DEBUG_PLATFORM_FREE_FILE(name) void name(void *fileMemory)
typedef DEBUG_PLATFORM_FREE_FILE(DebugPlatformFreeFileFunctionType);

#define DEBUG_PLATFORM_READ_FILE(name) DebugReadFileOutput name(const char *fileName)
typedef DEBUG_PLATFORM_READ_FILE(DebugPlatformReadFileFunctionType);

#define DEBUG_PLATFORM_WRITE_FILE(name)                                                            \
    bool name(const char *fileName, uint32 fileSize, void *fileMemory)
typedef DEBUG_PLATFORM_WRITE_FILE(DebugPlatformWriteFileFunctionType);
#endif

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

struct GameControllerInput
{
    union
    {
        GameButtonState buttons[4];
        struct
        {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
        };
    };
};

struct GameInput
{
    GameControllerInput controllers[4];
};

struct GameSoundOutput
{
    int16 *samples;
    int samplesPerSecond;
    int sampleCount;
};
struct GameState
{
    int xOffset;
    int yOffset;
    int toneHz;
    float tSine;

    int playerX;
    int playerY;
};

struct GameMemory
{
    bool isInitialized;

    uint64 PersistentStorageSize;
    void *PersistentStorageMemory;

    uint64 TransientStorageSize;
    void *TransientStorageMemory;

#if INTERNAL
    DebugPlatformFreeFileFunctionType *DebugPlatformFreeFile;
    DebugPlatformReadFileFunctionType *DebugPlatformReadFile;
    DebugPlatformWriteFileFunctionType *DebugPlatformWriteFile;
#endif
};

inline void CopyString(char *destination, const char *source)
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
inline void AddStrings(char *destination, const char *source)
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
}

#define GAME_UPDATE_AND_RENDER(name)                                                               \
    void name(GameMemory *memory, GameOffscreenBuffer *offscreenBuffer,                            \
              GameSoundOutput *soundBuffer, GameInput *input)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);


#define KEEPMOVINGFORWARD_H
#endif
