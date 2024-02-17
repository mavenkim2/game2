internal Arena *ArenaAlloc(u64 size)
{
    // TODO: hardcoded?
    u64 reserveGranularity = megabytes(64);
    size                   = AlignPow2(size, reserveGranularity);
    void *memory           = OS_Alloc(size);
    Arena *arena           = (Arena *)memory;
    if (arena)
    {
        arena->pos   = sizeof(Arena);
        arena->align = 8;
        arena->size  = size;
    }
    else
    {
        Assert(!"Allocation failed");
    }
    return arena;
}

internal Arena *ArenaAllocDefault()
{
    Arena *result = ArenaAlloc(kilobytes(64));
    return result;
}
internal Arena *ArenaAlloc(void *base, u64 size)
{
    Arena *arena = (Arena *)base;
    arena->pos   = sizeof(Arena);
    arena->align = 8;
    arena->size  = size;
    return arena;
}

internal void *ArenaPushNoZero(Arena *arena, u64 size)
{
    Assert(arena->pos + size <= arena->size);
    void *result = 0;
    u8 *base     = (u8 *)arena;

    u64 alignPos = AlignPow2(arena->pos, arena->align);
    u64 align    = alignPos - arena->pos;

    result = (void *)(base + alignPos);
    arena->pos += size + align;
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
    Assert(sizeof(Arena) <= pos && pos <= arena->pos + arena->size);
    arena->pos = pos;
}

internal TempArena TempBegin(Arena *arena)
{
    TempArena temp = {};
    temp.arena     = arena;
    temp.pos       = arena->pos;
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
