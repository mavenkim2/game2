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
    u64 u64[2];
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

    OS_Handle writeSemaphore;
    Mutex lock;
};

struct JS_Thread
{
    OS_Handle handle;
    Arena *arena;
};

struct JS_Stripe
{
    Arena *arena;

    // Free lists
    JS_Counter *freeCounter;

    // RW Mutex
    OS_Handle rwMutex;
};

struct JS_State
{
    Arena *arena;

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

internal b32 JS_PopJob(JS_Queue *queue, JS_Thread *thread);
