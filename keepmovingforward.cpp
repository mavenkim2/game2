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

#if 0
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
#endif

inline uint32 RoundFloatToUint32(float value) { return (uint32)(value + 0.5f); }
inline uint32 RoundFloatToInt32(float value) { return (int32)(value + 0.5f); }

void DrawRectangle(GameOffscreenBuffer *buffer, float floatMinX, float floatMinY, float floatMaxX,
                   float floatMaxY, float r, float g, float b)
{
    int minX = RoundFloatToInt32(floatMinX);
    int minY = RoundFloatToInt32(floatMinY);
    int maxX = RoundFloatToInt32(floatMaxX);
    int maxY = RoundFloatToInt32(floatMaxY);

    if (minX < 0)
    {
        minX = 0;
    }
    if (minY < 0)
    {
        minY = 0;
    }
    if (maxX > buffer->width)
    {
        maxX = buffer->width;
    }
    if (maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    uint32 red = RoundFloatToUint32(r * 255.f);
    uint32 blue = RoundFloatToUint32(b * 255.f);
    uint32 green = RoundFloatToUint32(g * 255.f);

    uint32 color = blue | green << 8 | red << 16;

    uint8 *row = (uint8 *)buffer->memory + minY * buffer->pitch + minX * buffer->bytesPerPixel;
    for (int y = minY; y < maxY; y++)
    {
        uint32 *pixel = (uint32 *)row;
        for (int x = minX; x < maxX; x++)
        {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    if (!memory->isInitialized)
    {
        memory->isInitialized = true;
    }

    // GameControllerInput *player1 = &(input->controllers[0]);

    // GameOutputSound(soundBuffer, gameState->toneHz);
    DrawRectangle(offscreenBuffer, 0.f, 0.f, 960, 540, 1.f, 1.f, 1.f);
}
