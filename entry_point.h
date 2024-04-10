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
#include "./offline/asset_processing.h"
#include "font.h"
#include "asset_cache.h"
#include "render/render.h"

#include "debug.h"
#include "entity.h"
#include "level.h"
#include "game.h"

#include "render/opengl.h"

struct AtomicRing
{
    u8 *buffer;
    u64 size;
    u64 volatile readPos;
    u64 volatile commitReadPos;
    u64 volatile writePos;
    u64 volatile commitWritePos;
};

// g2r = Game To Render
struct Shared
{
    OS_Handle windowHandle;

    AtomicRing g2rRing;

    // Input to Game
    AtomicRing i2gRing;
    b8 running;
};

global Shared *shared;
