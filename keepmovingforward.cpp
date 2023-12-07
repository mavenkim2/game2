#include "keepmovingforward.h"
#include <math.h>

void GameOutputSound(GameSoundOutput *soundBuffer, int toneHz)
{
    static float tSine = 0;
    int16 toneVolume = 3000;
    float wavePeriod = (float)soundBuffer->samplesPerSecond / toneHz;

    int16 *sampleOutput = soundBuffer->samples;
    for (int i = 0; i < soundBuffer->sampleCount; i++)
    {
        float sineValue = sinf(tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
        *sampleOutput++ = sampleValue;
        *sampleOutput++ = sampleValue;

        tSine += 2.f * PI * 1.f / wavePeriod;
        if (tSine > 2.f * PI)
        {
            tSine -= 2.f * PI;
        }
    }
}

void RenderGradient(GameOffscreenBuffer *buffer, int xOffset, int yOffset)
{
    uint8 *row = (uint8 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++)
    {
        uint32 *pixel = (uint32 *)row;
        for (int x = 0; x < buffer->width; x++)
        {
            // BB GG RR XX, because Windows!
            // Blue is first
            // Memory: BB GG RR XX
            // Register: XX RR GG BB
            uint8 blue = (uint8)(x + xOffset);
            uint8 green = (uint8)(y + yOffset);
            *pixel = blue | green << 8;
            pixel++;
        }
        row += buffer->pitch;
    }
}

void RenderPlayer(GameOffscreenBuffer *buffer, int playerX, int playerY)
{
    uint32 color = 0xFFFFFFFF;
    uint8 *memoryStart = (uint8 *)buffer->memory;
    uint8 *endOfBuffer = memoryStart + buffer->pitch * buffer->height;
    for (int x = playerX; x < playerX + 10; x++)
    {
        uint8 *pixel =
            memoryStart + (playerY * buffer->pitch) + (x * buffer->bytesPerPixel);
        for (int y = playerY; y < playerY + 10; y++)
        {
            if (pixel >= memoryStart && (pixel+4) <= endOfBuffer)
            {
                *(uint32*)pixel = color;
            }
            pixel += buffer->pitch;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    if (!memory->isInitialized)
    {
#if INTERNAL
        char const *fileName = __FILE__;
        DebugReadFileOutput readFileOutput = memory->DebugPlatformReadFile(fileName);
        if (readFileOutput.contents)
        {
            memory->DebugPlatformWriteFile("test.out", readFileOutput.fileSize,
                                           readFileOutput.contents);
            memory->DebugPlatformFreeFile(readFileOutput.contents);
        }
#endif
        gameState->tSine = 0.f;
        gameState->toneHz = 256;

        gameState->playerX = 100;
        gameState->playerY = 100;
        memory->isInitialized = true;
    }

    GameControllerInput *player1 = &(input->controllers[0]);

    gameState->toneHz = 256 + (player1->up.keyDown ? 128 : 0);

    gameState->yOffset += player1->up.keyDown ? -1 : 0;
    gameState->yOffset += player1->down.keyDown ? 1 : 0;
    gameState->xOffset += player1->left.keyDown ? -1 : 0;
    gameState->xOffset += player1->right.keyDown ? 1 : 0;

    gameState->playerY -= player1->up.keyDown ? 1 : 0;
    gameState->playerY += player1->down.keyDown ? 1 : 0;
    gameState->playerX -= player1->left.keyDown ? 1 : 0;
    gameState->playerX += player1->right.keyDown ? 1 : 0;

    // GameOutputSound(soundBuffer, gameState->toneHz);
    RenderGradient(offscreenBuffer, gameState->xOffset, gameState->yOffset);
    RenderPlayer(offscreenBuffer, gameState->playerX, gameState->playerY);
}
