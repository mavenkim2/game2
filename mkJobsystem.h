#include <functional>
#include <atomic>
#include <thread>

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkPlatformInc.h"
#include "mkList.h"
#endif

namespace jobsystem
{

b32 gTerminateJobs         = 0;
const i32 JOB_QUEUE_LENGTH = 256;

using std::atomic;

void JobThreadEntryPoint(void *p);

struct Counter
{
    atomic<u32> count;
};

enum class Priority
{
    Low,
    High,
};

struct JobArgs
{
    u32 jobId;
    u32 idInGroup;
    b32 isLastJob;
    u32 threadId;
};

using JobFunction = std::function<void(JobArgs)>;

struct Job
{
    Counter *counter;
    JobFunction func;
    u32 groupId;
    u32 groupJobStart;
    u32 groupJobSize;
};

// Jobs cannot be spawned from within a thread (for now)
struct JobQueue
{
    Job jobs[JOB_QUEUE_LENGTH];
    atomic<u64> writePos;
    atomic<u64> readPos;
    atomic<u64> commitReadPos;
};

struct JobSystem
{
    JobQueue highPriorityQueue;
    JobQueue lowPriorityQueue;

    OS_Handle threads[16]; // TODO: ?
    u32 threadCount;
    OS_Handle readSemaphore;
};

global JobSystem jobSystem;

void InitializeJobsystem()
{
    // jobSystem.threads.resize((size_t)platform.NumProcessors());
    u32 numProcessors       = platform.NumProcessors();
    jobSystem.threadCount   = Min(ArrayLength(jobSystem.threads), numProcessors);
    jobSystem.readSemaphore = platform.CreateSemaphore(jobSystem.threadCount);

    for (size_t i = 0; i < numProcessors; i++)
    {
        jobSystem.threads[i] = platform.ThreadStart(jobsystem::JobThreadEntryPoint, (void *)i);
        platform.SetThreadAffinity(jobSystem.threads[i], i);
    }
}

// why is the const necessary here?
void KickJob(Counter *counter, const JobFunction &func, Priority priority = Priority::Low)
{
    JobQueue *queue = 0;
    switch (priority)
    {
        case Priority::Low: queue = &jobSystem.lowPriorityQueue; break;
        case Priority::High: queue = &jobSystem.highPriorityQueue; break;
    }
    for (;;)
    {
        u64 writePos       = queue->writePos.load();
        u64 readPos        = queue->readPos.load();
        u64 availableSpots = JOB_QUEUE_LENGTH - (writePos - readPos);
        if (availableSpots >= 1)
        {
            Job *job           = &queue->jobs[writePos & (JOB_QUEUE_LENGTH - 1)];
            job->func          = func;
            job->counter       = counter;
            job->groupId       = 0;
            job->groupJobStart = 0;
            job->groupJobSize  = 1;
            job->counter->count.fetch_add(1);
            queue->writePos.fetch_add(1);
            platform.ReleaseSemaphore(jobSystem.readSemaphore);
            break;
        }
    }
}

void KickJobs(Counter *counter, u32 numJobs, u32 groupSize, const JobFunction &func, Priority priority = Priority::Low)
{
    JobQueue *queue = 0;
    switch (priority)
    {
        case Priority::Low: queue = &jobSystem.lowPriorityQueue; break;
        case Priority::High: queue = &jobSystem.highPriorityQueue; break;
    }

    u32 numGroups = ((numJobs + groupSize - 1) / groupSize);

    for (;;)
    {
        u64 writePos       = queue->writePos.load();
        u64 readPos        = queue->readPos.load();
        u64 availableSpots = JOB_QUEUE_LENGTH - (writePos - readPos);
        if (availableSpots >= numGroups)
        {
            for (u32 i = 0; i < numGroups; i++)
            {
                Job *job           = &queue->jobs[(writePos + i) & (JOB_QUEUE_LENGTH - 1)];
                job->func          = func;
                job->counter       = counter;
                job->groupId       = i;
                job->groupJobStart = i * groupSize;
                job->groupJobSize  = Min(groupSize, numJobs - job->groupJobStart);
            }
            counter->count.fetch_add(numGroups);
            queue->writePos.fetch_add(numGroups);
            platform.ReleaseSemaphores(jobSystem.readSemaphore, numGroups);
            break;
        }
    }
}

void WaitJobs(Counter *counter)
{
    while (counter->count.load() != 0)
    {
        std::this_thread::yield();
    }
}

b32 Pop(JobQueue &queue, u64 threadIndex)
{
    b32 result         = 0;
    u64 writePos       = queue.writePos.load();
    u64 readPos        = queue.readPos.load();
    u64 availableSpots = writePos - readPos;
    if (availableSpots >= 1)
    {
        result = 1;
        if (queue.commitReadPos.compare_exchange_strong(readPos, readPos + 1))
        {
            Job *readJob = &queue.jobs[(readPos) & (JOB_QUEUE_LENGTH - 1)];
            Printf("Read pos: %u\n", readPos);

            Job job;
            job.groupJobStart = readJob->groupJobStart;
            job.groupJobSize  = readJob->groupJobSize;
            job.groupId       = readJob->groupId;
            job.func          = readJob->func;
            job.counter       = readJob->counter;

            queue.readPos.fetch_add(1);

            JobArgs args;
            for (u32 i = job.groupJobStart; i < job.groupJobStart + job.groupJobSize; i++)
            {
                args.threadId  = (u32)threadIndex;
                args.jobId     = i;
                args.idInGroup = i - job.groupJobStart;
                args.isLastJob = i == (job.groupJobSize + job.groupJobSize - 1);
                job.func(args);
            }
            job.counter->count.fetch_sub(1);
        }
    }
    return result;
}

void JobThreadEntryPoint(void *p)
{
    u64 threadIndex = (u64)p;
    TempArena temp  = ScratchStart(0, 0);
    SetThreadName(PushStr8F(temp.arena, "[Jobsystem] Worker %u", threadIndex));
    ScratchEnd(temp);
    for (; !gTerminateJobs;)
    {
        if (!Pop(jobSystem.highPriorityQueue, threadIndex) && !Pop(jobSystem.lowPriorityQueue, threadIndex))
        {
            platform.SignalWait(jobSystem.readSemaphore);
        }
    }
}

void EndJobsystem()
{
    gTerminateJobs = 1;
    while (!Pop(jobSystem.highPriorityQueue, 0) && !Pop(jobSystem.lowPriorityQueue, 0)) continue;

    platform.ReleaseSemaphores(jobSystem.readSemaphore, jobSystem.threadCount);
    for (size_t i = 0; i < jobSystem.threadCount; i++)
    {
        platform.ThreadJoin(jobSystem.threads[i]);
    }
}

} // namespace jobsystem
