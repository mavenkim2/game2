#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_platform.h"
#include "keepmovingforward_types.h"
#include "keepmovingforward_math.h"

// NOTE: offset from start of struct


struct DebugBmpResult
{
    uint32 *pixels;
    int32 width;
    int32 height;
};

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

struct Entity
{
    // Physics
    Vector2 pos;
    Vector2 size;

    Vector2 velocity;
    Vector2 acceleration;

    bool airborne;
    bool swappable;

    // Render
    float r;
    float g;
    float b;
};

struct GameState
{
    Entity entities[256];
    int entityCount;
    
    int playerIndex;
    int swapIndex;
    // TODO: should probably be in entity

    MemoryArena worldArena;
    Level *level;

    DebugBmpResult bmpTest;
};

#define KEEPMOVINGFORWARD_H
#endif
