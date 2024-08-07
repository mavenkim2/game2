#include "../mkCrack.h"
#include <atomic>

#ifdef LSP_INCLUDE
#include "../mkMath.h"
#include "../mkAsset.h"
#include "../generated/render_graph_resources.h"
#include "mkRender.h"
#include "mkRenderCore.h"
#include "../mkCamera.h"
#include "mkGraphics.h"
#include "../mkGame.h"
#include "../mkJobsystem.h"
#endif

using namespace graphics;

global const V4 Color_Red               = {1, 0, 0, 1};
global const V4 Color_Green             = {0, 1, 0, 1};
global const V4 Color_Blue              = {0, 0, 1, 1};
global const V4 Color_Black             = {0, 0, 0, 1};
global const u32 r_primitiveSizeTable[] = {sizeof(R_LineInst), sizeof(DebugVertex), sizeof(R_PrimitiveInst),
                                           sizeof(R_PrimitiveInst)};
#define DEFAULT_SECTORS 12
#define DEFAULT_STACKS  12

internal void D_Init()
{
    Arena *arena     = ArenaAlloc();
    D_State *d_state = PushStruct(arena, D_State);
    engine->SetDrawState(d_state);
    d_state->arena = arena;

    RenderState *state = PushStruct(arena, RenderState);
    engine->SetRenderState(state);

    // TODO: cvars?
    state->fov         = Radians(45.f);
    state->aspectRatio = 16.f / 9.f;
    state->nearZ       = 3.f; //.1f;
    state->farZ        = 1000.f;

    // DebugRenderer *debug = &state->debugRenderer;

    // Create sphere primitive
    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        switch (type)
        {
            case R_PassType_UI:
            {
                state->passes[type].passUI = PushStruct(arena, R_PassUI);
                break;
            }
            case R_PassType_3D:
            {
                state->passes[type].pass3D = PushStruct(arena, R_Pass3D);
                break;
            }
            case R_PassType_Mesh:
            {
                state->passes[type].passMesh = PushStruct(arena, R_PassMesh);
                break;
            }
            default: Assert(!"Invalid default case");
        }
    }

    // Initialize UI Pass
    {
        R_PassUI *pass                   = state->passes[R_PassType_UI].passUI;
        pass->batchList.bytesPerInstance = sizeof(R_RectInst);
    }

    // Initialize 3D Pass
    {
        R_Pass3D *pass  = state->passes[R_PassType_3D].pass3D;
        pass->numGroups = R_Primitive_Count;
        pass->groups    = PushArray(arena, R_Batch3DGroup, pass->numGroups);
        for (R_Primitive type = (R_Primitive)0; type < R_Primitive_Count; type = (R_Primitive)(type + 1))
        {
            R_Batch3DGroup *group = &pass->groups[type];
            switch (type)
            {
                case R_Primitive_Lines:
                {
                    group->params.topology            = R_Topology_Lines;
                    group->params.primType            = type;
                    group->batchList.bytesPerInstance = r_primitiveSizeTable[type];
                    break;
                }
                case R_Primitive_Points:
                {
                    group->params.topology            = R_Topology_Points;
                    group->params.primType            = type;
                    group->batchList.bytesPerInstance = r_primitiveSizeTable[type];
                    break;
                }
                case R_Primitive_Cube:
                {
                    f32 point         = 1;
                    V3 cubeVertices[] = {
                        {-point, -point, -point},
                        {point, -point, -point},
                        {point, point, -point},
                        {-point, point, -point},
                        {-point, -point, point},
                        {point, -point, point},
                        {point, point, point},
                        {-point, point, point},
                    };

                    u32 cubeIndices[]       = {3, 0, 0, 4, 4, 7, 7, 3, 1, 2, 2, 6, 6, 5, 5, 1, 0, 1, 2, 3, 4, 5, 6, 7};
                    u32 vertexCount         = ArrayLength(cubeVertices);
                    u32 indexCount          = ArrayLength(cubeIndices);
                    R_Batch3DParams *params = &group->params;
                    params->vertices        = PushArray(arena, V3, vertexCount);
                    params->indices         = PushArray(arena, u32, indexCount);
                    MemoryCopy(params->vertices, &cubeVertices, sizeof(cubeVertices));
                    MemoryCopy(params->indices, &cubeIndices, sizeof(cubeIndices));
                    params->vertexCount = vertexCount;
                    params->indexCount  = indexCount;
                    params->topology    = R_Topology_Lines;
                    params->primType    = type;

                    group->batchList.bytesPerInstance = r_primitiveSizeTable[type];

                    break;
                }
                case R_Primitive_Sphere:
                {
                    u32 sectors = DEFAULT_SECTORS;
                    u32 stacks  = DEFAULT_STACKS;

                    f32 x, y, z;
                    f32 sectorStep = 2 * PI / sectors;
                    f32 stackStep  = PI / stacks;

                    f32 theta = 0;

                    u32 vertexCount = (sectors + 1) * (stacks + 1);
                    u32 indexCount  = sectors * stacks * 4;

                    u32 vertexIndex = 0;
                    u32 indexIndex  = 0;

                    R_Batch3DParams *params           = &group->params;
                    params->vertices                  = PushArray(arena, V3, vertexCount);
                    params->indices                   = PushArray(arena, u32, indexCount);
                    params->vertexCount               = vertexCount;
                    params->indexCount                = indexCount;
                    params->topology                  = R_Topology_Lines;
                    params->primType                  = type;
                    group->batchList.bytesPerInstance = r_primitiveSizeTable[type];

                    for (u32 i = 0; i < sectors + 1; i++)
                    {
                        f32 phi = -PI / 2;
                        for (u32 j = 0; j < stacks + 1; j++)
                        {
                            x = Cos(phi) * Cos(theta);
                            y = Cos(phi) * Sin(theta);
                            z = Sin(phi);

                            params->vertices[vertexIndex++] = MakeV3(x, y, z);

                            phi += stackStep;
                        }
                        theta += sectorStep;
                    }

                    // Indices
                    for (u32 i = 0; i < sectors; i++)
                    {
                        u32 i1 = i * (stacks + 1);
                        u32 i2 = i1 + (stacks + 1);
                        for (u32 j = 0; j < stacks; j++)
                        {
                            params->indices[indexIndex++] = i1;
                            params->indices[indexIndex++] = i2;
                            params->indices[indexIndex++] = i1;
                            params->indices[indexIndex++] = i1 + 1;
                            i1++;
                            i2++;
                        }
                    }
                    break;
                }
            }
        }
    }

    // state->vertexCache.VC_Init();
    // RenderFrameDataInit();

    d_state->frameStartPos = ArenaPos(arena);
}

internal void D_BeginFrame()
{
    D_State *d_state   = engine->GetDrawState();
    RenderState *state = engine->GetRenderState();
    ArenaPopTo(d_state->arena, d_state->frameStartPos);
    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        switch (type)
        {
            case R_PassType_UI:
            {
                R_PassUI *passUI        = state->passes[type].passUI;
                passUI->batchList.first = passUI->batchList.last = 0;
                passUI->batchList.numInstances                   = 0;
                break;
            }
            case R_PassType_3D:
            {
                R_Pass3D *pass3D = state->passes[type].pass3D;
                for (u32 i = 0; i < pass3D->numGroups; i++)
                {
                    R_Batch3DGroup *group         = &pass3D->groups[i];
                    group->batchList.numInstances = 0;
                    group->batchList.first = group->batchList.last = 0;
                }
                break;
            }
            case R_PassType_Mesh:
            {
                R_PassMesh *pass = state->passes[type].passMesh;
                pass->viewLight  = 0;
                pass->list.first = pass->list.last = 0;
                pass->list.mTotalSurfaceCount      = 0;
            }
            default:
            {
                break;
            }
        }
    }
}

global Rect3 BoundsUnitCube      = {{-1, -1, -1}, {1, 1, 1}};
global Rect3 BoundsZeroToOneCube = {{-1, -1, 0}, {1, 1, 0}};

internal void R_GetFrustumCorners(Mat4 &inInverseMvp, Rect3 &inBounds, V3 *outFrustumCorners)
{
    V4 p;
    for (i32 x = 0; x < 2; x++)
    {
        for (i32 y = 0; y < 2; y++)
        {
            for (i32 z = 0; z < 2; z++)
            {
                p.x = inBounds[x][0];
                p.y = inBounds[y][1];
                p.z = inBounds[z][2];
                p.w = 1;
                p   = inInverseMvp * p;

                // Why this?
                f32 oneOverW                                      = 1 / p.w;
                outFrustumCorners[(z << 2) | (y << 1) | (x << 0)] = p.xyz * oneOverW;
            }
        }
    }
}

// internal void D_PushHeightmap(Heightmap heightmap)
// {
//     R_CommandHeightMap *command = (R_CommandHeightMap *)R_CreateCommand(sizeof(*command));
//     command->type               = R_CommandType_Heightmap;
//     command->heightmap          = heightmap;
// }

internal void R_SetupViewFrustum()
{
    RenderState *renderState           = engine->GetRenderState();
    renderState->inverseViewProjection = Inverse(renderState->transform);
    R_GetFrustumCorners(renderState->inverseViewProjection, BoundsUnitCube, renderState->mFrustumCorners.corners);

    FrustumCorners *corners = &renderState->mFrustumCorners;

    // basically this sorts the list so that the minimum z values are in the first half, the maximum z values
    // are in the second half, the min x values are at even indices, and the miny values are at 0, 1, 4, 5
    // this makes computing the minkowski sum w/ aabb easier, because you can compute the half extent and add it
    // to the max values for the dimension  ou want, and subtract from the min values, so that the frustum grows
    // for x
    for (i32 i = 0; i < 8; i += 2)
    {
        if (corners->corners[i].x > corners->corners[i + 1].x)
        {
            Swap(V3, corners->corners[i], corners->corners[i + 1]);
        }
    }
    // for y
    if (corners->corners[0].y > corners->corners[2].y)
    {
        Swap(V3, corners->corners[0], corners->corners[2]);
    }
    if (corners->corners[1].y > corners->corners[3].y)
    {
        Swap(V3, corners->corners[1], corners->corners[3]);
    }
    if (corners->corners[4].y > corners->corners[6].y)
    {
        Swap(V3, corners->corners[4], corners->corners[6]);
    }
    if (corners->corners[5].y > corners->corners[7].y)
    {
        Swap(V3, corners->corners[5], corners->corners[7]);
    }

    // for z
    for (i32 i = 0; i < 4; i++)
    {
        if (corners->corners[i].z > corners->corners[i + 4].z)
        {
            Swap(V3, corners->corners[i], corners->corners[i + 4]);
        }
    }
}

struct D_FontAlignment
{
    V2 start;
    V2 advance;
};

// TODO: why don't I just pass in the dest rect as instance data?
internal void D_PushRect(Rect2 rect, R_Handle img)
{
    R_PassUI *passUI = R_GetPassFromKind(R_PassType_UI)->passUI;
    R_RectInst *inst = (R_RectInst *)R_BatchListPush(&passUI->batchList, 256);

    V2 scale = (rect.maxP - rect.minP);
    V2 pos   = rect.minP;

    inst->pos    = pos;
    inst->scale  = scale;
    inst->handle = img;
}

internal void D_PushText(AS_Handle font, V2 startPos, f32 size, string line)
{
    RenderState *renderState = engine->GetRenderState();
    TempArena temp           = ScratchStart(0, 0);
    F_Run *run               = F_GetFontRun(temp.arena, font, size, line);
    RenderState *state       = renderState;
    R_PassUI *pass           = R_GetPassFromKind(R_PassType_UI)->passUI;

    f32 advance = 0;
    // TODO: maybe get rid of this by having F_Run be an array w/ a count, which can be 0
    if (run)
    {
        for (F_PieceChunkNode *node = run->first; node != 0; node = node->next)
        {
            for (u32 i = 0; i < node->count; i++)
            {
                F_Piece *piece = node->pieces + i;
                if (!R_HandleMatch(piece->texture, R_HandleZero()))
                {
                    Rect2 rect =
                        MakeRect2({startPos.x + advance + piece->offset.minX, startPos.y + piece->offset.minY},
                                  {startPos.x + advance + piece->offset.maxX, startPos.y + piece->offset.maxY});
                    D_PushRect(rect, piece->texture);
                }
                advance += piece->advance;
            }
        }
    }
    ScratchEnd(temp);
}

internal void D_PushTextF(AS_Handle font, V2 startPos, f32 size, char *fmt, ...)
{
    TempArena temp = ScratchStart(0, 0);
    string result  = {};

    va_list args;
    va_start(args, fmt);
    result = PushStr8FV(temp.arena, fmt, args);
    va_end(args);

    D_PushText(font, startPos, size, result);

    ScratchEnd(temp);
}

inline R_Pass *R_GetPassFromKind(R_PassType type)
{
    RenderState *renderState = engine->GetRenderState();
    RenderState *state       = renderState;
    R_Pass *result           = &state->passes[type];
    return result;
}

internal u8 *R_BatchListPush(R_BatchList *list, u32 instCap)
{
    D_State *d_state  = engine->GetDrawState();
    R_BatchNode *node = list->last;
    if (node == 0 || node->val.byteCount + list->bytesPerInstance > node->val.byteCap)
    {
        node              = PushStruct(d_state->arena, R_BatchNode);
        node->val.byteCap = instCap * list->bytesPerInstance;
        node->val.data    = PushArray(d_state->arena, u8, node->val.byteCap);
        QueuePush(list->first, list->last, node);
    }
    u8 *inst = node->val.data + node->val.byteCount;
    node->val.byteCount += list->bytesPerInstance;
    list->numInstances += 1;
    return inst;
}

internal void DrawLine(V3 from, V3 to, V4 color)
{
    DebugVertex v1;
    v1.pos   = from;
    v1.color = color;
    DebugVertex v2;
    v2.pos   = to;
    v2.color = color;

    R_Pass3D *pass3D      = R_GetPassFromKind(R_PassType_3D)->pass3D;
    R_Batch3DGroup *group = &pass3D->groups[R_Primitive_Lines];
    R_LineInst *output    = (R_LineInst *)R_BatchListPush(&group->batchList, 256);
    output->v[0]          = v1;
    output->v[1]          = v2;
}

internal void DrawPoint(V3 point, V4 color)
{
    R_Pass3D *pass3D      = R_GetPassFromKind(R_PassType_3D)->pass3D;
    R_Batch3DGroup *group = &pass3D->groups[R_Primitive_Points];
    DebugVertex *output   = (DebugVertex *)R_BatchListPush(&group->batchList, 256);
    output->pos           = point;
    output->color         = color;
}

internal void DrawArrow(V3 from, V3 to, V4 color, f32 size)
{
    DrawLine(from, to, color);

    if (size > 0.f)
    {
        V3 dir = to - from;
        dir    = NormalizeOrZero(dir) * size;

        // Get perpendicular vector
        V3 up = {0, 0, 1};
        if (Dot(up, dir) > 0.95)
        {
            up = {1, 0, 0};
        }
        V3 perp = size * Normalize(Cross(Cross(dir, up), dir));

        DrawLine(to - dir + perp, to, color);
        DrawLine(to - dir - perp, to, color);
    }
}

enum PrimitiveType
{
    Primitive_Sphere,
    Primitive_Box,
    Primitive_MAX,
};

internal void DrawBox(V3 offset, V3 scale, V4 color)
{
    Mat4 transform        = Translate(Scale(scale), offset);
    R_Pass3D *pass3D      = R_GetPassFromKind(R_PassType_3D)->pass3D;
    R_Batch3DGroup *group = &pass3D->groups[R_Primitive_Cube];
    R_PrimitiveInst *inst = (R_PrimitiveInst *)R_BatchListPush(&group->batchList, 256);
    inst->color           = color;
    inst->transform       = transform;
}

internal void DrawBox(Rect3 rect, V4 color)
{
    V3 offset = (rect.minP + rect.maxP) / 2;
    V3 scale  = rect.maxP - offset;
    DrawBox(offset, scale, color);
}

internal void DrawSphere(V3 offset, f32 radius, V4 color)
{
    Mat4 transform        = Translate(Scale(radius), offset);
    R_Pass3D *pass3D      = R_GetPassFromKind(R_PassType_3D)->pass3D;
    R_Batch3DGroup *group = &pass3D->groups[R_Primitive_Sphere];
    R_PrimitiveInst *inst = (R_PrimitiveInst *)R_BatchListPush(&group->batchList, 256);
    inst->color           = color;
    inst->transform       = transform;
}

// internal void DebugDrawSkeleton(AS_Handle model, Mat4 transform, Mat4 *skinningMatrices, b32 showAxes = 0)
// {
//     LoadedSkeleton *skeleton = GetSkeletonFromModel(model);
//     loopi(0, skeleton->count)
//     {
//         u32 parentId = skeleton->parents[i];
//         if (parentId != -1)
//         {
//             Mat4 bindPoseMatrix = Inverse(skeleton->inverseBindPoses[i]);
//             V3 childTranslation = GetTranslation(skinningMatrices[i] * bindPoseMatrix);
//             V3 childPoint       = transform * childTranslation;
//
//             V3 parentTranslation =
//                 GetTranslation(skinningMatrices[parentId] * Inverse(skeleton->inverseBindPoses[parentId]));
//             V3 parentPoint = transform * parentTranslation;
//             DrawLine(childPoint, parentPoint, Color_Green);
//
//             if (showAxes)
//             {
//                 V3 axisPoint = Normalize(bindPoseMatrix.columns[0].xyz) + childPoint;
//                 DrawArrow(childPoint, axisPoint, Color_Red, .2f);
//
//                 axisPoint = Normalize(bindPoseMatrix.columns[1].xyz) + childPoint;
//                 DrawArrow(childPoint, axisPoint, Color_Green, .2f);
//
//                 axisPoint = Normalize(bindPoseMatrix.columns[2].xyz) + childPoint;
//                 DrawArrow(childPoint, axisPoint, Color_Blue, .2f);
//             }
//         }
//     }
// }

internal b32 IsAligned(u8 *ptr, i32 alignment)
{
    b32 result = (u64)ptr == AlignPow2((u64)ptr, alignment);
    return result;
}

#if 0
internal void R_SwapFrameData()
{
    RenderState *state                       = engine->GetRenderState();
    R_Command *currentHead                   = state->renderFrameState.currentFrameData->head;
    state->head                              = currentHead;
    u32 frameIndex                           = ++state->renderFrameState.currentFrame % cFrameDataNumFrames;
    state->renderFrameState.currentFrameData = &state->renderFrameState.frameData[frameIndex];

    u64 start = AlignPow2((u64)state->renderFrameState.currentFrameData->memory, 16) -
                (u64)state->renderFrameState.currentFrameData->memory;
    // TODO: barrier?
    state->renderFrameState.currentFrameData->allocated = (i32)start;

    R_FrameData *data = state->renderFrameState.currentFrameData;
    data->head = data->tail = (R_Command *)R_FrameAlloc(sizeof(R_Command));
    data->head->type        = R_CommandType_Null;
    data->head->next        = 0;
}

internal void *R_CreateCommand(i32 size)
{
    RenderState *state                                   = engine->GetRenderState();
    R_Command *command                                   = (R_Command *)R_FrameAlloc(size);
    command->next                                        = 0;
    state->renderFrameState.currentFrameData->tail->next = &command->type;
    state->renderFrameState.currentFrameData->tail       = command;
    return (void *)command;
}
#endif

//////////////////////////////
// Lights
//
// Render to depth buffer from the perspective of the light, test vertices against this depth buffer
// to see whether object is in shadow.
// Returns splits cascade distances, and splits + 1 matrices
#if 0
internal void D_PushLight(Light *light)
{
    R_PassMesh *pass = R_GetPassFromKind(R_PassType_Mesh)->passMesh;

    ViewLight *viewLight = (ViewLight *)R_FrameAlloc(sizeof(ViewLight));
    viewLight->type      = light->type;
    viewLight->dir       = light->dir;

    viewLight->next = pass->viewLight;
    pass->viewLight = viewLight;
}
#endif

internal void R_CullModelsToLight(ViewLight *light)
{
    RenderState *renderState = engine->GetRenderState();
    // for now
    Assert(light->type == LightType_Directional);

    // V3 globalLightPos    = renderState->camera.position + light->dir * 100000.f;
    // Mat4 lightViewMatrix = LookAt4(globalLightPos, renderState->camera.position, renderState->camera.forward);

    // TODO: the inverse view projection and the frustum corners in world space should be precomputed

    // world space frustum corners
    // R_GetFrustumCorners(inverseViewProjectionMatrix, BoundsUnitCube, renderState->mFrustumCorners.corners);

    // for (i32 i = 0; i < ArrayLength(frustumCorners); i++)
    // {
    //     // convert to view space of the light
    //     frustumCorners[i] = lightViewMatrix * frustumCorners[i];
    // }

    // 3 ways of doing this
    // 1. put ray into clip space, put model aabb into clip space, minkowski add aabb to unit cube (????)
    // then cast ray from center of the aabb towards the unit cube + aabb bounding box. im not sure about this one.
    // 2. alternatively find bounds of view frustum in world space (aabb), add the model bounds to these
    // frustum bounds. cast a ray from the center of the model and compare with the aabb to see if it intersects.
    // this test is lossy.
    // 3. project frustum corners to the view space of the light (faked by having a global position far int he distance).
    // find the orthographic from this. then, for each prospective model, multtply by the m v p matrix of the light,
    // and cull (same as view frustum culling func i already have).

    // i am going to try 2

    // another thing:

    V3 *frustumCorners = renderState->mFrustumCorners.corners;
    Rect3 frustumBounds;
    Init(&frustumBounds);
    for (i32 i = 0; i < ArrayLength(renderState->mFrustumCorners.corners); i++)
    {
        V3 *point          = &frustumCorners[i];
        frustumBounds.minX = Min(frustumBounds.minX, point->x);
        frustumBounds.minY = Min(frustumBounds.minX, point->y);
        frustumBounds.minZ = Min(frustumBounds.minX, point->z);

        frustumBounds.maxX = Max(frustumBounds.maxX, point->x);
        frustumBounds.maxY = Max(frustumBounds.maxY, point->y);
        frustumBounds.maxZ = Max(frustumBounds.maxZ, point->z);
    }

    R_PassMesh *pass = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
    // TODO: this is kind of dumb, because it means that a model can only have one shadow node. however, I will not
    // think about the future :)
    for (R_MeshParamsNode **node = &pass->list.first; *node != 0;)
    {
        R_MeshParamsNode *currentNode = *node;
        // If the node is directly visible in the view frusta, skip
        if (currentNode->val.mIsDirectlyVisible)
        {
            node = &currentNode->next;
            continue;
        }

        // returns the x, y, z extents from the center
        // TODO: use these to find the bounds of the sunlight for this frame
        V3 extents = GetExtents(currentNode->val.mBounds);
        V3 center  = GetCenter(currentNode->val.mBounds);

        Rect3 minkowskiFrustumBounds;
        minkowskiFrustumBounds.minX = frustumBounds.minX - extents.x;
        minkowskiFrustumBounds.minY = frustumBounds.minY - extents.y;
        minkowskiFrustumBounds.minZ = frustumBounds.minZ - extents.z;

        minkowskiFrustumBounds.maxX = frustumBounds.maxX + extents.x;
        minkowskiFrustumBounds.maxY = frustumBounds.maxY + extents.y;
        minkowskiFrustumBounds.maxZ = frustumBounds.maxZ + extents.z;

        Ray ray;
        ray.mStartP = center;
        ray.mDir    = -light->dir;

        // TODO: this could be done by minkowski adding with the frustum bounds themselves,
        // but they would have to be sorted or something
        f32 tMin;
        V3 point;

        pass->list.mTotalSurfaceCount -= currentNode->val.numSurfaces;
        R_MeshParamsNode *next = currentNode->next;

        // Only the node's shadow appears in the frustum, link with light
        if (IntersectRayAABB(ray, minkowskiFrustumBounds, tMin, point))
        {
            currentNode->next = light->modelNodes;
            light->modelNodes = currentNode;
            light->mNumShadowSurfaces += currentNode->val.numSurfaces;
        }
        // Remove from the main draw list
        *node = next;
    }
}

// TODO: frustum culling cuts out fragments that cast shadows. use the light's frustum to cull
// also restrict the bounds to more tightly enclose the bounds of the objects (for better quality shadows)
internal void R_CascadedShadowMap(const ViewLight *inLight, Mat4 *outLightViewProjectionMatrices,
                                  f32 *outCascadeDistances)
{
    RenderState *renderState   = engine->GetRenderState();
    Mat4 inverseViewProjection = Inverse(renderState->transform);
    f32 zNear                  = renderState->nearZ;
    f32 zFar                   = renderState->farZ;

    f32 cascadeDistances[cNumCascades];

    f32 range  = zFar - zNear;
    f32 ratio  = zFar / zNear;
    f32 lambda = 0.8f;
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    for (u32 i = 0; i < cNumCascades; i++)
    {
        f32 p               = (f32)(i + 1) / cNumCascades;
        f32 uniform         = zNear + range * p;
        f32 log             = zNear * Powf(ratio, p);
        cascadeDistances[i] = lambda * log + (1 - lambda) * uniform;
    }
    // Mat4 mvpMatrices[cNumCascades];
    MemoryCopy(outCascadeDistances, cascadeDistances, sizeof(cascadeDistances));
    // R_ShadowMapFrusta(cNumSplits, .9f, mvpMatrices, cascadeDistances);
    Assert(inLight->type == LightType_Directional);

    // V3 worldUp     = renderState->camera.right; //{0, 0, 1};
    Mat4 lightView = LookAt4({}, -inLight->dir, {0, 1, 0});
    // Step 0. Set up the mvp matrix for each frusta.

    // Step 1. Get the corners of each of the view frusta in world space.
    V3 corners[8] = {
        {-1, -1, 0},
        {-1, -1, 1},
        {1, -1, 0},
        {1, -1, 1},
        {1, 1, 0},
        {1, 1, 1},
        {-1, 1, 0},
        {-1, 1, 1},
    };

    // Map to world space
    for (i32 i = 0; i < ArrayLength(corners); i++)
    {
        V4 result  = inverseViewProjection * MakeV4(corners[i], 1.f);
        corners[i] = result.xyz / result.w;
    }

    for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    {
        V3 frustumVertices[8];

        // Find the corners of each split
        f32 nearStep = cascadeIndex == 0 ? 0.f : (cascadeDistances[cascadeIndex - 1] - zNear) / (zFar - zNear);
        f32 farStep  = (cascadeDistances[cascadeIndex] - zNear) / (zFar - zNear);

        for (i32 i = 0; i < ArrayLength(corners); i += 2)
        {
            frustumVertices[i]     = Lerp(corners[i], corners[i + 1], nearStep);
            frustumVertices[i + 1] = Lerp(corners[i], corners[i + 1], farStep);
        }

        // Find the center of each split
        // Find the radii of each cascade
        V3 center  = {};
        f32 radius = -FLT_MAX;

        for (i32 i = 0; i < ArrayLength(frustumVertices); i++)
        {
            center += frustumVertices[i];
        }
        center /= 8;
        for (i32 i = 0; i < ArrayLength(frustumVertices); i++)
        {
            radius = Max(Length(frustumVertices[i] - center), radius);
        }

        // Snap to shadow map texel size (prevents shimmering)
        Rect3 aabb;
        V3 extents = MakeV3(radius);
        aabb.minP  = center - extents;
        aabb.maxP  = center + extents;

        f32 texelsPerUnit = 1024.f / (2.f * radius); //(aabb.maxP - aabb.minP); // 2 * radius
        aabb.minP         = Floor(aabb.minP * texelsPerUnit) / texelsPerUnit;
        aabb.maxP         = Floor(aabb.maxP * texelsPerUnit) / texelsPerUnit;
        center            = (aabb.minP + aabb.maxP) / 2.f;

        // f32 ext   = Abs(center.z - aabb.minP.z);
        // ext       = Max(ext, Min(1500.f, zFar) / 2.f);
        // aabb.minZ = center.z - ext;
        // aabb.maxZ = center.z + ext;
        // center    = (aabb.minP + aabb.maxP) / 2.f;

        f32 zRange            = aabb.maxZ - aabb.minZ;
        Mat4 updatedLightView = LookAt4(center + inLight->dir * -aabb.minZ, center, {0, 1, 0});

        outLightViewProjectionMatrices[cascadeIndex] =
            // Orthographic4(aabb.minX, aabb.maxX, aabb.minY, aabb.maxY, aabb.minZ, aabb.maxZ) * lightView; // aabb.minZ, aabb.maxZ) * lightView;
            Orthographic4(-extents.x, extents.x, -extents.y, extents.y, 0, zRange) * updatedLightView;
    }
}

using namespace graphics;
using namespace scene;
namespace render
{
enum InputLayoutTypes
{
    IL_Type_MeshVertex,
    IL_Type_Count,
};

Arena *arena;
Swapchain swapchain;
PipelineState pipelineState;
PipelineState shadowMapPipeline;
PipelineState blockCompressPipeline;
PipelineState skinPipeline;
PipelineState triangleCullPipeline;
PipelineState clearIndirectPipeline;
PipelineState compactionPipeline;
PipelineState instanceCullPass1Pipeline;
PipelineState instanceCullPass2Pipeline;
PipelineState clusterCullPipeline;
PipelineState dispatchPrepPipeline;
PipelineState generateMipsPipeline;

// TODO: fold the below into here
// this should probably just be a linked list
global FrameAllocation uploads[UploadType_Count][4];
global u64 currentOffsets[UploadType_Count];
global u32 numUploads[UploadType_Count];
graphics::GPUBuffer buffers[UploadType_Count];

// Buffers
// TODO: these individual upload buffers should just not exist. should be able to just cull a function with a
// size to get a gpu buffer you can write to. would be easier to manage as well
graphics::GPUBuffer cascadeParamsBuffer;
graphics::GPUBuffer skinningBufferUpload[device->cNumBuffers];
graphics::GPUBuffer skinningBuffer;
graphics::GPUBuffer meshParamsBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshParamsBuffer;
graphics::GPUBuffer meshGeometryBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshGeometryBuffer;
graphics::GPUBuffer materialBufferUpload[device->cNumBuffers];
graphics::GPUBuffer materialBuffer;

// GPU Culling / Indirect
graphics::GPUBuffer indirectScratchBuffer;
graphics::GPUBuffer meshIndirectBuffer;
graphics::GPUBuffer meshIndirectCountBuffer;
graphics::GPUBuffer meshIndexBuffer;
graphics::GPUBuffer meshChunkBuffer;
graphics::GPUBuffer dispatchIndirectBuffer;
graphics::GPUBuffer viewsBuffer;
graphics::GPUBuffer meshClusterIndexBuffer;
graphics::GPUBuffer occludedInstanceBuffer;

graphics::GPUBuffer cullingStatisticsBuffer;
graphics::GPUBuffer debugAABBs;

u32 skinningBufferSize;
u32 meshParamsBufferSize;
u32 meshGeometryBufferSize;
u32 materialBufferSize;
u32 meshIndirectBufferSize;

u32 meshClusterCount;

Sampler samplerMax;
InputLayout inputLayouts[IL_Type_Count];
global Shader shaders[ShaderType_Count];
RasterizationState rasterizers[RasterType_Count];

Texture mainColorAttachment;
Texture depthBufferMain;
Texture shadowDepthMap;
list<i32> shadowSlices;
i32 depthPyramidSubresources[16];

Texture depthPyramid;

struct DeferredBlockCompressCmd
{
    Texture in;
    Texture out;
};

// Deferred block read
DeferredBlockCompressCmd blockCompressRing[256];
std::atomic<u64> volatile blockCompressWrite       = 0;
std::atomic<u64> volatile blockCompressCommitWrite = 0;
u64 blockCompressRead                              = 0;

// Per frame allocations
struct FrameData
{
    u8 *memory;
    std::atomic<u32> allocated;
    u32 totalSize;
};

u8 currentFrame;
u8 currentBuffer;
FrameData frameData[2];

rendergraph::RenderGraph renderGraph;

rendergraph::ResourceHandle depthPyramidResourceHandle;

internal void *
FrameAlloc(i32 size)
{
    i32 alignedSize   = AlignPow2(size, 16);
    FrameData *data   = &frameData[currentBuffer];
    u32 currentOffset = data->allocated.fetch_add(alignedSize);

    u8 *ptr = data->memory + currentOffset;
    MemoryZero(ptr, size);

    return ptr;
}

internal void Initialize()
{
    arena = ArenaAlloc();
    // Initialize shaders
    {
        shaders[ShaderType_Mesh_VS].name                 = "mesh_vs.hlsl";
        shaders[ShaderType_Mesh_VS].stage                = ShaderStage::Vertex;
        shaders[ShaderType_Mesh_FS].name                 = "mesh_fs.hlsl";
        shaders[ShaderType_Mesh_FS].stage                = ShaderStage::Fragment;
        shaders[ShaderType_ShadowMap_VS].name            = "depth_vs.hlsl";
        shaders[ShaderType_ShadowMap_VS].stage           = ShaderStage::Vertex;
        shaders[ShaderType_BC1_CS].name                  = "blockcompress_cs.hlsl";
        shaders[ShaderType_BC1_CS].stage                 = ShaderStage::Compute;
        shaders[ShaderType_Skin_CS].name                 = "skinning_cs.hlsl";
        shaders[ShaderType_Skin_CS].stage                = ShaderStage::Compute;
        shaders[ShaderType_TriangleCull_CS].name         = "cull_triangle_cs.hlsl";
        shaders[ShaderType_TriangleCull_CS].stage        = ShaderStage::Compute;
        shaders[ShaderType_ClearIndirect_CS].name        = "clear_indirect_cs.hlsl";
        shaders[ShaderType_ClearIndirect_CS].stage       = ShaderStage::Compute;
        shaders[ShaderType_DrawCompaction_CS].name       = "draw_compaction_cs.hlsl";
        shaders[ShaderType_DrawCompaction_CS].stage      = ShaderStage::Compute;
        shaders[ShaderType_InstanceCullPass1_CS].name    = "cull_instance_cs.hlsl";
        shaders[ShaderType_InstanceCullPass1_CS].stage   = ShaderStage::Compute;
        shaders[ShaderType_InstanceCullPass1_CS].outName = "cull_instance_pass1_cs";
        shaders[ShaderType_InstanceCullPass2_CS].name    = "cull_instance_cs.hlsl";
        shaders[ShaderType_InstanceCullPass2_CS].stage   = ShaderStage::Compute;
        shaders[ShaderType_InstanceCullPass2_CS].outName = "cull_instance_pass2_cs";
        shaders[ShaderType_ClusterCull_CS].name          = "cull_cluster_cs.hlsl";
        shaders[ShaderType_ClusterCull_CS].stage         = ShaderStage::Compute;
        shaders[ShaderType_DispatchPrep_CS].name         = "dispatch_prep_cs.hlsl";
        shaders[ShaderType_DispatchPrep_CS].stage        = ShaderStage::Compute;
        shaders[ShaderType_GenerateMips_CS].name         = "generate_mips_cs.hlsl";
        shaders[ShaderType_GenerateMips_CS].stage        = ShaderStage::Compute;
    }

    // Compile shaders
    jobsystem::Counter counter;
    {
        shadercompiler::InitShaderCompiler();

        jobsystem::KickJobs(&counter, ShaderType_Count, 2, [&](jobsystem::JobArgs args) {
            shadercompiler::CompileInput input = {};
            input.shaderName                   = shaders[args.jobId].name;
            input.stage                        = shaders[args.jobId].stage;
            input.outName                      = shaders[args.jobId].outName;

            shadercompiler::CompileOutput output;

            TempArena temp = ScratchStart(0, 0);
            switch (args.jobId)
            {
                case ShaderType_InstanceCullPass1_CS:
                {
                    input.defines        = PushStruct(temp.arena, shadercompiler::CompileInput::ShaderDefine);
                    input.defines[0].val = PushStr8F(temp.arena, "PASS = %u", FIRST_PASS);
                    input.numDefines     = 1;
                }
                break;
                case ShaderType_InstanceCullPass2_CS:
                {
                    input.defines        = PushStruct(temp.arena, shadercompiler::CompileInput::ShaderDefine);
                    input.defines[0].val = PushStr8F(temp.arena, "PASS = %u", SECOND_PASS);
                    input.numDefines     = 1;
                }
                break;
                default: break;
            }
            shadercompiler::CompileShader(temp.arena, &input, &output);
            device->CreateShader(&shaders[args.jobId], output.shaderData);
            if (args.jobId == ShaderType_ClusterCull_CS)
            {
                device->AddPCTemp(&shaders[args.jobId], 0, sizeof(ClusterCullPushConstants));
            }
            ScratchEnd(temp);
        });
    }

    // Initialize frame data
    {
        currentFrame  = 0;
        currentBuffer = 0;
        for (u32 i = 0; i < ArrayLength(frameData); i++)
        {
            FrameData *data = &frameData[i];
            data->totalSize = megabytes(8);
            data->memory    = PushArray(arena, u8, data->totalSize);
        }
    }
    // Initialize swap chain
    {
        SwapchainDesc desc;
        desc.width  = (u32)platform.GetWindowDimension(shared->windowHandle).x;
        desc.height = (u32)platform.GetWindowDimension(shared->windowHandle).y;
        desc.format = graphics::Format::R8G8B8A8_SRGB;

        device->CreateSwapchain((Window)shared->windowHandle.handle, &desc, &swapchain);
    }

    // Initialize samplers
    {
        SamplerDesc desc;
        desc.min           = Filter::Linear;
        desc.mag           = Filter::Linear;
        desc.mipMode       = Filter::Nearest;
        desc.mode          = SamplerMode::ClampToEdge;
        desc.maxLod        = 16.f;
        desc.reductionMode = ReductionMode::Max;
        device->CreateSampler(&samplerMax, desc);
    }

    // Initialize buffers
    // TODO IMPORTANT: automate the creation of all temporary shader resources (i.e. resources that are used every frame),
    // and automate binding, barriers, etc.
    {
        skinningBufferSize     = 0;
        meshParamsBufferSize   = 0;
        meshGeometryBufferSize = 0;
        materialBufferSize     = 0;
        meshIndirectBufferSize = 0;
        meshClusterCount       = 0;

        TempArena temp = ScratchStart(0, 0);
        GPUBufferDesc desc;
        desc.size          = kilobytes(64);
        desc.resourceUsage = ResourceUsage_UniformBuffer;
        device->CreateBuffer(&cascadeParamsBuffer, desc, 0);

        // Skinning
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::CPU_TO_GPU;
        desc.resourceUsage = ResourceUsage_TransferSrc;
        for (u32 i = 0; i < ArrayLength(skinningBufferUpload); i++)
        {
            device->CreateBuffer(&skinningBufferUpload[i], desc, 0);
            device->SetName(&skinningBufferUpload[i], "Skinning upload buffer");
        }

        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_ShaderGlobals;
        device->CreateBuffer(&skinningBuffer, desc, 0);
        device->SetName(&skinningBuffer, "Skinning buffer");

        // Mesh params
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::CPU_TO_GPU;
        desc.resourceUsage = ResourceUsage_TransferSrc;
        for (u32 i = 0; i < ArrayLength(meshParamsBufferUpload); i++)
        {
            device->CreateBuffer(&meshParamsBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh params upload buffer %i", i);
            device->SetName(&meshParamsBufferUpload[i], name);
        }

        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_ShaderGlobals;
        device->CreateBuffer(&meshParamsBuffer, desc, 0);
        device->SetName(&meshParamsBuffer, "Mesh params buffer");

        // Mesh geometry
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::CPU_TO_GPU;
        desc.resourceUsage = ResourceUsage_TransferSrc;

        for (u32 i = 0; i < ArrayLength(meshGeometryBufferUpload); i++)
        {
            device->CreateBuffer(&meshGeometryBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh geometry upload buffer %i", i);
            device->SetName(&meshGeometryBufferUpload[i], name);
        }

        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_ShaderGlobals;
        device->CreateBuffer(&meshGeometryBuffer, desc, 0);
        device->SetName(&meshGeometryBuffer, "Mesh geometry buffer");

        // Material buffer
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::CPU_TO_GPU;
        desc.resourceUsage = ResourceUsage_TransferSrc;

        for (u32 i = 0; i < ArrayLength(materialBufferUpload); i++)
        {
            device->CreateBuffer(&materialBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Material upload buffer %i", i);
            device->SetName(&materialBufferUpload[i], name);
        }

        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_ShaderGlobals;
        device->CreateBuffer(&materialBuffer, desc, 0);
        device->SetName(&materialBuffer, "Material buffer");

        // Indirect buffer
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_StorageBufferRead | ResourceUsage_Indirect;
        device->CreateBuffer(&meshIndirectBuffer, desc, 0);
        device->SetName(&meshIndirectBuffer, "Mesh indirect buffer");

        device->CreateBuffer(&indirectScratchBuffer, desc, 0);
        device->SetName(&indirectScratchBuffer, "Indirect scratch buffer");

        // Indirect count buffer
        desc               = {};
        desc.size          = sizeof(uint);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_StorageBufferRead | ResourceUsage_Indirect | ResourceUsage_TransferDst;
        device->CreateBuffer(&meshIndirectCountBuffer, desc, 0);
        device->SetName(&meshIndirectCountBuffer, "Mesh indirect count buffer");

        // Mesh index buffer
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_IndexBuffer;
        device->CreateBuffer(&meshIndexBuffer, desc, 0);
        device->SetName(&meshIndexBuffer, "Mesh index buffer");

        // Temp mesh chunk buffer
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_StorageBufferRead;
        device->CreateBuffer(&meshChunkBuffer, desc, 0);
        device->SetName(&meshChunkBuffer, "Mesh chunk buffer");

        // Chunk dispatch indirect buffer
        desc               = {};
        desc.size          = sizeof(DispatchIndirect) * NUM_DISPATCH_OFFSETS;
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_StorageBufferRead | ResourceUsage_Indirect | ResourceUsage_TransferDst;
        device->CreateBuffer(&dispatchIndirectBuffer, desc, 0);
        device->SetName(&dispatchIndirectBuffer, "Dispatch indirect buffer");

        // Cluster buffer
        desc               = {};
        desc.size          = kilobytes(16);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_Bindless | ResourceUsage_StorageBufferRead | ResourceUsage_TransferDst | ResourceUsage_TransferSrc;
        device->CreateBuffer(&buffers[UploadType_MeshClusters], desc, 0);
        device->SetName(&buffers[UploadType_MeshClusters], "Mesh Cluster Buffer");

        // Cluster index buffer (use after culling)
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer | ResourceUsage_StorageBufferRead;
        device->CreateBuffer(&meshClusterIndexBuffer, desc, 0);
        device->SetName(&meshClusterIndexBuffer, "Mesh Cluster Index Buffer");

        // Views buffer
        desc               = {};
        desc.size          = kilobytes(4);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBufferRead | ResourceUsage_TransferDst;
        device->CreateBuffer(&viewsBuffer, desc, 0);
        device->SetName(&viewsBuffer, "Views Buffer");

        // Occluded instance buffer
        // TODO: this needs to be able to grow
        desc               = {};
        desc.size          = kilobytes(16);
        desc.usage         = MemoryUsage::GPU_ONLY;
        desc.resourceUsage = ResourceUsage_StorageBuffer;
        device->CreateBuffer(&occludedInstanceBuffer, desc, 0);
        device->SetName(&occludedInstanceBuffer, "Occluded Instance Buffer");

        desc               = {};
        desc.size          = AlignPow2(sizeof(CullingStatistics), 16);
        desc.usage         = MemoryUsage::GPU_TO_CPU;
        desc.resourceUsage = ResourceUsage_StorageBuffer;
        device->CreateBuffer(&cullingStatisticsBuffer, desc, 0);
        device->SetName(&cullingStatisticsBuffer, "Culling Statistics Buffer");

        desc               = {};
        desc.size          = kilobytes(32);
        desc.usage         = MemoryUsage::GPU_TO_CPU;
        desc.resourceUsage = ResourceUsage_StorageBuffer;
        device->CreateBuffer(&debugAABBs, desc, 0);
        device->SetName(&debugAABBs, "Debug AABBs Buffer");
    }

    // Initialize render targets/depth buffers
    {
        TextureDesc desc;
        desc.width        = swapchain.desc.width;
        desc.height       = swapchain.desc.height;
        desc.initialUsage = ResourceUsage_DepthStencil;
        desc.futureUsages = ResourceUsage_SampledImage;
        desc.format       = Format::D32_SFLOAT;

        device->CreateTexture(&depthBufferMain, desc, 0);
        device->SetName(&depthBufferMain, "Main Depth Buffer");

        desc              = {};
        desc.width        = swapchain.desc.width;
        desc.height       = swapchain.desc.height;
        desc.initialUsage = ResourceUsage_ColorAttachment;
        desc.futureUsages = ResourceUsage_TransferSrc;
        desc.format       = swapchain.desc.format;

        device->CreateTexture(&mainColorAttachment, desc, 0);
        device->SetName(&mainColorAttachment, "Main Color Attachment");

        // Shadows
        desc.width        = 1024;
        desc.height       = 1024;
        desc.initialUsage = ResourceUsage_SampledImage; // TODO: this doesn't seem correct
        desc.futureUsages = ResourceUsage_DepthStencil;
        desc.format       = Format::D32_SFLOAT;
        desc.numLayers    = cNumCascades;
        desc.sampler      = TextureDesc::DefaultSampler::Nearest;
        desc.textureType  = TextureDesc::TextureType::Texture2DArray;

        device->CreateTexture(&shadowDepthMap, desc, 0);
        device->SetName(&shadowDepthMap, "Cascaded Shadow Depth Map");

        for (u32 i = 0; i < shadowDepthMap.desc.numLayers; i++)
        {
            shadowSlices.push_back(device->CreateSubresource(&shadowDepthMap, i, 1));
        }

        desc              = {};
        desc.width        = (u32)GetPreviousPowerOfTwo(swapchain.desc.width);
        desc.height       = (u32)GetPreviousPowerOfTwo(swapchain.desc.height);
        desc.initialUsage = ResourceUsage_SampledImage;
        desc.futureUsages = ResourceUsage_StorageImage;
        desc.format       = Format::R32_SFLOAT;
        desc.numMips      = GetNumMips(desc.width, desc.height);

        device->CreateTexture(&depthPyramid, desc, 0);
        device->SetName(&depthPyramid, "HZB Depth Pyramid");

        for (u32 i = 0; i < depthPyramid.desc.numMips; i++)
        {
            depthPyramidSubresources[i] = device->CreateSubresource(&depthPyramid, 0, ~0u, i, 1);
        }
        depthPyramidResourceHandle = renderGraph.Import("depthPyramid", &depthPyramid);
    }

    // Initialize rasterization state
    {
        rasterizers[RasterType_CCW_CullBack].cullMode  = RasterizationState::CullMode::Back;
        rasterizers[RasterType_CCW_CullFront].cullMode = RasterizationState::CullMode::Front;
        rasterizers[RasterType_CCW_CullNone].cullMode  = RasterizationState::CullMode::None;
    }

    // Initialize pipelines
    {
        jobsystem::WaitJobs(&counter);
        InputLayout &inputLayout = inputLayouts[IL_Type_MeshVertex];
        inputLayout.elements     = {
            Format::R32G32B32_SFLOAT, Format::R32G32B32_SFLOAT, Format::R32G32_SFLOAT, Format::R32G32B32_SFLOAT,
            Format::R32G32B32A32_UINT, Format::R32G32B32A32_SFLOAT};

        inputLayout.binding = 0;
        inputLayout.stride  = sizeof(MeshVertex);
        inputLayout.rate    = InputRate::Vertex;

        // Main
        PipelineStateDesc desc     = {};
        desc.depthStencilFormat    = Format::D32_SFLOAT;
        desc.colorAttachmentFormat = Format::R8G8B8A8_SRGB;
        desc.vs                    = &shaders[ShaderType_Mesh_VS];
        desc.fs                    = &shaders[ShaderType_Mesh_FS];
        // desc.rasterState           = &rasterizers[RasterType_CCW_CullBack];
        desc.rasterState = &rasterizers[RasterType_CCW_CullNone];
        device->CreatePipeline(&desc, &pipelineState, "Main pass");

        // Shadows
        desc                    = {};
        desc.depthStencilFormat = Format::D32_SFLOAT;
        desc.vs                 = &shaders[ShaderType_ShadowMap_VS];
        desc.rasterState        = &rasterizers[RasterType_CCW_CullNone];
        device->CreatePipeline(&desc, &shadowMapPipeline, "Depth pass");

        // Block compress compute
        desc         = {};
        desc.compute = &shaders[ShaderType_BC1_CS];
        device->CreateComputePipeline(&desc, &blockCompressPipeline, "Block compress");

        // Skinning Compute
        desc.compute = &shaders[ShaderType_Skin_CS];
        device->CreateComputePipeline(&desc, &skinPipeline, "Skinning compute");

        // Triangle culling compute
        desc.compute = &shaders[ShaderType_TriangleCull_CS];
        device->CreateComputePipeline(&desc, &triangleCullPipeline, "Triangle culling compute");

        // Clear indirect
        desc.compute = &shaders[ShaderType_ClearIndirect_CS];
        device->CreateComputePipeline(&desc, &clearIndirectPipeline, "Clear indirect compute");

        // Draw call compaction
        desc.compute = &shaders[ShaderType_DrawCompaction_CS];
        device->CreateComputePipeline(&desc, &compactionPipeline, "Draw compaction compute");

        // Instance cull
        desc.compute = &shaders[ShaderType_InstanceCullPass1_CS];
        device->CreateComputePipeline(&desc, &instanceCullPass1Pipeline, "Instance Cull Pass 1 Pipeline");

        desc.compute = &shaders[ShaderType_InstanceCullPass2_CS];
        device->CreateComputePipeline(&desc, &instanceCullPass2Pipeline, "Instance Cull Pass 2 Pipeline");

        // Cluster cull
        desc.compute = &shaders[ShaderType_ClusterCull_CS];
        device->CreateComputePipeline(&desc, &clusterCullPipeline, "Cluster cull compute");

        // Dispatch prep
        desc.compute = &shaders[ShaderType_DispatchPrep_CS];
        device->CreateComputePipeline(&desc, &dispatchPrepPipeline, "Dispatch prep compute");

        // Dispatch prep
        desc.compute = &shaders[ShaderType_GenerateMips_CS];
        device->CreateComputePipeline(&desc, &generateMipsPipeline, "Generate Mips Compute");
    }

    // Rendergraph testing
    {
        renderGraph.Init();
        //     rendergraph::RGClearIndirect *parametersIndirect = renderGraph.AllocParameters<rendergraph::RGClearIndirect>();
        //     const u32 indirectBufferSize                     = sizeof(DispatchIndirect) * NUM_DISPATCH_OFFSETS;
        //
        //     parametersIndirect->indirectCommands.handle = renderGraph.CreateBuffer("Buffer", indirectBufferSize);
        //     renderGraph.AddPass("Test pass 2", parametersIndirect, rendergraph::PassFlags::Indirect, [&](CommandList cmd) {
        //         // device->BindCompute(&skinPipeline, cmd);
        //         // device->Dispatch(cmd, (mesh->vertexCount + SKINNING_GROUP_SIZE - 1) / SKINNING_GROUP_SIZE, 1, 1);
        //         Printf("Nothing! :>)\n");
        //     });
        //
        //     rendergraph::RGClearIndirect *parametersIndirect2 = renderGraph.AllocParameters<rendergraph::RGClearIndirect>();
        //     parametersIndirect2->indirectCommands.handle      = renderGraph.CreateBuffer("Buffer", indirectBufferSize);
        //     renderGraph.AddPass("Test pass 1", parametersIndirect2, rendergraph::PassFlags::Indirect, [&](CommandList cmd) {
        //         Printf("Nothing! :>)\n");
        //     });
        //
        //     renderGraph.Compile();
        //     // should print test pass 2, test pass 1
    }
}

enum RenderPassType
{
    RenderPassType_Main,
    RenderPassType_Shadow,
};

internal void DebugRectangle(CommandList cmdList)
{
}

internal void CullInstances(CommandList cmdList, bool isSecondPass)
{
    i32 meshClusterDescriptor  = device->GetDescriptorIndex(&buffers[UploadType_MeshClusters], ResourceViewType::SRV);
    i32 meshGeometryDescriptor = device->GetDescriptorIndex(&meshGeometryBuffer, ResourceViewType::SRV);
    i32 meshParamsDescriptor   = device->GetDescriptorIndex(&meshParamsBuffer, ResourceViewType::SRV);

    RenderState *renderState = engine->GetRenderState();
#if 1
    {
        GPUBarrier barriers[] = {
            // GPUBarrier::ComputeReadToWrite(&dispatchIndirectBuffer),
            GPUBarrier::ComputeReadToWrite(&meshChunkBuffer),
            GPUBarrier::ComputeReadToWrite(&occludedInstanceBuffer),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }
#endif
    // Instance frustum/occlusion culling
    {
        string passName;
        if (!isSecondPass)
        {
            passName = Str8Lit("Instance Cull First Pass");
            // device->BeginEvent(cmdList, "Instance Cull First Pass");
        }
        else
        {
            passName = Str8Lit("Instance Cull Second Pass");
            // device->BeginEvent(cmdList, "Instance Cull Second Pass");
        }
        rendergraph::RGCullInstance *cullInstance = renderGraph.AllocParameters<rendergraph::RGCullInstance>();
        cullInstance->depthPyramid.handle         = depthPyramidResourceHandle;
        cullInstance->views.handle                = renderGraph.CreateBuffer("viewsBuffer", kilobytes(4));
        cullInstance->occludedInstances.handle    = renderGraph.CreateBuffer("occludedInstances", kilobytes(16));

        u32 numInstances              = gameScene->meshes.GetTotal();
        InstanceCullPushConstants *pc = &cullInstance->push;
        pc->pyramidWidth              = (f32)depthPyramid.desc.width;
        pc->pyramidHeight             = (f32)depthPyramid.desc.height;
        pc->nearZ                     = renderState->nearZ;
        pc->farZ                      = renderState->farZ;
        pc->numInstances              = numInstances;
        pc->meshParamsDescriptor      = meshParamsDescriptor;
        pc->screenSize.x              = (u32)swapchain.desc.width;
        pc->screenSize.y              = (u32)swapchain.desc.height;

        if (!isSecondPass)
        {
            renderGraph.AddComputePass(
                "Instance Cull First Pass",
                cullInstance,
                &instanceCullPass1Pipeline,
                {(numInstances + INSTANCE_CULL_GROUP_SIZE - 1) / INSTANCE_CULL_GROUP_SIZE, 1, 1});
        }
        else
        {
            renderGraph.AddComputeIndirectPass("Instance Cull Second Pass",
                                               cullInstance,
                                               &instanceCullPass2Pipeline,
                                               &dispatchIndirectBuffer,
                                               INSTANCE_SECOND_PASS_DISPATCH_OFFSET);
        }
        device->PushConstants(cmdList, sizeof(pc), &pc);
        device->BindResource(&depthPyramid, ResourceViewType::SRV, 0, cmdList);
        device->BindResource(&viewsBuffer, ResourceViewType::SRV, 1, cmdList);
        device->BindResource(&dispatchIndirectBuffer, ResourceViewType::UAV, 0, cmdList);
        device->BindResource(&meshChunkBuffer, ResourceViewType::UAV, 1, cmdList);
        device->BindResource(&occludedInstanceBuffer, ResourceViewType::UAV, 2, cmdList);
#if 1
        device->BindResource(&cullingStatisticsBuffer, ResourceViewType::UAV, 3, cmdList);
        device->BindResource(&debugAABBs, ResourceViewType::UAV, 4, cmdList);
#endif
        device->EndEvent(cmdList);
    }

    {
        GPUBarrier barriers[] = {
            GPUBarrier::ComputeWriteToRead(&dispatchIndirectBuffer, ResourceUsage_None, ResourceUsage_Indirect),
            GPUBarrier::ComputeWriteToRead(&meshChunkBuffer),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }

    // Cluster culling
    {
        device->BeginEvent(cmdList, "Cluster Cull");
        ClusterCullPushConstants pc;
        pc.pyramidWidth          = (f32)depthPyramid.desc.width;
        pc.pyramidHeight         = (f32)depthPyramid.desc.height;
        pc.nearZ                 = renderState->nearZ;
        pc.farZ                  = renderState->farZ;
        pc.meshClusterDescriptor = device->GetDescriptorIndex(&buffers[UploadType_MeshClusters], ResourceViewType::SRV);
        device->BindCompute(&clusterCullPipeline, cmdList);
        device->PushConstants(cmdList, sizeof(pc), &pc);
        device->BindResource(&meshChunkBuffer, ResourceViewType::SRV, 0, cmdList);
        device->BindResource(&viewsBuffer, ResourceViewType::SRV, 1, cmdList);
        device->BindResource(&dispatchIndirectBuffer, ResourceViewType::UAV, 0, cmdList);
        device->BindResource(&meshClusterIndexBuffer, ResourceViewType::UAV, 1, cmdList);
        // device->Dispatch(cmdList, (meshClusterCount + CLUSTER_CULL_GROUP_SIZE - 1) / CLUSTER_CULL_GROUP_SIZE, 1, 1);
        device->DispatchIndirect(cmdList, &dispatchIndirectBuffer,
                                 CLUSTER_DISPATCH_OFFSET * sizeof(DispatchIndirect) + Offset(DispatchIndirect, groupCountX));
        device->EndEvent(cmdList);
    }

    {
        GPUBarrier barriers[] = {
            GPUBarrier::Buffer(&dispatchIndirectBuffer, ResourceUsage_ComputeWrite, ResourceUsage_Indirect | ResourceUsage_ComputeRead),
            GPUBarrier::ComputeWriteToRead(&meshClusterIndexBuffer),
            GPUBarrier::ComputeWriteToRead(&indirectScratchBuffer, ResourceUsage_None, ResourceUsage_ComputeWrite),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }

    // Triangle culling
    {
        device->BeginEvent(cmdList, "Triangle Culling");
        TriangleCullPushConstant pc;
        pc.worldToClip            = renderState->transform;
        pc.meshGeometryDescriptor = meshGeometryDescriptor;
        pc.meshParamsDescriptor   = meshParamsDescriptor;
        pc.meshClusterDescriptor  = meshClusterDescriptor;
        pc.screenWidth            = (u32)swapchain.desc.width;
        pc.screenHeight           = (u32)swapchain.desc.height;
        pc.nearZ                  = renderState->nearZ;
        device->BindCompute(&triangleCullPipeline, cmdList);
        device->PushConstants(cmdList, sizeof(pc), &pc);
        device->BindResource(&meshClusterIndexBuffer, ResourceViewType::SRV, 0, cmdList);
        device->BindResource(&indirectScratchBuffer, ResourceViewType::UAV, 0, cmdList);
        device->BindResource(&meshIndexBuffer, ResourceViewType::UAV, 1, cmdList);
        // device->BindResource(&depthPyramid, ResourceViewType::SRV, 0, cmdList);
        device->DispatchIndirect(cmdList, &dispatchIndirectBuffer,
                                 TRIANGLE_DISPATCH_OFFSET * sizeof(DispatchIndirect) + Offset(DispatchIndirect, groupCountX));
        device->EndEvent(cmdList);
    }

    {
        GPUBarrier barriers[] = {
            GPUBarrier::ComputeWriteToRead(&indirectScratchBuffer),
            GPUBarrier::Buffer(&meshIndirectCountBuffer, ResourceUsage_ComputeWrite,
                               ResourceUsage_ComputeWrite | ResourceUsage_ComputeRead),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }

    // Draw compaction
    {
        device->BeginEvent(cmdList, "Draw Compaction");

        // DrawCompactionPushConstant pc;
        // pc.drawCount = meshClusterCount;
        device->BindCompute(&compactionPipeline, cmdList);
        // device->PushConstants(cmdList, sizeof(pc), &pc);
        device->BindResource(&indirectScratchBuffer, ResourceViewType::SRV, 0, cmdList);
        device->BindResource(&meshClusterIndexBuffer, ResourceViewType::SRV, 1, cmdList);
        device->BindResource(&dispatchIndirectBuffer, ResourceViewType::SRV, 2, cmdList);

        device->BindResource(&meshIndirectCountBuffer, ResourceViewType::UAV, 0, cmdList);
        device->BindResource(&meshIndirectBuffer, ResourceViewType::UAV, 1, cmdList);
        // device->Dispatch(cmdList, (meshClusterCount + 63) / 64, 1, 1);
        device->DispatchIndirect(cmdList, &dispatchIndirectBuffer,
                                 DRAW_COMPACTION_DISPATCH_OFFSET * sizeof(DispatchIndirect) + Offset(DispatchIndirect, groupCountX));
        device->EndEvent(cmdList);
    }
    if (!isSecondPass)
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Buffer(&dispatchIndirectBuffer, ResourceUsage_ComputeWrite | ResourceUsage_ComputeRead,
                               ResourceUsage_Indirect),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }
    else
    {
        GPUBarrier barriers[] = {
            GPUBarrier::ComputeWriteToRead(&meshIndirectCountBuffer),
            GPUBarrier::Buffer(&meshIndirectBuffer, ResourceUsage_ComputeWrite, ResourceUsage_Indirect),
            GPUBarrier::Buffer(&meshIndirectCountBuffer, ResourceUsage_ComputeWrite, ResourceUsage_Indirect),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }
} // namespace render

internal void UpdateHZBPyramid(CommandList cmdList)
{
    device->BeginEvent(cmdList, "Update HZB");
    u32 width          = depthPyramid.desc.width;
    u32 height         = depthPyramid.desc.height;
    i32 lastWriteIndex = 0;
    device->BindCompute(&generateMipsPipeline, cmdList);
    // THIS ONE
    // GPUBarrier barrier = GPUBarrier::Image(&depthPyramid, ImageUsage::ShaderRead, ImageUsage::General,
    //                                        PipelineStage::Compute, PipelineStage::Compute,
    //                                        ResourceAccess::ComputeSRV | ResourceAccess::ComputeUAV, ResourceAccess::ComputeUAV);
    GPUBarrier barrier = GPUBarrier::Image(&depthPyramid, ResourceUsage::ComputeRead, ResourceUsage::ComputeWrite);
    device->Barrier(cmdList, &barrier, 1);
    for (u32 i = 0; i < ArrayLength(depthPyramidSubresources) && (width > 0 || height > 0); i++)
    {
        if (i == 0)
        {
            device->BindResource(&depthBufferMain, ResourceViewType::SRV, 0, cmdList);
        }
        else
        {
            device->BindResource(&depthPyramid, ResourceViewType::SRV, 0, cmdList, depthPyramidSubresources[i - 1]);
        }
        GPUBarrier barriers[] = {
            i == 0 ? GPUBarrier::Image(&depthBufferMain, ResourceUsage_DepthStencil, ResourceUsage_ComputeRead)
                   : GPUBarrier::Image(&depthPyramid, ResourceUsage_ComputeWrite, ResourceUsage_ComputeRead,
                                       depthPyramidSubresources[i - 1]),
        };
        lastWriteIndex = depthPyramidSubresources[i];
        device->Barrier(cmdList, barriers, ArrayLength(barriers));

        device->BindResource(&depthPyramid, ResourceViewType::UAV, 0, cmdList, depthPyramidSubresources[i]);
        u32 groupCountX = (width + 7) / 8;
        u32 groupCountY = (height + 7) / 8;
        groupCountX     = groupCountX == 0 ? 1 : groupCountX;
        groupCountY     = groupCountY == 0 ? 1 : groupCountY;

        device->Dispatch(cmdList, groupCountX, groupCountY, 1);
        width >>= 1;
        height >>= 1;
    }

    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&depthPyramid, ResourceUsage_ComputeWrite, ResourceUsage_ComputeRead, lastWriteIndex),
            GPUBarrier::Image(&depthBufferMain, ResourceUsage_ComputeRead, ResourceUsage_DepthStencil),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }
    device->EndEvent(cmdList);
}

internal void RenderMeshes(CommandList cmdList, RenderPassType type, i32 cascadeNum = -1)
{
    b8 shadowPass = type == RenderPassType_Shadow;
    b8 mainPass   = type == RenderPassType_Main;

    RenderState *state = engine->GetRenderState();
    PushConstant pc;
    pc.worldToClip           = state->transform;
    pc.meshParamsDescriptor  = device->GetDescriptorIndex(&meshParamsBuffer, ResourceViewType::SRV);
    pc.meshClusterDescriptor = device->GetDescriptorIndex(&buffers[UploadType_MeshClusters], ResourceViewType::SRV);
    pc.materialDescriptor    = device->GetDescriptorIndex(&materialBuffer, ResourceViewType::SRV);
    pc.geometryDescriptor    = device->GetDescriptorIndex(&meshGeometryBuffer, ResourceViewType::SRV);

    if (shadowPass)
    {
        pc.cascadeNum = cascadeNum;
    }

    device->PushConstants(cmdList, sizeof(pc), &pc);
    device->BindIndexBuffer(cmdList, &meshIndexBuffer);
    device->DrawIndexedIndirectCount(cmdList, &meshIndirectBuffer, &meshIndirectCountBuffer, meshClusterCount);
}

internal void Render()
{
    // TIMED_FUNCTION();
    // TODO: eventually, this should not be accessed from here
    G_State *g_state         = engine->GetGameState();
    RenderState *renderState = engine->GetRenderState();

    // Read through deferred block compress commands
    CommandList cmd;
    cmd = device->BeginCommandList(graphics::QueueType_Compute);
    device->Wait(debugState.commandList, cmd);
    u32 computeIndex = TIMED_GPU_RANGE_BEGIN(cmd, "Compute");

    // Upload frame allocations
    {
        // Uploads views
        {
            GPUBarrier barriers[] = {
                GPUBarrier::Buffer(&viewsBuffer, ResourceUsage_ComputeRead, ResourceUsage_TransferDst),
            };
            device->Barrier(cmd, barriers, ArrayLength(barriers));
            GPUView view;
            view.worldToClip     = renderState->transform;
            view.prevWorldToClip = renderState->prevWorldToClip;
            view.p22             = renderState->projection[2][2];
            view.p23             = renderState->projection[3][2];
            // view.prevP22         = renderState->prevP22;
            // view.prevP23         = renderState->prevP23;

            device->FrameAllocate(&viewsBuffer, &view, cmd, sizeof(view));

            renderState->prevWorldToClip = renderState->transform;
            // renderState->prevP22         = renderState->projection[2][2];
            // renderState->prevP23         = renderState->projection[3][2];
        }

        for (u32 i = 0; i < UploadType_Count; i++)
        {
            for (u32 uploadIndex = 0; uploadIndex < ArrayLength(uploads[0]); uploadIndex++)
            {
                FrameAllocation *alloc = &uploads[i][uploadIndex];
                if (alloc->ptr)
                {
                    if (alloc->size + currentOffsets[i] > buffers[i].desc.size)
                    {
                        GPUBufferDesc desc = buffers[i].desc;
                        desc.size          = (desc.size + alloc->size) * 2;
                        GPUBuffer newBuffer;
                        device->CreateBuffer(&newBuffer, desc, 0);
                        device->SetName(&newBuffer, "Mesh Cluster Buffer");

                        GPUBarrier barrier = GPUBarrier::Buffer(&buffers[i], ResourceUsage_TransferDst | ResourceUsage_ComputeRead, ResourceUsage_TransferSrc);
                        device->Barrier(cmd, &barrier, 1);

                        device->CopyBuffer(cmd, &newBuffer, &buffers[i], SafeTruncateU64(currentOffsets[i]));
                        device->DeleteBuffer(&buffers[i]);
                        buffers[i] = newBuffer;
                    }
                    device->CommitFrameAllocation(cmd, *alloc, &buffers[i], currentOffsets[i]);
                    currentOffsets[i] += alloc->size;

                    alloc->ptr = 0;
                }
            }
        }

        GPUBuffer *currentSkinBufUpload   = &skinningBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMeshParamUpload = &meshParamsBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMeshGeoUpload   = &meshGeometryBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMaterialUpload  = &materialBufferUpload[device->GetCurrentBuffer()];

        // Indirect prep
        {
            // NOTE: WAR, only need execution dependency
            {
                GPUBarrier barriers[] = {
                    GPUBarrier::Buffer(&indirectScratchBuffer, ResourceUsage_Indirect, ResourceUsage_ComputeWrite),
                    GPUBarrier::Buffer(&meshIndirectCountBuffer, ResourceUsage_Indirect, ResourceUsage_TransferDst),
                    GPUBarrier::Buffer(&dispatchIndirectBuffer, ResourceUsage_Indirect, ResourceUsage_TransferDst),
#if 1
                    GPUBarrier::Buffer(&cullingStatisticsBuffer, ResourceUsage_ComputeWrite, ResourceUsage_TransferDst),
                    GPUBarrier::Buffer(&debugAABBs, ResourceUsage_ComputeWrite, ResourceUsage_TransferDst),
#endif
                };
                device->Barrier(cmd, barriers, ArrayLength(barriers));
            }
            device->BindCompute(&clearIndirectPipeline, cmd);
            device->BindResource(&indirectScratchBuffer, ResourceViewType::UAV, 0, cmd);
            device->Dispatch(cmd, (meshClusterCount + CLUSTER_SIZE - 1) / CLUSTER_SIZE, 1, 1);

            u32 zero = 0;
            device->FrameAllocate(&meshIndirectCountBuffer, &zero, cmd);

            TempArena temp          = ScratchStart(0, 0);
            CullingStatistics stats = {};
            device->FrameAllocate(&cullingStatisticsBuffer, &stats, cmd);

            device->ClearBuffer(cmd, &debugAABBs);

            DispatchIndirect *indirect                                  = PushArray(temp.arena, DispatchIndirect, NUM_DISPATCH_OFFSETS);
            indirect[CLUSTER_DISPATCH_OFFSET].groupCountX               = 0;
            indirect[CLUSTER_DISPATCH_OFFSET].groupCountY               = 1;
            indirect[CLUSTER_DISPATCH_OFFSET].groupCountZ               = 1;
            indirect[TRIANGLE_DISPATCH_OFFSET].groupCountX              = 0;
            indirect[TRIANGLE_DISPATCH_OFFSET].groupCountY              = 1;
            indirect[TRIANGLE_DISPATCH_OFFSET].groupCountZ              = 1;
            indirect[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].commandCount = 0;
            indirect[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].groupCountX  = 0;
            indirect[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].groupCountY  = 1;
            indirect[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].groupCountZ  = 1;
            indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountX       = 0;
            indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountY       = 1;
            indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountZ       = 1;
            device->FrameAllocate(&dispatchIndirectBuffer, indirect, cmd);
            ScratchEnd(temp);
        }

        if (skinningBufferSize)
        {
            device->CopyBuffer(cmd, &skinningBuffer, currentSkinBufUpload, skinningBufferSize);
        }
        if (meshParamsBufferSize)
        {
            device->CopyBuffer(cmd, &meshParamsBuffer, currentMeshParamUpload, meshParamsBufferSize);
        }
        if (meshGeometryBufferSize)
        {
            device->CopyBuffer(cmd, &meshGeometryBuffer, currentMeshGeoUpload, meshGeometryBufferSize);
        }
        if (materialBufferSize)
        {
            device->CopyBuffer(cmd, &materialBuffer, currentMaterialUpload, materialBufferSize);
        }

        {
            GPUBarrier barriers[] = {
                GPUBarrier::Memory(ResourceUsage_TransferDst, ResourceUsage_ComputeRead | ResourceUsage_ComputeWrite),
                GPUBarrier::Buffer(&indirectScratchBuffer, ResourceUsage_ComputeWrite, ResourceUsage_ComputeWrite),
#if 1
                GPUBarrier::Buffer(&debugAABBs, ResourceUsage_TransferDst, ResourceUsage_ComputeWrite), // | ResourceUsage_ComputeRead),
#endif
            };
            device->Barrier(cmd, barriers, ArrayLength(barriers));
        }
    }

    {
        u64 readPos    = blockCompressRead;
        u64 endPos     = blockCompressCommitWrite.load();
        const u64 size = ArrayLength(blockCompressRing);
        for (u64 i = readPos; i < endPos; i++)
        {
            DeferredBlockCompressCmd *deferCmd = &blockCompressRing[(i & (size - 1))];
            BlockCompressImage(&deferCmd->in, &deferCmd->out, cmd);
            blockCompressRead++;
        }
        std::atomic_thread_fence(std::memory_order_release);
    }

    // Compute skinning
    {
        device->BeginEvent(cmd, "Skinning");
        device->BindCompute(&skinPipeline, cmd);
        SkinningPushConstants pc;
        pc.skinningBuffer = device->GetDescriptorIndex(&skinningBuffer, ResourceViewType::SRV);

        for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
        {
            Mesh *mesh               = gameScene->Get(&iter);
            Entity entity            = gameScene->GetEntity(&iter);
            LoadedSkeleton *skeleton = gameScene->skeletons.GetFromEntity(entity);
            if (skeleton)
            {
                pc.vertexPos        = mesh->vertexPosView.srvDescriptor;
                pc.vertexNor        = mesh->vertexNorView.srvDescriptor;
                pc.vertexTan        = mesh->vertexTanView.srvDescriptor;
                pc.vertexBoneId     = mesh->vertexBoneIdView.srvDescriptor;
                pc.vertexBoneWeight = mesh->vertexBoneWeightView.srvDescriptor;

                pc.soPos          = mesh->soPosView.uavDescriptor;
                pc.soNor          = mesh->soNorView.uavDescriptor;
                pc.soTan          = mesh->soTanView.uavDescriptor;
                pc.skinningOffset = skeleton->skinningOffset;

                device->PushConstants(cmd, sizeof(pc), &pc);
                device->Dispatch(cmd, (mesh->vertexCount + SKINNING_GROUP_SIZE - 1) / SKINNING_GROUP_SIZE, 1, 1);
            }
        }
        device->EndEvent(cmd);
    }

    // Culling
    CullInstances(cmd, false);

    TIMED_RANGE_END(computeIndex);

    CommandList cmdList = device->BeginCommandList(QueueType_Graphics);
    device->Wait(cmd, cmdList);

    // TIMED_GPU(cmdList);
    u32 rangeIndex = TIMED_GPU_RANGE_BEGIN(cmdList, "Graphics");
    debugState.BeginTriangleCount(cmdList);

    // Setup cascaded shadowmaps
    {
        CascadeParams cascadeParams;
        ViewLight testLight     = {};
        testLight.dir           = {0, 0, 1};
        testLight.dir           = Normalize(testLight.dir);
        cascadeParams.rLightDir = MakeV4(testLight.dir, 1.0);
        cascadeParams.rViewPos  = MakeV4(renderState->camera.position, 1.0);

        R_CascadedShadowMap(&testLight, cascadeParams.rLightViewProjectionMatrices, cascadeParams.rCascadeDistances.elements);
        device->FrameAllocate(&cascadeParamsBuffer, &cascadeParams, cmdList, sizeof(cascadeParams));

        // TODO: Shouldn't VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT have FRAGMENT_SHADER in its synchronization scope???
        // validation says otherwise.
        GPUBarrier barrier = GPUBarrier::Buffer(&cascadeParamsBuffer,
                                                ResourceUsage_TransferDst,
                                                ResourceUsage_UniformRead | PipelineStage_VertexShader | PipelineStage_FragmentShader);
        device->Barrier(cmdList, &barrier, 1);
    }

    // Shadow pass :)
    // TODO: find something async to do during this
    {
        for (u32 shadowSlice = 0; shadowSlice < shadowDepthMap.desc.numLayers; shadowSlice++)
        {
            RenderPassImage images[] = {
                RenderPassImage::DepthStencil(&shadowDepthMap, ResourceUsage_None, shadowSlice, LoadOp::Clear, StoreOp::Store),
            };

            device->BeginRenderPass(images, ArrayLength(images), cmdList);
            device->BindPipeline(&shadowMapPipeline, cmdList);
            device->BindResource(&cascadeParamsBuffer, ResourceViewType::SRV, CASCADE_PARAMS_BIND, cmdList);
            device->UpdateDescriptorSet(cmdList);

            Viewport viewport;
            viewport.width  = (f32)shadowDepthMap.desc.width;
            viewport.height = (f32)shadowDepthMap.desc.height;

            device->SetViewport(cmdList, &viewport);
            Rect2 scissor;
            scissor.minP = {0, 0};
            scissor.maxP = {65536, 65536};

            device->SetScissor(cmdList, scissor);

            RenderMeshes(cmdList, RenderPassType_Shadow, shadowSlice);
            device->EndRenderPass(cmdList);
        }
    }

    // Main geo pass

    auto render = [&](CommandList commandList, bool isFirstPass) {
        RenderPassImage images[] = {
            RenderPassImage::DepthStencil(&depthBufferMain,
                                          isFirstPass ? ResourceUsage_Reset : ResourceUsage_None,
                                          -1,
                                          isFirstPass ? LoadOp::Clear : LoadOp::Load,
                                          StoreOp::Store),
            RenderPassImage::Color(&mainColorAttachment,
                                   isFirstPass ? ResourceUsage_Reset : ResourceUsage_None,
                                   -1,
                                   isFirstPass ? LoadOp::Clear : LoadOp::Load,
                                   StoreOp::Store),
        };

        device->BeginRenderPass(images, ArrayLength(images), commandList);
        device->BindPipeline(&pipelineState, commandList);
        device->BindResource(&cascadeParamsBuffer, ResourceViewType::SRV, CASCADE_PARAMS_BIND, commandList);
        device->BindResource(&shadowDepthMap, ResourceViewType::SRV, SHADOW_MAP_BIND, commandList);
        device->UpdateDescriptorSet(commandList);

        Viewport viewport;
        viewport.width  = (f32)swapchain.GetDesc().width;
        viewport.height = (f32)swapchain.GetDesc().height;

        device->SetViewport(commandList, &viewport);
        Rect2 scissor;
        scissor.minP = {0, 0};
        scissor.maxP = {65536, 65536};

        device->SetScissor(commandList, scissor);

        PushConstant pc;
        pc.meshParamsDescriptor = device->GetDescriptorIndex(&meshParamsBuffer, ResourceViewType::SRV);

        RenderMeshes(commandList, RenderPassType_Main);
        device->EndRenderPass(commandList);
    };
    device->BeginEvent(cmdList, "First Render Pass");
    render(cmdList, true);
    device->EndEvent(cmdList);
    UpdateHZBPyramid(cmdList);
    {
        // NOTE: WAR, only need execution dependency
        {
            GPUBarrier barriers[] = {
                GPUBarrier::Buffer(&indirectScratchBuffer, ResourceUsage_Indirect, ResourceUsage_ComputeWrite),
                GPUBarrier::Buffer(&meshIndirectCountBuffer, ResourceUsage_Indirect, ResourceUsage_TransferDst),
            };
            device->Barrier(cmdList, barriers, ArrayLength(barriers));
        }
        device->BindCompute(&clearIndirectPipeline, cmdList);
        device->BindResource(&indirectScratchBuffer, ResourceViewType::UAV, 0, cmdList);
        device->Dispatch(cmdList, (meshClusterCount + CLUSTER_SIZE - 1) / CLUSTER_SIZE, 1, 1);

        u32 zero = 0;
        device->FrameAllocate(&meshIndirectCountBuffer, &zero, cmdList);

        TempArena temp                                        = ScratchStart(0, 0);
        DispatchIndirect *indirect                            = PushArray(temp.arena, DispatchIndirect, NUM_DISPATCH_OFFSETS - 1);
        indirect[CLUSTER_DISPATCH_OFFSET].groupCountX         = 0;
        indirect[CLUSTER_DISPATCH_OFFSET].groupCountY         = 1;
        indirect[CLUSTER_DISPATCH_OFFSET].groupCountZ         = 1;
        indirect[TRIANGLE_DISPATCH_OFFSET].groupCountX        = 0;
        indirect[TRIANGLE_DISPATCH_OFFSET].groupCountY        = 1;
        indirect[TRIANGLE_DISPATCH_OFFSET].groupCountZ        = 1;
        indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountX = 0;
        indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountY = 1;
        indirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountZ = 1;
        device->FrameAllocate(&dispatchIndirectBuffer, indirect, cmdList, sizeof(DispatchIndirect) * NUM_DISPATCH_OFFSETS - 1);
        ScratchEnd(temp);
        {
            GPUBarrier barriers[] = {
                GPUBarrier::Buffer(&dispatchIndirectBuffer,
                                   ResourceUsage_TransferDst,
                                   ResourceUsage_Indirect | ResourceUsage_ComputeRead | ResourceUsage_ComputeWrite),
                GPUBarrier::Buffer(&meshIndirectCountBuffer, ResourceUsage_TransferDst, ResourceUsage_ComputeWrite),
            };
            device->Barrier(cmdList, barriers, ArrayLength(barriers));
        }
    }
#if 1
    CullInstances(cmdList, true);
    device->BeginEvent(cmdList, "Second Render Pass");
    render(cmdList, false);
    device->EndEvent(cmdList);
#endif
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&mainColorAttachment, ResourceUsage_ColorAttachment, ResourceUsage_TransferSrc),
        };
        device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }
    device->BeginRenderPass(&swapchain, cmdList);
    device->CopyImage(cmdList, &swapchain, &mainColorAttachment);
    device->EndRenderPass(&swapchain, cmdList);
#if 1
    UpdateHZBPyramid(cmdList);
#endif

    TIMED_RANGE_END(rangeIndex);

    debugState.EndTriangleCount(cmdList);
    debugState.EndFrame(cmdList);
    device->SubmitCommandLists();

    currentFrame++;
    currentBuffer = currentFrame % ArrayLength(frameData);

    // Get num culled
    {
        CullingStatistics *stats = (CullingStatistics *)cullingStatisticsBuffer.mappedData;
        V4 *data                 = (V4 *)debugAABBs.mappedData;
        Printf("Num occlusion culled: %u\n", stats[0].numOcclusionCulled);
        Printf("Num frustum culled: %u\n", stats[0].numFrustumCulled);
    }
    debugState.PrintDebugRecords();
} // namespace render

void DeferBlockCompress(graphics::Texture input, graphics::Texture output)
{
    DeferredBlockCompressCmd cmd;
    cmd.in  = input;
    cmd.out = output;

    u64 writePos   = blockCompressWrite.fetch_add(1);
    const u64 size = ArrayLength(blockCompressRing);

    for (;;)
    {
        u64 availableSpots = ArrayLength(blockCompressRing) - (writePos - blockCompressRead);
        if (availableSpots >= 1)
        {
            blockCompressRing[writePos & (size - 1)] = cmd;
            u64 testWritePos                         = writePos;
            // NOTE: compare_exchange_weak replaces the expected value with the value of the atomic, which I don't want in this case
            while (blockCompressCommitWrite.compare_exchange_weak(testWritePos, testWritePos + 1))
            {
                std::this_thread::yield();
                testWritePos = writePos;
            }
            break;
        }
    }
}

void BlockCompressImage(graphics::Texture *input, graphics::Texture *output, CommandList cmd)
{
    // Texture reused
    u32 blockSize = GetBlockSize(output->desc.format);
    static thread_global Texture bc1Uav;
    TextureDesc desc;
    desc.width        = input->desc.width / blockSize;
    desc.height       = input->desc.height / blockSize;
    desc.format       = Format::R32G32_UINT;
    desc.initialUsage = ResourceUsage_StorageImage;
    desc.futureUsages = ResourceUsage_TransferSrc;
    desc.textureType  = TextureDesc::TextureType::Texture2D;

    if (!bc1Uav.IsValid() || bc1Uav.desc.width < desc.width || bc1Uav.desc.height < desc.height)
    {
        TextureDesc bcDesc = desc;
        bcDesc.width       = (u32)GetNextPowerOfTwo(desc.width);
        bcDesc.height      = (u32)GetNextPowerOfTwo(desc.height);

        device->CreateTexture(&bc1Uav, bcDesc, 0);
        device->SetName(&bc1Uav, "BC1 UAV");
    }

    // Output block compression to intermediate texture
    Shader *shader = &shaders[ShaderType_BC1_CS];
    device->BindCompute(&blockCompressPipeline, cmd);
    device->BindResource(&bc1Uav, ResourceViewType::UAV, 0, cmd);
    device->BindResource(input, ResourceViewType::SRV, 0, cmd);

    device->Dispatch(cmd, (desc.width + 7) / 8, (desc.height + 7) / 8, 1);

    // Copy from uav to output
    {
        GPUBarrier barriers[] = {

            GPUBarrier::Image(&bc1Uav, ResourceUsage_ComputeWrite, ResourceUsage_TransferSrc),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
    device->CopyTexture(cmd, output, &bc1Uav);
    device->DeleteTexture(input);

    // Transfer the block compressed texture to its initial format
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&bc1Uav, ResourceUsage_TransferSrc, ResourceUsage_ComputeWrite),
            GPUBarrier::Image(output, ResourceUsage_TransferDst, PipelineStage_FragmentShader),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
}

// void UpdateSkinning(rendergraph::RenderGraph *graph)
// {
//     rendergraph::RGSkinning skinning;
//     skinning.push.skinningBuffer = device->GetDescriptorIndex(&skinningBuffer, ResourceViewType::SRV);
//
//     for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
//     {
//         Mesh *mesh               = gameScene->Get(&iter);
//         Entity entity            = gameScene->GetEntity(&iter);
//         LoadedSkeleton *skeleton = gameScene->skeletons.GetFromEntity(entity);
//         if (skeleton)
//         {
//             skinning.push.vertexPos        = mesh->vertexPosView.srvDescriptor;
//             skinning.push.vertexNor        = mesh->vertexNorView.srvDescriptor;
//             skinning.push.vertexTan        = mesh->vertexTanView.srvDescriptor;
//             skinning.push.vertexBoneId     = mesh->vertexBoneIdView.srvDescriptor;
//             skinning.push.vertexBoneWeight = mesh->vertexBoneWeightView.srvDescriptor;
//
//             skinning.push.soPos          = mesh->soPosView.uavDescriptor;
//             skinning.push.soNor          = mesh->soNorView.uavDescriptor;
//             skinning.push.soTan          = mesh->soTanView.uavDescriptor;
//             skinning.push.skinningOffset = skeleton->skinningOffset;
//
//             AddPass(graph, "Skinning", &skinning, [&](CommandList cmd) {
//                 device->BindCompute(&skinPipeline, cmd);
//                 device->Dispatch(cmd, (mesh->vertexCount + SKINNING_GROUP_SIZE - 1) / SKINNING_GROUP_SIZE, 1, 1);
//             });
//         }
//     }
// }

} // namespace render
