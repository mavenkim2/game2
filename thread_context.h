#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

struct ThreadContext
{
    Arena *arenas[2];
};

internal void ThreadContextInitialize(ThreadContext *t);
internal void ThreadContextRelease();
internal ThreadContext *ThreadContextGet();

internal Arena *ThreadContextScratch(Arena **conflicts, u32 count);

#define ScratchStart(conflicts, count) TempBegin(ThreadContextScrfatch((conflicts), (count)))

#endif
