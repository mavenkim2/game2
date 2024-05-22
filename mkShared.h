#ifndef SHARED_H
#define SHARED_H

struct AtomicRing
{
    u8 *buffer;
    u64 size;
    u64 volatile readPos;
    u64 volatile commitReadPos;
    u64 volatile writePos;
    u64 volatile commitWritePos;
};

// g2r = Game To Render
struct Shared
{
    OS_Handle windowHandle;

    AtomicRing g2rRing;

    // Input to Game
    AtomicRing i2gRing;
    b8 running;
};

extern Shared *shared;

//////////////////////////////
// Platform DLL
//
#define OS_GET_LAST_WRITE_TIME(name) u64 name(string path)
typedef OS_GET_LAST_WRITE_TIME(os_get_last_write_time);

#define OS_PAGE_SIZE(name) u64 name(void)
typedef OS_PAGE_SIZE(os_page_size);

#define OS_ALLOC(name) void *name(u64 size)
typedef OS_ALLOC(os_alloc);

#define OS_RESERVE(name) void *name(u64 size)
typedef OS_RESERVE(os_reserve);

#define OS_COMMIT(name) b8 name(void *ptr, u64 size)
typedef OS_COMMIT(os_commit);

#define OS_RELEASE(name) void name(void *memory)
typedef OS_RELEASE(os_release);

#define OS_GET_WINDOW_DIMENSION(name) V2 name(OS_Handle handle)
typedef OS_GET_WINDOW_DIMENSION(os_get_window_dimension);

#define OS_READ_FILE_HANDLE(name) u64 name(OS_Handle handle, void *out)
typedef OS_READ_FILE_HANDLE(os_read_file_handle);

#define OS_READ_ENTIRE_FILE(name) string name(Arena *arena, string path)
typedef OS_READ_ENTIRE_FILE(os_read_entire_file);

#define OS_GET_EVENTS(name) OS_Events name(Arena *arena)
typedef OS_GET_EVENTS(os_get_events);

#define OS_SET_THREAD_NAME(n) void n(string name)
typedef OS_SET_THREAD_NAME(os_set_thread_name);

#define OS_WRITE_FILE(name) b32 name(string filename, void *fileMemory, u32 fileSize)
typedef OS_WRITE_FILE(os_write_file);

#define OS_NUM_PROCESSORS(name) u32 name()
typedef OS_NUM_PROCESSORS(os_num_processors);

#define OS_CREATE_SEMAPHORE(name) OS_Handle name(u32 maxCount)
typedef OS_CREATE_SEMAPHORE(os_create_semaphore);

#define OS_THREAD_START(name) OS_Handle name(OS_ThreadFunction *func, void *ptr)
typedef OS_THREAD_START(os_thread_start);

#define OS_THREAD_JOIN(name) void name(OS_Handle handle)
typedef OS_THREAD_JOIN(os_thread_join);

#define OS_RELEASE_SEMAPHORE(name) void name(OS_Handle input)
typedef OS_RELEASE_SEMAPHORE(os_release_semaphore);

#define OS_DELETE_SEMAPHORE(name) void name(OS_Handle handle)
typedef OS_DELETE_SEMAPHORE(os_delete_semaphore);

#define OS_RELEASE_SEMAPHORES(name) void name(OS_Handle input, u32 count)
typedef OS_RELEASE_SEMAPHORES(os_release_semaphores);

#define OS_SIGNAL_WAIT(name) b32 name(OS_Handle input)
typedef OS_SIGNAL_WAIT(os_signal_wait);

#define OS_OPEN_FILE(name) OS_Handle name(OS_AccessFlags flags, string path)
typedef OS_OPEN_FILE(os_open_file);

#define OS_ATTRIBUTE_FROM_FILE(name) OS_FileAttributes name(OS_Handle input)
typedef OS_ATTRIBUTE_FROM_FILE(os_attributes_from_file);

#define OS_CLOSE_FILE(name) void name(OS_Handle input)
typedef OS_CLOSE_FILE(os_close_file);

#define OS_ATTRIBUTES_FROM_PATH(name) OS_FileAttributes name(string path)
typedef OS_ATTRIBUTES_FROM_PATH(os_attributes_from_path);

#define OS_SLEEP(name) void name(u32 milliseconds)
typedef OS_SLEEP(os_sleep);

#define OS_NOW_SECONDS(name) f32 name()
typedef OS_NOW_SECONDS(os_now_seconds);

#define OS_GET_MOUSE_POS(name) V2 name(OS_Handle handle)
typedef OS_GET_MOUSE_POS(os_get_mouse_pos);

#define OS_TOGGLE_CURSOR(name) void name(b32 on)
typedef OS_TOGGLE_CURSOR(os_toggle_cursor);

#define OS_GET_CENTER(name) V2 name(OS_Handle handle, b32 screenToClient)
typedef OS_GET_CENTER(os_get_center);

#define OS_SET_MOUSE_POS(name) void name(OS_Handle handle, V2 pos)
typedef OS_SET_MOUSE_POS(os_set_mouse_pos);

#define OS_GET_THREAD_CONTEXT(name) ThreadContext *name()
typedef OS_GET_THREAD_CONTEXT(os_get_thread_context);

#define OS_SET_THREAD_AFFINITY(name) void name(OS_Handle input, u32 index)
typedef OS_SET_THREAD_AFFINITY(os_set_thread_affinity);

#define OS_LOAD_DLL(name) void name(OS_DLL *dll)
typedef OS_LOAD_DLL(os_load_dll);

struct PlatformApi
{
    print_func *Printf;
    os_get_last_write_time *GetLastWriteTime;
    os_page_size *PageSize;
    os_alloc *Alloc;
    os_reserve *Reserve;
    os_commit *Commit;
    os_release *Release;
    os_get_window_dimension *GetWindowDimension;
    os_read_file_handle *ReadFileHandle;
    os_read_entire_file *ReadEntireFile;
    os_get_events *GetEvents;
    os_set_thread_name *SetThreadName;
    os_write_file *WriteFile;
    os_num_processors *NumProcessors;
    os_create_semaphore *CreateSemaphore;
    os_thread_start *ThreadStart;
    os_thread_join *ThreadJoin;
    os_release_semaphore *ReleaseSemaphore;
    os_delete_semaphore *DeleteSemaphore;
    os_release_semaphores *ReleaseSemaphores;
    os_signal_wait *SignalWait;
    os_open_file *OpenFile;
    os_attributes_from_file *AttributesFromFile;
    os_close_file *CloseFile;
    os_attributes_from_path *AttributesFromPath;
    os_sleep *Sleep;
    os_now_seconds *NowSeconds;
    os_get_mouse_pos *GetMousePos;
    os_toggle_cursor *ToggleCursor;
    os_get_center *GetCenter;
    os_set_mouse_pos *SetMousePos;
    os_set_thread_affinity *SetThreadAffinity;
    os_load_dll *LoadDLL;
    os_load_dll *LoadDLLNoTemp;
};
extern PlatformApi platform;

inline PlatformApi GetPlatform()
{
    PlatformApi platform_;
    Printf                       = Print;
    platform_.Printf             = Print;
    platform_.GetLastWriteTime   = OS_GetLastWriteTime;
    platform_.PageSize           = OS_PageSize;
    platform_.Alloc              = OS_Alloc;
    platform_.Reserve            = OS_Reserve;
    platform_.Commit             = OS_Commit;
    platform_.Release            = OS_Release;
    platform_.GetWindowDimension = OS_GetWindowDimension;
    platform_.ReadEntireFile     = OS_ReadEntireFile;
    platform_.ReadFileHandle     = OS_ReadEntireFile;
    platform_.GetEvents          = OS_GetEvents;
    platform_.SetThreadName      = OS_SetThreadName;
    platform_.WriteFile          = OS_WriteFile;
    platform_.NumProcessors      = OS_NumProcessors;
    platform_.CreateSemaphore    = OS_CreateSemaphore;
    platform_.ThreadStart        = OS_ThreadStart;
    platform_.ThreadJoin         = OS_ThreadJoin;
    platform_.ReleaseSemaphore   = OS_ReleaseSemaphore;
    platform_.ReleaseSemaphores  = OS_ReleaseSemaphores;
    platform_.SignalWait         = OS_SignalWait;
    platform_.OpenFile           = OS_OpenFile;
    platform_.AttributesFromFile = OS_AttributesFromFile;
    platform_.CloseFile          = OS_CloseFile;
    platform_.AttributesFromPath = OS_AttributesFromPath;
    platform_.Sleep              = OS_Sleep;
    platform_.NowSeconds         = OS_NowSeconds;
    platform_.GetMousePos        = OS_GetMousePos;
    platform_.ToggleCursor       = OS_ToggleCursor;
    platform_.GetCenter          = OS_GetCenter;
    platform_.SetMousePos        = OS_SetMousePos;
    platform_.SetThreadAffinity  = SetThreadAffinity;
    platform_.LoadDLL            = OS_LoadDLL;
    platform_.LoadDLLNoTemp      = OS_LoadDLLNoTemp;
    return platform_;
}

//////////////////////////////
// Renderer DLL
//
struct RenderState;

class PlatformRenderer
{
    void *pointer;
};

struct ThreadContext;

// struct RenderPlatformMemory
// {
//     Shared *mShared;
//     PlatformRenderer *mRenderer;
//     PlatformApi mPlatform;
//     ThreadContext *mTctx;
//     b8 mIsHotloaded;
//     b8 mIsLoaded;
// };
//
// #define R_INIT(name) void name(RenderPlatformMemory *ioRenderMem, OS_Handle handle)
// typedef R_INIT(r_initialize);
//
// #define R_ENDFRAME(name) void name(RenderState *renderState)
// typedef R_ENDFRAME(r_end_frame);

//////////////////////////////
// Game DLL
//
class Engine
{
private:
    struct G_State *g_state;
    struct RenderState *renderState;
    struct AS_CacheState *as_state;
    struct JS_State *js_state;
    struct F_State *f_state;
    struct D_State *d_state;

public:
    RenderState *GetRenderState()
    {
        return renderState;
    }
    G_State *GetGameState()
    {
        return g_state;
    }
    JS_State *GetJobState()
    {
        return js_state;
    }
    F_State *GetFontState()
    {
        return f_state;
    }
    AS_CacheState *GetAssetCacheState()
    {
        return as_state;
    }
    D_State *GetDrawState()
    {
        return d_state;
    }

    void SetRenderState(RenderState *state)
    {
        renderState = state;
    }
    void SetGameState(G_State *state)
    {
        g_state = state;
    }
    void SetJobState(JS_State *state)
    {
        js_state = state;
    }
    void SetFontState(F_State *state)
    {
        f_state = state;
    }
    void SetAssetCacheState(AS_CacheState *state)
    {
        as_state = state;
    }
    void SetDrawState(D_State *state)
    {
        d_state = state;
    }
};

extern Engine *engine;

struct GamePlatformMemory
{
    Shared *mShared;
    Engine *mEngine;
    // RendererApi mRenderer;
    graphics::mkGraphics *mGraphics;
    PlatformApi mPlatform;
    ThreadContext *mTctx;

    b8 mIsLoaded;
    b8 mIsHotloaded;
};

struct GamePlatformMemory;
#define G_INIT(name) void name(GamePlatformMemory *ioPlatformMemory)
typedef G_INIT(g_initialize);

#define G_UPDATE(name) void name(f32 dt)
typedef G_UPDATE(g_update);

#define G_FLUSH(name) void name(void)
typedef G_FLUSH(g_flush);

#if INTERNAL
#if WINDOWS
#define DLL extern "C" __declspec(dllexport)
#else
#define DLL extern "C"
#endif
#else
#define DLL
#endif

#endif
