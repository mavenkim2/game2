#include "keepmovingforward.h"
#include "keepmovingforward_intrinsic.h"
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

    int tileMapSideInPixels = 60;
    float tileMapSideInMeters = 1.4f;
    float metersToPixels = tileMapSideInPixels / tileMapSideInMeters;

    if (!memory->isInitialized)
    {
        memory->isInitialized = true;

        gameState->playerX = 10.f;
        gameState->playerY = 10.f;
        gameState->playerWidth = tileMapSideInMeters;
        gameState->playerHeight = tileMapSideInMeters;
    }
    GameControllerInput *player1 = &(input->controllers[0]);

    /* NOTE: this game is going to be like stick fight/duck game. 2d, multiplayer,
     no real camera work or anything like that. just a level with platforms.
     for now im going to represent this like tilemaps, but this will obviously change
     since there will be a notion of gravity, and you can't just walk wherever.

     just random ideas/thoughts:
     - notion of platform
     - resolution independent
     */
#define TILE_MAP_X_COUNT 17
#define TILE_MAP_Y_COUNT 9

    int lowerLeftX = -tileMapSideInPixels / 2;
    int lowerLeftY = offscreenBuffer->height;
    // NOTE: tileMap X Y
    uint32 tileMap00[TILE_MAP_Y_COUNT][TILE_MAP_X_COUNT]{
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
        {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    };
    uint32 tileMap10[TILE_MAP_Y_COUNT][TILE_MAP_X_COUNT]{
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    };
    uint32 tileMap01[TILE_MAP_Y_COUNT][TILE_MAP_X_COUNT]{
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };
    uint32 tileMap11[TILE_MAP_Y_COUNT][TILE_MAP_X_COUNT]{
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    DrawRectangle(offscreenBuffer, 0.f, 0.f, 960, 540, 1.f, 1.f, 1.f);
    // NOTE: Y goes up, X goes to the right
    for (int row = 0; row < TILE_MAP_Y_COUNT; row++)
    {
        for (int col = 0; col < TILE_MAP_X_COUNT; col++)
        {
            float gray = 1.f;
            if (tileMap00[row][col] == 1)
            {
                gray = 0.5f;
            }
            float minX = lowerLeftX + (float)col * tileMapSideInPixels;
            float minY = lowerLeftY - (float)row * tileMapSideInPixels;
            float maxX = minX + tileMapSideInPixels;
            float maxY = minY - tileMapSideInPixels;
            DrawRectangle(offscreenBuffer, minX, maxY, maxX, minY, gray, gray, gray);
        }
    }

    float dx = 0.f;
    float dy = 0.f;
    dx += player1->right.keyDown ? 1.f : 0.f;
    dx += player1->left.keyDown ? -1.f : 0.f;
    dy += player1->up.keyDown ? 1.f : 0.f;
    dy += player1->down.keyDown ? -1.f : 0.f;
    dx *= 3;
    dy *= 3;

    gameState->playerX += metersToPixels * dx * input->dT;
    gameState->playerY += metersToPixels * dy * input->dT;

    // NOTE: player's origin is bottom middle of sprite
    float playerMinX =
        lowerLeftX + gameState->playerX - gameState->playerWidth / 2;
    float playerMinY = lowerLeftY - gameState->playerY;
    float playerMaxX = playerMinX + gameState->playerWidth * metersToPixels;
    float playerMaxY = playerMinY - gameState->playerHeight * metersToPixels;
    DrawRectangle(offscreenBuffer, playerMinX, playerMaxY, playerMaxX, playerMinY, 1.f, 0.f, 1.f);

    // GameOutputSound(soundBuffer, gameState->toneHz);
}
