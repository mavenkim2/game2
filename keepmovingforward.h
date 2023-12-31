#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_entity.h"
#include "keepmovingforward_level.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_platform.h"
#include "keepmovingforward_types.h"

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

struct Camera
{
    v2 pos;
};

struct GameState
{
    uint64 entity_id_gen; 
    Entity player;

    MemoryArena worldArena;
    Level *level;

    Camera camera;

    DebugBmpResult bmpTest;
};

#define KEEPMOVINGFORWARD_H
#endif
