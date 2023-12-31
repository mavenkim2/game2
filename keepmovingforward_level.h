#ifndef KEEPMOVINGFORWARD_TILES_H
#define KEEPMOVINGFORWARD_TILES_H

// NOTE: For now, 320 x 180 pixels displayed on the screen, 40 x 22.5 tiles
// Each tile is 8x8 pixels
// IMPORTANT: 1 is ground, 2 is wall
#include "keepmovingforward_entity.h"
#include "keepmovingforward_math.h"

#define TILE_MAP_X_COUNT 80
#define TILE_MAP_Y_COUNT 46
#define MAX_ENTITIES 4096

const int TILE_PIXEL_SIZE = 8;
const float TILE_METER_SIZE = 1.f;
const float METERS_TO_PIXELS = TILE_PIXEL_SIZE / TILE_METER_SIZE;

struct Entity;

struct Level
{
    u32 levelWidth;
    u32 levelHeight;

    Entity entities[MAX_ENTITIES];
    // int entityCount;
};

// NOTE: test code
#endif
