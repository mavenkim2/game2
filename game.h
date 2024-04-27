#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"

#include "platform_inc.h"
#include "input.h"
#include "thread_context.h"
#include "job.h"

#include "physics.h"
#include "keepmovingforward_camera.h"
#include "render/render_core.h"
#include "asset.h"
#include "font.h"
#include "asset_cache.h"
#include "render/vertex_cache.h"
#include "render/render.h"
#include "shared.h"
#include "render/rendererapi.h"

#include "debug.h"
#include "entity.h"
#include "level.h"

//////////////////////////////
// Input
//
struct G_ButtonState
{
    b32 keyDown;
    u32 halfTransitionCount;
};

enum I_Button
{
    I_Button_Up,
    I_Button_Down,
    I_Button_Left,
    I_Button_Right,
    I_Button_Jump,
    I_Button_Shift,
    I_Button_Swap,
    I_Button_LeftClick,
    I_Button_RightClick,
    I_Button_Count,
};

struct G_Input
{
    union
    {
        G_ButtonState buttons[I_Button_Count];
        struct
        {
            G_ButtonState up;
            G_ButtonState down;
            G_ButtonState left;
            G_ButtonState right;
            G_ButtonState jump;
            G_ButtonState shift;
            G_ButtonState swap;
            G_ButtonState leftClick;
            G_ButtonState rightClick;
        };
    };
    V2 mousePos;
    V2 lastMousePos;
    V2 deltaMouse;
};

struct G_InputBindings
{
    OS_Key bindings[I_Button_Count];
};

//////////////////////////////
// : (
//
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

//////////////////////////////
// Input
//
struct GameSoundOutput
{
    i16 *samples;
    int samplesPerSecond;
    int sampleCount;
};

struct Manifold
{
    V3 normal;
    f32 penetration;
};

struct EntityState
{
    Entity *entities;
    u32 maxEntities;
    u32 entityCount;
};

struct G_State
{
    // Input
    G_Input input[2];
    i32 newInputIndex;
    G_InputBindings bindings;

    Arena *frameArena;
    Arena *permanentArena;

    u64 entity_id_gen;
    // TODO add entities
    Entity player;

    Level *level;

    Camera camera;

    // DebugBmpResult bmpTest;

    CameraMode cameraMode;

    AnimationPlayer animPlayer;

    // TODO: add entities please
    AS_Handle model;
    AS_Handle model2;
    AS_Handle eva;
    AS_Handle modelBall;

    AS_Handle font;

    Heightmap heightmap;
};

internal Manifold NarrowPhaseAABBCollision(const Rect3 a, const Rect3 b);
