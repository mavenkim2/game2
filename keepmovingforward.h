#ifndef KEEPMOVINGFORWARD_H
#define KEEPMOVINGFORWARD_H

#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#include "platform_inc.h"
#include "thread_context.h"
#include "job.h"

#include "keepmovingforward_math.h"
#include "physics.h"
#include "keepmovingforward_camera.h"
#include "render/render_core.h"
#include "asset.h"
#include "./offline/asset_processing.h"
#include "asset_cache.h"
#include "render/render.h"
#include "keepmovingforward_platform.h"

#include "keepmovingforward_entity.h"
#include "keepmovingforward_level.h"

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

enum CameraMode
{
    CameraMode_Debug,
    CameraMode_Player,
};

struct GameState
{
    Arena *frameArena;

    u64 entity_id_gen;
    Entity player;

    Arena *worldArena;
    Level *level;

    Camera camera;

    DebugBmpResult bmpTest;

    CameraMode cameraMode;

    AnimationPlayer animPlayer;

    AS_Handle model;
    AS_Handle model2;
};

global r_allocate_texture_2D *R_AllocateTexture2D;
global r_delete_texture_2D *R_DeleteTexture2D;
global r_allocate_buffer *R_AllocateBuffer;

#endif
