#define JOB_ENTRY_POINT(name) void name(void *data)
typedef JOB_ENTRY_POINT(JobCallback);

enum Priority
{
    Priority_Low,
    Priority_Normal,
    Priority_High,
    Priority_Count,
};

struct Counter
{
    u32 c;
};

struct Job
{
    OS_JobCallback *callback;
    void *data;
    Priority priority;
    Counter *counter;
};

struct JobQueue
{
    u64 volatile writePos;
    u64 volatile readPos;

    Job *jobs[256];

    // If there are somehow not enough space to queue jobs, wait on semaphore.
    OS_Handle semaphore;
};

struct JobNode
{

    void *result;
    Counter *counter;
    b32 isOccupied;
};

struct JobSlot
{
    JobNode *first;
    JobNode *last;
};

struct JS_State
{
    Arena *arena;

    // Request Ring Buffer
    u8 *ringBuffer;
    u64 ringBufferSize;
    u64 readPos;
    u64 writePos;

    // Store task results
    JobSlot *slots;
    u32 numSlots;

    // Threads
    OS_Handle *threads;
    u32 threadCount;

    OS_JobQueue highPriorityQueue;
    OS_JobQueue normalPriorityQueue;
    OS_JobQueue lowPriorityQueue;

    OS_Handle globalJobSemaphore;
};

global JS_State js_state;

internal void JS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(8));
    js_state        = PushStruct(arena, AS_State);
    js_state->arena = arena;

    js_state->numSlots = 1024;
    js_state->slots    = PushArray(arena, JobSlot, js_state->numSlots);

    js_state->ringBufferSize = kilobytes(64);
    js_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    js_state->threadCount = Clamp(OS_NumProcessors() - 1, 1, 8);
    for (u32 i = 0; i < js_state->threadCount; i++)
    {
        js_state->threads[i] = OS_ThreadStart(JobThreadEntryPoint, 0);
    }
}

internal JS_Kick(JobCallback *callback, void *data, Arena **arena, Priority priotity, Counter *counter)
{
    OS_JobQueue *queue = 0;
    switch (job->priority)
    {
        default:
        case Priority_Low:
        {
            queue = &lowPriorityQueue;
            break;
        }
        case Priority_Normal:
        {
            queue = &normalPriorityQueue;
        }
        case Priority_High:
        {
            queue = &highPriorityQueue;
        }
    }
    QueueJob(queue, job);
}

internal ExecuteJobs(Job job, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        ExecuteJob(job[i]);
    }
}

internal WaitForCounter(Counter *counter) {}

internal b32 QueueJob(OS_JobQueue *queue, Job job)
{
    b32 success = 0;
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
                success                                                   = 1;
                queue->jobs[curWritePos & (ArrayLength(queue->jobs) - 1)] = job;
                OS_ReleaseSemaphore(globalJobSemaphore, 1);
                break;
            }
        }
        else Assert(!"Ran out of job slots, I'll deal with this later");
    }
}

internal b32 PopJob(OS_JobQueue *queue)
{
    b32 sleep       = false;
    u64 curReadPos  = queue->readPos;
    u64 curWritePos = queue->writePos;

    if (curWritePos - curReadPos >= 1)
    {
        u64 index = AtomicCompareExchangeU64((LONG volatile *)&queue->readPos, curReadPos + 1, curReadPos);
        if (index == curReadPos)
        {
            Job *job = &queue->jobs[(index & (ArrayLength(queue->jobs) - 1)];
            job->callback(job->data);
            // TODO: is this right? will this happen after the job finishes            
            AtomicDecrementU64(job->counter->c);
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
    for (;;)
    {
        if (PopJob(&highPriorityQueue) && PopJob(&normalPriorityQueue) && PopJob(&lowPriorityQueue))
        {
            OS_WaitOnSemaphore(globalJobSemaphore);
        }
    }
}

internal void Win32CompleteJobs(OS_JobQueue *queue)
{
    while (queue->completionCount != queue->completionGoal)
    {
        if (Win32ExecuteJob(queue))
        {
            WaitForSingleObjectEx(queue->semaphore, INFINITE, FALSE);
        }
    }
    queue->completionCount = 0;
    queue->completionGoal  = 0;
}
