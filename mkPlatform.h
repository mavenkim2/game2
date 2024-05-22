#ifndef PLATFORM_H
#define PLATFORM_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkCommon.h"
#include "mkMemory.h"
#include "mkString.h"
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

struct ThreadContext;
#define THREAD_ENTRY_POINT(name) void name(void *ptr, ThreadContext *ctx)
typedef THREAD_ENTRY_POINT(OS_ThreadFunction);

struct OS_DLL
{
    OS_Handle mHandle;
    string mSource;
    string mLock;
    string mTemp;

    u64 mLastWriteTime;

    void **mFunctions;
    char **mFunctionNames;
    u32 mFunctionCount;
    b8 mValid;
};

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
    OS_EventType_LoseFocus,
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

struct OS_DLL;

void Print(char *fmt, ...);
OS_Handle OS_WindowInit();

f32 OS_NowSeconds();

u64 OS_GetLastWriteTime(string filename);
string ReadEntireFile(string filename);
u64 OS_PageSize();
void *OS_Alloc(u64 size);

void *OS_Reserve(u64 size);
b8 OS_Commit(void *ptr, u64 size);
void OS_Release(void *memory);

/////////////////////////////////////////////////////
// Initialization
//
void OS_Init();

//////////////////////////////
// Input
//
V2 OS_GetCenter(OS_Handle handle, b32 screenToClient);
V2 OS_GetWindowDimension(OS_Handle handle);
void OS_ToggleCursor(b32 on);
V2 OS_GetMousePos(OS_Handle handle);
void OS_SetMousePos(V2 pos);

//////////////////////////////
// Timing
//
f32 OS_GetWallClock();

//////////////////////////////
// File information
//
OS_FileAttributes OS_AttributesFromPath(string path);
OS_FileAttributes OS_AttributesFromFile(OS_Handle input);

//////////////////////////////
// Files
//

OS_Handle OS_OpenFile(OS_AccessFlags flags, string path);
void OS_CloseFile(OS_Handle input);
OS_FileAttributes OS_AtributesFromFile(OS_Handle input);
u64 OS_ReadEntireFile(OS_Handle handle, void *out);
u64 OS_ReadEntireFile(string path, void *out);
string OS_ReadEntireFile(Arena *arena, string path);
u32 OS_ReadFile(OS_Handle handle, void *out, u64 offset, u32 size);
b32 OS_WriteFile(string filename, void *fileMemory, u32 fileSize);

/////////////////////////////////////////////////////
// Threads
//
OS_Handle OS_ThreadAlloc(OS_ThreadFunction *func, void *ptr);
OS_Handle OS_ThreadStart(OS_ThreadFunction *func, void *ptr);
void OS_SetThreadName(string name);
void SetThreadAffinity(OS_Handle input, u32 index);

//////////////////////////////
// Semaphores
//
OS_Handle OS_CreateSemaphore(u32 maxCount);
void OS_ReleaseSemaphore(OS_Handle input);
void OS_ReleaseSemaphores(OS_Handle input, u32 count);

//////////////////////////////
// Mutexes
//
OS_Handle OS_CreateMutex();
void OS_DeleteMutex(OS_Handle input);
void OS_TakeMutex(OS_Handle input);
void OS_DropMutex(OS_Handle input);
OS_Handle OS_CreateRWMutex();

//////////////////////////////
// Read/Write Mutex
//
void OS_DeleteRWMutex(OS_Handle input);
void OS_TakeRMutex(OS_Handle input);
void OS_DropRMutex(OS_Handle input);
void OS_TakeWMutex(OS_Handle input);
void OS_DropWMutex(OS_Handle input);

//////////////////////////////
// Condition Variables
//
OS_Handle OS_CreateConditionVariable();
void OS_DeleteConditionVariable(OS_Handle input);
b32 OS_WaitConditionVariable(OS_Handle cv, OS_Handle m);
b32 OS_WaitRConditionVariable(OS_Handle cv, OS_Handle m);
b32 OS_WaitRWConditionVariable(OS_Handle cv, OS_Handle m);
void OS_SignalConditionVariable(OS_Handle cv);
void OS_BroadcastConditionVariable(OS_Handle cv);

//////////////////////////////
/// Events
///
OS_Handle OS_CreateSignal();
b32 OS_SignalWait(OS_Handle input);
void OS_RaiseSignal(OS_Handle input);

/////////////////////////////////////////////////////
// System Info
//
u32 OS_NumProcessors();

//////////////////////////////
// zzz
//
void OS_Sleep(u32 milliseconds);

OS_Events OS_GetEvents(Arena *arena);

//////////////////////////////
// File directory
//
string OS_GetCurrentWorkingDirectory();
OS_FileIter OS_DirectoryIterStart(string path, OS_FileIterFlags flags);
b32 OS_DirectoryIterNext(Arena *arena, OS_FileIter *input, OS_FileProperties *out);
void OS_DirectoryIterEnd(OS_FileIter *input);

string OS_GetBinaryDirectory();
void OS_ToggleFullscreen(OS_Handle handle);
b32 OS_WindowIsFocused(OS_Handle handle);
void OS_SetMousePos(OS_Handle handle, V2 pos);

void OS_ThreadJoin(OS_Handle handle);

void OS_UnloadDLL(OS_DLL *dll);
void OS_LoadDLL(OS_DLL *dll);
void OS_LoadDLLNoTemp(OS_DLL *dll);

#endif
