#include "keepmovingforward.h"

#include <math.h>
#include <stdio.h>
#include <windows.h>

#include "win32_keepmovingforward.h"

static bool RUNNING = true;

static int64 GLOBAL_PERFORMANCE_COUNT_FREQUENCY;
static Win32OffscreenBuffer GLOBAL_BACK_BUFFER;
static IXAudio2SourceVoice *SOURCE_VOICES[3];

static Win32WindowDimension Win32GetWindowDimension(HWND window)
{
    Win32WindowDimension result;
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;
    return result;
}

//*******************************************
// DEBUG START
//*******************************************

#if INTERNAL
DEBUG_PLATFORM_FREE_FILE(DebugPlatformFreeFile)
{
    if (fileMemory)
    {
        VirtualFree(fileMemory, 0, MEM_RELEASE);
    }
}
DEBUG_PLATFORM_READ_FILE(DebugPlatformReadFile)
{
    DebugReadFileOutput readFileOutput = {};
    HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSizeLargeInteger;
        if (GetFileSizeEx(fileHandle, &fileSizeLargeInteger))
        {
            uint32 fileSize = (uint32)fileSizeLargeInteger.QuadPart;
            readFileOutput.contents =
                VirtualAlloc(NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (readFileOutput.contents)
            {
                DWORD bytesToRead;
                if (ReadFile(fileHandle, readFileOutput.contents, fileSize, &bytesToRead, NULL))
                {
                    readFileOutput.fileSize = fileSize;
                }
                else
                {
                    DebugPlatformFreeFile(&readFileOutput);
                    readFileOutput.contents = NULL;
                }
            }
            else
            {
            }
        }
        else
        {
        }
        CloseHandle(fileHandle);
    }
    else
    {
    }
    return readFileOutput;
}

DEBUG_PLATFORM_WRITE_FILE(DebugPlatformWriteFile)
{
    bool result = false;
    HANDLE fileHandle =
        CreateFileA(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesToWrite;
        if (WriteFile(fileHandle, fileMemory, fileSize, &bytesToWrite, NULL))
        {
            result = (fileSize == bytesToWrite);
        }
        else
        {
        }
        CloseHandle(fileHandle);
    }
    else
    {
    }
    return result;
}
#endif

static void Win32GetEXEFilepath(Win32State *win32State)
{
    GetModuleFileNameA(NULL, win32State->executableFullPath,
                       sizeof(win32State->executableFullPath));

    DWORD executableDirectoryLength = 0;
    DWORD count = 0;
    for (char *scan = win32State->executableFullPath; *scan; scan++)
    {
        if (*scan == '\\')
        {
            executableDirectoryLength = count + 1;
        }
        count++;
    }
    CopyString(win32State->executableDirectory, win32State->executableFullPath);
    win32State->executableDirectory[executableDirectoryLength] = 0;
}

static void Win32GetEXEDirectoryFilepath(Win32State *win32State, char *destination,
                                         const char *filename)
{
    CopyString(destination, win32State->executableDirectory);
    AddStrings(destination, filename);
}

static FILETIME Win32LastWriteTime(const char *filename)
{
    FILETIME fileTime = {};
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data))
    {
        fileTime = data.ftLastWriteTime;
    }
    return fileTime;
}

static Win32GameCode Win32LoadGameCode(const char *sourceDLLFilename, const char *tempDLLFilename)
{
    Win32GameCode win32GameCode = {};
    // NOTE: when DLL is loaded, code won't compile; need to use copy
    CopyFileA(sourceDLLFilename, tempDLLFilename, false);

    HMODULE gameCodeDLL = LoadLibraryA(tempDLLFilename);
    win32GameCode.lastWriteTime = Win32LastWriteTime(sourceDLLFilename);
    if (gameCodeDLL)
    {
        win32GameCode.gameCodeDLL = gameCodeDLL;
        win32GameCode.GameUpdateAndRender =
            (GameUpdateAndRenderFunctionType *)GetProcAddress(gameCodeDLL, "GameUpdateAndRender");
        if (!win32GameCode.GameUpdateAndRender)
        {
            win32GameCode.GameUpdateAndRender = 0;
        }
    }
    else
    {
    }
    return win32GameCode;
}

static void Win32UnloadGameCode(Win32GameCode *win32GameCode)
{
    FreeLibrary(win32GameCode->gameCodeDLL);
    win32GameCode->gameCodeDLL = 0;
}

//*******************************************
// DEBUG END
//*******************************************

//*******************************************
// RENDER START
//*******************************************
static void Win32ResizeDIBSection(Win32OffscreenBuffer *buffer, int width, int height)
{
    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    buffer->width = width;
    buffer->height = height;
    int bytesPerPixel = 4;
    buffer->pitch = buffer->width * bytesPerPixel;
    buffer->bytesPerPixel = bytesPerPixel;

    buffer->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    // NOTE: Aligned on 4 Byte boundaries to prevent performance cost
    int BitmapMemorySize = bytesPerPixel * width * height;
    buffer->memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void Win32DisplayBufferInWindow(Win32OffscreenBuffer *buffer, HDC deviceContext,
                                       int clientWidth, int clientHeight)
{
    StretchDIBits(deviceContext, 0, 0, clientWidth, clientHeight, 0, 0, buffer->width,
                  buffer->height, buffer->memory, &(buffer->info), DIB_RGB_COLORS, SRCCOPY);
}

/*******************************************
// RENDER END
*/
/*******************************************
// AUDIO START
*/
HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD &dwChunkSize, DWORD &dwChunkDataPosition)
{
    HRESULT hr = S_OK;
    Assert(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, 0, 0, FILE_BEGIN));

    DWORD dwChunkType;
    DWORD dwChunkDataSize;
    DWORD dwRIFFDataSize = 0;
    DWORD dwFileType;
    DWORD dwOffset = 0;

    /* NOTE: RIFF files have four character code (FourCC) identifiers for each chunk
     * RIFF Header:
     *     Chunk Type: "RIFF" (FFIR cuz little endian)
     *     Chunk Size: total size of rest of file
     *     File Type: WAVE, others
     * Format Chunk:
     *     Chunk Type: 'fmt '
     *     Chunk Size: size of rest of subchunk
     *     Rest is fields for WAVEFORMATEX
     * Data Chunk:
     *     Chunk Type: 'data'
     *     Chunk Size:
     *     Rest is the actual raw audio data
     * */
    while (hr == S_OK)
    {
        DWORD dwRead;
        Assert(ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, 0));

        Assert(ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, 0));

        switch (dwChunkType)
        {
        case 'FFIR':
            dwRIFFDataSize = dwChunkDataSize;
            dwChunkDataSize = 4;
            Assert(ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, 0));
            Assert(dwFileType == 'EVAW');
            break;

        default:
            Assert(INVALID_SET_FILE_POINTER !=
                   SetFilePointer(hFile, dwChunkDataSize, 0, FILE_CURRENT));
        }

        dwOffset += sizeof(DWORD) * 2;

        if (dwChunkType == fourcc)
        {
            dwChunkSize = dwChunkDataSize;
            dwChunkDataPosition = dwOffset;
            return S_OK;
        }

        dwOffset += dwChunkDataSize;
    }

    return S_OK;
}

HRESULT ReadChunkData(HANDLE hFile, void *buffer, DWORD bufferSize, DWORD bufferOffset)
{
    Assert(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, bufferOffset, NULL, FILE_BEGIN));
    DWORD dwRead;
    Assert(ReadFile(hFile, buffer, bufferSize, &dwRead, 0));
    return S_OK;
}

static void PlayBanger(IXAudio2SourceVoice *sourceVoice)
{
    // Create file to play
    HANDLE fileHandle = CreateFileA("banger.wav", GENERIC_READ, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    Assert(fileHandle != INVALID_HANDLE_VALUE);
    Assert(INVALID_SET_FILE_POINTER != SetFilePointer(fileHandle, 0, 0, FILE_BEGIN));

    // Parse wav file in RIFF format, life sucks
    DWORD chunkDataSize;
    DWORD chunkPosition;
    WAVEFORMATEX wfx = {0};
    XAUDIO2_BUFFER buffer = {0};

    FindChunk(fileHandle, ' tmf', chunkDataSize, chunkPosition);
    ReadChunkData(fileHandle, &wfx, chunkDataSize, chunkPosition);

    FindChunk(fileHandle, 'atad', chunkDataSize, chunkPosition);
    BYTE *audioData =
        (BYTE *)VirtualAlloc(0, chunkDataSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    ReadChunkData(fileHandle, audioData, chunkDataSize, chunkPosition);

    buffer.AudioBytes = chunkDataSize;
    buffer.pAudioData = audioData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    sourceVoice->SubmitSourceBuffer(&buffer);
    sourceVoice->Start(0);

    CloseHandle(fileHandle);
}

static void PlaySineWave(IXAudio2SourceVoice *sourceVoice, int32 samplesPerSecond, int32 bufferSize)
{
    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = samplesPerSecond;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    BYTE *audioData = (BYTE *)VirtualAlloc(0, bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    XAUDIO2_BUFFER buffer = {};
    int toneHz = 256;
    int16 toneVolume = 3000;
    float wavePeriod = (float)samplesPerSecond / toneHz;

    int16 *location = (int16 *)audioData;
    for (int i = 0; i < samplesPerSecond; i++)
    {
        float sineOutput = sinf(2.f * PI * i / wavePeriod);
        int16 sampleOutput = (int16)(sineOutput * toneVolume);
        *location++ = sampleOutput;
        *location++ = sampleOutput;
    }
    buffer.AudioBytes = bufferSize;
    buffer.pAudioData = audioData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    Assert(SUCCEEDED(sourceVoice->SubmitSourceBuffer(&buffer)));
    Assert(SUCCEEDED(sourceVoice->Start(0)));
}

static void Win32InitializeXAudio2(int samplesPerSecond)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return;
    HMODULE xAudio2Library = LoadLibraryA("xaudio2_9.dll");
    if (!xAudio2Library)
    {
        return;
    }
    XAudio2CreateFunctionType *XAudio2Create =
        (XAudio2CreateFunctionType *)GetProcAddress(xAudio2Library, "XAudio2Create");
    IXAudio2 *xAudio2;
    if (!XAudio2Create || FAILED(XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
    {
        return;
    }
    IXAudio2MasteringVoice *masteringVoice;
    if (FAILED(xAudio2->CreateMasteringVoice(&masteringVoice)))
    {
        return;
    }

    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = samplesPerSecond;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    for (int sourceVoiceIndex = 0; sourceVoiceIndex < ArrayLength(SOURCE_VOICES);
         sourceVoiceIndex++)
    {
        IXAudio2SourceVoice *sourceVoice = SOURCE_VOICES[sourceVoiceIndex];
        if (FAILED(xAudio2->CreateSourceVoice(&sourceVoice, &waveFormat)))
        {
            OutputDebugStringA("Failed to create source voice");
            return;
        }
    }
}

/*******************************************
// AUDIO END
*/

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

inline float Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    return ((float)(end.QuadPart - start.QuadPart) / (float)GLOBAL_PERFORMANCE_COUNT_FREQUENCY);
}

static void Win32ProcessKeyboardMessages(GameButtonState *buttonState, bool isDown)
{
    if (buttonState->keyDown != isDown)
    {
        buttonState->keyDown = isDown;
        buttonState->halfTransitionCount++;
    }
}

static void Win32ProcessPendingMessages(GameControllerInput *keyboardController)
{
    MSG message;
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        {
            uint32 keyCode = (uint32)message.wParam;
            bool wasDown = (message.lParam & (1 << 30)) != 0;
            bool isDown = (message.lParam & (1 << 31)) == 0;
            bool altWasDown = (message.lParam & (1 << 29)) != 0;

            if (keyCode == 'W')
            {
                Win32ProcessKeyboardMessages(&keyboardController->up, isDown);
            }
            if (keyCode == 'A')
            {
                Win32ProcessKeyboardMessages(&keyboardController->left, isDown);
            }
            if (keyCode == 'S')
            {
                Win32ProcessKeyboardMessages(&keyboardController->down, isDown);
            }
            if (keyCode == 'D')
            {
                Win32ProcessKeyboardMessages(&keyboardController->right, isDown);
            }
            if (keyCode == VK_F4 && altWasDown)
            {
                RUNNING = false;
            }
            break;
        }
        case WM_QUIT:
        {
            RUNNING = false;
            break;
        }
        default:
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        }
    }
}

LRESULT WindowsCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
    case WM_SIZE:
    {
        break;
    }
    case WM_ACTIVATEAPP:
    {
        break;
    }
    case WM_DESTROY:
    {
        RUNNING = false;
        break;
    }
    case WM_CLOSE:
    {
        RUNNING = false;
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC deviceContext = BeginPaint(window, &paint);
        Win32WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&GLOBAL_BACK_BUFFER, deviceContext, dimension.width,
                                   dimension.height);
        EndPaint(window, &paint);
        break;
    }
    default:
    {
        result = DefWindowProcW(window, message, wParam, lParam);
    }
    }
    return result;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    LARGE_INTEGER performanceFrequencyUnion;
    QueryPerformanceFrequency(&performanceFrequencyUnion);
    GLOBAL_PERFORMANCE_COUNT_FREQUENCY = performanceFrequencyUnion.QuadPart;

    // NOTE: Makes sleep have 1ms granularity, because OS scheduler may sleep longer/shorter than
    // input value
    UINT desiredSchedulerMs = 1;
    bool sleepIsGranular = (timeBeginPeriod(desiredSchedulerMs) == TIMERR_NOERROR);

    Win32ResizeDIBSection(&GLOBAL_BACK_BUFFER, 1280, 720);

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WindowsCallback;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = L"Keep Moving Forward";

    if (!RegisterClassW(&windowClass))
    {
        return 1;
    }
    HWND windowHandle = CreateWindowExW(
        0, windowClass.lpszClassName, L"keep moving forward", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
    if (!windowHandle)
    {
        return 1;
    }
    HDC deviceContext = GetDC(windowHandle);

    int gameUpdateHz = 144;
    float expectedSecondsPerFrame = 1.f / (float)gameUpdateHz;

    // NOTE: janky audio setup:
    // 3 source voices, submit sine wave infinitely repeating to one
    // when input occurs, create a new sine wave buffer and submit to another voice
    // stop the previously running voice, start the other voice.

    // for 1 sec buffer, samplesPerSecond = samplesPerBuffer
    int samplesPerSecond = 48000;
    int bytesPerSample = sizeof(int16) * 2;
    // 2 frames of audio data
    int bufferSize = (int)(2 * samplesPerSecond * bytesPerSample / (float)gameUpdateHz);
    Win32InitializeXAudio2(samplesPerSecond);

#if INTERNAL
    LPVOID sampleBaseAddress = (LPVOID)terabytes(4);
#else
    LPVOID sampleBaseAddress = 0;
#endif

    // 3 sound buffers
    int16 *samples = (int16 *)VirtualAlloc(sampleBaseAddress, bufferSize, MEM_RESERVE | MEM_COMMIT,
                                           PAGE_READWRITE);

#if INTERNAL
    LPVOID baseAddress = (LPVOID)terabytes(2);
#else
    LPVOID baseAddress = 0;
#endif

    GameMemory gameMemory = {};
    gameMemory.PersistentStorageSize = megabytes(64);
    gameMemory.TransientStorageSize = gigabytes(4);
    gameMemory.PersistentStorageMemory = VirtualAlloc(
        baseAddress, gameMemory.PersistentStorageSize + gameMemory.TransientStorageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    gameMemory.TransientStorageMemory =
        (uint8 *)gameMemory.PersistentStorageMemory + gameMemory.PersistentStorageSize;

    if (!samples || !gameMemory.PersistentStorageMemory || !gameMemory.TransientStorageMemory)
    {
        OutputDebugStringA("Memory allocation failed.");
        return 1;
    }

#if INTERNAL
    gameMemory.DebugPlatformFreeFile = DebugPlatformFreeFile;
    gameMemory.DebugPlatformReadFile = DebugPlatformReadFile;
    gameMemory.DebugPlatformWriteFile = DebugPlatformWriteFile;
#endif

    GameInput input[2] = {};
    GameInput *oldInput = &input[0];
    GameInput *newInput = &input[1];

    Win32State win32State = {};
    Win32GetEXEFilepath(&win32State);

    char sourceDLLFilename[MAX_PATH] = {};
    char tempDLLFilename[MAX_PATH] = {};
    Win32GetEXEDirectoryFilepath(&win32State, sourceDLLFilename, "keepmovingforward.dll");
    Win32GetEXEDirectoryFilepath(&win32State, tempDLLFilename, "keepmovingforward_temp.dll");

    Win32GameCode win32GameCode = Win32LoadGameCode(sourceDLLFilename, tempDLLFilename);

    LARGE_INTEGER lastPerformanceCount = Win32GetWallClock();
    uint64 lastCycleCount = __rdtsc();

    int currentSample = 0;
    int currentSound = 0;

    while (RUNNING)
    {
        FILETIME lastWriteTime = Win32LastWriteTime(sourceDLLFilename);
        if (CompareFileTime(&lastWriteTime, &win32GameCode.lastWriteTime) > 0)
        {
            Win32UnloadGameCode(&win32GameCode);
            win32GameCode = Win32LoadGameCode(sourceDLLFilename, tempDLLFilename);
        }

        GameControllerInput *oldKeyboardController = &(oldInput->controllers[0]);
        GameControllerInput *newKeyboardController = &(newInput->controllers[0]);
        *newKeyboardController = {};
        for (int buttonIndex = 0; buttonIndex < ArrayLength(newKeyboardController->buttons);
             buttonIndex++)
        {
            newKeyboardController->buttons[buttonIndex].keyDown =
                oldKeyboardController->buttons[buttonIndex].keyDown;
        }

        Win32ProcessPendingMessages(newKeyboardController);

        GameOffscreenBuffer backBuffer = {};
        backBuffer.memory = GLOBAL_BACK_BUFFER.memory;
        backBuffer.width = GLOBAL_BACK_BUFFER.width;
        backBuffer.height = GLOBAL_BACK_BUFFER.height;
        backBuffer.pitch = GLOBAL_BACK_BUFFER.pitch;
        backBuffer.bytesPerPixel = GLOBAL_BACK_BUFFER.bytesPerPixel;

        GameSoundOutput soundOutput = {};
        soundOutput.samplesPerSecond = samplesPerSecond;

        if (win32GameCode.GameUpdateAndRender)
        {
            win32GameCode.GameUpdateAndRender(&gameMemory, &backBuffer, &soundOutput, newInput);
        }

        XAUDIO2_BUFFER audioBuffer = {};
        audioBuffer.AudioBytes = bufferSize;
        audioBuffer.pAudioData = (BYTE *)soundOutput.samples;

        // Sleep until next frame
        {
            LARGE_INTEGER workCounter = Win32GetWallClock();
            float secondsElapsedForFrame =
                Win32GetSecondsElapsed(lastPerformanceCount, workCounter);
            if (secondsElapsedForFrame < expectedSecondsPerFrame)
            {
                if (sleepIsGranular)
                {
                    DWORD sleepMs =
                        (DWORD)(1000.f * (expectedSecondsPerFrame - secondsElapsedForFrame));
                    if (sleepMs > 0)
                    {
                        Sleep(sleepMs);
                    }
                }
                // if any sleep leftover
                while (secondsElapsedForFrame < expectedSecondsPerFrame)
                {
                    secondsElapsedForFrame =
                        Win32GetSecondsElapsed(lastPerformanceCount, Win32GetWallClock());
                }
            }
            else
            {
                OutputDebugStringA("Missed frame rate somehow");
            }
        }

        // NOTE: Get time before flip, doesn't actually sync with monitor YET
        LARGE_INTEGER endPerformanceCount = Win32GetWallClock();
        float msPerFrame =
            1000.f * Win32GetSecondsElapsed(lastPerformanceCount, endPerformanceCount);
        lastPerformanceCount = endPerformanceCount;

        Win32WindowDimension dimension = Win32GetWindowDimension(windowHandle);
        Win32DisplayBufferInWindow(&GLOBAL_BACK_BUFFER, deviceContext, dimension.width,
                                   dimension.height);

        GameInput *Temp = oldInput;
        oldInput = newInput;
        newInput = Temp;

        uint64 endCycleCount = __rdtsc();

        double framesPerSecond = 1 / (double)(msPerFrame / 1000.f);
        double megaCyclesPerFrame = (double)(endCycleCount - lastCycleCount) / (1000.f * 1000.f);

        char printBuffer[256];
        _snprintf_s(printBuffer, sizeof(printBuffer), "%fms/F, %fFPS, %fmc/F\n", msPerFrame,
                    framesPerSecond, megaCyclesPerFrame);
        OutputDebugStringA(printBuffer);

        lastCycleCount = endCycleCount;
    }

    return 0;
}
