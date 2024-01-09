#include "keepmovingforward.h"
#include "keepmovingforward_entity.cpp"
#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.cpp"

const float GRAVITY = 49.f;
// TODO: simulate actual drag???
const float MAX_Y_SPEED = 50.f;

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

void RenderGradient(Gamebuffer *buffer, int xOffset, int yOffset)
{
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++)
    {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; x++)
        {
            // BB GG RR XX, because Windows!
            // Blue is first
            // Memory: BB GG RR XX
            // Register: XX RR GG BB
            u8 blue = (u8)(x + xOffset);
            u8 green = (u8)(y + yOffset);
            *pixel = blue | green << 8;
            pixel++;
        }
        row += buffer->pitch;
    }
}
#endif

void DrawRectangle(GameOffscreenBuffer *buffer, const V2 min, const V2 max, const float r, const float g, const float b)
{
    int minX = RoundF32ToI32(min.x);
    int minY = RoundF32ToI32(min.y);
    int maxX = RoundF32ToI32(max.x);
    int maxY = RoundF32ToI32(max.y);

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
    u32 red = RoundF32ToU32(r * 255.f);
    u32 blue = RoundF32ToU32(b * 255.f);
    u32 green = RoundF32ToU32(g * 255.f);

    u32 color = blue | green << 8 | red << 16;

    u8 *row = (u8 *)buffer->memory + minY * buffer->pitch + minX * buffer->bytesPerPixel;
    for (int y = minY; y < maxY; y++)
    {
        u32 *pixel = (u32 *)row;
        for (int x = minX; x < maxX; x++)
        {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

void DrawBitmap(GameOffscreenBuffer *buffer, DebugBmpResult *bmp, const V2 min)
{
    int minX = RoundF32ToI32(min.x);
    int minY = RoundF32ToI32(min.y);
    int maxX = minX + bmp->width;
    int maxY = minY + bmp->height;

    int alignX = 0;
    int alignY = 0;

    if (minX < 0)
    {
        alignX = -minX;
        minX = 0;
    }
    if (minY < 0)
    {
        alignY = -minY;
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
    // u32 *sourceRow = (u32 *)bmp->pixels + bmp->width * (bmp->height - 1);
    u32 *sourceRow = (u32 *)bmp->pixels + alignY * bmp->width + alignX;
    u8 *destRow = ((u8 *)buffer->memory + minY * buffer->pitch + minX * buffer->bytesPerPixel);
    for (int y = minY; y < maxY; y++)
    {
        u32 *source = sourceRow;
        u32 *dest = (u32 *)destRow;
        for (int x = minX; x < maxX; x++)
        {
            *dest++ = *source++;
        }
        sourceRow += bmp->width;
        destRow += buffer->pitch;
    }
}

internal DebugBmpResult DebugLoadBMP(DebugPlatformReadFileFunctionType *PlatformReadFile, const char *filename)
{
    DebugBmpResult result = {};
    DebugReadFileOutput output = PlatformReadFile(filename);
    if (output.fileSize != 0)
    {
        BmpHeader *header = (BmpHeader *)output.contents;
        u32 *pixels = (u32 *)((u8 *)output.contents + header->offset);
        result.pixels = pixels;

        result.width = header->width;
        result.height = header->height;
    }
    return result;
}

inline Entity *GetEntity(Level *level, int handle)
{
    Assert(handle < MAX_ENTITIES);

    Entity *entity = level->entities + handle;
    return entity;
}

inline Entity *GetPlayer(GameState *gameState)
{
    Entity *player = &gameState->player;
    return player;
}

inline Entity *CreateEntity(GameState *gameState, Level *level)
{
    Entity *entity = 0;
    for (int i = 1; i < MAX_ENTITIES; i++)
    {
        if (!HasFlag(level->entities + i, Entity_Valid))
        {
            entity = level->entities + i;
            break;
        }
    }
    // TODO: no 0 initialization?
    // *entity = {};
    entity->id = gameState->entity_id_gen++;

    return entity;
}

inline Entity *CreateWall(GameState *gameState, Level *level)
{
    Entity *wall = CreateEntity(gameState, level);

    AddFlag(2, wall, Entity_Valid, Entity_Collidable);
    return wall;
}

// NOTE: this assumes that the entity arg is in the level arg
inline void RemoveEntity(Level *level, Entity *entity) { RemoveFlag(entity, Entity_Valid); }

internal void GenerateLevel(GameState *gameState, u32 tileWidth, u32 tileHeight)
{
    Level *level = gameState->level;
    level->levelWidth = tileWidth;
    level->levelHeight = tileHeight;

    for (u32 y = 0; y < tileHeight; y++)
    {
        for (u32 x = 0; x < tileWidth; x++)
        {
            if (x == 0 || y == 0 || x == tileWidth - 1)
            {
                Entity *entity = CreateWall(gameState, gameState->level);

                entity->pos = V2{(float)x, (float)y};
                entity->size = V2{TILE_METER_SIZE, TILE_METER_SIZE};
            }
        }
    }
}

struct Polygon
{
    // V2 *points;
    // TODO: actually support all polygons
    V2 points[4];
    int numPoints;
};

// TODO: probably needs to store more info to actually get the distance/closest point
struct Simplex
{
    V2 vertexA;
    V2 vertexB;
    V2 vertexC;

    V2 vertexD;

    int count;
};

// TODO: define different support function for different shapes (e.g circle)
static int GetSupport(const Polygon *polygon, const V2 d)
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

internal bool lineCase(Simplex *simplex, V2 *d);
internal bool triangleCase(Simplex *simplex, V2 *d);
// NOTE: implement this once you use something other than boxes for collision, not before

struct GJKResult
{
    bool hit;
    Simplex simplex;
};

static GJKResult GJKMainLoop(Polygon *p1, Polygon *p2)
{
    GJKResult result = {};

    V2 d = Normalize(p2->points[0] - p1->points[0]);
    Simplex simplex = {};

    simplex.vertexA = p2->points[GetSupport(p2, d)] - p1->points[GetSupport(p1, -d)];
    d = -simplex.vertexA;
    simplex.count = 1;

    while (1)
    {
        if (Dot(d, d) == 0)
        {
            return result;
        }

        simplex.vertexC = simplex.vertexB;
        simplex.vertexB = simplex.vertexA;

        V2 a = p2->points[GetSupport(p2, d)] - p1->points[GetSupport(p1, -d)];
        // Didn't cross origin
        if (Dot(a, d) < 0)
        {
            return result;
        }
        simplex.vertexA = a;
        simplex.count += 1;

        bool hit = false;
        switch (simplex.count)
        {
        case 1:
            break;
        case 2:
            hit = lineCase(&simplex, &d);
            break;
        case 3:
            hit = triangleCase(&simplex, &d);
            break;
        default:
            Assert(false);
        }
        if (hit)
        {
            result.hit = true;
            result.simplex = simplex;
            return result;
        }
    }
}

internal bool lineCase(Simplex *simplex, V2 *d)
{
    V2 a = simplex->vertexA;
    V2 b = simplex->vertexB;
    // Don't need to check voronoi regions, since A is the most recently added point,
    // it is past origin with respect to B.
    V2 ab = b - a;
    V2 ao = -a;

    V2 abperp = Cross(Cross(ab, ao), ab);
    // case where it's on line
    if (Dot(abperp, ao) == 0)
    {
        return true;
    }

    *d = abperp;
    return false;
}

internal bool triangleCase(Simplex *simplex, V2 *d)
{
    V2 a = simplex->vertexA;
    V2 b = simplex->vertexB;
    V2 c = simplex->vertexC;

    V2 ab = b - a;
    V2 ac = c - a;
    V2 ao = -a;

    V2 abperp = Cross(Cross(ac, ab), ab);
    V2 acperp = Cross(Cross(ab, ac), ac);

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

internal bool TestWallCollision(float wallLocation, float playerRelWall, float playerRelWallParallel, float playerDelta,
                                float *tMin, float wallStart, float wallEnd)
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

// TODO: need to handle case with two moving objects? we'll see
internal bool BroadPhaseCollision(const Rect2 dynamic, const Rect2 fixed, V2 delta)
{
    Rect2 broadPhase = {};
    if (delta.x > 0)
    {
        broadPhase.x = dynamic.x;
        broadPhase.width = dynamic.width + delta.x;
    }
    else
    {
        broadPhase.x = dynamic.x + delta.x;
        broadPhase.width = dynamic.width - delta.x;
    }
    if (delta.y > 0)
    {
        broadPhase.y = dynamic.y;
        broadPhase.height = dynamic.height + delta.y;
    }
    else
    {
        broadPhase.y = dynamic.y + delta.y;
        broadPhase.height = dynamic.height - delta.y;
    }
    return Rect2Overlap(broadPhase, fixed);
}

struct Manifold
{
    V2 normal;
    float penetration;
};
internal Manifold NarrowPhaseAABBCollision(const Rect2 a, const Rect2 b)
{
    Manifold manifold = {};
    V2 relative = GetRectCenter(a) - GetRectCenter(b);
    float aHalfExtent = a.width / 2;
    float bHalfExtent = b.width / 2;
    float xOverlap = aHalfExtent + bHalfExtent - Abs(relative.x);

    if (xOverlap > 0)
    {
        aHalfExtent = a.height / 2;
        bHalfExtent = b.height / 2;
        float yOverlap = aHalfExtent + bHalfExtent - Abs(relative.y);
        if (yOverlap > 0)
        {
            if (xOverlap < yOverlap)
            {
                if (relative.x < 0)
                {
                    manifold.normal = {-1, 0};
                }
                else
                {
                    manifold.normal = {1, 0};
                }
                manifold.penetration = xOverlap;
            }
            else
            {
                if (relative.y < 0)
                {
                    manifold.normal = {0, -1};
                }
                else
                {
                    manifold.normal = {0, 1};
                }
                manifold.penetration = yOverlap;
            }
        }
    }
    return manifold;
}

internal V2 TestMovingEntityCollision(const Rect2 *dynamic, const Rect2 *fixed, V2 delta, float *tResult)
{
    V2 normal = {};

    if (!BroadPhaseCollision(*dynamic, *fixed, delta))
    {
        return normal;
    }

    float tMin = *tResult;

    float diameterWidth = dynamic->width + fixed->width;
    float diameterHeight = dynamic->height + fixed->height;

    V2 minCorner = -0.5f * V2{diameterWidth, diameterHeight};
    V2 maxCorner = 0.5f * V2{diameterWidth, diameterHeight};

    // subtract centers of boxes
    V2 relative = {(dynamic->pos + 0.5 * dynamic->size) - (fixed->pos + 0.5 * fixed->size)};

    if (delta.y > 0)
    {
        if (TestWallCollision(minCorner.y, relative.y, relative.x, delta.y, tResult, minCorner.x, maxCorner.x))
        {

            normal = V2{0, -1};
        }
    }

    if (delta.y < 0)
    {
        if (TestWallCollision(maxCorner.y, relative.y, relative.x, delta.y, tResult, minCorner.x, maxCorner.x))
        {

            normal = V2{0, 1};
        }
    }

    if (delta.x > 0)
    {
        if (TestWallCollision(minCorner.x, relative.x, relative.y, delta.x, tResult, minCorner.y, maxCorner.y))
        {
            normal = V2{-1, 0};
        }
    }
    if (delta.x < 0)
    {
        if (TestWallCollision(maxCorner.x, relative.x, relative.y, delta.x, tResult, minCorner.y, maxCorner.y))
        {
            normal = V2{1, 0};
        }
    }

    return normal;
}

// TODO: return (0, 0) instead of bifurcating code path?
internal bool ScaleMousePosition(V2 mousePos, V2 resolutionScale, u32 bufferWidth, u32 bufferHeight, V2 *mouseTilePos)
{
    if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < resolutionScale.x && mousePos.y < resolutionScale.y)
    {
        *mouseTilePos = mousePos;
        mouseTilePos->y = resolutionScale.y - mouseTilePos->y;

        resolutionScale.x /= bufferWidth;
        resolutionScale.y /= bufferHeight;
        mouseTilePos->x /= resolutionScale.x;
        mouseTilePos->y /= resolutionScale.y;

        // *mouseTilePos /= (float)TILE_PIXEL_SIZE;

        return true;
    }
    return false;
}

internal void InitializePlayer(GameState *gameState)
{
    Entity *player = &gameState->player;

    player->pos.x = 6.f;
    player->pos.y = 6.f;
    player->size.x = TILE_METER_SIZE;
    player->size.y = 2 * TILE_METER_SIZE;
    player->r = 1.f;
    player->g = 0.f;
    player->b = 1.f;

    AddFlag(3, player, Entity_Valid, Entity_Collidable, Entity_Airborne, Entity_Swappable);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // Initialization
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    float screenCenterX = buffer->width * .5f;
    float screenCenterY = buffer->height * .5f;

    if (!memory->isInitialized)
    {
        gameState->bmpTest = DebugLoadBMP(memory->DebugPlatformReadFile, "test/tile.bmp");
        gameState->worldArena = ArenaAlloc((void *)((u8 *)(memory->PersistentStorageMemory) + sizeof(GameState)),
                                           memory->PersistentStorageSize - sizeof(GameState));

        gameState->level = PushStruct(gameState->worldArena, Level);

        Entity *nilEntity = CreateEntity(gameState, gameState->level);

        GenerateLevel(gameState, TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT * 2);

        InitializePlayer(gameState);

        Entity *swap = CreateEntity(gameState, gameState->level);
        *swap = {};
        swap->pos.x = 8.f;
        swap->pos.y = 8.f;
        swap->size.x = TILE_METER_SIZE;
        swap->size.y = TILE_METER_SIZE;
        swap->r = 0.f;
        swap->g = 1.f;
        swap->b = 1.f;
        AddFlag(3, swap, Entity_Valid, Entity_Collidable, Entity_Swappable);

        memory->isInitialized = true;

        gameState->camera.pos = V2{TILE_MAP_X_COUNT / 2.f, TILE_MAP_Y_COUNT / 2.f};
    }
    Level *level = gameState->level;
    GameInput *playerController = input;

    // Physics
    Entity *player = GetPlayer(gameState);
    Entity *swap = {};

    // TODO: swap w/ shortest distance
    for (int i = 0; i < MAX_ENTITIES; i++)
    {
        if (HasFlag(level->entities + i, Entity_Swappable))
        {
            swap = level->entities + i;
            break;
        }
    }

    // Swap Position
    if (playerController->swap.keyDown && playerController->swap.halfTransitionCount >= 1)
    {
        if (swap && IsValid(swap))
        {
            Swap(V2, player->pos, swap->pos);
        }
    }

    // Swap Soul
    if (playerController->soul.keyDown && playerController->soul.halfTransitionCount >= 1)
    {
        u64 swapFlags = swap->flags;
        u64 playerFlags = player->flags;

        Swap(Entity, *player, *swap);
        player->flags |= playerFlags;
        swap->flags = swapFlags;
    }

    V2 *dPlayerXY = &player->velocity;
    V2 *ddPlayerXY = &player->acceleration;

    dPlayerXY->x = 0;
    dPlayerXY->x += playerController->right.keyDown ? 1.f : 0.f;
    dPlayerXY->x += playerController->left.keyDown ? -1.f : 0.f;
    float multiplier = 5;
    if (playerController->shift.keyDown)
    {
        multiplier = 7;
        // gameState->ddPlayerXY +=
    }
    dPlayerXY->x *= multiplier;

    *ddPlayerXY = V2{0, -GRAVITY};
    if (playerController->jump.keyDown && !HasFlag(player, Entity_Airborne))
    {
        dPlayerXY->y += 50.f;
        AddFlag(1, player, Entity_Airborne);
    }

    // *dPlayerXY += *ddPlayerXY * input->dT;
    // V2 playerDelta = 0.5f * input->dT * input->dT * *ddPlayerXY + *dPlayerXY * input->dT;

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
#if 1
#else
        Rect2 swapBox;
        swapBox.x = swap->pos.x;
        swapBox.y = swap->pos.y;
        swapBox.size = swap->size;

        Polygon p1;
        p1.points[0] = playerBox.pos;
        p1.points[1] = playerBox.pos + V2{playerBox.width, 0};
        p1.points[2] = playerBox.pos + V2{playerBox.width, playerBox.height};
        p1.points[3] = playerBox.pos + V2{0, playerBox.height};
        p1.numPoints = 4;

        Polygon p2;
        p2.points[0] = swapBox.pos;
        p2.points[1] = swapBox.pos + V2{swapBox.width, 0};
        p2.points[2] = swapBox.pos + V2{swapBox.width, swapBox.height};
        p2.points[3] = swapBox.pos + V2{0, swapBox.height};
        p2.numPoints = 4;

        if (GJKMainLoop(&p1, &p2).hit)
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
#endif

#if 0
        const int iterationCount = 3;

        for (int iteration = 0; iteration < iterationCount; iteration++)
        {
            V2 normal = {};
            float tResult = 1.f;
            V2 desiredPos = player->pos + playerDelta;
            for (Entity *entity = 0; IncrementEntity(level, &entity);)
            {
                // NOTE ALGORITHM:
                // 1. Find minkowski sum of two AABBs
                // 2. Find relative positive of player with respect to the currently checked box.
                // 3. Four cases:
                //      a. If the player is moving left, check that the horizontal length between the
                //      player and the right side of the box is less playerDelta.x. If so, check to
                //      see if the player's y coordinate is within the box. If so, there is a
                //      collision. b, c, d. Similar for the other sides of the box.
                // 4. For the smallest time to collision, simply just place the player at that
                // location. May want to revise this.

                // Ray vs Minkowski Sum AABB
                if (HasFlag(entity, Entity_Collidable))
                {
                    Rect2 collisionBox = CreateRectFromBottomLeft(entity->pos, entity->size);
                    // V2 newNormal = TestMovingEntityCollision(&playerBox, &collisionBox, playerDelta, &tResult);
                    // if (newNormal.x != 0 || newNormal.y != 0)
                    // {
                    //     normal = newNormal;
                    // }
                    if (normal.x == 0 && normal.y == 1)
                    {
                        RemoveFlag(player, Entity_Airborne);
                    }
                }
            }

            player->pos += playerDelta * tResult;
            *dPlayerXY -= normal * Dot(*dPlayerXY, normal);
            playerDelta = desiredPos - player->pos;
            playerDelta -= normal * Dot(playerDelta, normal);
        }
#endif

        Rect2 playerBox;
        *dPlayerXY += *ddPlayerXY * input->dT;
        dPlayerXY->y = Min(Max(dPlayerXY->y, -MAX_Y_SPEED), MAX_Y_SPEED);

        V2 playerDelta = *dPlayerXY * input->dT;
        player->pos += playerDelta;
        playerBox.pos = player->pos;
        playerBox.size = player->size;

        for (Entity *entity = 0; IncrementEntity(level, &entity);)
        {
            if (HasFlag(entity, Entity_Collidable))
            {
                Rect2 collisionBox = CreateRectFromBottomLeft(entity->pos, entity->size);
                // if (BroadPhaseCollision(playerBox, collisionBox, playerDelta))
                // {
                Manifold manifold = NarrowPhaseAABBCollision(playerBox, collisionBox);
                float velocityAlongNormal = Dot(manifold.normal, *dPlayerXY);
                if (velocityAlongNormal > 0)
                {
                    continue;
                }
                *dPlayerXY -= manifold.normal * velocityAlongNormal;
                player->pos += manifold.normal * manifold.penetration;
                playerBox.pos = player->pos;
                if (manifold.normal.x == 0 && manifold.normal.y == 1)
                {
                    RemoveFlag(player, Entity_Airborne);
                }
            }
        }
    }

// Move camera
#if 1
    if (player->pos.y - gameState->camera.pos.y > TILE_MAP_Y_COUNT * TILE_METER_SIZE / 2.f)
    {
        gameState->camera.pos.y += TILE_MAP_Y_COUNT * TILE_METER_SIZE;
    }
    if (player->pos.y - gameState->camera.pos.y < -TILE_MAP_Y_COUNT * TILE_METER_SIZE / 2.f)
    {
        gameState->camera.pos.y -= TILE_MAP_Y_COUNT * TILE_METER_SIZE;
    }
    if (player->pos.x - gameState->camera.pos.x > TILE_MAP_X_COUNT * TILE_METER_SIZE / 2.f)
    {
        gameState->camera.pos.x += TILE_MAP_X_COUNT * TILE_METER_SIZE;
    }
    if (player->pos.x - gameState->camera.pos.x < -TILE_MAP_X_COUNT * TILE_METER_SIZE / 2.f)
    {
        gameState->camera.pos.x -= TILE_MAP_X_COUNT * TILE_METER_SIZE;
    }
// Smooth scrolling
#else
    gameState->camera.pos = player->pos;
#endif

// Debug / Level Editor
#if INTERNAL
    V2 resolutionScale = memory->DebugPlatformGetResolution(memory->handle);
    V2 mouseTilePos;
    if (input->leftClick.keyDown)
    {
        if (ScaleMousePosition(input->mousePos, resolutionScale, buffer->width, buffer->height, &mouseTilePos))
        {
            mouseTilePos /= METERS_TO_PIXELS;
            mouseTilePos.x =
                floorf(mouseTilePos.x + gameState->camera.pos.x - (TILE_METER_SIZE * TILE_MAP_X_COUNT / 2.f));
            mouseTilePos.y =
                floorf(mouseTilePos.y + gameState->camera.pos.y - (TILE_METER_SIZE * TILE_MAP_Y_COUNT / 2.f));
            bool result = true;
            for (Entity *entity = 0; IncrementEntity(level, &entity);)
            {
                if (entity->pos == mouseTilePos)
                {
                    result = false;
                    break;
                }
            }
            if (result)
            {
                Entity *entity = CreateWall(gameState, gameState->level);

                entity->pos = mouseTilePos;
                entity->size = V2{TILE_METER_SIZE, TILE_METER_SIZE};
            }
        }
    }
    if (input->rightClick.keyDown)
    {
        if (ScaleMousePosition(input->mousePos, resolutionScale, buffer->width, buffer->height, &mouseTilePos))
        {
            mouseTilePos /= METERS_TO_PIXELS;
            mouseTilePos.x =
                floorf(mouseTilePos.x + gameState->camera.pos.x - (TILE_METER_SIZE * TILE_MAP_X_COUNT / 2.f));
            mouseTilePos.y =
                floorf(mouseTilePos.y + gameState->camera.pos.y - (TILE_METER_SIZE * TILE_MAP_Y_COUNT / 2.f));
            for (Entity *entity = 0; IncrementEntity(level, &entity);)
            {
                if (entity->pos == mouseTilePos)
                {
                    RemoveEntity(level, entity);
                    break;
                }
            }
        }
    }

#endif

    // Draw
    {
        // TODO: I hate this, I'm learning OPENGL
        DrawRectangle(buffer, V2{0, 0}, V2{RESX, RESY}, 1.f, 1.f, 1.f);
        Camera *camera = &gameState->camera;

        for (Entity *entity = 0; IncrementEntity(level, &entity);)
        {
            V2 relative = entity->pos - camera->pos;
            V2 min;
            min.x = screenCenterX + relative.x * METERS_TO_PIXELS;
            min.y = screenCenterY + relative.y * METERS_TO_PIXELS;
            DrawBitmap(buffer, &gameState->bmpTest, min);
        }

        V2 playerTopLeft = V2{screenCenterX, screenCenterY} + (player->pos - camera->pos) * METERS_TO_PIXELS;
        DrawRectangle(buffer, playerTopLeft, playerTopLeft + player->size * METERS_TO_PIXELS, player->r, player->g,
                      player->b);

        if (IsValid(swap))
        {
            V2 swapTopLeft = V2{screenCenterX, screenCenterY} + (swap->pos - camera->pos) * METERS_TO_PIXELS;
            DrawRectangle(buffer, swapTopLeft, swapTopLeft + swap->size * METERS_TO_PIXELS, swap->r, swap->g, swap->b);
        }

        // GameOutputSound(soundBuffer, gameState->toneHz);
    }
}
