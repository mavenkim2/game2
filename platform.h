#ifndef PLATFORM_H
#define PLATFORM_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#endif

struct OS_Handle
{
    u64 handle;
};

typedef u32 OS_AccessFlags;
enum
{
    OS_AccessFlag_Read       = (1 << 0),
    OS_AccessFlag_Write      = (1 << 1),
    OS_AccessFlag_ShareRead  = (1 << 2),
    OS_AccessFlag_ShareWrite = (1 << 3),
};

struct OS_FileAttributes
{
    u64 size;
    u64 lastModified;
};

// TODO: merge with above
struct OS_FileProperties
{
    string name;
    u64 size;
    u64 lastModified;
    b32 isDirectory;
};

typedef u32 OS_FileIterFlags;
enum
{
    OS_FileIterFlag_SkipDirectories = (1 << 0),
    OS_FileIterFlag_SkipFiles       = (1 << 1),
    OS_FileIterFlag_SkipHiddenFiles = (1 << 2),
    OS_FileIterFlag_Complete        = (1 << 31),
};

struct OS_FileIter
{
    OS_FileIterFlags flags;
    u8 memory[600];
};

typedef void OS_ThreadFunction(void *);

internal void Printf(char *fmt, ...);
internal u64 OS_GetLastWriteTime(string filename);
internal string ReadEntireFile(string filename);
internal b32 WriteFile(string filename, void *fileMemory, u32 fileSize);
internal u64 OS_PageSize();
internal void *OS_Alloc(u64 size);
internal void *OS_Reserve(u64 size);
internal b8 OS_Commit(void *ptr, u64 size);
internal void OS_Release(void *memory);

/////////////////////////////////////////////////////
// Initialization
//
internal void OS_Init();

//////////////////////////////
// Input
//
internal V2 OS_GetCenter(OS_Handle handle, b32 screenToClient);
internal V2 OS_GetWindowDimension(OS_Handle handle);
internal void OS_ToggleCursor(b32 on);
internal V2 OS_GetMousePos(OS_Handle handle);
internal void OS_SetMousePos(V2 pos);

//////////////////////////////
// Timing
//
internal f32 OS_GetWallClock();

//////////////////////////////
// File information
//
internal OS_FileAttributes OS_AttributesFromPath(string path);
internal OS_FileAttributes OS_AttributesFromFile(OS_Handle input);

//////////////////////////////
// Files
//

internal OS_Handle OS_OpenFile(OS_AccessFlags flags, string path);
internal void OS_CloseFile(OS_Handle input);
internal OS_FileAttributes OS_AtributesFromFile(OS_Handle input);
internal u64 OS_ReadEntireFile(OS_Handle handle, void *out);
internal u64 OS_ReadEntireFile(string path, void *out);
internal string OS_ReadEntireFile(Arena *arena, string path);

/////////////////////////////////////////////////////
// Threads
//
internal OS_Handle OS_ThreadAlloc(OS_ThreadFunction *func, void *ptr);
internal OS_Handle OS_ThreadStart(OS_ThreadFunction *func, void *ptr);
internal void OS_SetThreadName(string name);

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

//////////////////////////////
// zzz
//
internal void OS_Sleep(u32 milliseconds);

//////////////////////////////
// I/O
//
enum OS_Key
{
    OS_Mouse_L,
    OS_Mouse_R,
    OS_Key_A,
    OS_Key_B,
    OS_Key_C,
    OS_Key_D,
    OS_Key_E,
    OS_Key_F,
    OS_Key_G,
    OS_Key_H,
    OS_Key_I,
    OS_Key_J,
    OS_Key_K,
    OS_Key_L,
    OS_Key_M,
    OS_Key_N,
    OS_Key_O,
    OS_Key_P,
    OS_Key_Q,
    OS_Key_R,
    OS_Key_S,
    OS_Key_T,
    OS_Key_U,
    OS_Key_V,
    OS_Key_W,
    OS_Key_X,
    OS_Key_Y,
    OS_Key_Z,
    OS_Key_Space,
    OS_Key_Shift,
    OS_Key_F1,
    OS_Key_F2,
    OS_Key_F3,
    OS_Key_F4,
    OS_Key_F5,
    OS_Key_F6,
    OS_Key_F7,
    OS_Key_F8,
    OS_Key_F9,
    OS_Key_F10,
    OS_Key_F11,
    OS_Key_F12,
    OS_Key_Count,
};

enum OS_EventType
{
    OS_EventType_Quit,
    OS_EventType_KeyPressed,
    OS_EventType_KeyReleased,
};

struct OS_Event
{
    OS_EventType type;

    // key info
    OS_Key key;
    b32 transition;

    // mouse click info
    V2 pos;
};

struct OS_Events
{
    OS_Event *events;
    u32 numEvents;
};

struct OS_EventChunk
{
    OS_EventChunk *next;
    OS_Event *events;

    u32 count;
    u32 cap;
};

struct OS_EventList
{
    OS_EventChunk *first;
    OS_EventChunk *last;
    u32 numEvents;
};

internal OS_Events OS_GetEvents(Arena *arena);

#endif
