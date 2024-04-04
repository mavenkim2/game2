#include "entry_point.h"

#include "platform_inc.cpp"
#include "input.cpp"
#include "thread_context.cpp"

#include "physics.cpp"
#include "keepmovingforward_memory.cpp"
#include "keepmovingforward_string.cpp"
#include "keepmovingforward_camera.cpp"
#include "entity.cpp"
#include "level.cpp"
#include "job.cpp"
#include "font.cpp"
#include "asset.cpp"
#include "asset_cache.cpp"
#include "render/render.cpp"
#include "debug.cpp"
#include "game.cpp"

// TODO: make this conditional on renderer backend
#include "render/opengl.cpp"

#if WINDOWS
#define MAIN() int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
#define MAIN() int main(int argc, char **argv)
#endif

// TODO: synchronizing DLL changes across threads seems a bit awful

// global const char *G_DLLTable[] = {
//     "GameUpdateAndRender",
// };
//
// global const char *R_DLLTable[] = {
//
// };
//

// TODO: job graph with fibers :)
MAIN()
{ // Initialization

    Arena *arena = ArenaAlloc();

    shared = PushStruct(arena, Shared);

    ThreadContext *tctx = PushStruct(arena, ThreadContext);
    ThreadContextInitialize(tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

    OS_Init();
    shared->windowHandle = OS_WindowInit();
    shared->running = 1;

#if WINDOWS
    win32State->mHInstance = hInstance;
#endif

    JS_Init();
    AS_Init();
    F_Init();
    R_Init(arena, shared->windowHandle);
    D_Init();
    G_Init();
    DBG_Init();

    string binaryDirectory = OS_GetBinaryDirectory();
    // string sourceDLLFilename = StrConcat(debugState->arena, binaryDirectory, Str8Lit("keepmovingforward.dll"));
    // string tempDLLFilename = StrConcat(debugState->arena, binaryDirectory,
    // Str8Lit("keepmovingforward_temp.dll")); string lockFilename    = StrConcat(debugState->arena,
    // binaryDirectory, Str8Lit("lock.tmp"));

    // G_Init();
    // Game state initialization

    // OS_GameDLL gameDLL = OS_LoadGame2
    // OS_RenderDLL????
    // Win32GameCode win32GameCode = Win32LoadGameCode(sourceDLLFilename, tempDLLFilename, lockFilename);

    // Input ring buffer initialization
    {
        shared->i2gRing.size = kilobytes(64);
        shared->i2gRing.buffer = PushArray(arena, u8, shared->i2gRing.size);
    }

    G_EntryPoint(0);

    // Initialize separate game/render threads
    // OS_Handle gameThreadHandle   = OS_ThreadStart(G_EntryPoint, 0);
    // OS_Handle renderThreadHandle = OS_ThreadStart(R_EntryPoint, 0);
    // OS_Handle inputThreadHandle = OS_ThreadStart(I_

    // OS_ThreadJoin(gameThreadHandle);
    // OS_ThreadJoin(renderThreadHandle);
}
