#ifndef KEEPMOVINGFORWARD_H
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
    V3 pos;
    V3 size;

    V3 velocity;
    V3 acceleration;

    u64 flags; 

    // Render
    V4 color;
};

#define KEEPMOVINGFORWARD_H
#endif
