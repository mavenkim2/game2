#include "keepmovingforward.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_tiles.cpp"
#include "keepmovingforward_types.h"

const float GRAVITY = 98.f;

#if 0
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
#endif

void DrawRectangle(GameOffscreenBuffer *buffer, const float floatMinX, const float floatMinY,
                   const float floatMaxX, const float floatMaxY, const float r, const float g,
                   const float b)
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

struct DrawData
{
    int lowerLeftX;
    int lowerLeftY;
    float metersToPixels;
};

// TODO: x and y located at bottom center, maybe change to center center?
void DrawMovable(GameOffscreenBuffer *buffer, DrawData data, const Entity *entity)
{
    float playerMinX =
        data.lowerLeftX + entity->pos.x * data.metersToPixels - entity->size.x / 2 * data.metersToPixels;
    float playerMinY = data.lowerLeftY - entity->pos.y * data.metersToPixels;
    float playerMaxX = playerMinX + entity->size.x * data.metersToPixels;
    float playerMaxY = playerMinY - entity->size.y * data.metersToPixels;
    DrawRectangle(buffer, playerMinX, playerMaxY, playerMaxX, playerMinY, entity->r, entity->g,
                  entity->b);
}

void DrawBitmap(GameOffscreenBuffer *buffer, DebugBmpResult *bmp, const float floatX, const float floatY)
{
    int minX = RoundFloatToInt32(floatX);
    int minY = RoundFloatToInt32(floatY);
    int maxX = RoundFloatToInt32(floatX + bmp->width);
    int maxY = RoundFloatToInt32(floatY + bmp->height);

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

    // NOTE: assuming first row in pixels is bottom of screen
    uint32 *sourceRow = (uint32 *)bmp->pixels + bmp->width * (bmp->height - 1);
    uint8 *destRow = ((uint8 *)buffer->memory + minY * buffer->pitch + minX * buffer->bytesPerPixel);
    for (int y = minY; y < maxY; y++)
    {
        uint32 *source = sourceRow;
        uint32 *dest = (uint32 *)destRow;
        for (int x = minX; x < maxX; x++)
        {
            *dest++ = *source++;
        }
        sourceRow -= bmp->width;
        destRow += buffer->pitch;
    }
}

internal DebugBmpResult DebugLoadBMP(DebugPlatformReadFileFunctionType *PlatformReadFile,
                                     const char *filename)
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

enum CollisionType
{
    GROUND,
    WALL,
    GROUNDWALL,
    TERMINATOR,
};

// TODO: struct to represent rect?
struct CollisionBox
{
    union
    {
        struct
        {
            v2 pos;
            v2 size;
        };
        struct
        {
            float x;
            float y;
            float width;
            float height;
        };
    };
    CollisionType type;
};

struct CollisionResponse
{
    bool collided;
    v2 normal;
};

internal void InitializeArena(MemoryArena *arena, void *base, size_t size)
{
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define PushArray(arena, type, count) (type *)PushArena(arena, sizeof(type) * (count))
#define PushStruct(arena, type) (type *)PushArray(arena, type, 1)
internal void *PushArena(MemoryArena *arena, size_t size)
{
    Assert(arena->used + size <= arena->size);
    void *result = (uint8 *)arena->base + arena->used;
    arena->used += size;
    return result;
}

inline Entity *GetPlayer(GameState *gameState)
{
    Entity *player = &gameState->entities[gameState->playerIndex];
    return player;
}

// TODO: IDs, remove entity
inline int CreateEntity(GameState *gameState)
{
    int entityIndex = gameState->entityCount++;
    Entity *entity = &gameState->entities[entityIndex];
    // TODO: no 0 initialization?
    *entity = {};

    return entityIndex;
}

struct Polygon
{
    // v2 *points;
    // TODO: actually support all polygons
    v2 points[4];
    int numPoints;
};

// TODO: probably needs to store more info to actually get the distance/closest point
struct Simplex
{
    v2 vertexA;
    v2 vertexB;
    v2 vertexC;

    int count;
};

// TODO: define different support function for different shapes (e.g circle)
static int GetSupport(const Polygon *polygon, const v2 d)
{
    int index = 0;
    float bestValue = Dot(polygon->points[0], d);
    for (int i = 1; i < polygon->numPoints; i++)
    {
        float tempValue = Dot(polygon->points[i], d);
        if (tempValue > bestValue)
        {
            index = i;
            bestValue = tempValue;
        }
    }
    return index;
}

internal bool lineCase(Simplex *simplex, v2 *d);
internal bool triangleCase(Simplex *simplex, v2 *d);
// NOTE: implement this once you use something other than boxes for collision, not before
static bool GJKMainLoop(Polygon *p1, Polygon *p2)
{

    // * d = ??
    // * S = Support( ? ), Support(polygon1, d) - Support(polygon2, -d)
    // * Points += S
    // * d = -S (points to origin)
    // * while
    // *      S = GetSupport(d)
    // *      if (S * d) < 0 (point is opposite direction of d, meaning you can't cross origin)
    // *          game over
    // *      Points += S
    // *      if HandleSimplex(Points, d)
    //              collision detected
    // *
    v2 d = Normalize(p2->points[0] - p1->points[0]);
    Simplex simplex = {};

    simplex.vertexA = p2->points[GetSupport(p2, d)] - p1->points[GetSupport(p1, -d)];
    d = -simplex.vertexA;
    simplex.count = 1;

    while (1)
    {
        if (Dot(d, d) == 0)
        {
            return false;
        }

        simplex.vertexC = simplex.vertexB; 
        simplex.vertexB = simplex.vertexA;

        v2 a = p2->points[GetSupport(p2, d)] - p1->points[GetSupport(p1, -d)];
        // Didn't cross origin
        if (Dot(a, d) < 0)
        {
            return false;
        }
        simplex.vertexA = a;
        simplex.count += 1;

        bool result = false;
        switch (simplex.count)
        {
        case 1:
            break;
        case 2:
            result = lineCase(&simplex, &d);
            break;
        case 3:
            result = triangleCase(&simplex, &d);
            break;
        default:
            Assert(false);
        }
        if (result)
        {
            return true;
        }
    }
}

internal bool lineCase(Simplex *simplex, v2 *d)
{
    v2 a = simplex->vertexA;
    v2 b = simplex->vertexB;
    // Don't need to check voronoi regions, since A is the most recently added point,
    // it is past origin with respect to B.
    v2 ab = b - a;
    v2 ao = -a;

    v2 abperp = Cross(Cross(ab, ao), ab);
    // case where it's on line
    if (Dot(abperp, ao) == 0)
    {
        return true;
    }

    *d = abperp;
    return false;
}

internal bool triangleCase(Simplex *simplex, v2 *d)
{
    v2 a = simplex->vertexA;
    v2 b = simplex->vertexB;
    v2 c = simplex->vertexC;

    v2 ab = b - a;
    v2 ac = c - a;
    v2 ao = -a;

    v2 abperp = Cross(Cross(ac, ab), ab);
    v2 acperp = Cross(Cross(ab, ac), ac);

    if (Dot(abperp, ao) > 0)
    {
        simplex->vertexC = {};
        simplex->count -= 1;
        *d = abperp;
        return false;
    }
    else if (Dot(acperp, ao) > 0)
    {
        simplex->vertexB = simplex->vertexC;
        simplex->vertexC = {};
        simplex->count -= 1;
        *d = acperp;
        return false;
    }
    return true;
}

internal bool TestWallCollision(float wallLocation, float playerRelWall, float playerRelWallParallel,
                                float playerDelta, float *tMin, float wallStart, float wallEnd)
{
    bool hit = false;
    float epsilon = 0.00001f;
    float t = (wallLocation - playerRelWall) / playerDelta;
    if (t >= 0 && t < *tMin)
    {
        float y = playerRelWallParallel + t * playerDelta;
        if (y >= wallStart && y <= wallEnd)
        {
            hit = true;
            *tMin = Max(0.f, t - epsilon);
        }
    }
    return hit;
}

struct CollisionResult
{
    v2 normal;
    bool hit;
};

internal CollisionResult TestMovingEntityCollision(const CollisionBox *dynamic,
                                                   const CollisionBox *fixed, v2 delta, float *tResult)

{
    CollisionResult result = {};

    float diameterWidth = dynamic->width + fixed->width;
    float diameterHeight = dynamic->height + fixed->height;

    v2 minCorner = -0.5f * v2{diameterWidth, diameterHeight};
    v2 maxCorner = 0.5f * v2{diameterWidth, diameterHeight};

    // subtract centers of boxes
    v2 relative = (dynamic->pos + v2{dynamic->width / 2.f, dynamic->height / 2.f}) -
                  (fixed->pos + v2{fixed->width / 2.f, fixed->height / 2.f});

    v2 normal = {};
    if (delta.x != 0)
    {
        if (delta.x > 0)
        {
            if (TestWallCollision(minCorner.x, relative.x, relative.y, delta.x, tResult, minCorner.y,
                                  maxCorner.y))
            {
                normal = v2{-1, 0};
            }
        }
        if (delta.x < 0)
        {
            if (TestWallCollision(maxCorner.x, relative.x, relative.y, delta.x, tResult, minCorner.y,
                                  maxCorner.y))
            {

                normal = v2{1, 0};
            }
        }
    }
    if (delta.y != 0 && fixed->type != WALL)
    {
        if (delta.y > 0)
        {
            if (TestWallCollision(minCorner.y, relative.y, relative.x, delta.y, tResult, minCorner.x,
                                  maxCorner.x))
            {

                normal = v2{0, -1};
            }
        }

        if (delta.y < 0)
        {
            if (TestWallCollision(maxCorner.y, relative.y, relative.x, delta.y, tResult, minCorner.x,
                                  maxCorner.x))
            {

                normal = v2{0, 1};
            }
        }
    }
    if (normal.x != 0 || normal.y != 0)
    {
        result.hit = true;
    }
    result.normal = normal;
    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // Initialization
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    int tileMapSideInPixels = 8;
    float tileMapSideInMeters = 1.f;
    float metersToPixels = tileMapSideInPixels / tileMapSideInMeters;
    int lowerLeftX = 0;
    int lowerLeftY = offscreenBuffer->height + tileMapSideInPixels / 2;

    if (!memory->isInitialized)
    {
        gameState->bmpTest = DebugLoadBMP(memory->DebugPlatformReadFile, "test/tile.bmp");

        CreateEntity(gameState);

        // player init

        int playerIndex = gameState->entityCount;
        Entity *player = GetPlayer(gameState);
        player->airborne = true;
        player->pos.x = 4.f;
        player->pos.y = 4.f;
        player->size.x = tileMapSideInMeters;
        player->size.y = 2 * tileMapSideInMeters;
        player->r = 1.f;
        player->g = 0.f;
        player->b = 1.f;

        gameState->swapIndex = CreateEntity(gameState);
        Entity *swap = &gameState->entities[gameState->swapIndex];
        *swap = {};
        swap->pos.x = 8.f;
        swap->pos.y = 8.f;
        swap->size.x = tileMapSideInMeters;
        swap->size.y = tileMapSideInMeters;
        swap->r = 0.f;
        swap->g = 1.f;
        swap->b = 1.f;

        InitializeArena(&gameState->worldArena,
                        (uint8 *)memory->PersistentStorageMemory + sizeof(GameState),
                        memory->PersistentStorageSize - sizeof(GameState));

        gameState->level = PushStruct(&gameState->worldArena, Level);
        Level *level = gameState->level;
        level->tileMap = PushArray(&gameState->worldArena, uint32, TILE_MAP_X_COUNT * TILE_MAP_Y_COUNT);
        GenerateLevel(level->tileMap, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT);

        memory->isInitialized = true;
    }
    Level *level = gameState->level;
    GameControllerInput *player1 = &(input->controllers[0]);

    // Collision

    CollisionBox boxes[TILE_MAP_Y_COUNT * TILE_MAP_X_COUNT] = {};
    int index = 0;
    for (int y = 0; y < TILE_MAP_Y_COUNT; y++)
    {
        for (int x = 0; x < TILE_MAP_X_COUNT; x++)
        {
            if (CheckTileIsSolid(level->tileMap, x, y, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT))
            {
                CollisionBox box;
                box.x = x * tileMapSideInMeters;
                box.y = y * tileMapSideInMeters;
                box.width = tileMapSideInMeters;
                box.height = tileMapSideInMeters;
                if (GetTileValue(level->tileMap, x, y, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT) == 1)
                {
                    box.type = WALL;
                }
                else
                {
                    box.type = GROUND;
                }
                boxes[index++] = box;
            }
        }
    }

    // Physics
    Entity *player = GetPlayer(gameState);
    Entity *swap = &gameState->entities[gameState->swapIndex];

    // Swap Position
    if (player1->swap.keyDown && player1->swap.halfTransitionCount >= 1)
    {
        v2 temp = player->pos;
        player->pos = swap->pos;
        swap->pos = temp;
    }

    // Swap Soul
    if (player1->soul.keyDown && player1->soul.halfTransitionCount >= 1)
    {
        Entity temp = *player;
        *player = *swap;
        *swap = temp;

        swap->airborne = true;
    }

    v2 *dPlayerXY = &player->velocity;
    v2 *ddPlayerXY = &player->acceleration;

    dPlayerXY->x = 0;
    dPlayerXY->x += player1->right.keyDown ? 1.f : 0.f;
    dPlayerXY->x += player1->left.keyDown ? -1.f : 0.f;
    float multiplier = 5;
    if (player1->shift.keyDown)
    {
        multiplier = 7;
        // gameState->ddPlayerXY +=
    }
    dPlayerXY->x *= multiplier;

    if (player1->jump.keyDown && !player->airborne)
    {
        dPlayerXY->y += 20.f;
        player->airborne = true;
    }

    if (player->airborne)
    {
        *ddPlayerXY = v2{0, -GRAVITY};
    }

    else
    {
        *ddPlayerXY = v2{0, 0};
    }
    v2 playerDelta = 0.5f * input->dT * input->dT * *ddPlayerXY + *dPlayerXY * input->dT;
    *dPlayerXY += *ddPlayerXY * input->dT;

    // v2 expectedPlayerPos = gameState->player.pos + gameState->dPlayerXY * input->dT;

    // TODO: actually somehow check if the player lands, this will
    // mess up when collide with wall midair
    /* NOTE: this is probably how I want basic physics to work
    initially
        1. Some notion of being airborne. Gravity applies here.
        2. Some notion of being on the ground/on a surface. Gravity
    doesn't apply.
        3. need to figure out how to differentiate collide with
    something in the air vs. landing

    SOLUTION: some tiles are ground tiles, some tiles are air
     */

    // Collision Detetion
    // TODO: handle the case where you're sweeping with two moving AABBs
    // TODO: represent everything from the center?
    // TODO: weird bug where I fell into the floor after swapping??? lmfao? 
    //        I think it's because I switched positions right as I'm about to touch floor, 
    //        or maybe when my position was just past the floor
    //        this can be fixed by treating the ground as a solid instead of a line
    {
        CollisionBox playerBox;
        playerBox.x = player->pos.x - player->size.x / 2;
        playerBox.y = player->pos.y;
        playerBox.size = player->size;

        CollisionBox swapBox;
        swapBox.x = swap->pos.x - swap->size.x / 2;
        swapBox.y = swap->pos.y;
        swapBox.size = swap->size;

        Polygon p1;
        p1.points[0] = playerBox.pos;
        p1.points[1] = playerBox.pos + v2{playerBox.width, 0};
        p1.points[2] = playerBox.pos + v2{playerBox.width, playerBox.height};
        p1.points[3] = playerBox.pos + v2{0, playerBox.height};
        p1.numPoints = 4;

        Polygon p2;
        p2.points[0] = swapBox.pos;
        p2.points[1] = swapBox.pos + v2{swapBox.width, 0};
        p2.points[2] = swapBox.pos + v2{swapBox.width, swapBox.height};
        p2.points[3] = swapBox.pos + v2{0, swapBox.height};
        p2.numPoints = 4;

        if (GJKMainLoop(&p1, &p2))
        {
            player->r = 0.f;
            player->g = 0.f;
            player->b = 0.f;
        }
        else
        {
            player->r = 1.f;
            player->g = 0.f;
            player->b = 1.f;
        }

        float tRemaining = 1.f;
        int iterationCount = 3;

        for (int iteration = 0; tRemaining > 0.f && iteration < iterationCount; iteration++)
        {
            v2 normal = {};
            float tResult = 1.f;
            // TODO: should not be checking every box every frame twice
            bool hit = false;
            for (int i = 0; i < ArrayLength(boxes); i++)
            {
                // NOTE ALGORITHM:
                // 1. Find minkowski sum of two AABBs
                // 2. Find relative positive of player with respect to the currently checked box.
                // 3. Four cases:
                //      a. If the player is moving left, check that the horizontal length between the
                //      player and the right side of the box is less playerDelta.x. If so, check to see
                //      if the player's y coordinate is within the box. If so, there is a collision. b,
                //      c, d. Similar for the other sides of the box.
                // 4. For the smallest time to collision, simply just place the player at that location.
                // May want to revise this.

                // Ray vs Minkowski Sum AABB

                CollisionResult result =
                    TestMovingEntityCollision(&playerBox, &boxes[i], playerDelta, &tResult);
                if (result.hit)
                {
                    normal = result.normal;
                    if (result.normal.x == 0 && result.normal.y == 1)
                    {

                        player->airborne = false;
                    }
                }
            }

            player->pos += playerDelta * tResult;
            *dPlayerXY -= normal * Dot(*dPlayerXY, normal);
            playerDelta = playerDelta - normal * Dot(playerDelta, normal);
            tRemaining -= tResult;
        }
    }

    // Draw
    {
        DrawRectangle(offscreenBuffer, 0.f, 0.f, 320, 180, 1.f, 1.f, 1.f);
        for (int row = 0; row < TILE_MAP_Y_COUNT; row++)
        {
            for (int col = 0; col < TILE_MAP_X_COUNT; col++)
            {
                float gray = 1.f;
                float minX = lowerLeftX + (float)col * tileMapSideInPixels;
                float minY = lowerLeftY - (float)row * tileMapSideInPixels;
                float maxX = minX + tileMapSideInPixels;
                float maxY = minY - tileMapSideInPixels;
                if (CheckTileIsSolid(level->tileMap, col, row, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT))
                {
                    DrawBitmap(offscreenBuffer, &gameState->bmpTest, minX, maxY);
                }
                else
                {
                    DrawRectangle(offscreenBuffer, minX, maxY, maxX, minY, gray, gray, gray);
                }
            }
        }

        DrawData data;
        data.lowerLeftX = lowerLeftX;
        data.lowerLeftY = lowerLeftY;
        data.metersToPixels = metersToPixels;

        DrawMovable(offscreenBuffer, data, swap);
        DrawMovable(offscreenBuffer, data, player);

        // GameOutputSound(soundBuffer, gameState->toneHz);
    }
}
