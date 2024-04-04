#if COMPILER_MSVC
#pragma section(".roglob", read)
#define readonly __declspec(allocate(".roglob"))
#else
#define readonly
#endif

global readonly Entity NIL_ENTITY = {};

inline void AddFlag(Entity *entity, u64 flag)
{
    entity->flags |= flag;
}

inline void RemoveFlag(Entity *entity, u64 flag)
{
    entity->flags &= ~flag;
}

inline b64 HasFlag(Entity *entity, u64 flag)
{
    b64 result = entity->flags & flag;
    return result;
}

inline b64 IsValid(Entity *entity)
{
    b64 result = entity->flags & Entity_Valid;
    return result;
}

internal b32 IncrementEntity(Level *level, Entity **entityPtr)
{
    Entity *entities = level->entities;
    Entity *entity   = *entityPtr;
    u64 startIndex   = 0;
    if (entity != 0)
    {
        startIndex = entity - entities + 1;
    }
    entity = 0;
    for (u64 i = startIndex; i < MAX_ENTITIES; i++)
    {
        if (IsValid(entities + i) && (entities + i)->id != NIL_ENTITY.id)
        {
            entity = entities + i;
            break;
        }
    }
    *entityPtr = entity;
    return !!entity;
}

inline Entity *GetEntity(Level *level, int handle)
{
    Assert(handle < MAX_ENTITIES);

    Entity *entity = level->entities + handle;
    return entity;
}

inline Entity *GetPlayer(G_State *gameState)
{
    Entity *player = &gameState->player;
    return player;
}

inline Entity *CreateEntity(G_State *gameState, Level *level)
{
    Entity *entity = 0;
    for (int i = 1; i < MAX_ENTITIES; i++)
    {
        if (!HasFlag(level->entities + i, Entity_Valid))
        {
            entity = level->entities + i;
            break;
        }
    }
    // TODO: no 0 initialization?
    // *entity = {};
    entity->id = gameState->entity_id_gen++;

    return entity;
}

inline Entity *CreateWall(G_State *gameState, Level *level)
{
    Entity *wall = CreateEntity(gameState, level);

    AddFlag(wall, Entity_Valid | Entity_Collidable);
    return wall;
}

// NOTE: this assumes that the entity arg is in the level arg
inline void RemoveEntity(Level *level, Entity *entity)
{
    RemoveFlag(entity, Entity_Valid);
}
