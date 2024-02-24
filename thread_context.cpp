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
