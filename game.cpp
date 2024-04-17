#include "crack.h"
#ifdef LSP_INCLUDE
#include "platform.h"
#include "keepmovingforward_common.h"
#include "asset.h"
#include "asset.cpp"
#include "asset_cache.h"
#include "/render/render.h"
#include "/render/render.cpp"
#include "debug.cpp"
#include "game.h"
#endif

const f32 GRAVITY = 49.f;

global G_State *g_state;

internal void InitializePlayer(G_State *gameState)
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

internal void G_EntryPoint(void *p)
{
    SetThreadName(Str8Lit("[G] Thread"));

    f32 frameDt    = 1.f / 144.f;
    f32 multiplier = 1.f;

    f32 frameTime = OS_NowSeconds();

    for (; shared->running == 1;)
    {
        frameTime = OS_NowSeconds();

        G_Update(frameDt * multiplier);

        // Wait until new update
        f32 endWorkFrameTime = OS_NowSeconds();
        f32 timeElapsed      = endWorkFrameTime - frameTime;

        if (timeElapsed < frameDt)
        {
            u32 msTimeToSleep = (u32)(1000.f * (frameDt - timeElapsed));
            if (msTimeToSleep > 0)
            {
                OS_Sleep(msTimeToSleep);
            }
        }

        while (timeElapsed < frameDt)
        {
            timeElapsed = OS_NowSeconds() - frameTime;
        }

        // TODO: on another thread
        R_EndFrame();
    }
}

internal Manifold NarrowPhaseAABBCollision(const Rect3 a, const Rect3 b)
{
    Manifold manifold = {};
    V3 relative       = Center(a) - Center(b);
    f32 aHalfExtent   = (a.maxX - a.minX) / 2;
    f32 bHalfExtent   = (b.maxX - b.minX) / 2;
    f32 xOverlap      = aHalfExtent + bHalfExtent - Abs(relative.x);

    if (xOverlap <= 0)
    {
        return manifold;
    }

    aHalfExtent  = (a.maxY - a.minY) / 2;
    bHalfExtent  = (b.maxY - b.minY) / 2;
    f32 yOverlap = aHalfExtent + bHalfExtent - Abs(relative.y);
    if (yOverlap <= 0)
    {
        return manifold;
    }

    aHalfExtent  = (a.maxZ - a.minZ) / 2;
    bHalfExtent  = (b.maxZ - b.minZ) / 2;
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

internal void G_Init()
{
    Arena *frameArena       = ArenaAlloc();
    Arena *permanentArena   = ArenaAlloc();
    g_state                 = PushStruct(permanentArena, G_State);
    g_state->permanentArena = permanentArena;
    g_state->frameArena     = frameArena;

    // Load assets

    g_state->model  = AS_GetAsset(Str8Lit("data/dragon/scene.model"));
    g_state->model2 = AS_GetAsset(Str8Lit("data/hero/scene.model"));
    g_state->eva    = AS_GetAsset(Str8Lit("data/eva/Eva01.model"));

    KeyframedAnimation *animation = PushStruct(g_state->permanentArena, KeyframedAnimation);

    // ReadAnimationFile(g_state->worldArena, animation, Str8Lit("data/dragon_attack_01.anim"));
    AS_Handle anim = AS_GetAsset(Str8Lit("data/dragon/Qishilong_attack01.anim"));
    // AS_Handle anim = LoadAssetFile(Str8Lit("data/dragon/Qishilong_attack02.anim"));
    g_state->font = AS_GetAsset(Str8Lit("data/liberation_mono.ttf"));

    g_state->level           = PushStruct(g_state->permanentArena, Level);
    g_state->camera.position = g_state->player.pos - V3{0, 10, 0};
    g_state->camera.pitch    = 0; //-PI / 4;
    g_state->camera.yaw      = PI / 2;
    g_state->cameraMode      = CameraMode_Debug;

    Entity *nilEntity = CreateEntity(g_state, g_state->level);

    GenerateLevel(g_state, 40, 24);

    InitializePlayer(g_state);

    Entity *swap  = CreateEntity(g_state, g_state->level);
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

    AnimationPlayer *aPlayer = &g_state->animPlayer;
    StartLoopedAnimation(g_state->permanentArena, aPlayer, anim);

    // Initialize input
    g_state->bindings.bindings[I_Button_Up]         = OS_Key_W;
    g_state->bindings.bindings[I_Button_Down]       = OS_Key_S;
    g_state->bindings.bindings[I_Button_Left]       = OS_Key_A;
    g_state->bindings.bindings[I_Button_Right]      = OS_Key_D;
    g_state->bindings.bindings[I_Button_Jump]       = OS_Key_Space;
    g_state->bindings.bindings[I_Button_Shift]      = OS_Key_Shift;
    g_state->bindings.bindings[I_Button_Swap]       = OS_Key_C;
    g_state->bindings.bindings[I_Button_LeftClick]  = OS_Mouse_L;
    g_state->bindings.bindings[I_Button_RightClick] = OS_Mouse_R;

    g_state->heightmap = CreateHeightmap(Str8Lit("data/heightmap.png"));
}

internal OS_Event *GetKeyEvent(OS_Events *events, OS_Key key)
{
    OS_Event *result = 0;
    for (u32 i = 0; i < events->numEvents; i++)
    {
        OS_Event *event = events->events + i;
        if (event->key == key)
        {
            result = event;
            break;
        }
    }
    return result;
}

internal OS_Event *GetEventType(OS_Events *events, OS_EventType type)
{
    OS_Event *result = 0;
    for (u32 i = 0; i < events->numEvents; i++)
    {
        OS_Event *event = events->events + i;
        if (event->type == type)
        {
            result = event;
            break;
        }
    }
    return result;
}

internal void G_Update(f32 dt)
{
    ArenaClear(g_state->frameArena);

    //////////////////////////////
    // Input
    //

    G_Input *playerController;
    // TODO: move polling to a different frame?
    {
        I_PollInput();

        OS_Events events = I_GetInput(g_state->frameArena);
        V2 mousePos      = OS_GetMousePos(shared->windowHandle);

        if (GetEventType(&events, OS_EventType_Quit))
        {
            shared->running = 0;
            return;
        }

        // Double buffer input
        g_state->newInputIndex = (g_state->newInputIndex + 1) & 1;
        G_Input *newInput      = &g_state->input[g_state->newInputIndex];
        G_Input *oldInput      = &g_state->input[(g_state->newInputIndex + 1) & 1];
        *newInput              = {};

        newInput->mousePos     = mousePos;
        newInput->lastMousePos = oldInput->mousePos;

        if (!GetEventType(&events, OS_EventType_LoseFocus))
        {
            for (I_Button button = (I_Button)0; button < I_Button_Count; button = (I_Button)(button + 1))
            {
                newInput->buttons[button].keyDown             = oldInput->buttons[button].keyDown;
                newInput->buttons[button].halfTransitionCount = 0;
            }

            for (I_Button button = (I_Button)0; button < I_Button_Count; button = (I_Button)(button + 1))
            {
                OS_Event *event = GetKeyEvent(&events, g_state->bindings.bindings[button]);
                if (event)
                {
                    Printf("Key: %u\n", button);
                    b32 isDown = event->type == OS_EventType_KeyPressed ? 1 : 0;
                    // u32 transition = isDown != newInput->buttons[button].keyDown;
                    u32 transition                                = event->transition;
                    newInput->buttons[button].keyDown             = isDown;
                    newInput->buttons[button].halfTransitionCount = transition;
                }
            }
        }
        playerController = newInput;
    }

    // RenderState *renderState = PushStruct(g_state->frameArena, RenderState);

    Level *level = g_state->level;

    // Physics

    Entity *player = GetPlayer(g_state);
    Entity *swap   = {};

    V3 *acceleration = &player->acceleration;
    *acceleration    = {};

    if (playerController->swap.keyDown)
    {
        if (playerController->swap.halfTransitionCount > 0)
        {
            if (g_state->cameraMode == CameraMode_Player)
            {
                g_state->cameraMode = CameraMode_Debug;
                OS_ToggleCursor(1);
            }
            else
            {
                g_state->cameraMode = CameraMode_Player;
                OS_ToggleCursor(0);
            }
        }
    }

    D_BeginFrame();

    // TODO: separate out movement code from input
    switch (g_state->cameraMode)
    {
        // TODO: when you alt tab, the game still tracks your mouse. probably need to switch to raw input,
        // and only use deltas when the game is capturing your mouse
        case CameraMode_Player:
        {
            // TODO: hide mouse, move camera about player
            V2 center = OS_GetCenter(shared->windowHandle, 1);
            // V2 mousePos = OS_GetMousePos(shared->windowHandle);
            V2 dMouseP = playerController->mousePos - center;

            center = OS_GetCenter(shared->windowHandle, 0);
            OS_SetMousePos(shared->windowHandle, center);

            Camera *camera   = &g_state->camera;
            f32 cameraOffset = 10.f;
            f32 speed        = 3.f;
            if (playerController->shift.keyDown)
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
            Mat4 projection =
                Perspective4(renderState->fov, renderState->aspectRatio, renderState->nearZ, renderState->farZ);

            Mat4 transform = projection * cameraMatrix;

            renderState->transform = transform;
            break;
        }
        // TODO: get back to parity
        // features needed: lock mouse invisible to middle of screen
        // debug right click moving
        // im pretty sure that was it
        case CameraMode_Debug:
        {
            V2 dMouseP = playerController->mousePos - playerController->lastMousePos;

            Camera *camera = &g_state->camera;
            f32 speed      = 3.f;
            if (playerController->shift.keyDown)
            {
                speed = 100.f;
            }

            Mat4 rotation = MakeMat4(1.f);
            if (playerController->rightClick.keyDown)
            {
                f32 rotationSpeed = 0.0005f * PI;
                RotateCamera(camera, dMouseP, rotationSpeed);

                if (playerController->up.keyDown)
                {
                    camera->position += camera->forward * speed * dt;
                }
                if (playerController->down.keyDown)
                {
                    camera->position += camera->forward * -speed * dt;
                }
                if (playerController->left.keyDown)
                {
                    camera->position += camera->right * -speed * dt;
                }
                if (playerController->right.keyDown)
                {
                    camera->position += camera->right * speed * dt;
                }
            }

            V3 worldUp = {0, 0, 1};

            camera->forward.x = Cos(camera->yaw) * Cos(camera->pitch);
            camera->forward.y = Sin(camera->yaw) * Cos(camera->pitch);
            camera->forward.z = Sin(camera->pitch);
            camera->forward   = Normalize(camera->forward);
            camera->right     = Normalize(Cross(camera->forward, worldUp));
            // V3 up             = Normalize(Cross(camera->right, camera->forward));

            Mat4 projection =
                Perspective4(renderState->fov, renderState->aspectRatio, renderState->nearZ, renderState->farZ);

            Mat3 dir = ToMat3(camera->forward);
            Mat4 cameraMatrix;
            CalculateViewMatrix(camera->position, dir, cameraMatrix);

            Mat4 transform = projection * cameraMatrix;

            renderState->viewMatrix = cameraMatrix;
            renderState->transform  = transform;

            // TODO: mouse project into world
            {
                // screen space -> homogeneous clip space -> view space -> world space
                Mat4 screenSpaceToWorldMatrix = Inverse(transform);
                V2 mouseP                     = playerController->mousePos;
                V2 viewport                   = OS_GetWindowDimension(shared->windowHandle);
                // Screen space ->NDC
                mouseP.x = Clamp(2 * mouseP.x / viewport.x - 1, -1, 1);
                mouseP.y = Clamp(1 - (2 * mouseP.y / viewport.y), -1, 1);

                struct Ray
                {
                    V3 mStartP;
                    V3 mDir;
                };

                Ray ray;
                // TODO: do I need to convert from NDC to homogeneous clip space?

                // NDC -> View Space
                V3 viewSpacePos = Inverse(projection) * MakeV3(mouseP, 0.f);

                // View Space -> World Space
                Mat4 viewToWorldSpace = Inverse(cameraMatrix);
                V3 worldSpacePos      = viewToWorldSpace * viewSpacePos;
                // V4 worldSpacePos = Inverse(cameraMatrix) * MakeV4(viewSpacePos, 0.f);
                ray.mStartP = worldSpacePos;
                ray.mDir    = Normalize(ray.mStartP - camera->position);

                if (playerController->leftClick.keyDown)
                {
                    // DrawSphere(ray.mStartP, 10.f, Color_Red);
                    // DrawPoint(ray.mStartP, Color_Black);
                    V3 p = ray.mStartP + ray.mDir * 100.f;
                    // DrawLine(ray.mStartP, p, Color_Black);
                    DrawSphere(p, 2.f, Color_Red);
                    // DrawLine(camera->position, camera->position + camera->forward, Color_Black);
                }
            }
            break;
        }
    }

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
    *acceleration += MakeV3(-800.f * velocity->xy * dt, -10.f * velocity->z * dt);
    *velocity += *acceleration * dt;

    V3 playerDelta = *velocity * dt;
    player->pos += playerDelta;
    playerBox.minP = player->pos;
    playerBox.maxP = player->pos + player->size;

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
            playerBox.minP = player->pos;
            if (manifold.normal.x == 0 && manifold.normal.y == 0 && manifold.normal.z == 1)
            {
                RemoveFlag(player, Entity_Airborne);
            }
        }
    }

    // Physics
    {
        // ConvexShape a;
        // f32 p = 1;
        //
        // V3 points[8] = {MakeV3(-p, -p, -p), MakeV3(p, -p, -p), MakeV3(p, p, -p), MakeV3(-p, p, -p),
        //                 MakeV3(-p, -p, p),  MakeV3(p, -p, p),  MakeV3(p, p, p),  MakeV3(-p, p, p)};
        // a.points     = points;
        // a.numPoints  = ArrayLength(points);
        // loopi(0, a.numPoints - 1)
        // {
        //     DrawLine(&renderState->debugRenderer, a.points[i], a.points[i + 1], Color_Red);
        // }

        // ConvexShape b;
        // V3 points2[8] = {MakeV3(-p, -p, -p+1), MakeV3(p, -p, -p+1),
        //                  MakeV3(p, p, -p+1),   MakeV3(-p, p, -p+1),
        //                  MakeV3(-p + 1, -p + 1, p + 2),  MakeV3(p + 1, -p + 1, p + 2),
        //                  MakeV3(p + 1, p + 1, p + 2),    MakeV3(-p + 1, p + 1, p + 2)};
        // b.points      = points2;
        // b.numPoints   = ArrayLength(points2);
        // loopi(0, b.numPoints)
        // {
        //     DrawPoint(&renderState->debugRenderer, b.points[i], Color_Blue);
        // }
        ConvexShape sphereA = MakeSphere(MakeV3(5, 0, 0), 1.f);
        ConvexShape sphereB = MakeSphere(MakeV3(7.f, 0, 0), 1.f);

        b32 result = Intersects(&sphereA, &sphereB, MakeV3(0, 0, 0));
        V4 color;
        if (result)
        {
            color = Color_Red;
        }
        else
        {
            color = Color_Green;
        }

        DrawSphere(sphereA.center, sphereA.radius, color);
        DrawSphere(sphereB.center, sphereB.radius, color);

        DrawSphere(player->pos, 2.f, color);

        DrawBox({5, 1, 1}, {2, 1, 1}, Color_Black);
        DrawBox({8, 3, 3}, {1, 4, 2}, Color_Green);
        DrawArrow({0, 0, 0}, {5, 0, 0}, {1, 0, 0, 1}, 1.f);
        DrawArrow({0, 0, 0}, {0, 5, 0}, {0, 1, 0, 1}, 1.f);
        DrawArrow({0, 0, 0}, {0, 0, 5}, {0, 0, 1, 1}, 1.f);
    }

    // Render
    {
        // for (Entity *entity = 0; IncrementEntity(level, &entity);)
        // {
        //     PushCube(&openGL->group, entity->pos, entity->size, entity->color);
        // }
        // PushCube(&openGL->group, player->pos, player->size, player->color);
        // DrawRectangle(&renderState, V2(0, 0), V2(renderState.width / 10, renderState.height / 10));

        renderState->camera = g_state->camera;

        static b8 test = 0;
        static VC_Handle vertex;
        static VC_Handle index;
        if (!test)
        {
            test = 1;
            MeshVertex vertices[4];
            vertices[0].position = {-1, -1, 0};
            vertices[1].position = {1, -1, 0};
            vertices[2].position = {1, 1, 0};
            vertices[3].position = {-1, 1, 0};

            vertices[0].normal = {0, 0, 1.f};
            vertices[1].normal = {0, 0, 1.f};
            vertices[2].normal = {0, 0, 1.f};
            vertices[3].normal = {0, 0, 1.f};

            u32 indices[6];
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 0;
            indices[4] = 2;
            indices[5] = 3;

            vertex = VC_AllocateBuffer(BufferType_Vertex, BufferUsage_Static, vertices, sizeof(MeshVertex), 4);
            index  = VC_AllocateBuffer(BufferType_Index, BufferUsage_Static, indices, sizeof(u32), 6);
        }

        Mat4 transform  = Identity();
        transform[0][0] = 3;
        transform[1][1] = 3;
        transform[2][2] = 3;

        R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
        R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

        node->val.numSurfaces = 1;
        D_Surface *surfaces   = (D_Surface *)R_FrameAlloc(node->val.numSurfaces * sizeof(*surfaces));
        node->val.surfaces    = surfaces;

        D_Surface *surface = &surfaces[0];

        surface->vertexBuffer = vertex;
        surface->indexBuffer  = index;

        // TODO: ptr or copy?

        QueuePush(pass->list.first, pass->list.last, node);

        // Model 1
        Mat4 translate  = Translate4(V3{0, 20, 5});
        Mat4 scale      = Scale(V3{0.5f, 0.5f, 0.5f});
        Mat4 rotate     = Rotate4(MakeV3(1, 0, 0), PI / 2);
        Mat4 transform1 = translate * rotate * scale;

        LoadedSkeleton *skeleton    = GetSkeletonFromModel(g_state->model);
        AnimationTransform *tforms1 = PushArray(g_state->frameArena, AnimationTransform, skeleton->count);
        Mat4 *skinningMatrices1     = PushArray(g_state->frameArena, Mat4, skeleton->count);
        PlayCurrentAnimation(g_state->permanentArena, &g_state->animPlayer, dt, tforms1);
        // SkinModelToAnimation(&g_state->animPlayer, g_state->model, tforms1, skinningMatrices1);
        DebugDrawSkeleton(g_state->model, transform1, skinningMatrices1);
        SkinModelToBindPose(g_state->model, skinningMatrices1);
        Mat4 mvp1 = renderState->transform * transform1;
        D_PushModel(g_state->model, transform1, mvp1, skinningMatrices1, skeleton->count);

        // Model 2
        translate       = Translate4(V3{0, 20, 20});
        scale           = Scale(V3{1, 1, 1});
        Mat4 transform2 = translate * rotate;

        LoadedSkeleton *skeleton2   = GetSkeletonFromModel(g_state->model2);
        AnimationTransform *tforms2 = PushArray(g_state->frameArena, AnimationTransform, skeleton2->count);
        Mat4 *skinningMatrices2     = PushArray(g_state->frameArena, Mat4, skeleton2->count);
        // PlayCurrentAnimation(g_state->worldArena, &g_state->animPlayer, dt, tforms2);
        // SkinModelToAnimation(&g_state->animPlayer, &g_state->model2, tforms2, finalTransforms2);
        SkinModelToBindPose(g_state->model2, skinningMatrices2);
        Mat4 mvp2 = renderState->transform * transform2;
        D_PushModel(g_state->model2, transform2, mvp2, skinningMatrices2, skeleton2->count);

        // Eva model 01
        translate       = Translate4(V3{-10, 10, 0});
        scale           = Scale(V3{.1f, .1f, .1f});
        rotate          = Rotate4(MakeV3(1, 0, 0), PI / 2);
        Mat4 transform3 = translate * scale;
        Mat4 mvp3       = renderState->transform * transform3;

        D_PushModel(g_state->eva, transform3, mvp3);
        Light light;
        light.type = LightType_Directional;
        light.dir  = MakeV3(0, 0, 1.f);
        light.pos  = MakeV3(0.f, 0.f, 0.f);
        D_PushLight(&light);

        // D_PushHeightmap(g_state->heightmap);
        D_CollateDebugRecords();

        // R_SubmitFrame();
    }
    D_EndFrame();
}
