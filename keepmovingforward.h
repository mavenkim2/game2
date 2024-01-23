#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_camera.h"
#include "keepmovingforward_asset.h"
#include "render/keepmovingforward_renderer.h"

// TODO IMPORTANT GET RID OF THE WIN32 AND GL STUFF FROM HERE
// also platform should definitely not need to depend on OPENGL function
#if WINDOWS
#include <windows.h>
#include <gl/GL.h>
#include "render/win32_keepmovingforward_opengl.h"
#endif

#include "keepmovingforward_platform.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
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

enum CameraMode {
    CameraMode_Debug,
    CameraMode_Player,
};

struct GameState
{
    u64 entity_id_gen; 
    Entity player;

    Arena* worldArena;
    Level *level;

    Camera camera;

    DebugBmpResult bmpTest;

    CameraMode cameraMode;
};

#define KEEPMOVINGFORWARD_H
#endif
