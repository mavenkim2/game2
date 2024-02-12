#include "keepmovingforward.h"

#if WINDOWS
#include "win32.cpp"
#endif

#include "keepmovingforward_memory.cpp"
#include "keepmovingforward_string.cpp"
#include "keepmovingforward_camera.cpp"
#include "keepmovingforward_entity.cpp"
#include "keepmovingforward_asset.cpp"
#include "render/debugdraw.cpp"
#include "render/renderer.cpp"

const f32 GRAVITY = 49.f;

#if 0
void GameOutputSound(GameSoundOutput *soundBuffer, int toneHz)
{
    static f32 tSine = 0;
    int16 toneVolume = 3000;
    f32 wavePeriod = (f32)soundBuffer->samplesPerSecond / toneHz;

    int16 *sampleOutput = soundBuffer->samples;
    for (int i = 0; i < soundBuffer->sampleCount; i++)
    {
        f32 sineValue = sinf(tSine);
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

void DrawRectangle(GameOffscreenBuffer *buffer, const V2 min, const V2 max, const f32 r, const f32 g, const f32 b)
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
    u32 red   = RoundF32ToU32(r * 255.f);
    u32 blue  = RoundF32ToU32(b * 255.f);
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
        minX   = 0;
    }
    if (minY < 0)
    {
        alignY = -minY;
        minY   = 0;
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
    u8 *destRow    = ((u8 *)buffer->memory + minY * buffer->pitch + minX * buffer->bytesPerPixel);
    for (int y = minY; y < maxY; y++)
    {
        u32 *source = sourceRow;
        u32 *dest   = (u32 *)destRow;
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
    DebugBmpResult result      = {};
    DebugReadFileOutput output = PlatformReadFile(filename);
    if (output.fileSize != 0)
    {
        BmpHeader *header = (BmpHeader *)output.contents;
        u32 *pixels       = (u32 *)((u8 *)output.contents + header->offset);
        result.pixels     = pixels;

        result.width  = header->width;
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

    AddFlag(wall, Entity_Valid | Entity_Collidable);
    return wall;
}

// NOTE: this assumes that the entity arg is in the level arg
inline void RemoveEntity(Level *level, Entity *entity)
{
    RemoveFlag(entity, Entity_Valid);
}

internal void GenerateLevel(GameState *gameState, i32 tileWidth, i32 tileHeight)
{
    Level *level       = gameState->level;
    level->levelWidth  = tileWidth;
    level->levelHeight = tileHeight;

    for (i32 y = -tileHeight / 2; y < tileHeight / 2; y++)
    {
        for (i32 x = -tileWidth / 2; x < tileWidth / 2; x++)
        {
            Entity *entity = CreateWall(gameState, gameState->level);

            entity->pos  = V3{(f32)x, (f32)y, 0.f};
            entity->size = V3{0.5f, 0.5f, 0.5f};
            if ((x + tileWidth / 2) % 3 == 0)
            {
                entity->color = V4{1.f, 0.f, 0.f, 1.f};
            }
            else if ((x + tileWidth / 2) % 3 == 1)
            {
                entity->color = V4{0.f, 1.f, 0.f, 1.f};
            }
            else
            {
                entity->color = V4{1.f, 0.f, 1.f, 1.f};
            }
        }
    }
}

#if 0
internal b32 TestWallCollision(f32 wallLocation, f32 playerRelWall, f32 playerRelWallParallel, f32 playerDelta,
                               f32 *tMin, f32 wallStart, f32 wallEnd)
{
    b32 hit = false;
    f32 epsilon = 0.00001f;
    f32 t = (wallLocation - playerRelWall) / playerDelta;
    if (t >= 0 && t < *tMin)
    {
        f32 y = playerRelWallParallel + t * playerDelta;
        if (y >= wallStart && y <= wallEnd)
        {
            hit = true;
            *tMin = Max(0.f, t - epsilon);
        }
    }
    return hit;
}

// TODO: need to handle case with two moving objects? we'll see
internal b32 BroadPhaseCollision(const Rect2 dynamic, const Rect2 fixed, V2 delta)
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
#endif

struct Manifold
{
    V3 normal;
    f32 penetration;
};

internal Manifold NarrowPhaseAABBCollision(const Rect3 a, const Rect3 b)
{
    Manifold manifold = {};
    V3 relative       = Center(a) - Center(b);
    f32 aHalfExtent   = a.xSize / 2;
    f32 bHalfExtent   = b.xSize / 2;
    f32 xOverlap      = aHalfExtent + bHalfExtent - Abs(relative.x);

    if (xOverlap <= 0)
    {
        return manifold;
    }

    aHalfExtent  = a.ySize / 2;
    bHalfExtent  = b.ySize / 2;
    f32 yOverlap = aHalfExtent + bHalfExtent - Abs(relative.y);
    if (yOverlap <= 0)
    {
        return manifold;
    }

    aHalfExtent  = a.zSize / 2;
    bHalfExtent  = b.zSize / 2;
    f32 zOverlap = aHalfExtent + bHalfExtent - Abs(relative.z);
    if (zOverlap <= 0)
    {
        return manifold;
    }

    if (xOverlap < yOverlap && xOverlap < zOverlap)
    {
        if (relative.x < 0)
        {
            manifold.normal = {-1, 0, 0};
        }
        else
        {
            manifold.normal = {1, 0, 0};
        }
        manifold.penetration = xOverlap;
    }
    else if (yOverlap < zOverlap)
    {
        if (relative.y < 0)
        {
            manifold.normal = {0, -1, 0};
        }
        else
        {
            manifold.normal = {0, 1, 0};
        }
        manifold.penetration = yOverlap;
    }
    else
    {
        if (relative.z < 0)
        {
            manifold.normal = {0, 0, -1};
        }
        else
        {
            manifold.normal = {0, 0, 1};
        }
        manifold.penetration = zOverlap;
    }

    return manifold;
}

#if 0
internal V2 TestMovingEntityCollision(const Rect2 *dynamic, const Rect2 *fixed, V2 delta, f32 *tResult)
{
    V2 normal = {};

    if (!BroadPhaseCollision(*dynamic, *fixed, delta))
    {
        return normal;
    }

    f32 tMin = *tResult;

    f32 diameterWidth = dynamic->width + fixed->width;
    f32 diameterHeight = dynamic->height + fixed->height;

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
#endif

// TODO: return (0, 0) instead of bifurcating code path?
internal b32 ScaleMousePosition(V2 mousePos, V2 resolutionScale, u32 bufferWidth, u32 bufferHeight,
                                V2 *mouseTilePos)
{
    if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < resolutionScale.x && mousePos.y < resolutionScale.y)
    {
        *mouseTilePos   = mousePos;
        mouseTilePos->y = resolutionScale.y - mouseTilePos->y;

        resolutionScale.x /= bufferWidth;
        resolutionScale.y /= bufferHeight;
        mouseTilePos->x /= resolutionScale.x;
        mouseTilePos->y /= resolutionScale.y;

        // *mouseTilePos /= (f32)TILE_PIXEL_SIZE;

        return true;
    }
    return false;
}

internal void InitializePlayer(GameState *gameState)
{
    Entity *player = &gameState->player;

    player->pos.x = 6.f;
    player->pos.y = 6.f;
    player->pos.z = 5.f;

    player->size.x = TILE_METER_SIZE;
    player->size.y = TILE_METER_SIZE;
    player->size.z = TILE_METER_SIZE * 2;

    player->color.r = 0.5f;
    player->color.g = 0.f;
    player->color.b = 0.5f;
    player->color.a = 1.f;

    AddFlag(player, Entity_Valid | Entity_Collidable | Entity_Airborne | Entity_Swappable);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // Initialization
    GameState *gameState = (GameState *)memory->PersistentStorageMemory;

    f32 screenCenterX = renderState->width * .5f;
    f32 screenCenterY = renderState->height * .5f;

    if (!memory->isInitialized)
    {
        scratchArena = ArenaAlloc(megabytes(64));

        gameState->worldArena = ArenaAlloc((void *)((u8 *)(memory->PersistentStorageMemory) + sizeof(GameState)),
                                           memory->PersistentStorageSize - sizeof(GameState));

        // Load assets
        //
        // ASSIMP
        //
        // ModelOutput output = AssimpDebugLoadModel(gameState->worldArena, Str8Lit("data/dragon/scene.gltf"));
        // gameState->model   = output.model;
        // WriteModelToFile(&gameState->model, Str8Lit("data/dragon.model"));
        // WriteSkeletonToFile(&gameState->model.skeleton, Str8Lit("data/dragon.skel"));

        ReadModelFromFile(gameState->worldArena, &gameState->model, Str8Lit("data/dragon.model"));
        ReadSkeletonFromFile(gameState->worldArena, &gameState->model.skeleton, Str8Lit("data/dragon.skel"));
        KeyframedAnimation *animation = PushStruct(gameState->worldArena, KeyframedAnimation);

        // AssimpLoadAnimation(gameState->worldArena, Str8Lit("data/dragon/scene.gltf"), animation);
        // WriteAnimationToFile(animation, Str8Lit("data/dragon_attack_01.anim"));

        ReadAnimationFile(gameState->worldArena, animation, Str8Lit("data/dragon_attack_01.anim"));

        int width, height, nChannels;
        // stbi_set_flip_vertically_on_load(true);

        // TODO: it's annoying that for every array I have to manually initialize the memory. maybe use
        // stb arrays
        ArrayInit(gameState->worldArena, gameState->model.textures, Texture, 4);

        void *data =
            stbi_load("data/dragon/MI_M_B_44_Qishilong_body02_Inst_diffuse.png", &width, &height, &nChannels, 0);
        Texture texture;
        texture.id       = 0;
        texture.width    = width;
        texture.height   = height;
        texture.type     = TextureType_Diffuse;
        texture.contents = (u8 *)data;

        PushTexture(texture, &gameState->model);

        void *data2 =
            stbi_load("data/dragon/MI_M_B_44_Qishilong_body02_Inst_normal.png", &width, &height, &nChannels, 0);
        Texture texture2;
        texture2.id       = 0;
        texture2.width    = width;
        texture2.height   = height;
        texture2.type     = TextureType_Normal;
        texture2.contents = (u8 *)data2;
        PushTexture(texture2, &gameState->model);

        void *data3 =
            stbi_load("data/dragon/MI_M_B_44_Qishilong_body02_2_Inst_diffuse.png", &width, &height, &nChannels, 0);
        Texture texture3;
        texture3.id       = 0;
        texture3.width    = width;
        texture3.height   = height;
        texture3.type     = TextureType_Diffuse;
        texture3.contents = (u8 *)data3;
        PushTexture(texture3, &gameState->model);

        void *data4 =
            stbi_load("data/dragon/MI_M_B_44_Qishilong_body02_2_Inst_normal.png", &width, &height, &nChannels, 0);
        Texture texture4;
        texture4.id       = 0;
        texture4.width    = width;
        texture4.height   = height;
        texture4.type     = TextureType_Normal;
        texture4.contents = (u8 *)data4;
        PushTexture(texture4, &gameState->model);

        gameState->level      = PushStruct(gameState->worldArena, Level);
        gameState->cameraMode = CameraMode_Player;

        Entity *nilEntity = CreateEntity(gameState, gameState->level);

        GenerateLevel(gameState, 40, 24); // TILE_MAP_X_COUNT, TILE_MAP_Y_COUNT);

        InitializePlayer(gameState);

        Entity *swap  = CreateEntity(gameState, gameState->level);
        *swap         = {};
        swap->pos.x   = 8.f;
        swap->pos.y   = 8.f;
        swap->size.x  = TILE_METER_SIZE;
        swap->size.y  = TILE_METER_SIZE;
        swap->color.r = 0.f;
        swap->color.g = 1.f;
        swap->color.b = 1.f;
        swap->color.a = 1.f;
        AddFlag(swap, Entity_Valid | Entity_Collidable | Entity_Swappable);

        renderState->camera.position = gameState->player.pos - V3{0, 10, 0};
        renderState->camera.pitch    = -PI / 4;
        renderState->camera.yaw      = 0; // PI / 2;
        memory->isInitialized        = true;

        AnimationPlayer *aPlayer = &gameState->animPlayer;
        StartLoopedAnimation(aPlayer, animation);

        gameState->tforms = PushArray(gameState->worldArena, AnimationTransform, gameState->model.skeleton.count);
        gameState->finalTransforms = PushArray(gameState->worldArena, Mat4, gameState->model.skeleton.count);
    }
    Level *level                = gameState->level;
    GameInput *playerController = input;

    // Physics

    Entity *player = GetPlayer(gameState);
    Entity *swap   = {};

    V3 *acceleration = &player->acceleration;
    *acceleration    = {};

    if (playerController->swap.keyDown && playerController->swap.halfTransitionCount > 0)
    {
        if (gameState->cameraMode == CameraMode_Player)
        {
            gameState->cameraMode = CameraMode_Debug;
            memory->PlatformToggleCursor(true);
        }
        else
        {
            gameState->cameraMode = CameraMode_Player;
            memory->PlatformToggleCursor(false);
        }
    }
    // TODO: separate out movement code from input
    switch (gameState->cameraMode)
    {
        // TODO: when you alt tab, the game still tracks your mouse. probably need to switch to raw input,
        // and only use deltas when the game is capturing your mouse
        case CameraMode_Player:
        {
            // TODO: hide mouse, move camera about player
            V2 dMouseP = input->deltaMouse;

            Camera *camera   = &renderState->camera;
            f32 cameraOffset = 10.f;
            f32 speed        = 3.f;
            if (input->shift.keyDown)
            {
                speed = 10.f;
            }
            f32 rotationSpeed = 0.0005f * PI;
            RotateCamera(camera, dMouseP, rotationSpeed);

            Mat4 rotation = MakeMat4(1.f);

            V3 worldUp = {0, 0, 1};

            camera->forward.x = Cos(camera->yaw) * Cos(camera->pitch);
            camera->forward.y = Sin(camera->yaw) * Cos(camera->pitch);
            camera->forward.z = Sin(camera->pitch);
            camera->forward   = Normalize(camera->forward);
            // camera->right = Normalize(Cross(camera->forward, worldUp));
            camera->position  = player->pos - cameraOffset * camera->forward;
            Mat4 cameraMatrix = LookAt4(camera->position, player->pos, worldUp);

            V3 forward = {Cos(camera->yaw), Sin(camera->yaw), 0};
            forward    = Normalize(forward);
            V3 right   = {Sin(camera->yaw), -Cos(camera->yaw), 0};
            right      = Normalize(right);

            if (playerController->right.keyDown)
            {
                *acceleration = right;
            }
            if (playerController->left.keyDown)
            {
                *acceleration = -right;
            }
            if (playerController->up.keyDown)
            {
                *acceleration = forward;
            }
            if (playerController->down.keyDown)
            {
                *acceleration = -forward;
            }
            Mat4 projection = Perspective4(Radians(45.f), 16.f / 9.f, .1f, 1000.f);

            Mat4 transform = projection * cameraMatrix;

            renderState->transform = transform;
            break;
        }
        case CameraMode_Debug:
        {
            V2 dMouseP = input->mousePos - input->lastMousePos;

            Camera *camera = &renderState->camera;
            f32 speed      = 3.f;
            if (input->shift.keyDown)
            {
                speed = 100.f;
            }

            Mat4 rotation = MakeMat4(1.f);
            if (input->rightClick.keyDown)
            {
                f32 rotationSpeed = 0.0005f * PI;
                RotateCamera(camera, dMouseP, rotationSpeed);

                if (input->up.keyDown)
                {
                    camera->position += camera->forward * speed * input->dT;
                }
                if (input->down.keyDown)
                {
                    camera->position += camera->forward * -speed * input->dT;
                }
                if (input->left.keyDown)
                {
                    camera->position += camera->right * -speed * input->dT;
                }
                if (input->right.keyDown)
                {
                    camera->position += camera->right * speed * input->dT;
                }
            }

            V3 worldUp = {0, 0, 1};

            camera->forward.x = Cos(camera->yaw) * Cos(camera->pitch);
            camera->forward.y = Sin(camera->yaw) * Cos(camera->pitch);
            camera->forward.z = Sin(camera->pitch);
            camera->forward   = Normalize(camera->forward);
            camera->right     = Normalize(Cross(camera->forward, worldUp));
            V3 up             = Normalize(Cross(camera->right, camera->forward));

            Mat4 projection   = Perspective4(Radians(45.f), 16.f / 9.f, .1f, 10000.f);
            Mat4 cameraMatrix = LookAt4(camera->position, camera->position + camera->forward, worldUp);
            // CameraTransform(camera->right, up, -camera->forward, camera->position);

            Mat4 transform = projection * cameraMatrix;

            renderState->transform = transform;
            break;
        }
    }
    // TODO: swap w/ shortest distance
    /* for (int i = 0; i < MAX_ENTITIES; i++)
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
            Swap(V3, player->pos, swap->pos);
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
    } */

    V3 *velocity = &player->velocity;

    Normalize(acceleration->xy);

    f32 multiplier = 50;
    if (playerController->shift.keyDown)
    {
        multiplier = 100;
    }
    acceleration->xy *= multiplier;

    acceleration->z = -GRAVITY;
    if (playerController->jump.keyDown && !HasFlag(player, Entity_Airborne))
    {
        velocity->z += 25.f;
        AddFlag(player, Entity_Airborne);
    }

    Rect3 playerBox;
    // TODO: pull out friction
    *acceleration += MakeV3(-800.f * velocity->xy * input->dT, -10.f * velocity->z * input->dT);
    *velocity += *acceleration * input->dT;

    V3 playerDelta = *velocity * input->dT;
    player->pos += playerDelta;
    playerBox.pos  = player->pos;
    playerBox.size = player->size;

    // TODO: fix position of boxes (center vs bottom left vs bottom center)
    // TODO: capsule collision for player? GJK?
    for (Entity *entity = 0; IncrementEntity(level, &entity);)
    {
        if (HasFlag(entity, Entity_Collidable))
        {
            Rect3 collisionBox      = Rect3BottomLeft(entity->pos, entity->size);
            Manifold manifold       = NarrowPhaseAABBCollision(playerBox, collisionBox);
            f32 velocityAlongNormal = Dot(manifold.normal, *velocity);
            if (velocityAlongNormal > 0)
            {
                continue;
            }
            *velocity -= manifold.normal * velocityAlongNormal;
            player->pos += manifold.normal * manifold.penetration;
            playerBox.pos = player->pos;
            if (manifold.normal.x == 0 && manifold.normal.y == 0 && manifold.normal.z == 1)
            {
                RemoveFlag(player, Entity_Airborne);
            }
        }
    }

// Move camera
#if 1
    // if (player->pos.y - gameState->camera.pos.y > TILE_MAP_Y_COUNT * TILE_METER_SIZE / 2.f)
    // {
    //     gameState->camera.pos.y += TILE_MAP_Y_COUNT * TILE_METER_SIZE;
    // }
    // if (player->pos.y - gameState->camera.pos.y < -TILE_MAP_Y_COUNT * TILE_METER_SIZE / 2.f)
    // {
    //     gameState->camera.pos.y -= TILE_MAP_Y_COUNT * TILE_METER_SIZE;
    // }
    // if (player->pos.x - gameState->camera.pos.x > TILE_MAP_X_COUNT * TILE_METER_SIZE / 2.f)
    // {
    //     gameState->camera.pos.x += TILE_MAP_X_COUNT * TILE_METER_SIZE;
    // }
    // if (player->pos.x - gameState->camera.pos.x < -TILE_MAP_X_COUNT * TILE_METER_SIZE / 2.f)
    // {
    //     gameState->camera.pos.x -= TILE_MAP_X_COUNT * TILE_METER_SIZE;
    // }
// Smooth scrolling
#else
    gameState->camera.pos = player->pos;
#endif

// Debug / Level Editor
#if INTERNAL
#else
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
            b32 result = true;
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

                entity->pos  = mouseTilePos;
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

    // Render
    {
        BeginRenderFrame(renderState);
        renderState->commands.count = 0;
        // for (Entity *entity = 0; IncrementEntity(level, &entity);)
        // {
        //     PushCube(&openGL->group, entity->pos, entity->size, entity->color);
        // }
        // PushCube(&openGL->group, player->pos, player->size, player->color);
        // PushModel();

        // ANIMATION
        PlayCurrentAnimation(&gameState->animPlayer, input->dT, gameState->tforms);

        SkinModelToAnimation(&gameState->animPlayer, &gameState->model, gameState->tforms,
                             gameState->finalTransforms);
        PushModel(renderState, &gameState->model, gameState->finalTransforms);
        DrawLine(&renderState->debugRenderer, {0, 0, 0}, {0, 0, 5}, {1, 0, 0, 1});
    }
    // GameOutputSound(soundBuffer, gameState->toneHz);
}
