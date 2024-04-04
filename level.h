#ifndef LEVEL_H
#define LEVEL_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "entity.h"
#endif

#define MAX_ENTITIES 4096

const int TILE_PIXEL_SIZE  = 8;
const f32 TILE_METER_SIZE  = 1.f;
const f32 METERS_TO_PIXELS = TILE_PIXEL_SIZE / TILE_METER_SIZE;

struct Level
{
    u32 levelWidth;
    u32 levelHeight;

    Entity entities[MAX_ENTITIES];
    // int entityCount;
};

struct G_State;

// NOTE: test code
#endif
