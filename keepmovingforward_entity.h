#ifndef KEEPMOVINGFORWARD_H
#include "keepmovingforward_math.h"


struct LevelPosition
{
    uint32 tileX;
    uint32 tileY;

    v2 offset;
};

enum EntityFlag {
    Entity_Valid = 0, 
    Entity_Swappable = 1, 
    Entity_Collidable = 2, 
    Entity_Airborne = 3, 

    Entity_Max, 
};

struct Entity
{
    uint64 id; 

    // Physics
    Vector2 pos;
    Vector2 size;

    Vector2 velocity;
    Vector2 acceleration;

    uint64 flags; 

    // Render
    float r;
    float g;
    float b;
};

#define KEEPMOVINGFORWARD_H
#endif
