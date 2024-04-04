// // NOTE: this file is retired
// #include <windows.h>
// #include <gl/GL.h>
// #include <xaudio2.h>
//
// #include "keepmovingforward_common.h"
// #include "keepmovingforward_math.h"
//
// #include "keepmovingforward_memory.h"
// #include "keepmovingforward_string.h"
// #include "platform_inc.h"
// #include "thread_context.h"
// #include "keepmovingforward_camera.h"
// #include "job.h"
// #include "render/render_core.h"
// #include "font.h"
// #include "asset.h"
// #include "asset_cache.h"
// #include "render/render.h"
// #include "render/opengl.h"
// #include "./offline/asset_processing.h"
// #include "keepmovingforward_platform.h"
// #include "win32_keepmovingforward.h"
//
// #include "platform_inc.cpp"
// #include "thread_context.cpp"
// #include "keepmovingforward_memory.cpp"
// #include "keepmovingforward_string.cpp"
// #include "job.cpp"
// #include "font.cpp"
// #include "asset.cpp"
// #include "asset_cache.cpp"
// #include "render/opengl.cpp"
//
// #include "crack.h"
// #ifdef LSP_INCLUDE
// #endif
//
// global b32 RUNNING = true;
//
// global Win32OffscreenBuffer GLOBAL_BACK_BUFFER;
// global IXAudio2SourceVoice *SOURCE_VOICES[3];
// global WINDOWPLACEMENT GLOBAL_WINDOW_POSITION = {sizeof(GLOBAL_WINDOW_POSITION)};
// global b32 GLOBAL_SHOW_CURSOR;
//
// internal V2 Win32GetWindowDimension(HWND window)
// {
//     V2 result;
//     RECT clientRect;
//     GetClientRect(window, &clientRect);
//     result.x = (f32)(clientRect.right - clientRect.left);
//     result.y = (f32)(clientRect.bottom - clientRect.top);
//     return result;
// }
//
// void PlatformToggleCursor(b32 value)
// {
//     GLOBAL_SHOW_CURSOR = value;
// }
//
// //*******************************************
// // DEBUG START
// //*******************************************
//
// // internal string Win32GetBinaryDirectory(Win32State *win32State)
// // {
// //     DWORD size   = kilobytes(4);
// //     u8 *fullPath = PushArray(win32State->arena, u8, size);
// //     DWORD length = GetModuleFileNameA(0, (char *)fullPath, size);
// //
// //     string binaryDirectory;
// //     binaryDirectory.str  = fullPath;
// //     binaryDirectory.size = length;
// //
// //     binaryDirectory = Str8PathChopLastSlash(binaryDirectory);
// //     return binaryDirectory;
// // }
//
// // internal string Win32GetFilePathInBinaryDirectory(Win32State *win32State, string filename)
// // {
// //     string result = PushStr8F(win32State->arena, (char *)"%S/%S", win32State->binaryDirectory, filename);
// //     return result;
// // }
//
// internal FILETIME Win32LastWriteTime(string filename)
// {
//     FILETIME fileTime = {};
//     WIN32_FILE_ATTRIBUTE_DATA data;
//     if (GetFileAttributesExA((char *)(filename.str), GetFileExInfoStandard, &data))
//     {
//         fileTime = data.ftLastWriteTime;
//     }
//     return fileTime;
// }
//
// internal Win32GameCode Win32LoadGameCode(string sourceDLLFilename, string tempDLLFilename, string lockFilename)
// {
//     Win32GameCode win32GameCode = {};
//
//     WIN32_FILE_ATTRIBUTE_DATA ignored;
//     if (!GetFileAttributesExA((char *)(lockFilename.str), GetFileExInfoStandard, &ignored))
//     {
//         CopyFileA((char *)(sourceDLLFilename.str), (char *)(tempDLLFilename.str), false);
//
//         HMODULE gameCodeDLL         = LoadLibraryA((char *)(tempDLLFilename.str));
//         win32GameCode.lastWriteTime = Win32LastWriteTime(sourceDLLFilename);
//         if (gameCodeDLL)
//         {
//             win32GameCode.gameCodeDLL = gameCodeDLL;
//             win32GameCode.GameUpdateAndRender =
//                 (GameUpdateAndRenderFunctionType *)GetProcAddress(gameCodeDLL, "GameUpdateAndRender");
//         }
//     }
//     if (!win32GameCode.GameUpdateAndRender)
//     {
//         win32GameCode.GameUpdateAndRender = 0;
//     }
//     return win32GameCode;
// }
//
// internal void Win32UnloadGameCode(Win32GameCode *win32GameCode)
// {
//     FreeLibrary(win32GameCode->gameCodeDLL);
//     win32GameCode->gameCodeDLL = 0;
// }
//
// inline Win32ReplayState *Win32GetReplayState(Win32State *state, int index)
// {
//     return &state->replayStates[index];
// }
// // begin writing
// internal void Win32BeginRecording(Win32State *state, int recordingIndex)
// {
//     state->currentRecordingIndex  = recordingIndex;
//     Win32ReplayState *replayState = Win32GetReplayState(state, recordingIndex);
//
//     TempArena temp = ScratchBegin(state->arena);
//     string filename =
//         PushStr8F(temp.arena, (char *)"%Skeepmovingforward_%d_input.kmf", state->binaryDirectory, recordingIndex);
//
//     state->recordingHandle =
//         CreateFileA((char *)filename.str, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
//     if (state->recordingHandle != INVALID_HANDLE_VALUE)
//     {
//         CopyMemory(replayState->fileMemory, state->memory, replayState->totalSize);
//     }
//     else
//     {
//         OutputDebugStringA("Recording file could not be created.");
//     }
//     ScratchEnd(temp);
// }
//
// // stop writing
// internal void Win32EndRecording(Win32State *state)
// {
//     CloseHandle(state->recordingHandle);
//     state->currentRecordingIndex = 0;
// }
//
// internal void Win32Record(Win32State *state, GameInput *input)
// {
//     DWORD bytesWritten;
//     WriteFile(state->recordingHandle, input, sizeof(*input), &bytesWritten, NULL);
// }
//
// // begin reading
// internal void Win32BeginPlayback(Win32State *state, int playbackIndex)
// {
//     state->currentPlaybackIndex   = playbackIndex;
//     Win32ReplayState *replayState = Win32GetReplayState(state, playbackIndex);
//
//     TempArena temp = ScratchBegin(state->arena);
//     string filename =
//         PushStr8F(temp.arena, (char *)"%Skeepmovingforward_%d_input.kmf", state->binaryDirectory, playbackIndex);
//
//     state->playbackHandle = CreateFileA((char *)filename.str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
//                                         FILE_ATTRIBUTE_NORMAL, NULL);
//     if (state->playbackHandle != INVALID_HANDLE_VALUE)
//     {
//         CopyMemory(state->memory, replayState->fileMemory, replayState->totalSize);
//     }
//     else
//     {
//         OutputDebugStringA("Could not open file to playback input.");
//     }
//
//     ScratchEnd(temp);
// }
//
// // stop reading
// internal void Win32EndPlayback(Win32State *state)
// {
//     CloseHandle(state->playbackHandle);
//     state->currentPlaybackIndex = 0;
// }
//
// internal void Win32Playback(Win32State *state, GameInput *input)
// {
//     DWORD bytesRead;
//     if (ReadFile(state->playbackHandle, input, sizeof(*input), &bytesRead, NULL))
//     {
//         if (bytesRead != sizeof(*input))
//         {
//             Win32EndPlayback(state);
//             Win32BeginPlayback(state, 1);
//             ReadFile(state->playbackHandle, input, sizeof(*input), &bytesRead, NULL);
//         }
//     }
// }
//
// //*******************************************
// // DEBUG END
// //*******************************************
//
// /*******************************************
// // AUDIO START
// */
// HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD &dwChunkSize, DWORD &dwChunkDataPosition)
// {
//     HRESULT hr = S_OK;
//     Assert(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, 0, 0, FILE_BEGIN));
//
//     DWORD dwChunkType;
//     DWORD dwChunkDataSize;
//     DWORD dwRIFFDataSize = 0;
//     DWORD dwFileType;
//     DWORD dwOffset = 0;
//
//     /* NOTE: RIFF files have four character code (FourCC) identifiers for each chunk
//      * RIFF Header:
//      *     Chunk Type: "RIFF" (FFIR cuz little endian)
//      *     Chunk Size: total size of rest of file
//      *     File Type: WAVE, others
//      * Format Chunk:
//      *     Chunk Type: 'fmt '
//      *     Chunk Size: size of rest of subchunk
//      *     Rest is fields for WAVEFORMATEX
//      * Data Chunk:
//      *     Chunk Type: 'data'
//      *     Chunk Size:
//      *     Rest is the actual raw audio data
//      * */
//     while (hr == S_OK)
//     {
//         DWORD dwRead;
//         Assert(ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, 0));
//
//         Assert(ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, 0));
//
//         switch (dwChunkType)
//         {
//             case 'FFIR':
//                 dwRIFFDataSize  = dwChunkDataSize;
//                 dwChunkDataSize = 4;
//                 Assert(ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, 0));
//                 Assert(dwFileType == 'EVAW');
//                 break;
//
//             default: Assert(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, dwChunkDataSize, 0, FILE_CURRENT));
//         }
//
//         dwOffset += sizeof(DWORD) * 2;
//
//         if (dwChunkType == fourcc)
//         {
//             dwChunkSize         = dwChunkDataSize;
//             dwChunkDataPosition = dwOffset;
//             return S_OK;
//         }
//
//         dwOffset += dwChunkDataSize;
//     }
//
//     return S_OK;
// }
//
// HRESULT ReadChunkData(HANDLE hFile, void *buffer, DWORD bufferSize, DWORD bufferOffset)
// {
//     Assert(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, bufferOffset, NULL, FILE_BEGIN));
//     DWORD dwRead;
//     Assert(ReadFile(hFile, buffer, bufferSize, &dwRead, 0));
//     return S_OK;
// }
//
// internal void PlayBanger(IXAudio2SourceVoice *sourceVoice)
// {
//     // Create file to play
//     HANDLE fileHandle =
//         CreateFileA("banger.wav", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//     Assert(fileHandle != INVALID_HANDLE_VALUE);
//     Assert(INVALID_SET_FILE_POINTER != SetFilePointer(fileHandle, 0, 0, FILE_BEGIN));
//
//     // Parse wav file in RIFF format, life sucks
//     DWORD chunkDataSize;
//     DWORD chunkPosition;
//     WAVEFORMATEX wfx      = {0};
//     XAUDIO2_BUFFER buffer = {0};
//
//     FindChunk(fileHandle, ' tmf', chunkDataSize, chunkPosition);
//     ReadChunkData(fileHandle, &wfx, chunkDataSize, chunkPosition);
//
//     FindChunk(fileHandle, 'atad', chunkDataSize, chunkPosition);
//     BYTE *audioData = (BYTE *)VirtualAlloc(0, chunkDataSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
//     ReadChunkData(fileHandle, audioData, chunkDataSize, chunkPosition);
//
//     buffer.AudioBytes = chunkDataSize;
//     buffer.pAudioData = audioData;
//     buffer.Flags      = XAUDIO2_END_OF_STREAM;
//
//     sourceVoice->SubmitSourceBuffer(&buffer);
//     sourceVoice->Start(0);
//
//     CloseHandle(fileHandle);
// }
//
// #if 0
// internal void PlaySineWave(IXAudio2SourceVoice *sourceVoice, int32 samplesPerSecond, int32 bufferSize)
// {
//     WAVEFORMATEX waveFormat = {};
//     waveFormat.wFormatTag = WAVE_FORMAT_PCM;
//     waveFormat.nChannels = 2;
//     waveFormat.nSamplesPerSec = samplesPerSecond;
//     waveFormat.wBitsPerSample = 16;
//     waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
//     waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
//     waveFormat.cbSize = 0;
//
//     BYTE *audioData = (BYTE *)VirtualAlloc(0, bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
//
//     XAUDIO2_BUFFER buffer = {};
//     int toneHz = 256;
//     i16 toneVolume = 3000;
//     f32 wavePeriod = (f32)samplesPerSecond / toneHz;
//
//     i16 *location = (i16 *)audioData;
//     for (int i = 0; i < samplesPerSecond; i++)
//     {
//         // f32 sineOutput = sinf(2.f * PI * i / wavePeriod);
//         i16 sampleOutput = (i16)(sineOutput * toneVolume);
//         *location++ = sampleOutput;
//         *location++ = sampleOutput;
//     }
//     buffer.AudioBytes = bufferSize;
//     buffer.pAudioData = audioData;
//     buffer.Flags = XAUDIO2_END_OF_STREAM;
//     buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
//
//     Assert(SUCCEEDED(sourceVoice->SubmitSourceBuffer(&buffer)));
//     Assert(SUCCEEDED(sourceVoice->Start(0)));
// }
// #endif
//
// internal void Win32InitializeXAudio2(int samplesPerSecond)
// {
//     if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return;
//     HMODULE xAudio2Library = LoadLibraryA("xaudio2_9.dll");
//     if (!xAudio2Library)
//     {
//         return;
//     }
//     XAudio2CreateFunctionType *XAudio2Create =
//         (XAudio2CreateFunctionType *)GetProcAddress(xAudio2Library, "XAudio2Create");
//     IXAudio2 *xAudio2;
//     if (!XAudio2Create || FAILED(XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
//     {
//         return;
//     }
//     IXAudio2MasteringVoice *masteringVoice;
//     if (FAILED(xAudio2->CreateMasteringVoice(&masteringVoice)))
//     {
//         return;
//     }
//
//     WAVEFORMATEX waveFormat    = {};
//     waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
//     waveFormat.nChannels       = 2;
//     waveFormat.nSamplesPerSec  = samplesPerSecond;
//     waveFormat.wBitsPerSample  = 16;
//     waveFormat.nBlockAlign     = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
//     waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
//     waveFormat.cbSize          = 0;
//
//     for (int sourceVoiceIndex = 0; sourceVoiceIndex < ArrayLength(SOURCE_VOICES); sourceVoiceIndex++)
//     {
//         IXAudio2SourceVoice *sourceVoice = SOURCE_VOICES[sourceVoiceIndex];
//         if (FAILED(xAudio2->CreateSourceVoice(&sourceVoice, &waveFormat)))
//         {
//             OutputDebugStringA("Failed to create source voice");
//             return;
//         }
//     }
// }
//
// /*******************************************
// // AUDIO END
// */
//
// inline LARGE_INTEGER Win32GetWallClock()
// {
//     LARGE_INTEGER result;
//     QueryPerformanceCounter(&result);
//     return result;
// }
//
// inline f32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
// {
//     return ((f32)(end.QuadPart - start.QuadPart) / (f32)GLOBAL_PERFORMANCE_COUNT_FREQUENCY);
// }
//
// // TODO: make this platform nonspecific
// f32 Win32GetTimeElapsed()
// {
//     return ((f32)(Win32GetWallClock().QuadPart - START_TIME.QuadPart) / (f32)GLOBAL_PERFORMANCE_COUNT_FREQUENCY);
// }
//
// internal void Win32ProcessKeyboardMessages(GameButtonState *buttonState, b32 isDown)
// {
//     if (buttonState->keyDown != isDown)
//     {
//         buttonState->keyDown = isDown;
//         buttonState->halfTransitionCount++;
//     }
// }
//
// // TODO: things to do
// // finish input from win32, input file uses it, sends it to ring queue atomically
// // game reads input atomically, compiles render state, sends it to another ring atomically
// // render reads from the ring atomically
// internal void Win32ProcessPendingMessages(HWND window, Win32State *state, GameInput *keyboardController)
// {
//     MSG message;
//     b32 isRelease = 0;
//     while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
//     {
//         switch (message.message)
//         {
//             case WM_LBUTTONUP:
//             {
//                 ReleaseCapture();
//                 isRelease = 1;
//             }
//             case WM_LBUTTONDOWN:
//             {
//                 if (isRelease == 0) SetCapture(window);
//                 b32 isDown = message.wParam & 1;
//                 Win32ProcessKeyboardMessages(&keyboardController->leftClick, isDown);
//                 break;
//             }
//             case WM_RBUTTONUP:
//             {
//                 ReleaseCapture();
//                 isRelease = 1;
//             }
//             case WM_RBUTTONDOWN:
//             {
//                 if (isRelease == 0) SetCapture(window);
//                 b32 isDown = message.wParam & (1 << 1);
//                 Win32ProcessKeyboardMessages(&keyboardController->rightClick, isDown);
//                 break;
//             }
//             case WM_KEYUP:
//             case WM_KEYDOWN:
//             case WM_SYSKEYUP:
//             case WM_SYSKEYDOWN:
//             {
//                 u32 keyCode    = (u32)message.wParam;
//                 b32 wasDown    = (message.lParam & (1 << 30)) != 0;
//                 b32 isDown     = (message.lParam & (1 << 31)) == 0;
//                 b32 altWasDown = (message.lParam & (1 << 29)) != 0;
//
//                 if (keyCode == 'W')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->up, isDown);
//                 }
//                 if (keyCode == 'A')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->left, isDown);
//                 }
//                 if (keyCode == 'S')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->down, isDown);
//                 }
//                 if (keyCode == 'D')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->right, isDown);
//                 }
//                 if (keyCode == VK_SPACE)
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->jump, isDown);
//                 }
//                 if (keyCode == VK_SHIFT)
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->shift, isDown);
//                 }
//                 if (keyCode == 'C')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->swap, isDown);
//                 }
//                 if (keyCode == 'X')
//                 {
//                     Win32ProcessKeyboardMessages(&keyboardController->soul, isDown);
//                 }
//
//                 if (keyCode == VK_F4 && altWasDown)
//                 {
//                     RUNNING = false;
//                 }
//                 if (isDown && wasDown != isDown && keyCode == VK_RETURN && altWasDown)
//                 {
//                     ToggleFullscreen(message.hwnd);
//                 }
//                 if (keyCode == 'L')
//                 {
//                     if (isDown && isDown != wasDown)
//                     {
//                         if (state->currentPlaybackIndex)
//                         {
//                             Win32EndPlayback(state);
//                         }
//                         else if (!state->currentRecordingIndex)
//                         {
//                             Win32BeginRecording(state, 1);
//                         }
//                         else if (state->currentRecordingIndex && !state->currentPlaybackIndex)
//                         {
//                             Win32EndRecording(state);
//                             Win32BeginPlayback(state, 1);
//                         }
//                     }
//                 }
//                 break;
//             }
//             case WM_QUIT:
//             {
//                 RUNNING = false;
//                 break;
//             }
//             case WM_PAINT:
//             {
//                 PAINTSTRUCT paint;
//                 HDC dc = BeginPaint(window, &paint);
//                 EndPaint(window, &paint);
//                 break;
//             }
//             default:
//             {
//                 TranslateMessage(&message);
//                 DispatchMessageW(&message);
//             }
//         }
//     }
// }
//
// int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
// {
//
//     // NOTE: this works, lowered framerate for testing
//     // int gameUpdateHz = GetDeviceCaps(deviceContext, VREFRESH);
//     // if (gameUpdateHz <= 1)
//     // {
//     //     gameUpdateHz = 60;
//     // }
//     // int gameUpdateHz            = 144;
//     // f32 expectedSecondsPerFrame = 1.f / (f32)gameUpdateHz;
//
//     //
//     // GAME MEMORY
//     //
//
//     // GameMemory gameMemory            = {};
//     // gameMemory.PersistentStorageSize = megabytes(64);
//     // gameMemory.TransientStorageSize  = gigabytes(1);
//     // u64 totalStorageSize             = gameMemory.PersistentStorageSize + gameMemory.TransientStorageSize;
//     // gameMemory.PersistentStorageMemory =
//     //     VirtualAlloc(baseAddress, totalStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
//     // gameMemory.TransientStorageMemory =
//     //     (u8 *)gameMemory.PersistentStorageMemory + gameMemory.PersistentStorageSize;
//
//     // Arena *win32Arena = ArenaAlloc();
//
//     // gameMemory.PlatformToggleCursor = PlatformToggleCursor;
//     // gameMemory.R_AllocateTexture2D  = R_AllocateTextureInArray;
//     // gameMemory.R_DeleteTexture2D    = R_DeleteTexture2D;
//     // gameMemory.R_AllocateBuffer     = R_AllocateBuffer;
//     //
//
//     //////////////////////////////
//     // AUDIO
//     //
//     // for 1 sec buffer, samplesPerSecond = samplesPerBuffer
//     // int samplesPerSecond = 48000;
//     // int bytesPerSample   = sizeof(i16) * 2;
//     // // 2 frames of audio data
//     // int bufferSize = (int)(2 * samplesPerSecond * bytesPerSample / (f32)gameUpdateHz);
//     // Win32InitializeXAudio2(samplesPerSecond);
//     // // 3 sound buffers
//     // i16 *samples = (i16 *)VirtualAlloc(sampleBaseAddress, bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
//
//     //////////////////////////////
//     // INPUT
//     //
//     GameInput oldInput = {};
//     GameInput newInput = {};
//
//     //////////////////////////////
//     // Thread Context
//     //
//
//     //////////////////////////////
//     // Render
//     //
//     RenderState renderState = {};
//     R_Init(windowHandle);
//
//     string sourceDLLFilename = Win32GetFilePathInBinaryDirectory(&win32State, Str8Lit("keepmovingforward.dll"));
//     string tempDLLFilename = Win32GetFilePathInBinaryDirectory(&win32State, Str8Lit("keepmovingforward_temp.dll"));
//     string lockFilename    = Win32GetFilePathInBinaryDirectory(&win32State, Str8Lit("lock.tmp"));
//
//     Win32GameCode win32GameCode = Win32LoadGameCode(sourceDLLFilename, tempDLLFilename, lockFilename);
//
//     // OTE: creates memory mapped files which hold data for looped live coding
//     for (int i = 0; i < ArrayLength(win32State.replayStates); i++)
//     {
//         Win32ReplayState *replayState = Win32GetReplayState(&win32State, i);
//
//         TempArena temp = ScratchBegin(win32State.arena);
//         replayState->filename =
//             // help me god
//             PushStr8F(temp.arena, (char *)"%S/keepmovingforward_%d_state.kmf", win32State.binaryDirectory, i);
//
//         replayState->fileHandle = CreateFileA((char *)replayState->filename.str, GENERIC_READ | GENERIC_WRITE, 0,
//                                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
//         if (replayState->fileHandle != INVALID_HANDLE_VALUE)
//         {
//             LARGE_INTEGER fileSize;
//             fileSize.QuadPart = totalStorageSize;
//
//             HANDLE fileMappingHandle = CreateFileMappingA(replayState->fileHandle, NULL, PAGE_READWRITE,
//                                                           fileSize.HighPart, fileSize.LowPart, NULL);
//             replayState->fileMemory =
//                 MapViewOfFile(fileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, totalStorageSize);
//             replayState->totalSize = totalStorageSize;
//         }
//         ScratchEnd(temp);
//     }
//
//     START_TIME = Win32GetWallClock();
//
//     LARGE_INTEGER lastPerformanceCount = Win32GetWallClock();
//     u64 lastCycleCount                 = __rdtsc();
//
//     int currentSample = 0;
//     int currentSound  = 0;
//
//     f32 previousTime = 0;
//     f32 dT           = 0;
//     f32 currentTime  = 0;
//
//     f32 lag = 0;
//
//     while (RUNNING)
//     {
//         previousTime = currentTime;
//         currentTime  = Win32GetTimeElapsed();
//         dT           = currentTime - previousTime;
//
//         FILETIME lastWriteTime = Win32LastWriteTime(sourceDLLFilename);
//         if (CompareFileTime(&lastWriteTime, &win32GameCode.lastWriteTime) > 0)
//         {
//             Win32UnloadGameCode(&win32GameCode);
//             win32GameCode = Win32LoadGameCode(sourceDLLFilename, tempDLLFilename, lockFilename);
//         }
//
//         V2 dimension = Win32GetWindowDimension(windowHandle);
//         R_BeginFrame((i32)dimension.x, (i32)dimension.y);
//
//         newInput    = {};
//         newInput.dT = expectedSecondsPerFrame;
//         for (int buttonIndex = 0; buttonIndex < ArrayLength(newInput.buttons); buttonIndex++)
//         {
//             newInput.buttons[buttonIndex].keyDown = oldInput.buttons[buttonIndex].keyDown;
//         }
//         newInput.lastMousePos = oldInput.mousePos;
//
//         // Mouse
//         // if (!GLOBAL_SHOW_CURSOR)
//         // {
//         //     SetCursorPos((i32)center.x, (i32)center.y);
//         // }
//
//         Win32ProcessPendingMessages(windowHandle, &win32State, &newInput);
//
//         GameOffscreenBuffer backBuffer = {};
//         backBuffer.memory              = GLOBAL_BACK_BUFFER.memory;
//         backBuffer.width               = GLOBAL_BACK_BUFFER.width;
//         backBuffer.height              = GLOBAL_BACK_BUFFER.height;
//         backBuffer.pitch               = GLOBAL_BACK_BUFFER.pitch;
//         backBuffer.bytesPerPixel       = GLOBAL_BACK_BUFFER.bytesPerPixel;
//
//         GameSoundOutput soundOutput  = {};
//         soundOutput.samplesPerSecond = samplesPerSecond;
//
//         XAUDIO2_BUFFER audioBuffer = {};
//         audioBuffer.AudioBytes     = bufferSize;
//         audioBuffer.pAudioData     = (BYTE *)soundOutput.samples;
//
//         if (win32State.currentRecordingIndex)
//         {
//             Win32Record(&win32State, &newInput);
//         }
//         if (win32State.currentPlaybackIndex)
//         {
//             Win32Playback(&win32State, &newInput);
//         }
//         if (win32GameCode.GameUpdateAndRender)
//         {
//             win32GameCode.GameUpdateAndRender(&gameMemory, &renderState, &soundOutput, &newInput, dT);
//         }
//         // Sleep until next frame
//         {
//             LARGE_INTEGER workCounter  = Win32GetWallClock();
//             f32 secondsElapsedForFrame = Win32GetSecondsElapsed(lastPerformanceCount, workCounter);
//             if (secondsElapsedForFrame < expectedSecondsPerFrame)
//             {
//                 if (sleepIsGranular)
//                 {
//                     DWORD sleepMs = (DWORD)(1000.f * (expectedSecondsPerFrame - secondsElapsedForFrame));
//                     if (sleepMs > 0)
//                     {
//                         Sleep(sleepMs);
//                     }
//                 }
//                 // if any sleep leftover
//                 while (secondsElapsedForFrame < expectedSecondsPerFrame)
//                 {
//                     secondsElapsedForFrame = Win32GetSecondsElapsed(lastPerformanceCount, Win32GetWallClock());
//                 }
//             }
//             else
//             {
//                 // OutputDebugStringA("Missed frame rate somehow\n");
//             }
//         }
//
//         // NOTE: Get time before flip, doesn't actually sync with monitor YET
//         LARGE_INTEGER endPerformanceCount = Win32GetWallClock();
//         f32 msPerFrame       = 1000.f * Win32GetSecondsElapsed(lastPerformanceCount, endPerformanceCount);
//         lastPerformanceCount = endPerformanceCount;
//
//         HDC deviceContext = GetDC(windowHandle);
//         R_EndFrame(&renderState, deviceContext, (i32)(dimension.x), (i32)(dimension.y));
//         ReleaseDC(windowHandle, deviceContext);
//
//         oldInput = newInput;
//     }
//     return 0;
// }
