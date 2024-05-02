#ifndef JOB_H
#define JOB_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "platform_inc.h"
#endif

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
    u32 volatile c;
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
    JS_Counter *counter;
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

struct JS_State
{
    Arena *arena;

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

//////////////////////////////
// Initialization
//
internal void JS_Init();

//////////////////////////////
// Job API Functions
//
internal void JS_Kick(JobCallback *callback, void *data, Arena **arena, Priority priority, JS_Counter *counter);
internal void JS_Join(JS_Counter *counter);

//////////////////////////////
// Worker Thread Tasks
//

internal b32 JS_PopJob(JS_Queue *queue, JS_Thread *thread);
internal void JobThreadEntryPoint(void *p);

#endif
