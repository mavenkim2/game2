#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkCommon.h"
#include "mkMath.h"
#include "mkMemory.h"
#include "mkString.h"
#include "mkPlatformInc.h"
#endif

struct ThreadContext
{
    Arena *arenas[2];
    u8 threadName[64];
    u64 threadNameSize;

    b32 isMainThread;
    u32 index;
};

internal void ThreadContextInitialize(ThreadContext *t, b32 isMainThread);
internal void ThreadContextRelease();
internal ThreadContext *ThreadContextGet();
internal Arena *ThreadContextScratch(Arena **conflicts, u32 count);
internal void SetThreadName(string name);
internal void SetThreadIndex(u32 index);
internal u32 GetThreadIndex();
internal void BaseThreadEntry(OS_ThreadFunction *func, void *params);

#define ScratchStart(conflicts, count) TempBegin(ThreadContextScratch((conflicts), (count)))

#endif
