#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#include "platform_inc.h"
#include "input.h"
#include "debug.h"
#include "thread_context.h"
#include "job.h"

#include "physics.h"
#include "keepmovingforward_camera.h"
#include "render/render_core.h"
#include "asset.h"
#include "./offline/asset_processing.h"
#include "font.h"
#include "asset_cache.h"
#include "render/render.h"
#include "keepmovingforward_platform.h"

#include "entity.h"
#include "level.h"
#include "game.h"

#include "render/opengl.h"

struct AtomicRing
{
    u8 *buffer;
    u64 size;
    u64 readPos;
    u64 commitReadPos;
    u64 writePos;
    u64 commitWritePos;
};

// g2r = Game To Render
struct Shared
{
    OS_Handle windowHandle;
    // u8 *g2rRingBuffer;
    // u64 g2rReadPos;
    //
    // u64 g2rWritePos;
    // u64 g2rCommitWritePos;

    // Input to Game
    AtomicRing i2gRing;

    // u8 *i2gRingBuffer;
    // u64 i2gRingBufferSize;
    // u64 i2gReadPos;
    // u64 i2gWritePos;

    b8 running;
};

global Shared *shared;
