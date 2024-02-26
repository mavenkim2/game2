struct OS_Handle
{
    u64 handle;
};

typedef void OS_ThreadFunction(void *);

internal void Printf(char *fmt, ...);
internal u64 OS_GetLastWriteTime(string filename);
internal string ReadEntireFile(string filename);
internal b32 WriteFile(string filename, void *fileMemory, u32 fileSize);
internal u64 OS_PageSize();
internal void *OS_Alloc(u64 size);
internal void OS_Release(void *memory);

/////////////////////////////////////////////////////
// Handles
//

/////////////////////////////////////////////////////
// Threads
//
internal OS_Handle OS_ThreadAlloc(OS_ThreadFunction *func, void *ptr);
internal OS_Handle OS_ThreadStart(OS_ThreadFunction *func, void *ptr);

//////////////////////////////
// Semaphores
//
internal OS_Handle OS_CreateSemaphore(u32 maxCount);
internal void OS_ReleaseSemaphore(OS_Handle input);
internal void OS_ReleaseSemaphore(OS_Handle input, u32 count);

//////////////////////////////
// Mutexes
//
internal OS_Handle OS_CreateMutex();
internal void OS_DeleteMutex(OS_Handle input);
internal void OS_TakeMutex(OS_Handle input);
internal void OS_DropMutex(OS_Handle input);
internal OS_Handle OS_CreateRWMutex();

//////////////////////////////
// Read/Write Mutex
//
internal void OS_DeleteRWMutex(OS_Handle input);
internal void OS_TakeRMutex(OS_Handle input);
internal void OS_DropRMutex(OS_Handle input);
internal void OS_TakeWMutex(OS_Handle input);
internal void OS_DropWMutex(OS_Handle input);

//////////////////////////////
// Condition Variables
//
internal OS_Handle OS_CreateConditionVariable();
internal void OS_DeleteConditionVariable(OS_Handle input);
internal b32 OS_WaitConditionVariable(OS_Handle cv, OS_Handle m);
internal b32 OS_WaitRConditionVariable(OS_Handle cv, OS_Handle m);
internal b32 OS_WaitRWConditionVariable(OS_Handle cv, OS_Handle m);
internal void OS_SignalConditionVariable(OS_Handle cv);
internal void OS_BroadcastConditionVariable(OS_Handle cv);

//////////////////////////////
/// Events
///
internal OS_Handle OS_CreateSignal();
internal b32 OS_SignalWait(OS_Handle input, u32 time);
internal void OS_RaiseSignal(OS_Handle input);

/////////////////////////////////////////////////////
// System Info
//
internal u32 OS_NumProcessors();
