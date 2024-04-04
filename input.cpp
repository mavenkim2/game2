#include "crack.h"
#ifdef LSP_INCLUDE
#include "asset_cache.h"
#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "platform_inc.h"
#endif

internal u64 StartAtomicRead(AtomicRing *ring, u64 size)
{
    u64 commitSize = 0;
    for (;;)
    {
        u64 readPos       = ring->readPos;
        u64 writePos      = ring->writePos;
        u64 commitReadPos = ring->commitReadPos;

        Assert(commitReadPos >= readPos);

        u64 alignedReadPos = AlignPow2(readPos + size, 8);

        u64 availableSize = writePos - readPos;
        if (availableSize >= size)
        {
            if (AtomicCompareExchangeU64(&ring->commitReadPos, alignedReadPos, readPos) == readPos)
            {
                commitSize = size;
                break;
            }
        }
    }
    return commitSize;
}

internal u64 StartAtomicWrite(AtomicRing *ring, u64 size)
{
    u64 committedSize = 0;
    for (;;)
    {
        u64 readPos        = ring->readPos;
        u64 writePos       = ring->writePos;
        u64 commitWritePos = ring->commitWritePos;

        Assert(commitWritePos >= writePos);

        u64 alignedWritePos = AlignPow2(writePos + size, 8);

        u64 availableSize = ring->size - (writePos - readPos);

        if (availableSize >= size)
        {
            if (AtomicCompareExchangeU64(&ring->commitWritePos, alignedWritePos, writePos) == writePos)
            {
                committedSize = size;
                break;
            }
        }
    }
    return committedSize;
}

internal void EndAtomicRead(AtomicRing *ring, u64 size)
{
    ring->commitReadPos += size;
    WriteBarrier();
    ring->readPos = ring->commitReadPos;
}

internal void EndAtomicWrite(AtomicRing *ring)
{
    WriteBarrier();
    ring->writePos = ring->commitWritePos;
}

internal void I_Init() {}

internal void I_PollInput()
{
    TempArena temp = ScratchStart(0, 0);

    OS_Events events = OS_GetEvents(temp.arena);

    u64 eventsSize = sizeof(events.events[0]) * events.numEvents;
    u64 commitSize = sizeof(u64) + eventsSize;

    AtomicRing *inputRing = &shared->i2gRing;

    StartAtomicWrite(inputRing, commitSize);
    u64 writePos = inputRing->writePos;
    writePos += RingWriteStruct(inputRing->buffer, inputRing->size, writePos, &eventsSize);
    RingWrite(inputRing->buffer, inputRing->size, writePos, events.events, eventsSize);
    EndAtomicWrite(inputRing);

    ScratchEnd(temp);
}

internal OS_Events I_GetInput(Arena *arena)
{
    StartAtomicRead(&shared->i2gRing, sizeof(u64));
    u64 size    = 0;
    u64 readPos = shared->i2gRing.readPos;
    readPos += RingReadStruct(shared->i2gRing.buffer, shared->i2gRing.size, readPos, &size);
    size = AlignPow2(size, 8);
    OS_Events events;
    events.numEvents = (u32)(size / sizeof(OS_Event));
    events.events    = PushArray(arena, OS_Event, events.numEvents);
    RingRead(shared->i2gRing.buffer, shared->i2gRing.size, readPos, events.events, size);
    EndAtomicRead(&shared->i2gRing, size);

    return events;
}

#if 0
internal void I_EntryPoint(void *p)
{
    I_Init();

    f32 dt          = 1 / 144.f;
    f32 currentTime = OS_GetWallClock();
    f32 lastTime    = 0.f;

    for (; shared->running == 1;)
    {
        lastTime        = currentTime;
        currentTime = OS_GetWallClock();
        f32 timeElapsed = currentTime - lastTime;

        I_PollInput();

        OS_Sleep(timeElapsed - dt);

        for (; currentTime >= lastTime + dt;)
        {
        }
    }
}
#endif
