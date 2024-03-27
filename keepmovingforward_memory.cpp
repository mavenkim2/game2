#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_memory.h"
#include "platform_inc.h"
#endif

internal Arena *ArenaAlloc(u64 resSize, u64 cmtSize)
{
    u64 pageSize = OS_PageSize();
    resSize      = AlignPow2(resSize, pageSize);
    cmtSize      = AlignPow2(cmtSize, pageSize);

    void *memory = OS_Reserve(resSize);
    if (!OS_Commit(memory, cmtSize))
    {
        memory = 0;
        OS_Release(memory);
    }

    Arena *arena = (Arena *)memory;
    if (arena)
    {
        arena->prev    = 0;
        arena->current = arena;
        arena->basePos = 0;
        arena->pos     = ARENA_HEADER_SIZE;
        arena->cmt     = cmtSize;
        arena->res     = resSize;
        arena->align   = 8;
        arena->grow    = 1;
    }

    return arena;
}

internal Arena *ArenaAlloc(u64 size)
{
    Arena *result = ArenaAlloc(size, ARENA_COMMIT_SIZE);
    return result;
}

internal Arena *ArenaAlloc()
{
    Arena *result = ArenaAlloc(ARENA_RESERVE_SIZE, ARENA_COMMIT_SIZE);
    return result;
}

internal void *ArenaPushNoZero(Arena *arena, u64 size)
{
    Arena *current      = arena->current;
    u64 currentAlignPos = AlignPow2(current->pos, current->align);
    u64 newPos          = currentAlignPos + size;
    if (current->res < newPos && current->grow)
    {
        Arena *newArena = 0;
        if (size < ARENA_RESERVE_SIZE / 2 + 1)
        {
            newArena = ArenaAlloc();
        }
        else
        {
            u64 newBlockSize = size + ARENA_HEADER_SIZE;
            newArena         = ArenaAlloc(newBlockSize, newBlockSize);
        }
        if (newArena)
        {
            newArena->basePos = current->basePos + current->res;
            newArena->prev    = current;
            arena->current    = newArena;
            current           = newArena;
            currentAlignPos   = AlignPow2(current->pos, current->align);
            newPos            = currentAlignPos + size;
        }
        else
        {
            Assert(!"Arena alloc failed");
        }
    }
    if (current->cmt < newPos)
    {
        u64 cmtAligned = AlignPow2(newPos, ARENA_COMMIT_SIZE);
        cmtAligned     = Min(cmtAligned, current->res);
        u64 cmtSize    = cmtAligned - current->cmt;
        b8 result      = OS_Commit((u8 *)current + current->cmt, cmtSize);
        Assert(result);
        current->cmt = cmtAligned;
    }
    void *result = 0;
    if (current->cmt >= newPos)
    {
        result       = (u8 *)current + currentAlignPos;
        current->pos = newPos;
    }
    if (result == 0)
    {
        Assert(!"Allocation failed");
    }
    return result;
}

internal void *ArenaPush(Arena *arena, u64 size)
{
    void *result = ArenaPushNoZero(arena, size);
    MemoryZero(result, size);
    return result;
}

internal void ArenaPopTo(Arena *arena, u64 pos)
{
    pos            = Max(ARENA_HEADER_SIZE, pos);
    Arena *current = arena->current;
    for (Arena *prev = 0; current->basePos >= pos; current = prev)
    {
        prev = current->prev;
        OS_Release(current);
    }
    Assert(current);
    u64 newPos = pos - current->basePos;
    Assert(newPos <= current->pos);
    current->pos = newPos;
}

internal void ArenaPopToZero(Arena *arena, u64 pos)
{
    pos            = Max(ARENA_HEADER_SIZE, pos);
    Arena *current = arena->current;
    for (Arena *prev = 0; current->basePos >= pos; current = prev)
    {
        prev = current->prev;
        OS_Release(current);
    }
    Assert(current);
    u64 newPos = pos - current->basePos;
    Assert(newPos <= current->pos);
    current->pos = newPos;
}

internal u64 ArenaPos(Arena *arena)
{
    Arena *current = arena->current;
    u64 pos        = current->basePos + current->pos;
    return pos;
}

internal TempArena TempBegin(Arena *arena)
{
    u64 pos        = ArenaPos(arena);
    TempArena temp = {arena, pos};
    return temp;
}

internal void TempEnd(TempArena temp)
{
    ArenaPopTo(temp.arena, temp.pos);
}

internal b32 CheckZero(u32 size, u8 *instance)
{
    b32 result = true;
    while (size-- > 0)
    {
        if (*instance++)
        {
            result = false;
            break;
        }
    }
    return result;
}

internal void ArenaClear(Arena *arena)
{
    ArenaPopTo(arena, ARENA_HEADER_SIZE);
}

internal void ArenaRelease(Arena *arena)
{
    for (Arena *a = arena->current, *prev = 0; a != 0; a = prev)
    {
        prev = a->prev;
        OS_Release(a);
    }
}
