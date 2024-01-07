#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_entity.h"
#include "keepmovingforward_level.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_platform.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_common.h"

struct DebugBmpResult
{
    u32 *pixels;
    i32 width;
    i32 height;
};

#pragma pack(push, 1)
struct BmpHeader
{
    // File Header
    u16 fileType;
    u32 fileSize;
    u16 reserved1;
    u16 reserved2;
    u32 offset;
    // BMP Info Header
    u32 structSize;
    i32 width;
    i32 height;
    u16 planes;
    u16 bitCount;
    u32 compression;
    u32 imageSize;
    i32 xPixelsPerMeter;
    i32 yPixelsPerMeter;
    u32 colororUsed;
    u32 importantColors;
    // Masks (why does this exist? who knows)
    u32 redMask;
    u32 greenMask;
    u32 blueMask;
};
#pragma pack(pop)

struct Camera
{
    v2 pos;
};

struct GameState
{
    u64 entity_id_gen; 
    Entity player;

    Arena* worldArena;
    Level *level;

    Camera camera;

    DebugBmpResult bmpTest;
};

#define KEEPMOVINGFORWARD_H
#endif
