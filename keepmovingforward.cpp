#include "keepmovingforward.h"
#include "keepmovingforward_intrinsic.h"
#include "keepmovingforward_tiles.cpp"
#include "keepmovingforward_types.h"
#include <math.h>

const float GRAVITY = 98.f;

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

void DrawRectangle(GameOffscreenBuffer *buffer, const float floatMinX, const float floatMinY, const float floatMaxX, const float floatMaxY,
                   const float r, const float g, const float b)
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

// NOTE: offset from start of struct
struct BmpHeader
{
    // File Header
    uint16 fileType;
    uint32 fileSize;
    uint16 reserved1;
    uint16 reserved2;
    uint32 offset;
    // BMP Info Header
    uint32 structSize;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bitCount;
    uint32 compression;
    uint32 imageSize;
    int32 xPixelsPerMeter;
    int32 yPixelsPerMeter;
    uint32 colororUsed;
    uint32 importantColors;
};

struct DebugBmpResult
{
    uint32 *pixels;
    int32 width;
    int32 height;
};

static DebugBmpResult DebugLoadBMP(DebugPlatformReadFileFunctionType *PlatformReadFile, const char *filename)
{
    DebugBmpResult result = {};
    DebugReadFileOutput output = PlatformReadFile(filename);
    if (output.fileSize != 0)
    {
        BmpHeader *header = (BmpHeader *)output.contents;
        uint32 *pixels = (uint32 *)((uint8 *)output.contents + header->offset);
        result.pixels = pixels;

        result.width = header->width;
        result.height = header->height;
    }
    return result;
}

// NOTE: this is dumb, but that's ok. x y are bottom left. in meters.
struct CollisionBox
{
    float x;
    float y;
    float width;
    float height;

    uint32 collisionLevel;
};

static bool CheckCollision(const CollisionBox box1, const CollisionBox box2)
{
    if (box1.x < box2.x + box2.width && box1.y < box2.y + box2.height && box2.x < box1.x + box1.width && box2.y < box1.y + box1.height)
    {
        return true;
    }
    return false;
}

static bool CheckCollision(const uint32 *tileMap, const int tileMapSideInPixels, const uint32 tileMapXCount, const uint32 tileMapYCount,
                           const float playerX, const float playerY)
{
    bool result = false;
    uint32 playerTileX = (uint32)((float)playerX / tileMapSideInPixels);
    uint32 playerTileY = (uint32)((float)playerY / tileMapSideInPixels);

    if (playerTileX >= 0 && playerTileY >= 0 && playerTileX < tileMapXCount && playerTileY < tileMapYCount)
    {
        if (tileMap[playerTileY * tileMapXCount + playerTileX] == 1)
        {
            result = true;
        }
    }

    return result;
}

static void InitializeArena(MemoryArena *arena, void *base, size_t size)
{
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define PushArray(arena, type, count) (type *)PushArena(arena, sizeof(type) * (count))
#define PushStruct(arena, type) (type *)PushArray(arena, type, 1)
static void *PushArena(MemoryArena *arena, size_t size)
{
    Assert(arena->used + size <= arena->size);
    void *result = (uint8 *)arena->base + arena->used;
    arena->used += size;
    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    int tileMapSideInPixels = 8;
    float tileMapSideInMeters = 1.f;
    float metersToPixels = tileMapSideInPixels / tileMapSideInMeters;

    if (!memory->isInitialized)
    {
        DebugBmpResult bmpTest = DebugLoadBMP(memory->DebugPlatformReadFile, "test/tree.bmp");
        gameState->playerX = 4.f;
        gameState->playerY = 4.f;
        gameState->playerWidth = tileMapSideInMeters;
        gameState->playerHeight = 2 * tileMapSideInMeters;

        InitializeArena(&gameState->worldArena, (uint8 *)memory->PersistentStorageMemory + sizeof(GameState),
                        memory->PersistentStorageSize - sizeof(GameState));

        gameState->level = PushStruct(&gameState->worldArena, Level);
        Level *level = gameState->level;
        level->tileMap = PushArray(&gameState->worldArena, uint32, TILE_MAP_X_COUNT * TILE_MAP_Y_COUNT);
        GenerateLevel(level->tileMap, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT);

        memory->isInitialized = true;
    }
    Level *level = gameState->level;
    GameControllerInput *player1 = &(input->controllers[0]);

    /* NOTE: this game is going to be like stick fight/duck game. 2d, multiplayer,
     no real camera work or anything like that. just a level with platforms.
     for now im going to represent this like tilemaps, but this will obviously change
     since there will be a notion of gravity, and you can't just walk wherever.

     just random ideas/thoughts:
     - notion of platform
     - resolution independent
     */

    int lowerLeftX = 0;
    int lowerLeftY = offscreenBuffer->height + tileMapSideInPixels / 2;
    // NOTE: tileMap X Y

    CollisionBox boxes[TILE_MAP_Y_COUNT * TILE_MAP_X_COUNT] = {};
    int index = 0;
    for (int y = 0; y < TILE_MAP_Y_COUNT; y++)
    {
        for (int x = 0; x < TILE_MAP_X_COUNT; x++)
        {
            if (CheckTileHasCollision(level->tileMap, x, y, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT))
            {
                CollisionBox box;
                box.x = x * tileMapSideInMeters;
                box.y = y * tileMapSideInMeters;
                box.width = tileMapSideInMeters;
                box.height = tileMapSideInMeters;
                if (GetTileValue(level->tileMap, x, y, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT) == 2)
                {
                    box.collisionLevel = 2;
                }
                else
                {
                    box.collisionLevel = 1;
                }
                boxes[index++] = box;
            }
        }
    }

    DrawRectangle(offscreenBuffer, 0.f, 0.f, 320, 180, 1.f, 1.f, 1.f);
    // NOTE: Y goes up, X goes to the right
    for (int row = 0; row < TILE_MAP_Y_COUNT; row++)
    {
        for (int col = 0; col < TILE_MAP_X_COUNT; col++)
        {
            float gray = 1.f;
            if (CheckTileHasCollision(level->tileMap, col, row, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT))
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

    gameState->dx = 0;
    gameState->dx += player1->right.keyDown ? 1.f : 0.f;
    gameState->dx += player1->left.keyDown ? -1.f : 0.f;
    gameState->dx *= 2;

    // JUMP!
    // TODO: finite state machine for movement: OnGround, AirBorn, etc.
    // TODO IMPORTANT: bounding box for collision xD
    if (player1->jump.keyDown && !gameState->isJumping)
    {
        gameState->dy += 20.f;
        gameState->isJumping = true;
    }
    if (gameState->isJumping)
    {
        gameState->dy -= GRAVITY * input->dT;
    }

    float expectedPlayerX = gameState->playerX + gameState->dx * input->dT;
    float expectedPlayerY = gameState->playerY + gameState->dy * input->dT;
    // float expectedPlayerX = gameState->playerX + metersToPixels * dx * input->dT;
    // float expectedPlayerY = gameState->playerY + metersToPixels * gameState->dy * input->dT;

    // TODO: actually somehow check if the player lands, this will mess up when collide with wall
    // midair
    /* NOTE: this is probably how I want basic physics to work initially
        1. Some notion of being airborne. Gravity applies here.
        2. Some notion of being on the ground/on a surface. Gravity doesn't apply.
        3. need to figure out how to differentiate collide with something in the air vs. landing

    SOLUTION: some tiles are ground tiles, some tiles are air
     */
    CollisionBox playerBox;
    playerBox.x = expectedPlayerX - gameState->playerWidth / 2;
    playerBox.y = expectedPlayerY;
    playerBox.width = gameState->playerWidth;
    playerBox.height = gameState->playerHeight;
    bool collided = false;
    for (int i = 0; i < ArrayLength(boxes); i++)
    {
        if (CheckCollision(playerBox, boxes[i]))
        {
            collided = true;
            if (boxes[i].collisionLevel == 2)
            {
                gameState->isJumping = false;
                gameState->dy = 0;
            }
            break;
        }
    }
    if (!collided)
    {
        gameState->playerX = expectedPlayerX;
        gameState->playerY = expectedPlayerY;
    }

    // NOTE: player's origin is bottom middle of sprite
    // TODO: decide on player origin location
    // TODO: bounding box for collisions

    float playerMinX = lowerLeftX + gameState->playerX * metersToPixels - gameState->playerWidth / 2 * metersToPixels;
    float playerMinY = lowerLeftY - gameState->playerY * metersToPixels;
    float playerMaxX = playerMinX + gameState->playerWidth * metersToPixels;
    float playerMaxY = playerMinY - gameState->playerHeight * metersToPixels;
    DrawRectangle(offscreenBuffer, playerMinX, playerMaxY, playerMaxX, playerMinY, 1.f, 0.f, 1.f);

    // GameOutputSound(soundBuffer, gameState->toneHz);
}
