#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_math.h"


struct LevelPosition
{
    u32 tileX;
    u32 tileY;

    V2 offset;
};

enum EntityFlag {
    Entity_Valid = (1 << 0), 
    Entity_Swappable = (1 << 1), 
    Entity_Collidable = (1 << 2), 
    Entity_Airborne = (1 << 3), 
};

struct Entity
{
    u64 id; 

    // Physics
    Vector3 pos;
    Vector3 size;

    Vector3 velocity;
    Vector3 acceleration;

    u64 flags; 

    // Render
    V4 color;
};

#define KEEPMOVINGFORWARD_H
#endif
