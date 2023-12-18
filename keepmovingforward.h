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

#define DEBUG_PLATFORM_WRITE_FILE(name) bool name(const char *fileName, uint32 fileSize, void *fileMemory)
typedef DEBUG_PLATFORM_WRITE_FILE(DebugPlatformWriteFileFunctionType);
#endif

// NOTE: offset from start of struct
#pragma pack(push, 1)
struct BmpHeader
{
    // File Header
    uint16 fileType;
    uint32 fileSize;
    uint16 reserved1;
    uint16 reserved2;
    uint32 offset;
    // BMP Info Header
    uint32 structSize;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bitCount;
    uint32 compression;
    uint32 imageSize;
    int32 xPixelsPerMeter;
    int32 yPixelsPerMeter;
    uint32 colororUsed;
    uint32 importantColors;
    // Masks (why does this exist? who knows)
    uint32 redMask;
    uint32 greenMask;
    uint32 blueMask;
};
#pragma pack(pop)

struct DebugBmpResult
{
    uint32 *pixels;
    int32 width;
    int32 height;
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

struct GameControllerInput
{
    union
    {
        GameButtonState buttons[6];
        struct
        {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
            GameButtonState jump;
            GameButtonState shift;
        };
    };
};

struct GameInput
{
    float dT;
    GameControllerInput controllers[4];
};

struct GameSoundOutput
{
    int16 *samples;
    int samplesPerSecond;
    int sampleCount;
};

// struct Player
// {
//     Vector2 position;
//     Vector2 velocity;
//     Vector2 acceleration;
// };

struct MemoryArena
{
    void *base;
    size_t size;
    size_t used;
};

struct Level
{
    uint32 *tileMap;
};

struct GameState
{
    float playerX;
    float playerY;

    float dx;
    float dy;

    float playerWidth;
    float playerHeight;

    bool isJumping;

    MemoryArena worldArena;
    Level *level;

    DebugBmpResult bmpTest;
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

#define GAME_UPDATE_AND_RENDER(name)                                                                                                                 \
    void name(GameMemory *memory, GameOffscreenBuffer *offscreenBuffer, GameSoundOutput *soundBuffer, GameInput *input)

typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunctionType);

#define KEEPMOVINGFORWARD_H
#endif
