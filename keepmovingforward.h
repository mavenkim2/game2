#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#include "platform.h"

#if WINDOWS
#include "win32.h"
#endif

#include "keepmovingforward_math.h"
#include "physics.h"
#include "keepmovingforward_camera.h"
#include "keepmovingforward_platform.h"
#include "asset.h"
#include "asset_cache.h"
#include "render/render.h"

#include "keepmovingforward_entity.h"
#include "keepmovingforward_level.h"

global Arena *scratchArena;

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
    u64 entity_id_gen;
    Entity player;

    Arena *worldArena;
    Level *level;

    Camera camera;

    DebugBmpResult bmpTest;

    CameraMode cameraMode;

    // TODO IMPORTANT: TEMPORARY, move transforms into animation related struct
    // also mesh node hierarchy should use id's for parent name.
    // also need to use the same hierarchy for animation/skeleton/mesh
    AnimationPlayer animPlayer;
    AnimationTransform *tforms;
    MeshNodeInfoArray *meshNodeHierarchy;
    Mat4 *finalTransforms;

    Model model;

    AssetState assetState;
    OS_JobQueue *highPriorityQueue;
};

global os_queue_job *OS_QueueJob;
global r_allocate_texture_2D *R_AllocateTexture2D;
global r_submit_texture_2D *R_SubmitTexture2D;
