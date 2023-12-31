#include "keepmovingforward_entity.h"
#include "keepmovingforward_level.h"
#include "keepmovingforward_types.h"
#include <stdarg.h>

#pragma section("keepmovingforward", read)
// TODO: make this read only
#if _MSC_VER
#define readonly __declspec(allocate("keepmovingforward"))
global readonly Entity NIL_ENTITY = {};
#else
#define readonly const
global Entity NIL_ENTITY = {};
#endif

// TODO: remove num param
internal void AddFlag(int num, Entity *entity, ...)
{
    va_list args;
    va_start(args, entity);
    for (int i = 0; i < num; i++)
    {
        entity->flags |= (uint64)1 << va_arg(args, EntityFlag);
    }
    va_end(args);
}

internal void RemoveFlag(Entity *entity, EntityFlag flag) { entity->flags &= ~((uint64)1 << flag); }

internal bool HasFlag(Entity *entity, EntityFlag flag)
{
    bool result = entity->flags & ((uint64)1 << flag);
    return result;
}

internal bool IsValid(Entity *entity)
{
    bool result = entity->flags & (uint64)1 << Entity_Valid;
    return result;
}

// TODO: this seems scuffed
// NOTE: gets the next valid entity
internal bool IncrementEntity(Level *level, Entity **entityPtr)
{
    Entity *entities = level->entities;
    Entity *entity = *entityPtr;
    size_t startIndex = 0;
    if (entity != 0)
    {
        startIndex = entity - entities + 1;
    }
    entity = 0;
    for (size_t i = startIndex; i < MAX_ENTITIES; i++)
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
