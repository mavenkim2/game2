#ifndef KEEPMOVINGFORWARD_MEMORY_H
#define KEEPMOVINGFORWARD_MEMORY_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#endif

#define ARENA_HEADER_SIZE  128
#define ARENA_COMMIT_SIZE  kilobytes(64)
#define ARENA_RESERVE_SIZE megabytes(64)

struct Arena
{
    struct Arena *prev;
    struct Arena *current;
    u64 basePos;
    u64 pos;
    u64 cmt;
    u64 res;
    u64 align;
    b8 grow;
};

struct TempArena
{
    Arena *arena;
    u64 pos;
};

internal Arena *ArenaAlloc(u64 resSize, u64 cmtSize);
internal Arena *ArenaAlloc(u64 size);
internal Arena *ArenaAlloc();
internal void *ArenaPushNoZero(Arena *arena, u64 size);
internal void *ArenaPush(Arena *arena, u64 size);
internal u64 ArenaPos(Arena *arena);
internal void ArenaPopTo(Arena *arena, u64 pos);
internal TempArena TempBegin(Arena *arena);
internal void TempEnd(TempArena temp);
internal b32 CheckZero(u32 size, u8 *instance);
internal void ArenaRelease(Arena *arena);
internal void ArenaClear(Arena *arena);

#define PushArrayNoZero(arena, type, count) (type *)ArenaPushNoZero(arena, sizeof(type) * (count))
#define PushStructNoZero(arena, type)       (type *)PushArrayNoZero(arena, type, 1)
#define PushArray(arena, type, count)       (type *)ArenaPush(arena, sizeof(type) * (count))
#define PushStruct(arena, type)             (type *)PushArray(arena, type, 1)

#define ScratchBegin(arena) TempBegin(arena)
#define ScratchEnd(temp)    TempEnd(temp)

#define IsZero(instance) CheckZero(sizeof(instance), (u8 *)(&instance))

#endif
