#include "keepmovingforward_memory.h"
// TODO: have the arena allocate the memory itself, instead of having to allocate separately and then pass in 
internal Arena* ArenaAlloc(void *base, u64 size) {
    Arena* arena = (Arena*)base;
    arena->pos = sizeof(Arena);
    arena->align = 8;
    arena->size = size;
    return arena;
}

internal void *ArenaPush(Arena *arena, u64 size) {
    Assert(arena->pos + size <= arena->size);
    void* result = (void*)((u8*)(arena) + arena->pos);
    arena->pos += size;
    return result;
}

internal void ArenaPopTo(Arena *arena, u64 pos) {
    Assert(sizeof(Arena) <= pos && pos <= arena->pos + arena->size);
    arena->pos = pos;
}

internal TempArena TempBegin(Arena* arena) {
    TempArena temp = {};
    temp.arena = arena; 
    temp.pos = arena->pos;
    return temp;
}

internal void TempEnd(TempArena temp) {
    ArenaPopTo(temp.arena, temp.pos);
}
