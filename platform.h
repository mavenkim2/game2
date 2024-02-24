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

/////////////////////////////////////////////////////
// Semaphores
//
internal OS_Handle OS_CreateSemaphore(u32 maxCount);
internal void OS_WaitOnSemaphore(OS_Handle input, u32 time);
internal void OS_ReleaseSemaphore(OS_Handle input, u32 count);

/////////////////////////////////////////////////////
// System Info
//
internal u32 OS_NumProcessors();
