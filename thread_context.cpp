#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#include "thread_context.h"
#endif

thread_global ThreadContext *tLocalContext;

internal void ThreadContextInitialize(ThreadContext *t, b32 isMainThread = 0)
{
    for (u32 i = 0; i < ArrayLength(t->arenas); i++)
    {
        t->arenas[i] = ArenaAllocDefault();
    }
    t->isMainThread = isMainThread;
    tLocalContext   = t;
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

internal void SetThreadName(string name)
{
    ThreadContext *context  = ThreadContextGet();
    context->threadNameSize = Min(name.size, sizeof(context->threadName));
    MemoryCopy(context->threadName, name.str, context->threadNameSize);
    OS_SetThreadName(name);
}

internal void SetThreadIndex(u32 index)
{
    ThreadContext *context = ThreadContextGet();
    context->index         = index;
}

internal u32 GetThreadIndex()
{
    ThreadContext *context = ThreadContextGet();
    u32 result             = context->index;
    return result;
}

internal void BaseThreadEntry(OS_ThreadFunction *func, void *params)
{
    ThreadContext tContext_ = {};
    ThreadContextInitialize(&tContext_);
    func(params);
    ThreadContextRelease();
}
