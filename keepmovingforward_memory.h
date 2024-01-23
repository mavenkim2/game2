#ifndef KEEPMOVINGFORWARD_MEMORY_H
#define KEEPMOVINGFORWARD_MEMORY_H

struct Arena {
    u64 pos; 
    u64 commitPos;
    u64 align; 
    u64 size; 
    u64 _unused_[5];
};

struct TempArena {
    Arena* arena; 
    u64 pos;
};

internal Arena* ArenaAlloc(void *base, u64 size);
internal void *ArenaPushNoZero(Arena *arena, u64 size);
internal void *ArenaPush(Arena *arena, u64 size);
internal void ArenaPopTo(Arena *arena, u64 pos);

internal TempArena TempBegin(Arena* arena);
internal void TempEnd(TempArena temp);

#define PushArrayNoZero(arena, type, count) (type *)ArenaPushNoZero(arena, sizeof(type) * (count))
#define PushStructNoZero(arena, type) (type *)PushArrayNoZero(arena, type, 1)
#define PushArray(arena, type, count) (type *)ArenaPush(arena, sizeof(type) * (count))
#define PushStruct(arena, type) (type *)PushArray(arena, type, 1)

#define ScratchBegin(arena) TempBegin(arena)
#define ScratchEnd(temp) TempEnd(temp)

#endif
