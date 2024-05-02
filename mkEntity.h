#ifndef ENTITY_H
#define ENTITY_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#endif

//////////////////////////////
// Entities
//
struct LevelPosition
{
    u32 tileX;
    u32 tileY;

    V2 offset;
};

enum EntityFlag
{
    Entity_Valid      = (1 << 0),
    Entity_Swappable  = (1 << 1),
    Entity_Collidable = (1 << 2),
    Entity_Airborne   = (1 << 3),
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
    AS_Handle model;
    V4 color;
};

struct Level;
struct G_State;

inline void AddFlag(Entity *entity, u64 flag);
inline void RemoveFlag(Entity *entity, u64 flag);
inline b64 HasFlag(Entity *entity, u64 flag);
inline b64 IsValid(Entity *entity);
internal b32 IncrementEntity(Level *level, Entity **entityPtr);
inline Entity *GetEntity(Level *level, int handle);
inline Entity *GetPlayer(G_State *gameState);
inline Entity *CreateEntity(G_State *gameState, Level *level);
inline Entity *CreateWall(G_State *gameState, Level *level);

// NOTE: this assumes that the entity arg is in the level arg
inline void RemoveEntity(Level *level, Entity *entity);

#endif
