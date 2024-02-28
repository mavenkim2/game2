internal void JobThreadEntryPoint(void *p);
global JS_State *js_state;

//////////////////////////////
// Job System initialization
//
internal void JS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(8));
    js_state        = PushStruct(arena, JS_State);
    js_state->arena = arena;

    // Initialize stripes
    js_state->numStripes = 64;
    js_state->stripes    = PushArray(arena, JS_Stripe, js_state->numStripes);
    for (u32 i = 0; i < js_state->numStripes; i++)
    {
        js_state->stripes[i].arena   = ArenaAllocDefault();
        js_state->stripes[i].rwMutex = OS_CreateRWMutex();
    }

    // js_state->threadCount   = Clamp(OS_NumProcessors() - 1, 1, 8);
    js_state->threadCount   = OS_NumProcessors();
    js_state->readSemaphore = OS_CreateSemaphore(js_state->threadCount);

    // Initialize priority queues
    js_state->highPriorityQueue.writeSemaphore   = OS_CreateSemaphore(js_state->threadCount);
    js_state->normalPriorityQueue.writeSemaphore = OS_CreateSemaphore(js_state->threadCount);
    js_state->lowPriorityQueue.writeSemaphore    = OS_CreateSemaphore(js_state->threadCount);

    js_state->threads = PushArray(arena, JS_Thread, js_state->threadCount);

    // Main thread
    js_state->threads[0].handle = {};
    js_state->threads[0].arena  = ArenaAllocDefault();
    SetThreadIndex(0);
    for (u64 i = 1; i < js_state->threadCount; i++)
    {
        js_state->threads[i].handle = OS_ThreadStart(JobThreadEntryPoint, (void *)i);
        js_state->threads[i].arena  = ArenaAllocDefault();
    }
}

//////////////////////////////
// Job API Functions
//
internal JS_Ticket JS_Kick(JobCallback *callback, void *data, Arena **arena, Priority priority,
                           JS_Ticket *optionalTicket = 0)
{
    u64 numJobs       = AtomicIncrementU64(&js_state->numJobs);
    u64 stripeIndex   = numJobs % js_state->numStripes;
    JS_Stripe *stripe = js_state->stripes + stripeIndex;
    JS_Queue *queue   = 0;

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

    // Create ticket w/ a job/counter. This will store info about the job's execution and its output.
    JS_Counter *counter = 0;
    JS_Ticket ticket    = {};

    if (optionalTicket != 0)
    {
        counter = (JS_Counter *)optionalTicket->u64[1];
        ticket  = *optionalTicket;
    }
    else
    {
        OS_TakeWMutex(stripe->rwMutex);
        counter = stripe->freeCounter;
        if (counter == 0)
        {
            counter = PushStruct(stripe->arena, JS_Counter);
        }
        else
        {
            StackPop(stripe->freeCounter);
        }
        OS_DropWMutex(stripe->rwMutex);
        ticket = {numJobs, (u64)counter};
    }
    AtomicIncrementU32(&counter->c);

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
            newJob->ticket   = ticket;

            EndMutex(&queue->lock);
            OS_ReleaseSemaphore(js_state->readSemaphore);

            if (arena != 0)
            {
                *arena = 0;
            }
            break;
        }
        EndMutex(&queue->lock);
        OS_SignalWait(queue->writeSemaphore);
    }
    return ticket;
}

internal void JS_Join(JS_Ticket ticket)
{
    void *result        = 0;
    u64 id              = ticket.u64[0];
    JS_Counter *counter = (JS_Counter *)ticket.u64[1];

    u64 stripeIndex   = id % js_state->numStripes;
    JS_Stripe *stripe = js_state->stripes + stripeIndex;

    u32 threadIndex = GetThreadIndex();

    JS_Thread *thread = &js_state->threads[threadIndex];

    for (;;)
    {
        b32 taskCompleted = (AtomicAddU64(&counter->c, 0) == 0);
        if (taskCompleted)
        {
            OS_TakeWMutex(stripe->rwMutex);

            StackPush(stripe->freeCounter, counter);

            OS_DropWMutex(stripe->rwMutex);
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
internal b32 JS_PopJob(JS_Queue *queue, JS_Thread *thread)
{
    b32 sleep = false;
    BeginMutex(&queue->lock);
    u64 curReadPos  = queue->readPos;
    u64 curWritePos = queue->writePos;

    if (curWritePos - curReadPos >= 1)
    {
        queue->readPos += 1;
        // Read the correct job
        Job *job          = &queue->jobs[curReadPos & (ArrayLength(queue->jobs) - 1)];
        JS_Ticket ticket  = job->ticket;
        Arena *arena      = job->arena;
        void *data        = job->data;
        JobCallback *func = job->callback;

        EndMutex(&queue->lock);
        OS_ReleaseSemaphore(queue->writeSemaphore);

        if (arena == 0)
        {
            arena = thread->arena;
        }
        // Execute
        void *result = func(data, arena);

        u64 id = ticket.u64[0];

        // Decrement counter
        JS_Counter *counter = (JS_Counter *)ticket.u64[1];
        AtomicDecrementU32(&counter->c);
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
    u64 threadIndex   = (u64)p;
    JS_Thread *thread = &js_state->threads[threadIndex];

    TempArena temp = ScratchStart(0, 0);
    SetThreadName(PushStr8F(temp.arena, "[JS] Worker %u", threadIndex));
    ScratchEnd(temp);

    for (;;)
    {
        if (JS_PopJob(&js_state->highPriorityQueue, thread) && JS_PopJob(&js_state->normalPriorityQueue, thread) &&
            JS_PopJob(&js_state->lowPriorityQueue, thread))
        {
            OS_SignalWait(js_state->readSemaphore);
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

    JS_Ticket ticket = JS_Kick(TestCall3, d, 0, Priority_Normal);
    JS_Join(ticket);

    return 0;
}
