#include "mkGame.h"

#include "mkMalloc.cpp"
#include "mkInput.cpp"
#include "mkThreadContext.cpp"

#include "mkJobsystem.cpp"
#include "mkPhysics.cpp"
#include "mkMemory.cpp"
#include "mkString.cpp"
#include "mkCamera.cpp"
#include "mkJob.cpp"
#include "mkFont.cpp"
#include "mkAsset.cpp"
#include "mkAssetCache.cpp"
#include "render/mkRenderGraph.cpp"
#include "render/mkRender.cpp"
#include "mkShaderCompiler.cpp"
#include "mkDebug.cpp"
#include "mkScene.cpp"

internal Manifold NarrowPhaseAABBCollision(const Rect3 a, const Rect3 b)
{
    Manifold manifold = {};
    V3 relative       = GetCenter(a) - GetCenter(b);
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

void G_State::Insert(string name, u32 index)
{
    u32 sid              = AddSID(name);
    EntitySlotNode *node = freeNode;
    if (node)
    {
        StackPop(freeNode);
    }
    else
    {
        node = PushStruct(permanentArena, EntitySlotNode);
    }
    node->sid        = sid;
    node->index      = index;
    EntitySlot *slot = &entityMap[sid & (numSlots - 1)];
    QueuePush(slot->first, slot->last, node);
}

u32 G_State::GetIndex(string name)
{
    u32 sid          = GetSID(name);
    EntitySlot *slot = &entityMap[sid & (numSlots - 1)];
    for (EntitySlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->sid == sid)
        {
            return node->index;
        }
    }
    return 0;
}

// simply waits for all threads to finish executing
G_FLUSH(G_Flush)
{
    delete shadercompiler::compiler;
    AS_Flush();
    if (reload)
    {
        jobsystem::EndJobsystem();
    }
    else
    {
        jobsystem::ForceQuit();
    }
}

G_INIT(G_Init)
{
    if (ioPlatformMemory->mIsHotloaded || !ioPlatformMemory->mIsLoaded)
    {
        engine    = ioPlatformMemory->mEngine;
        gameScene = ioPlatformMemory->mScene;
        platform  = ioPlatformMemory->mPlatform;
        shared    = ioPlatformMemory->mShared;
        device    = ioPlatformMemory->mGraphics;
        Printf    = platform.Printf;
        ThreadContextSet(ioPlatformMemory->mTctx);
    }
    if (ioPlatformMemory->mIsHotloaded)
    {
        // restart terminated threads
        jobsystem::InitializeJobsystem();
        AS_Restart();
        ioPlatformMemory->mIsHotloaded = 0;
    }
    if (!ioPlatformMemory->mIsLoaded)
    {
        ioPlatformMemory->mIsLoaded = 1;
        Arena *frameArena           = ArenaAlloc(ARENA_RESERVE_SIZE, 16);
        Arena *permanentArena       = ArenaAlloc();
        Arena *sceneArena           = ArenaAlloc();

        jobsystem::InitializeJobsystem();
        AS_Init();
        gameScene->Init(sceneArena);
        // F_Init();
        D_Init();

        G_State *g_state = PushStruct(permanentArena, G_State);
        engine->SetGameState(g_state);
        g_state->permanentArena = permanentArena;
        g_state->frameArena     = frameArena;

        g_state->entityMap = PushArray(permanentArena, G_State::EntitySlot, g_state->numSlots);
        // Load assets
        {
            Mat4 translate          = Translate4(V3{0, 20, 0});
            Mat4 scale              = Scale(V3{0.5f, 0.5f, 0.5f});
            Mat4 rotate             = Rotate4(MakeV3(1, 0, 0), PI / 2);
            g_state->mTransforms[1] = translate * rotate * scale;

            translate = Translate4(V3{0, 20, 20});
            // scale                   = Scale(V3{1, 1, 1});
            g_state->mTransforms[2] = translate * rotate;

            translate               = Translate4(V3{0, 0, 0});
            scale                   = Scale(V3{1, 1, 1});
            g_state->mTransforms[3] = translate * rotate;

            string dragonName = Str8Lit("data/models/dragon.model");
            g_state->Insert(dragonName, 1);
            g_state->mEntities[1].mAssetHandle = AS_GetAsset(dragonName);

            string heroName = Str8Lit("data/models/hero.model");
            g_state->Insert(heroName, 2);
            g_state->mEntities[2].mAssetHandle = AS_GetAsset(heroName);

            string sponza = Str8Lit("data/models/Main.1_Sponza.model");
            g_state->Insert(sponza, 3);
            g_state->mEntities[3].mAssetHandle = AS_GetAsset(sponza);

            // g_state->numEntities = 3;

            AS_Handle anim;
            AnimationPlayer *aPlayer;
            anim    = AS_GetAsset(Str8Lit("data/animations/Qishilong_attack01.anim"));
            aPlayer = &g_state->mAnimPlayers[0];
            StartLoopedAnimation(aPlayer, anim);

            anim    = AS_GetAsset(Str8Lit("data/animations/Mon_BlackDragon31_Btl_Atk01.anim"));
            aPlayer = &g_state->mAnimPlayers[1];
            StartLoopedAnimation(aPlayer, anim);
        }

        // g_state->eva    = AS_GetAsset(Str8Lit("data/eva/Eva01.model"));
        // g_state->modelBall = AS_GetAsset(Str8Lit("data/ball/scene.model"));
        // g_state->font = AS_GetAsset(Str8Lit("data/liberation_mono.ttf"));

        g_state->camera.position = V3{0, -10, 0};
        g_state->camera.pitch    = 0; //-PI / 4;
        g_state->camera.yaw      = PI / 2;
        g_state->cameraMode      = CameraMode_Debug;

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

        // Testing Memory
        // {
        //     DynamicArray<u32> array;
        //     for (u32 i = 0; i < 100; i++)
        //     {
        //         array.Push(i);
        //     }
        //     u32 test   = array[40];
        //     u32 amount = Memory::GetAllocationAmount();
        //
        //     jobsystem::Counter counter = {};
        //     jobsystem::KickJob(&counter, [&](jobsystem::JobArgs args) {
        //         for (u32 i = 0; i < array.Length(); i++)
        //         {
        //             Printf("%u\n", array[i]);
        //         }
        //     });
        //     jobsystem::WaitJobs(&counter);
        // }

        // g_state->heightmap = CreateHeightmap(Str8Lit("data/heightmap.png"));

        // Stuff
        render::Initialize();
    }
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

using namespace graphics;
using namespace scene;
// using namespace render;

DLL G_UPDATE(G_Update)
{
    debugState.BeginFrame();
    G_State *g_state         = engine->GetGameState();
    RenderState *renderState = engine->GetRenderState();
    ArenaClear(g_state->frameArena);

    //////////////////////////////
    // Input
    //

    G_Input *playerController;
    // TODO: move polling to a different frame?
    {
        I_PollInput();

        OS_Events events = I_GetInput(g_state->frameArena);
        V2 mousePos      = platform.GetMousePos(shared->windowHandle);

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

    // if (playerController->swap.keyDown)
    // {
    //     if (playerController->swap.halfTransitionCount > 0)
    //     {
    //         if (g_state->cameraMode == CameraMode_Player)
    //         {
    //             g_state->cameraMode = CameraMode_Debug;
    //             platform.ToggleCursor(1);
    //         }
    //         else
    //         {
    //             g_state->cameraMode = CameraMode_Player;
    //             platform.ToggleCursor(0);
    //         }
    //     }
    // }

    D_BeginFrame();

    // TODO: separate out movement code from input
    switch (g_state->cameraMode)
    {
        // TODO: when you alt tab, the game still tracks your mouse. probably need to switch to raw input,
        // and only use deltas when the game is capturing your mouse
        // case CameraMode_Player:
        // {
        //     // TODO: hide mouse, move camera about player
        //     V2 center = platform.GetCenter(shared->windowHandle, 1);
        //     // V2 mousePos = OS_GetMousePos(shared->windowHandle);
        //     V2 dMouseP = playerController->mousePos - center;
        //
        //     center = platform.GetCenter(shared->windowHandle, 0);
        //     platform.SetMousePos(shared->windowHandle, center);
        //
        //     Camera *camera   = &g_state->camera;
        //     f32 cameraOffset = 10.f;
        //     f32 speed        = 3.f;
        //     if (playerController->shift.keyDown)
        //     {
        //         speed = 10.f;
        //     }
        //     f32 rotationSpeed = 0.0005f * PI;
        //     RotateCamera(camera, dMouseP, rotationSpeed);
        //
        //     Mat4 rotation = MakeMat4(1.f);
        //
        //     V3 worldUp = {0, 0, 1};
        //
        //     camera->forward.x = Cos(camera->yaw) * Cos(camera->pitch);
        //     camera->forward.y = Sin(camera->yaw) * Cos(camera->pitch);
        //     camera->forward.z = Sin(camera->pitch);
        //     camera->forward   = Normalize(camera->forward);
        //     // camera->right = Normalize(Cross(camera->forward, worldUp));
        //     camera->position  = player->pos - cameraOffset * camera->forward;
        //     Mat4 cameraMatrix = LookAt4(camera->position, player->pos, worldUp);
        //
        //     V3 forward = {Cos(camera->yaw), Sin(camera->yaw), 0};
        //     forward    = Normalize(forward);
        //     V3 right   = {Sin(camera->yaw), -Cos(camera->yaw), 0};
        //     right      = Normalize(right);
        //
        //     if (playerController->right.keyDown)
        //     {
        //         *acceleration = right;
        //     }
        //     if (playerController->left.keyDown)
        //     {
        //         *acceleration = -right;
        //     }
        //     if (playerController->up.keyDown)
        //     {
        //         *acceleration = forward;
        //     }
        //     if (playerController->down.keyDown)
        //     {
        //         *acceleration = -forward;
        //     }
        //     Mat4 projection =
        //         Perspective4(renderState->fov, renderState->aspectRatio, renderState->nearZ, renderState->farZ);
        //
        //     Mat4 transform = projection * cameraMatrix;
        //
        //     renderState->viewMatrix = cameraMatrix;
        //     renderState->transform  = transform;
        //     renderState->projection = projection;
        //     break;
        // }
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
            renderState->projection = projection;

            // TODO: mouse project into world
            {
                // screen space -> homogeneous clip space -> view space -> world space
                Mat4 screenSpaceToWorldMatrix = Inverse(transform);
                V2 mouseP                     = playerController->mousePos;
                V2 viewport                   = platform.GetWindowDimension(shared->windowHandle);
                // Screen space ->NDC
                mouseP.x = Clamp(2 * mouseP.x / viewport.x - 1, -1, 1);
                mouseP.y = Clamp(1 - (2 * mouseP.y / viewport.y), -1, 1);

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

    renderState->camera = g_state->camera;
    // Update
    gameScene->ProcessRequests();
    u32 totalMatrixCount      = 0;
    u32 totalMeshCount        = gameScene->meshes.GetTotal();
    u32 totalMeshClusterCount = 0;
    u32 totalMaterialCount    = gameScene->materials.GetEndPos();

    // Process component system requests

    for (SkeletonIter iter = gameScene->BeginSkelIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        LoadedSkeleton *skeleton = gameScene->Get(&iter);
        skeleton->skinningOffset = totalMatrixCount;
        totalMatrixCount += skeleton->count;
    }

    // TODO: this should probably be r_framealloced for the renderer backend to use
    // TODO: all children must be ensured to be after parents in hierarchycomponent

    Mat4 *frameTransforms = PushArray(g_state->frameArena, Mat4, gameScene->transforms.GetEndPos());
    frameTransforms[0]    = Identity();

    for (HierarchyIter iter = gameScene->BeginHierIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        HierarchyComponent *h = gameScene->Get(&iter);
        Entity entity         = gameScene->GetEntity(&iter);

        TransformHandle handle = gameScene->transforms.GetHandle(entity);
        Mat4 transform         = *gameScene->transforms.GetFromHandle(handle);
        u32 transformIndex     = gameScene->transforms.GetIndex(handle);
        Assert(transformIndex != 0);

        handle          = gameScene->transforms.GetHandle(h->parent);
        u32 parentIndex = gameScene->transforms.GetIndex(handle);

        frameTransforms[transformIndex] = frameTransforms[parentIndex] * transform;
    }

    for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        Mesh *mesh = gameScene->Get(&iter);
        Assert(mesh->numSubsets >= 1);
        totalMeshClusterCount += mesh->clusterCount;
    }
    render::meshClusterCount = totalMeshClusterCount;

    // Resize
    u32 totalSkinningSize     = totalMatrixCount * sizeof(Mat4);
    u32 totalMeshParamsSize   = totalMeshCount * sizeof(MeshParams);
    u32 totalMeshGeometrySize = totalMeshCount * sizeof(MeshGeometry);
    u32 totalMaterialSize     = totalMaterialCount * sizeof(ShaderMaterial);
    u32 totalIndirectSize     = totalMeshClusterCount * sizeof(DrawIndexedIndirectCommand);
    u32 totalMeshIndexSize    = totalMeshClusterCount * sizeof(u32) * CLUSTER_SIZE * 3;
    u32 totalClusterIndexSize = totalMeshClusterCount * sizeof(u32);

    GPUBuffer *skinningUpload = &render::skinningBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *skinningBuffer = &render::skinningBuffer;

    GPUBuffer *meshParamsUpload = &render::meshParamsBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *meshParamsBuffer = &render::meshParamsBuffer;

    GPUBuffer *meshGeometryUpload = &render::meshGeometryBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *meshGeometryBuffer = &render::meshGeometryBuffer;

    // TODO: uploading the clusters should happen when the scene loads them
    GPUBuffer *materialUpload = &render::materialBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *materialBuffer = &render::materialBuffer;

    // GPUBuffer *indirectUpload = &render::meshIndirectBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *indirectBuffer = &render::meshIndirectBuffer;
    GPUBuffer *indexBuffer    = &render::meshIndexBuffer;
    {
        if (totalSkinningSize > skinningUpload->desc.size)
        {
            for (u32 frame = 0; frame < device->cNumBuffers; frame++)
            {
                device->ResizeBuffer(&render::skinningBufferUpload[frame], totalSkinningSize * 2);
            }
            device->ResizeBuffer(skinningBuffer, totalSkinningSize * 2);
        }
        if (totalMeshParamsSize > meshParamsUpload->desc.size)
        {
            for (u32 frame = 0; frame < device->cNumBuffers; frame++)
            {
                device->ResizeBuffer(&render::meshParamsBufferUpload[frame], totalMeshParamsSize * 2);
            }
            device->ResizeBuffer(meshParamsBuffer, totalMeshParamsSize * 2);
        }
        if (totalMeshGeometrySize > meshGeometryUpload->desc.size)
        {
            for (u32 frame = 0; frame < device->cNumBuffers; frame++)
            {
                device->ResizeBuffer(&render::meshGeometryBufferUpload[frame], totalMeshGeometrySize * 2);
            }
            device->ResizeBuffer(meshGeometryBuffer, totalMeshGeometrySize * 2);
        }
        if (totalMaterialSize > materialUpload->desc.size)
        {
            for (u32 frame = 0; frame < device->cNumBuffers; frame++)
            {
                device->ResizeBuffer(&render::materialBufferUpload[frame], totalMaterialSize * 2);
            }
            device->ResizeBuffer(materialBuffer, totalMaterialSize * 2);
        }
        if (totalIndirectSize > indirectBuffer->desc.size)
        {
            device->ResizeBuffer(indirectBuffer, totalIndirectSize * 2);
            device->ResizeBuffer(&render::indirectScratchBuffer, totalIndirectSize * 2);
        }
        if (totalMeshIndexSize > indexBuffer->desc.size)
        {
            device->ResizeBuffer(indexBuffer, totalMeshIndexSize * 2);
        }
        if (totalClusterIndexSize > render::meshClusterIndexBuffer.desc.size)
        {
            device->ResizeBuffer(&render::meshClusterIndexBuffer, totalClusterIndexSize * 2);
        }
    }

    render::skinningBufferSize     = totalSkinningSize;
    render::meshParamsBufferSize   = totalMeshParamsSize;
    render::meshGeometryBufferSize = totalMeshGeometrySize;
    render::materialBufferSize     = totalMaterialSize;
    render::meshIndirectBufferSize = totalIndirectSize;

    Mat4 *skinningMappedData             = (Mat4 *)skinningUpload->mappedData;
    MeshParams *meshParamsMappedData     = (MeshParams *)meshParamsUpload->mappedData;
    MeshGeometry *meshGeometryMappedData = (MeshGeometry *)meshGeometryUpload->mappedData;
    ShaderMaterial *materialMappedData   = (ShaderMaterial *)materialUpload->mappedData;

    for (SkeletonIter iter = gameScene->BeginSkelIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        LoadedSkeleton *skeleton   = gameScene->Get(&iter);
        AnimationTransform *tforms = PushArray(g_state->frameArena, AnimationTransform, skeleton->count);

        u32 globalIndex = iter.globalIndex;
        PlayCurrentAnimation(&g_state->mAnimPlayers[globalIndex], dt, tforms);
        SkinModelToAnimation(&g_state->mAnimPlayers[globalIndex], skeleton, tforms,
                             skinningMappedData + skeleton->skinningOffset);

        Init(&skeleton->aabb);
        for (u32 boneIndex = 0; boneIndex < skeleton->count; boneIndex++)
        {
            V3 bonePos   = skinningMappedData[skeleton->skinningOffset + boneIndex] * -GetTranslation(skeleton->inverseBindPoses[boneIndex]);
            Rect3 bounds = MakeRect3Center(bonePos, {1.f, 1.f, 1.f});
            AddBounds(skeleton->aabb, bounds);
        }
    }

    for (MaterialIter iter = gameScene->BeginMatIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        MaterialComponent *mat   = gameScene->Get(&iter);
        u32 index                = iter.globalIndex;
        ShaderMaterial *material = &materialMappedData[index];
        material->normal         = -1;
        material->albedo         = -1;
        if (mat->IsRenderable())
        {
            graphics::Texture *texture = GetTexture(mat->textures[TextureType_Diffuse]);
            i32 descriptorIndex        = device->GetDescriptorIndex(texture, ResourceViewType::SRV);
            material->albedo           = descriptorIndex;

            texture          = GetTexture(mat->textures[TextureType_Normal]);
            descriptorIndex  = device->GetDescriptorIndex(texture, ResourceViewType::SRV);
            material->normal = descriptorIndex;
        }
    }

    // Frustum culling
    Plane planes[6];
    ExtractPlanes(planes, renderState->transform);
    u32 meshCountAligned    = AlignPow2(totalMeshCount, 4);
    b32 *frustumCullResults = PushArrayNoZero(g_state->frameArena, b32, meshCountAligned);

    b32 *testResults = PushArrayNoZero(g_state->frameArena, b32, meshCountAligned);
    // MemorySet(frustumCullResults, 1, sizeof(frustumCullResults[0]) * totalMeshCount);
    f32 *boundingBoxes = PushArrayNoZero(g_state->frameArena, f32, 6 * meshCountAligned);

    u32 meshCount = 0;
    for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        Mesh *mesh    = gameScene->Get(&iter);
        Entity entity = gameScene->GetEntity(&iter);

        Mat4 transform         = frameTransforms[gameScene->transforms.GetIndex(entity)];
        Rect3 worldSpaceBounds = Transform(transform, mesh->bounds);

        u32 startIndex                                   = meshCount / 4 * 24;
        boundingBoxes[startIndex + (meshCount & 3) + 0]  = worldSpaceBounds.minX;
        boundingBoxes[startIndex + (meshCount & 3) + 4]  = worldSpaceBounds.minY;
        boundingBoxes[startIndex + (meshCount & 3) + 8]  = worldSpaceBounds.minZ;
        boundingBoxes[startIndex + (meshCount & 3) + 12] = worldSpaceBounds.maxX;
        boundingBoxes[startIndex + (meshCount & 3) + 16] = worldSpaceBounds.maxY;
        boundingBoxes[startIndex + (meshCount & 3) + 20] = worldSpaceBounds.maxZ;

        meshCount++;

        // Debug
    }
    {
        Assert(meshCount == totalMeshCount);
        for (u32 i = 0; i < meshCountAligned; i += 4)
        {
            u32 rangeId = TIMED_CPU_RANGE_NAME_BEGIN("Frustum cull");
            IntersectFrustumAABB(planes, &boundingBoxes[6 * i], &frustumCullResults[i]);
            TIMED_RANGE_END(rangeId);
        }
    }

    u32 totalClusterCount = 0;
    for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    {
        Mesh *mesh      = gameScene->Get(&iter);
        Entity entity   = gameScene->GetEntity(&iter);
        mesh->meshIndex = -1;

        // if (!mesh->IsRenderable()) continue;
        // if (!frustumCullResults[iter.globalIndex]) continue;

        Mat4 transform = frameTransforms[gameScene->transforms.GetIndex(entity)];
        // Mat4 mvp       = renderState->transform * transform;

#if 0
        LoadedSkeleton *skeleton = gameScene->skeletons.GetFromEntity(entity);
        Rect3 bounds             = mesh->bounds;
        if (skeleton)
        {
            bounds = skeleton->aabb;
        }
        u32 rangeId = TIMED_CPU_RANGE_NAME_BEGIN("Frustum cull");
        if (!IntersectFrustumAABB(mvp, mesh->bounds, renderState->nearZ, renderState->farZ)) continue;
        TIMED_RANGE_END(rangeId);
#endif

        mesh->meshIndex = iter.globalIndex;

        MeshParams *meshParams   = &meshParamsMappedData[mesh->meshIndex];
        meshParams->localToWorld = transform;

        meshParams->minP          = mesh->bounds.minP; // mesh space
        meshParams->maxP          = mesh->bounds.maxP;
        meshParams->clusterOffset = mesh->clusterOffset;
        meshParams->clusterCount  = mesh->clusterCount;

        totalClusterCount += mesh->clusterCount;

        MeshGeometry *geometry = &meshGeometryMappedData[mesh->meshIndex];
        geometry->vertexPos    = mesh->posDescriptor;
        geometry->vertexNor    = mesh->norDescriptor;
        geometry->vertexTan    = mesh->tanDescriptor;
        geometry->vertexUv     = mesh->vertexUvView.srvDescriptor;
        geometry->vertexInd    = mesh->indexView.srvDescriptor;
    }

    Assert(totalClusterCount == totalMeshClusterCount);
    render::meshClusterCount = totalClusterCount;

    // DebugDrawSkeleton(g_state->model, transform1, skinningMappedData);
    // SkinModelToBindPose(g_state->model, skinningMatrices1);
    // Mat4 mvp1 = renderState->transform * transform1;
    // D_PushModel(g_state->model, transform1, mvp1, skinningMatrices1, skeleton->count);

    // Render
    render::Render();
}

// #if 0
//     V3 *velocity = &player->velocity;
//
//     Normalize(acceleration->xy);
//
//     f32 multiplier = 50;
//     if (playerController->shift.keyDown)
//     {
//         multiplier = 100;
//     }
//     acceleration->xy *= multiplier;
//
//     acceleration->z = -GRAVITY;
//     if (playerController->jump.keyDown && !HasFlag(player, Entity_Airborne))
//     {
//         velocity->z += 25.f;
//         AddFlag(player, Entity_Airborne);
//     }
//
//     Rect3 playerBox;
//     // TODO: pull out friction
//     *acceleration += MakeV3(-800.f * velocity->xy * dt, -10.f * velocity->z * dt);
//     *velocity += *acceleration * dt;
//
//     V3 playerDelta = *velocity * dt;
//     player->pos += playerDelta;
//     playerBox.minP = player->pos;
//     playerBox.maxP = player->pos + player->size;
//
//     // TODO: fix position of boxes (center vs bottom left vs bottom center)
//     // TODO: capsule collision for player? GJK?
//     for (Entity *entity = 0; IncrementEntity(level, &entity);)
//     {
//         if (HasFlag(entity, Entity_Collidable))
//         {
//             Rect3 collisionBox      = Rect3BottomLeft(entity->pos, entity->size);
//             Manifold manifold       = NarrowPhaseAABBCollision(playerBox, collisionBox);
//             f32 velocityAlongNormal = Dot(manifold.normal, *velocity);
//             if (velocityAlongNormal > 0)
//             {
//                 continue;
//             }
//             *velocity -= manifold.normal * velocityAlongNormal;
//             player->pos += manifold.normal * manifold.penetration;
//             playerBox.minP = player->pos;
//             if (manifold.normal.x == 0 && manifold.normal.y == 0 && manifold.normal.z == 1)
//             {
//                 RemoveFlag(player, Entity_Airborne);
//             }
//         }
//     }
//
//     // Physics
//     {
//         // ConvexShape a;
//         // f32 p = 1;
//         //
//         // V3 points[8] = {MakeV3(-p, -p, -p), MakeV3(p, -p, -p), MakeV3(p, p, -p), MakeV3(-p, p, -p),
//         //                 MakeV3(-p, -p, p),  MakeV3(p, -p, p),  MakeV3(p, p, p),  MakeV3(-p, p, p)};
//         // a.points     = points;
//         // a.numPoints  = ArrayLength(points);
//         // loopi(0, a.numPoints - 1)
//         // {
//         //     DrawLine(&renderState->debugRenderer, a.points[i], a.points[i + 1], Color_Red);
//         // }
//
//         // ConvexShape b;
//         // V3 points2[8] = {MakeV3(-p, -p, -p+1), MakeV3(p, -p, -p+1),
//         //                  MakeV3(p, p, -p+1),   MakeV3(-p, p, -p+1),
//         //                  MakeV3(-p + 1, -p + 1, p + 2),  MakeV3(p + 1, -p + 1, p + 2),
//         //                  MakeV3(p + 1, p + 1, p + 2),    MakeV3(-p + 1, p + 1, p + 2)};
//         // b.points      = points2;
//         // b.numPoints   = ArrayLength(points2);
//         // loopi(0, b.numPoints)
//         // {
//         //     DrawPoint(&renderState->debugRenderer, b.points[i], Color_Blue);
//         // }
//         ConvexShape sphereA = MakeSphere(MakeV3(5, 0, 0), 1.f);
//         ConvexShape sphereB = MakeSphere(MakeV3(7.f, 0, 0), 1.f);
//
//         b32 result = Intersects(&sphereA, &sphereB, MakeV3(0, 0, 0));
//         V4 color;
//         if (result)
//         {
//             color = Color_Red;
//         }
//         else
//         {
//             color = Color_Green;
//         }
//
//         DrawSphere(sphereA.center, sphereA.radius, color);
//         DrawSphere(sphereB.center, sphereB.radius, color);
//
//         DrawSphere(player->pos, 2.f, color);
//
//         DrawBox({5, 1, 1}, {2, 1, 1}, Color_Black);
//         DrawBox({8, 3, 3}, {1, 4, 2}, Color_Green);
//         DrawArrow({0, 0, 0}, {5, 0, 0}, {1, 0, 0, 1}, 1.f);
//         DrawArrow({0, 0, 0}, {0, 5, 0}, {0, 1, 0, 1}, 1.f);
//         DrawArrow({0, 0, 0}, {0, 0, 5}, {0, 0, 1, 1}, 1.f);
//     }
//
// // Render
// {
//     // for (Entity *entity = 0; IncrementEntity(level, &entity);)
//     // {
//     //     PushCube(&openGL->group, entity->pos, entity->size, entity->color);
//     // }
//     // PushCube(&openGL->group, player->pos, player->size, player->color);
//     // DrawRectangle(&renderState, V2(0, 0), V2(renderState.width / 10, renderState.height / 10));
//
//         static b8 test = 0;
//         static VC_Handle vertex;
//         static VC_Handle index;
//         if (!test)
//         {
//             test                   = 1;
//             MeshVertex vertices[4] = {};
//             vertices[0].position   = {-1, -1, 0};
//             vertices[1].position   = {1, -1, 0};
//             vertices[2].position   = {1, 1, 0};
//             vertices[3].position   = {-1, 1, 0};
//
//             vertices[0].normal = {0, 0, 1.f};
//             vertices[1].normal = {0, 0, 1.f};
//             vertices[2].normal = {0, 0, 1.f};
//             vertices[3].normal = {0, 0, 1.f};
//
//             vertices[0].tangent = {1.f, 0, 0};
//             vertices[1].tangent = {1.f, 0, 0};
//             vertices[2].tangent = {1.f, 0, 0};
//             vertices[3].tangent = {1.f, 0, 0};
//
//             u32 indices[6];
//             indices[0] = 0;
//             indices[1] = 1;
//             indices[2] = 2;
//             indices[3] = 0;
//             indices[4] = 2;
//             indices[5] = 3;
//
//             vertex = renderState->vertexCache.VC_AllocateBuffer(BufferType_Vertex, BufferUsage_Static, vertices, sizeof(MeshVertex), 4);
//             index  = renderState->vertexCache.VC_AllocateBuffer(BufferType_Index, BufferUsage_Static, indices, sizeof(u32), 6);
//         }
//
//         Mat4 transform = Scale({20.f, 20.f, 20.f});
//         D_PushModel(vertex, index, renderState->transform * transform);
//
//         LoadedSkeleton *skeleton    = GetSkeletonFromModel(g_state->model);
//         AnimationTransform *tforms1 = PushArray(g_state->frameArena, AnimationTransform, skeleton->count);
//         Mat4 *skinningMatrices1     = PushArray(g_state->frameArena, Mat4, skeleton->count);
//         PlayCurrentAnimation(g_state->permanentArena, &g_state->animPlayer, dt, tforms1);
//         SkinModelToAnimation(&g_state->animPlayer, g_state->model, tforms1, skinningMatrices1);
//         DebugDrawSkeleton(g_state->model, transform1, skinningMatrices1);
//         // SkinModelToBindPose(g_state->model, skinningMatrices1);
//         Mat4 mvp1 = renderState->transform * transform1;
//         D_PushModel(g_state->model, transform1, mvp1, skinningMatrices1, skeleton->count);
//
//         LoadedSkeleton *skeleton2   = GetSkeletonFromModel(g_state->model2);
//         AnimationTransform *tforms2 = PushArray(g_state->frameArena, AnimationTransform, skeleton2->count);
//         Mat4 *skinningMatrices2     = PushArray(g_state->frameArena, Mat4, skeleton2->count);
//         // PlayCurrentAnimation(g_state->worldArena, &g_state->animPlayer, dt, tforms2);
//         // SkinModelToAnimation(&g_state->animPlayer, &g_state->model2, tforms2, finalTransforms2);
//         SkinModelToBindPose(g_state->model2, skinningMatrices2);
//         Mat4 mvp2 = renderState->transform * transform2;
//         D_PushModel(g_state->model2, transform2, mvp2, skinningMatrices2, skeleton2->count);
//
//         // Eva model 01
//         translate       = Translate4(V3{-10, 10, 0});
//         scale           = Scale(V3{.1f, .1f, .1f});
//         rotate          = Rotate4(MakeV3(1, 0, 0), PI / 2);
//         Mat4 transform3 = translate * scale;
//         Mat4 mvp3       = renderState->transform * transform3;
//
//         D_PushModel(g_state->eva, transform3, mvp3);
//
//         // Ball
//
//         translate       = Translate4(V3{20, 10, 0});
//         scale           = Scale(V3{.1f, .1f, .1f});
//         rotate          = Rotate4(MakeV3(1, 0, 0), PI / 2);
//         Mat4 transform4 = translate;
//         Mat4 mvp4       = renderState->transform * transform4;
//
//         D_PushModel(g_state->modelBall, transform4, mvp4);
//
//         Light light;
//         light.type = LightType_Directional;
//         // TODO: no matter what direction is specified the shadow map is always {0, 0, -1}
//         // light.dir = MakeV3(0, .75f, 0.5f);
//         light.dir = MakeV3(0.f, 0.f, 1.f);
//         light.dir = Normalize(light.dir);
//         // light.pos = MakeV3(0.f, 0.f, 0.f);
//         D_PushLight(&light);
//
//         // D_PushHeightmap(g_state->heightmap);
//         D_CollateDebugRecords();
//
//         // R_SubmitFrame();
//     }
//     D_EndFrame();
// }
// #endif
