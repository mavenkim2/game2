#include "mkEntryPoint.h"

#include "mkPlatformInc.cpp"
#include "mkThreadContext.cpp"

#include "mkMemory.cpp"
#include "mkString.cpp"
#include "mkScene.cpp"

#if VULKAN
#include "render/mkGraphicsVulkan.cpp"
#endif

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
{
    // Initialization
    platform = GetPlatform();

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
#if VULKAN
    mkGraphicsVulkan graphics(ValidationMode::Verbose, GPUDevicePreference::Discrete);
#else
#error
#endif

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

    Engine engineLocal;
    engine = &engineLocal;

    Arena *sceneArena = ArenaAlloc();
    scene::Scene sceneLocal;
    sceneLocal.Init(sceneArena);

    GamePlatformMemory gameMem;
    gameMem.mIsLoaded    = 0;
    gameMem.mIsHotloaded = 0;
    gameMem.mEngine      = engine;
    gameMem.mScene       = &sceneLocal;
    gameMem.mPlatform    = platform;
    gameMem.mGraphics    = &graphics;
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
    // OS_UnloadDLL(&renderDLL);

    ThreadContextRelease();

    // R_EntryPoint(0);
    // Initialize separate game/render threads
    // OS_Handle gameThreadHandle = OS_ThreadStart(G_EntryPoint, 0);
    // OS_Handle renderThreadHandle = OS_ThreadStart(R_EntryPoint, 0);
    // OS_Handle inputThreadHandle = OS_ThreadStart(I_

    // OS_ThreadJoin(gameThreadHandle);
    // OS_ThreadJoin(renderThreadHandle);
}
