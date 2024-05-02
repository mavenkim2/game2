#include "mkEntryPoint.h"

#include "mkPlatformInc.cpp"
#include "mkThreadContext.cpp"

#include "mkMemory.cpp"
#include "mkString.cpp"

#include "render/mkGraphicsVulkan.cpp"

Shared *shared;
PlatformApi platform;
Engine *engine;
// TODO: make this conditional on renderer backend

#if WINDOWS
#define MAIN() int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
#define MAIN() int main(int argc, char **argv)
#endif

using namespace graphics;

// TODO: job graph with fibers :)
MAIN()
{ // Initialization
    Printf                         = Print;
    platform.Printf                = Print;
    platform.OS_GetLastWriteTime   = OS_GetLastWriteTime;
    platform.OS_PageSize           = OS_PageSize;
    platform.OS_Alloc              = OS_Alloc;
    platform.OS_Reserve            = OS_Reserve;
    platform.OS_Commit             = OS_Commit;
    platform.OS_Release            = OS_Release;
    platform.OS_GetWindowDimension = OS_GetWindowDimension;
    platform.OS_ReadEntireFile     = OS_ReadEntireFile;
    platform.OS_ReadFileHandle     = OS_ReadEntireFile;
    platform.OS_GetEvents          = OS_GetEvents;
    platform.OS_SetThreadName      = OS_SetThreadName;
    platform.OS_WriteFile          = OS_WriteFile;
    platform.OS_NumProcessors      = OS_NumProcessors;
    platform.OS_CreateSemaphore    = OS_CreateSemaphore;
    platform.OS_ThreadStart        = OS_ThreadStart;
    platform.OS_ThreadJoin         = OS_ThreadJoin;
    platform.OS_ReleaseSemaphore   = OS_ReleaseSemaphore;
    platform.OS_ReleaseSemaphores  = OS_ReleaseSemaphores;
    platform.OS_SignalWait         = OS_SignalWait;
    platform.OS_OpenFile           = OS_OpenFile;
    platform.OS_AttributesFromFile = OS_AttributesFromFile;
    platform.OS_CloseFile          = OS_CloseFile;
    platform.OS_AttributesFromPath = OS_AttributesFromPath;
    platform.OS_Sleep              = OS_Sleep;
    platform.OS_NowSeconds         = OS_NowSeconds;
    platform.OS_GetMousePos        = OS_GetMousePos;
    platform.OS_ToggleCursor       = OS_ToggleCursor;
    platform.OS_GetCenter          = OS_GetCenter;
    platform.OS_SetMousePos        = OS_SetMousePos;

    Arena *arena        = ArenaAlloc();
    ThreadContext *tctx = PushStruct(arena, ThreadContext);
    ThreadContextInitialize(tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

    OS_Init();

    shared = PushStruct(arena, Shared);

    shared->windowHandle = OS_WindowInit();
    shared->running      = 1;

#if WINDOWS
    win32State->mHInstance = hInstance;
#endif

    // R_Init(arena, shared->windowHandle);

    string binaryDirectory = OS_GetBinaryDirectory();
    mkGraphicsVulkan graphics(shared->windowHandle, ValidationMode::Verbose, GPUDevicePreference::Discrete);

    Swapchain swapchain;
    SwapchainDesc desc;
    desc.width  = (u32)platform.OS_GetWindowDimension(shared->windowHandle).x;
    desc.height = (u32)platform.OS_GetWindowDimension(shared->windowHandle).y;
    desc.format = graphics::Format::B8G8R8A8_UNORM;

    graphics.CreateSwapchain((Window)shared->windowHandle.handle, hInstance, &desc, &swapchain);
    graphics.CreateShader();

    for (; shared->running == 1;)
    {
        CommandList cmdList = graphics.BeginCommandList(QueueType_Graphics);
        graphics.BeginRenderPass(&swapchain, &cmdList);
        graphics.WaitForGPU();
    }
#if 0

    // Ring buffer initialization
    {
        shared->i2gRing.size   = kilobytes(64);
        shared->i2gRing.buffer = PushArray(arena, u8, shared->i2gRing.size);

        shared->g2rRing.size   = kilobytes(256);
        shared->g2rRing.buffer = PushArray(arena, u8, shared->g2rRing.size);
    }

    OS_DLL gameDLL = {};
    struct GameFunctionTable
    {
        g_initialize *G_Initialize;
        g_update *G_Update;
        g_flush *G_Flush;
    } gameFunctions;

    {
        char *gameFunctionTableNames[] = {"G_Init", "G_Update", "G_Flush"};
        gameDLL.mFunctionNames         = gameFunctionTableNames;
        gameDLL.mFunctionCount         = ArrayLength(gameFunctionTableNames);
        gameDLL.mSource                = StrConcat(arena, OS_GetBinaryDirectory(), "/game.dll");
        gameDLL.mTemp                  = StrConcat(arena, OS_GetBinaryDirectory(), "/game_temp.dll");
        gameDLL.mLock                  = StrConcat(arena, OS_GetBinaryDirectory(), "/game_lock.dll");
        gameDLL.mFunctions             = (void **)&gameFunctions;
    }
    OS_LoadDLL(&gameDLL);

    struct RendererFunctionTable
    {
        RendererApi api;
        // r_allocate_texture_2D *R_AllocateTexture;
        // r_initialize_buffer *R_InitializeBuffer;
        // r_map_gpu_buffer *R_MapGPUBuffer;
        // r_unmap_gpu_buffer *R_UnmapGPUBuffer;
        // r_update_buffer *R_UpdateBuffer;
        r_initialize *R_Initialize;
        r_end_frame *R_EndFrame;
    } rendererFunctions;

    OS_DLL renderDLL = {};
    {
        char *renderFuncTableNames[] = {"R_AllocateTextureInArray", "R_InitializeBuffer", "R_MapGPUBuffer", "R_UnmapGPUBuffer", "R_UpdateBuffer", "R_Init", "R_EndFrame"};
        renderDLL.mFunctionNames     = renderFuncTableNames;
        renderDLL.mFunctionCount     = ArrayLength(renderFuncTableNames);
        renderDLL.mSource            = StrConcat(arena, OS_GetBinaryDirectory(), "/render.dll");
        renderDLL.mTemp              = StrConcat(arena, OS_GetBinaryDirectory(), "/render_temp.dll");
        renderDLL.mLock              = StrConcat(arena, OS_GetBinaryDirectory(), "/render_lock.dll");
        renderDLL.mFunctions         = (void **)&rendererFunctions;
    }
    OS_LoadDLL(&renderDLL);

    RenderPlatformMemory renderMem;
    renderMem.mIsLoaded    = 0;
    renderMem.mIsHotloaded = 0;
    renderMem.mPlatform    = platform;
    renderMem.mRenderer    = 0;
    renderMem.mShared      = shared;
    renderMem.mTctx        = tctx;
    if (renderDLL.mValid)
    {
        rendererFunctions.R_Initialize(&renderMem, shared->windowHandle);
    }

    Engine engineLocal;
    engine = &engineLocal;
    GamePlatformMemory gameMem;
    gameMem.mIsLoaded    = 0;
    gameMem.mIsHotloaded = 0;
    gameMem.mEngine      = engine;
    gameMem.mPlatform    = platform;
    gameMem.mRenderer    = rendererFunctions.api;
    gameMem.mShared      = shared;
    // TODO: ?
    gameMem.mTctx = tctx;
    if (gameDLL.mValid)
    {
        gameFunctions.G_Initialize(&gameMem);
    }

    f32 frameDt    = 1.f / 144.f;
    f32 multiplier = 1.f;

    f32 frameTime = OS_NowSeconds();

    for (; shared->running == 1;)
    {
        u64 renderDLLWriteTime = OS_GetLastWriteTime(renderDLL.mSource);
        if (renderDLL.mLastWriteTime != renderDLLWriteTime)
        {
            OS_UnloadDLL(&renderDLL);
            OS_LoadDLL(&renderDLL);
            gameMem.mRenderer      = rendererFunctions.api;
            renderMem.mIsHotloaded = 1;
            // REINITIALIZATION
            if (gameDLL.mValid)
            {
                gameFunctions.G_Initialize(&gameMem);
            }

            if (renderDLL.mValid)
            {
                rendererFunctions.R_Initialize(&renderMem, shared->windowHandle);
            }
        }
        // TODO: even when a file hasn't changed at all it recompiles
        u64 gameDLLWriteTime = OS_GetLastWriteTime(gameDLL.mSource);
        if (gameDLL.mLastWriteTime != gameDLLWriteTime)
        {
            gameMem.mIsHotloaded = 1;
            gameFunctions.G_Flush();
            OS_UnloadDLL(&gameDLL);
            OS_LoadDLL(&gameDLL);
            gameFunctions.G_Initialize(&gameMem);
        }

        frameTime = OS_NowSeconds();

        // TODO: dll this and the endframe
        if (gameDLL.mValid)
        {
            gameFunctions.G_Update(frameDt * multiplier);
        }

        // TODO: on another thread
        if (renderDLL.mValid)
        {
            rendererFunctions.R_EndFrame(gameMem.mEngine->GetRenderState());
        }

        // Wait until new update
        f32 endWorkFrameTime = OS_NowSeconds();
        f32 timeElapsed      = endWorkFrameTime - frameTime;

        if (timeElapsed < frameDt)
        {
            u32 msTimeToSleep = (u32)(1000.f * (frameDt - timeElapsed));
            if (msTimeToSleep > 0)
            {
                OS_Sleep(msTimeToSleep);
            }
        }

        while (timeElapsed < frameDt)
        {
            timeElapsed = OS_NowSeconds() - frameTime;
        }
    }

    gameFunctions.G_Flush();
    OS_UnloadDLL(&gameDLL);
    OS_UnloadDLL(&renderDLL);
#endif

    ThreadContextRelease();

    // R_EntryPoint(0);
    // Initialize separate game/render threads
    // OS_Handle gameThreadHandle = OS_ThreadStart(G_EntryPoint, 0);
    // OS_Handle renderThreadHandle = OS_ThreadStart(R_EntryPoint, 0);
    // OS_Handle inputThreadHandle = OS_ThreadStart(I_

    // OS_ThreadJoin(gameThreadHandle);
    // OS_ThreadJoin(renderThreadHandle);
}
