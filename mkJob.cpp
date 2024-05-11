#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "job.h"
#include "thread_context.h"
#include "platform_inc.h"
#endif

global b32 volatile gTerminateJobThreads;
internal void JobThreadEntryPoint(void *p);

//////////////////////////////
// Job System initialization
//
internal void JS_Init()
{
    Arena *arena       = ArenaAlloc(megabytes(8));
    JS_State *js_state = PushStruct(arena, JS_State);
    js_state->arena    = arena;
    engine->SetJobState(js_state);

    // js_state->threadCount   = Clamp(OS_NumProcessors() - 1, 1, 8);
    js_state->threadCount   = platform.NumProcessors();
    js_state->readSemaphore = platform.CreateSemaphore(js_state->threadCount);

    // Initialize priority queues
    js_state->highPriorityQueue.writeSemaphore   = platform.CreateSemaphore(js_state->threadCount);
    js_state->normalPriorityQueue.writeSemaphore = platform.CreateSemaphore(js_state->threadCount);
    js_state->lowPriorityQueue.writeSemaphore    = platform.CreateSemaphore(js_state->threadCount);

    js_state->threads = PushArray(arena, JS_Thread, js_state->threadCount);

    // Main thread
    js_state->threads[0].handle = {};
    js_state->threads[0].arena  = ArenaAlloc();
    SetThreadIndex(0);
    for (u64 i = 1; i < js_state->threadCount; i++)
    {
        js_state->threads[i].handle = platform.ThreadStart(JobThreadEntryPoint, (void *)i);
        js_state->threads[i].arena  = ArenaAlloc();
    }
}

// Flush job queue
internal void JS_Flush()
{
    gTerminateJobThreads = 1;
    WriteBarrier();
    JS_State *js_state = engine->GetJobState();
    JS_Thread *thread  = &js_state->threads[0];
    while (!JS_PopJob(&js_state->highPriorityQueue, thread) || !JS_PopJob(&js_state->normalPriorityQueue, thread) ||
           !JS_PopJob(&js_state->lowPriorityQueue, thread))
        ;

    platform.ReleaseSemaphores(js_state->readSemaphore, js_state->threadCount);

    for (u64 i = 1; i < js_state->threadCount; i++)
    {
        platform.ThreadJoin(js_state->threads[i].handle);
    }
}

internal void JS_Restart()
{
    gTerminateJobThreads = 0;
    JS_State *js_state   = engine->GetJobState();
    for (u64 i = 1; i < js_state->threadCount; i++)
    {
        js_state->threads[i].handle = platform.ThreadStart(JobThreadEntryPoint, (void *)i);
    }
}

//////////////////////////////
// Job API Functions
//
internal void JS_Kick(JobCallback *callback, void *data, Arena **arena, Priority priority, JS_Counter *counter = 0)
{
    JS_State *js_state = engine->GetJobState();
    u64 numJobs        = AtomicIncrementU64(&js_state->numJobs);
    JS_Queue *queue    = 0;

    switch (priority)
    {
        default:
        case Priority_Low:
        {
            queue = &js_state->lowPriorityQueue;
            break;
        }
        case Priority_Normal:
        {
            queue = &js_state->normalPriorityQueue;
            break;
        }
        case Priority_High:
        {
            queue = &js_state->highPriorityQueue;
            break;
        }
    }

    b32 success = 0;

    // Increments counter if one is passed in.
    if (counter != 0)
    {
        AtomicIncrementU32(&counter->c);
    }

    // Queue a task.
    for (;;)
    {
        BeginMutex(&queue->lock);
        u64 curWritePos = queue->writePos;
        u64 curReadPos  = queue->readPos;

        u64 availableSlots = ArrayLength(queue->jobs) - (curWritePos - curReadPos);
        if (availableSlots >= 1)
        {
            queue->writePos += 1;
            success          = 1;
            Arena *taskArena = 0;
            if (arena != 0)
            {
                taskArena = *arena;
            }
            Job *newJob      = queue->jobs + (curWritePos & (ArrayLength(queue->jobs) - 1));
            newJob->callback = callback;
            newJob->data     = data;
            newJob->arena    = taskArena;
            newJob->counter  = counter;

            EndMutex(&queue->lock);
            platform.ReleaseSemaphore(js_state->readSemaphore);

            if (arena != 0)
            {
                *arena = 0;
            }
            break;
        }
        EndMutex(&queue->lock);
        platform.SignalWait(queue->writeSemaphore);
    }
}

internal void JS_Join(JS_Counter *counter)
{
    JS_State *js_state = engine->GetJobState();
    u32 threadIndex    = GetThreadIndex();

    JS_Thread *thread = &js_state->threads[threadIndex];

    for (;;)
    {
        u64 result        = AtomicAddU32(&counter->c, 0);
        b32 taskCompleted = (result == 0);
        if (taskCompleted)
        {
            break;
        }

        // NOTE: crazytown incoming

        if (JS_PopJob(&js_state->highPriorityQueue, thread) && JS_PopJob(&js_state->normalPriorityQueue, thread) &&
            JS_PopJob(&js_state->lowPriorityQueue, thread))
        {
            _mm_pause();
        }
    }
}

//////////////////////////////
// Worker Thread Tasks
//

// TODO: should the job system "own" the results of the job callbacks, or should the callback handle how the
// data is handled?
internal b32 JS_PopJob(JS_Queue *queue, JS_Thread *thread)
{
    JS_State *js_state = engine->GetJobState();
    b32 sleep          = false;
    BeginMutex(&queue->lock);
    u64 curReadPos  = queue->readPos;
    u64 curWritePos = queue->writePos;

    if (curWritePos - curReadPos >= 1)
    {
        queue->readPos += 1;
        // Read the correct job
        Job *job            = &queue->jobs[curReadPos & (ArrayLength(queue->jobs) - 1)];
        JS_Counter *counter = job->counter;
        Arena *arena        = job->arena;
        void *data          = job->data;
        JobCallback *func   = job->callback;

        EndMutex(&queue->lock);
        platform.ReleaseSemaphore(queue->writeSemaphore);

        if (arena == 0)
        {
            arena = thread->arena;
        }
        // Execute
        void *result = func(data, arena);

        // Decrement counter
        if (counter)
        {
            AtomicDecrementU32(&counter->c);
        }
    }
    else
    {
        EndMutex(&queue->lock);
        sleep = true;
    }
    return sleep;
}

internal void JobThreadEntryPoint(void *p)
{
    JS_State *js_state = engine->GetJobState();
    u64 threadIndex    = (u64)p;
    JS_Thread *thread  = &js_state->threads[threadIndex];

    TempArena temp = ScratchStart(0, 0);
    SetThreadName(PushStr8F(temp.arena, "[JS] Worker %u", threadIndex));
    ScratchEnd(temp);

    for (; !gTerminateJobThreads;)
    {
        if (JS_PopJob(&js_state->highPriorityQueue, thread) && JS_PopJob(&js_state->normalPriorityQueue, thread) &&
            JS_PopJob(&js_state->lowPriorityQueue, thread))
        {
            platform.SignalWait(js_state->readSemaphore);
        }
    }
}

//////////////////////////////
// Test
//
struct DumbData
{
    u32 j;
};

JOB_CALLBACK(TestCall2)
{
    DumbData *d = (DumbData *)data;
    AtomicAddU64(&d->j, 5);
    return 0;
}

JOB_CALLBACK(TestCall3)
{
    DumbData *d = (DumbData *)data;
    AtomicAddU64(&d->j, 3);
    return 0;
}

JOB_CALLBACK(TestCall1)
{
    DumbData *d = (DumbData *)data;
    AtomicAddU64(&d->j, 4);

    JS_Counter counter = {};
    JS_Kick(TestCall3, d, 0, Priority_Normal, &counter);
    JS_Join(&counter);

    return 0;
}
