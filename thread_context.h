#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

struct ThreadContext
{
    Arena *arenas[2];
    u8 threadName[64];
    u64 threadNameSize;
};

internal void ThreadContextInitialize(ThreadContext *t);
internal void ThreadContextRelease();
internal ThreadContext *ThreadContextGet();

internal Arena *ThreadContextScratch(Arena **conflicts, u32 count);

internal void SetThreadName(string name);

#define ScratchStart(conflicts, count) TempBegin(ThreadContextScratch((conflicts), (count)))

#endif
