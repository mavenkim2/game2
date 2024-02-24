#define JOB_CALLBACK(name) void *name(void *data, Arena *arena)
typedef JOB_CALLBACK(JobCallback);

enum Priority
{
    Priority_Low,
    Priority_Normal,
    Priority_High,
    Priority_Count,
};

struct JS_Counter
{
    u32 c;
    JS_Counter *next;
};

struct JS_Ticket
{
    u64 u64[3];
};

struct Job
{
    JobCallback *callback;
    void *data;
    Arena *arena;
    JS_Ticket ticket;
};

struct JS_Queue
{
    u64 volatile writePos;
    u64 volatile readPos;

    Job jobs[256];

    // If there are somehow not enough space to queue jobs, wait on semaphore.
    OS_Handle writeSemaphore;
};

struct JS_Node
{
    void *result;
    b32 isCompleted;
    u64 id;
    JS_Node *next;
};

// struct JobSlot
// {
//     JS_Node *first;
//     JS_Node *last;
// };

struct JS_Thread
{
    OS_Handle handle;
    Arena *arena;
};

struct JS_Stripe
{
    Arena *arena;

    // Free lists
    JS_Node *freeJob;
    JS_Counter *freeCounter;

    // Ticket mutex
    TicketMutex lock;
};

struct JS_State
{
    Arena *arena;

    // Store task results
    // JobSlot *slots;
    // u32 numSlots;

    // Stripes
    JS_Stripe *stripes;
    u32 numStripes;

    // Threads
    JS_Thread *threads;
    u32 threadCount;

    // Queues
    JS_Queue highPriorityQueue;
    JS_Queue normalPriorityQueue;
    JS_Queue lowPriorityQueue;

    OS_Handle readSemaphore;

    // Job number
    u64 numJobs;
};

internal void JobThreadEntryPoint(void *p);
global JS_State *js_state;

internal void JS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(8));
    js_state        = PushStruct(arena, JS_State);
    js_state->arena = arena;

    // js_state->numSlots = 1024;
    // js_state->slots    = PushArray(arena, JobSlot, js_state->numSlots);

    js_state->threadCount = Clamp(OS_NumProcessors() - 1, 1, 8);
    for (u64 i = 0; i < js_state->threadCount; i++)
    {
        js_state->threads[i].handle = OS_ThreadStart(JobThreadEntryPoint, (void *)i);
        js_state->threads[i].arena  = ArenaAllocDefault();
    }

    // Initialize stripes
    js_state->numStripes = 64;
    js_state->stripes    = PushArray(arena, JS_Stripe, js_state->numStripes);
    for (u32 i = 0; i < js_state->numStripes; i++)
    {
        js_state->stripes[i].arena = ArenaAllocDefault();
    }

    js_state->readSemaphore = OS_CreateSemaphore(js_state->threadCount);

    // Initialize priority queues
    js_state->highPriorityQueue.writePos       = 0;
    js_state->highPriorityQueue.readPos        = 0;
    js_state->highPriorityQueue.writeSemaphore = OS_CreateSemaphore(js_state->threadCount);

    js_state->normalPriorityQueue.writePos       = 0;
    js_state->normalPriorityQueue.readPos        = 0;
    js_state->normalPriorityQueue.writeSemaphore = OS_CreateSemaphore(js_state->threadCount);

    js_state->lowPriorityQueue.writePos       = 0;
    js_state->lowPriorityQueue.readPos        = 0;
    js_state->lowPriorityQueue.writeSemaphore = OS_CreateSemaphore(js_state->threadCount);
}

// things i want from this system:
// - join/wait for a job to finish
//      - maybe use fibers so that other jobs can run while a job is waiting???
// - super easy to start a job (just one function call)
// - wait on batches of jobs to finish
//      -
//
// for later:
// - jobs should be able to spawn jobs

//////////////////////////////
// Job start/stop
//
internal JS_Ticket JS_Kick(JobCallback *callback, void *data, Arena **arena, Priority priority)
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
        }
        case Priority_High:
        {
            queue = &js_state->highPriorityQueue;
        }
    }

    b32 success = 0;

    // Create ticket w/ a job/counter. This will store info about the job's execution and its output.
    JS_Node *job        = stripe->freeJob;
    JS_Counter *counter = stripe->freeCounter;

    JS_Ticket ticket = {numJobs, (u64)job, (u64)counter};

    // NOTE: theoretically the stripes should minimize congestion. I don't know what I'm doing though.
    TicketMutexScope(&stripe->lock)
    {
        if (job == 0)
        {
            job = PushStructNoZero(stripe->arena, JS_Node);
        }
        else
        {
            StackPop(stripe->freeJob);
        }
        job->id          = numJobs;
        job->result      = 0;
        job->isCompleted = 0;

        break;
    }

    TicketMutexScope(&stripe->lock)
    {
        if (counter == 0)
        {
            counter = PushStructNoZero(stripe->arena, JS_Counter);
        }
        else
        {
            StackPop(stripe->freeCounter);
        }
        counter->c = 1;
        break;
    }

    // Queue a task.
    for (;;)
    {
        u64 curWritePos = queue->writePos;
        u64 curReadPos  = queue->readPos;

        u64 availableSlots = ArrayLength(queue->jobs) - (curWritePos - curReadPos);
        if (availableSlots >= 1)
        {
            u64 check = AtomicCompareExchangeU64(&queue->writePos, curWritePos + 1, curWritePos);
            if (check == curWritePos)
            {
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
                if (arena != 0)
                {
                    *arena = 0;
                }
                OS_ReleaseSemaphore(js_state->readSemaphore);
                break;
            }
        }
        else OS_WaitOnSemaphore(queue->writeSemaphore);
    }
    return ticket;
}

internal void JS_Join(JS_Ticket ticket)
{
    void *result        = 0;
    u64 id              = ticket.u64[0];
    JS_Node *jobNode    = (JS_Node *)ticket.u64[1];
    JS_Counter *counter = (JS_Counter *)ticket.u64[2];

    u64 stripeIndex   = id % js_state->numStripes;
    JS_Stripe *stripe = js_state->stripes + stripeIndex;

    // TODO Note: This will spin lock waiting for the job to finish. I should probably use a mutex
    // so that it put the thread to sleep instead of busy waiting, but idc!!!

    // NOTE: if I go with the model of waiting for a counter to reach 0, then I cannot return the result
    // of the job from this method. This means that the job has to edit some global state.
    TicketMutexScope(&stripe->lock)
    {
        // jobNode->result      = result;
        // jobNode->isCompleted = 1;

        break;
    }
}

internal b32 JS_PopJob(JS_Queue *queue, JS_Thread *thread)
{
    b32 sleep       = false;
    u64 curReadPos  = queue->readPos;
    u64 curWritePos = queue->writePos;

    if (curWritePos - curReadPos >= 1)
    {
        u64 index = AtomicCompareExchangeU64((LONG volatile *)&queue->readPos, curReadPos + 1, curReadPos);
        if (index == curReadPos)
        {
            Job *job     = &queue->jobs[index & (ArrayLength(queue->jobs) - 1)];
            Arena *arena = job->arena;
            if (arena == 0)
            {
                arena = thread->arena;
            }
            void *result = job->callback(job->data, job->arena);

            u64 id            = job->ticket.u64[0];
            u64 stripeIndex   = id % js_state->numStripes;
            JS_Stripe *stripe = js_state->stripes + stripeIndex;

            JS_Node *jobNode     = (JS_Node *)job->ticket.u64[1];
            jobNode->result      = result;
            jobNode->isCompleted = 1;

            JS_Counter *counter = (JS_Counter *)job->ticket.u64[2];
            AtomicDecrementU32(&counter->c);

            OS_ReleaseSemaphore(queue->writeSemaphore);
        }
    }
    else
    {
        sleep = true;
    }
    return sleep;
}

internal void JobThreadEntryPoint(void *p)
{
    u64 threadIndex   = (u64)p;
    JS_Thread *thread = &js_state->threads[threadIndex];
    for (;;)
    {

        if (JS_PopJob(&js_state->highPriorityQueue, thread) && JS_PopJob(&js_state->normalPriorityQueue, thread) &&
            JS_PopJob(&js_state->lowPriorityQueue, thread))
        {
            OS_WaitOnSemaphore(js_state->readSemaphore);
        }
    }
}
