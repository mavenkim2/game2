#include "mkCommon.h"
#include "mkMath.h"
#include "mkMemory.h"
#include "mkString.h"
#include "mkList.h"
#include "mkPlatformInc.h"

#include "render/mkGraphics.h"
#include "mkShaderCompiler.h"
#include "mkInput.h"
#include "mkThreadContext.h"
#include "mkJob.h"
#include "mkJobsystem.h"

#include "mkPhysics.h"
#include "mkCamera.h"
#include "render/mkRenderCore.h"
#include "mkAsset.h"
#include "mkFont.h"
#include "mkAssetCache.h"
// #include "render/mkVertexCache.h"
#include "render/mkRender.h"
#include "mkShared.h"
#include "render/mkRendererapi.h"

#include "mkDebug.h"
#include "mkEntity.h"
#include "mkLevel.h"
#include "mkScene.h"

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

namespace game
{
struct Entity
{
    AS_Handle mAssetHandle;
    i32 mSkinningOffset;
};
}; // namespace game

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
    u32 mSkinningBufferSize;

    CameraMode cameraMode;

    game::Entity mEntities[4];
    AnimationPlayer mAnimPlayers[4];
    Mat4 mTransforms[4];
    u32 mEntityCount = 0;

    // AS_Handle model;
    // AS_Handle model2;
    // AS_Handle eva;
    // AS_Handle modelBall;

    AS_Handle font;

    Heightmap heightmap;
};

PlatformApi platform;
Engine *engine;
Shared *shared;
graphics::mkGraphics *device;

internal Manifold NarrowPhaseAABBCollision(const Rect3 a, const Rect3 b);
