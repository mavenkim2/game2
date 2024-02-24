thread_global ThreadContext *tLocalContext;

internal void ThreadContextInitialize(ThreadContext *t)
{
    for (u32 i = 0; i < ArrayLength(t->arenas); i++)
    {
        t->arenas[i] = ArenaAllocDefault();
    }
    tLocalContext = t;
}

internal void ThreadContextRelease()
{
    for (u32 i = 0; i < ArrayLength(tLocalContext->arenas); i++)
    {
        ArenaRelease(tLocalContext->arenas[i]);
    }
}

internal ThreadContext *ThreadContextGet()
{
    return tLocalContext;
}

internal Arena *ThreadContextScratch(Arena **conflicts, u32 count)
{
    ThreadContext *t = ThreadContextGet();
    Arena *result    = 0;
    for (u32 i = 0; i < ArrayLength(tLocalContext->arenas); i++)
    {
        Arena *arenaPtr = tLocalContext->arenas[i];
        b32 hasConflict = 0;
        for (u32 j = 0; j < count; j++)
        {
            Arena *conflictPtr = conflicts[j];
            if (arenaPtr == conflictPtr)
            {
                hasConflict = 1;
                break;
            }
        }
        if (!hasConflict)
        {
            result = arenaPtr;
            return result;
        }
    }
    return result;
}
