#include "../mkCrack.h"
#include <atomic>

#ifdef LSP_INCLUDE
#include "../mkMath.h"
#include "../mkAsset.h"
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
    state->nearZ       = .1f;
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

internal b32 D_IsInBounds(Rect3 bounds, Mat4 &mvp)
{
    V4 p;
    p.w      = 1;
    int bits = 0;
    // Loop over every point on the AABB bounding box
    for (u32 i = 0; i < 2; i++)
    {
        p.x = bounds[i][0];
        for (u32 j = 0; j < 2; j++)
        {
            p.y = bounds[j][1];
            for (u32 k = 0; k < 2; k++)
            {
                p.z = bounds[k][2];

                // Find point in homogeneous clip space
                V4 test = mvp * p;

                // Compare to the hcs AABB
                const f32 minW = -test.w;
                const f32 maxW = test.w;
                const f32 minZ = minW;

                if (test.x > minW)
                {
                    bits |= (1 << 0);
                }
                if (test.x < maxW)
                {
                    bits |= (1 << 1);
                }
                if (test.y > minW)
                {
                    bits |= (1 << 2);
                }
                if (test.y < maxW)
                {
                    bits |= (1 << 3);
                }
                if (test.z > minZ)
                {
                    bits |= (1 << 4);
                }
                if (test.z < maxW)
                {
                    bits |= (1 << 5);
                }
            }
        }
    }
    // If any bits not set, the model isn't in bounds
    b32 result = (bits == 63);
    return result;
}

// internal void D_PushHeightmap(Heightmap heightmap)
// {
//     R_CommandHeightMap *command = (R_CommandHeightMap *)R_CreateCommand(sizeof(*command));
//     command->type               = R_CommandType_Heightmap;
//     command->heightmap          = heightmap;
// }

// so i want to have a ring buffer between the game and the renderer. simple enough. let's start with single
// threaded, similar to what we have now. right now the way the renderer works is that there's a permanent render
// state global, and each frame the simulation/game sends data to the render pass to use, etc etc
// yadda yadda.

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

// Buffers
graphics::GPUBuffer cascadeParamsBuffer;
graphics::GPUBuffer skinningBufferUpload[device->cNumBuffers];
graphics::GPUBuffer skinningBuffer;
graphics::GPUBuffer meshParamsBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshParamsBuffer;
graphics::GPUBuffer meshGeometryBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshGeometryBuffer;
graphics::GPUBuffer meshBatchBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshBatchBuffer;
// TODO: I think this will eventually disappear
graphics::GPUBuffer meshSubsetBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshSubsetBuffer;
graphics::GPUBuffer materialBufferUpload[device->cNumBuffers];
graphics::GPUBuffer materialBuffer;

// graphics::GPUBuffer instanceToDrawIDBuffer;
graphics::GPUBuffer meshIndirectBuffer;
// graphics::GPUBuffer meshIndirectBufferUpload[device->cNumBuffers];
graphics::GPUBuffer meshIndexBuffer;

u32 skinningBufferSize;
u32 meshParamsBufferSize;
u32 meshGeometryBufferSize;
u32 meshBatchBufferSize;
u32 meshSubsetBufferSize;
u32 materialBufferSize;
u32 meshIndirectBufferSize;
u32 drawCount;

u32 meshBatchCount;

InputLayout inputLayouts[IL_Type_Count];
Shader shaders[ShaderType_Count];
RasterizationState rasterizers[RasterType_Count];

Texture depthBufferMain;
Texture shadowDepthBuffer;
list<i32> shadowSlices;

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

// graphics::GPUBuffer globalVertexBuffer;
//
// internal void AllocateGeometry(void *data, u64 size)
// {
// }

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

    // Initialize buffers
    {
        skinningBufferSize     = 0;
        meshParamsBufferSize   = 0;
        meshGeometryBufferSize = 0;
        meshBatchBufferSize    = 0;
        meshSubsetBufferSize   = 0;
        materialBufferSize     = 0;
        meshIndirectBufferSize = 0;
        drawCount              = 0;
        meshBatchCount         = 0;

        TempArena temp = ScratchStart(0, 0);
        GPUBufferDesc desc;
        desc.mSize          = kilobytes(64);
        desc.mResourceUsage = ResourceUsage::UniformBuffer | ResourceUsage::NotBindless;
        device->CreateBuffer(&cascadeParamsBuffer, desc, 0);

        // Skinning
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;
        for (u32 i = 0; i < ArrayLength(skinningBufferUpload); i++)
        {
            device->CreateBuffer(&skinningBufferUpload[i], desc, 0);
            device->SetName(&skinningBufferUpload[i], "Skinning upload buffer");
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&skinningBuffer, desc, 0);
        device->SetName(&skinningBuffer, "Skinning buffer");

        // Mesh params
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc; // ResourceUsage::UniformBuffer;
        for (u32 i = 0; i < ArrayLength(meshParamsBufferUpload); i++)
        {
            device->CreateBuffer(&meshParamsBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh params upload buffer %i", i);
            device->SetName(&meshParamsBufferUpload[i], name);
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&meshParamsBuffer, desc, 0);
        device->SetName(&meshParamsBuffer, "Mesh params buffer");

        // Mesh geometry
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(meshGeometryBufferUpload); i++)
        {
            device->CreateBuffer(&meshGeometryBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh geometry upload buffer %i", i);
            device->SetName(&meshGeometryBufferUpload[i], name);
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&meshGeometryBuffer, desc, 0);
        device->SetName(&meshGeometryBuffer, "Mesh geometry buffer");

        // Mesh batches
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(meshBatchBufferUpload); i++)
        {
            device->CreateBuffer(&meshBatchBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh batch upload buffer %i", i);
            device->SetName(&meshBatchBufferUpload[i], name);
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&meshBatchBuffer, desc, 0);
        device->SetName(&meshBatchBuffer, "Mesh batch buffer");

        // Mesh subset
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(meshSubsetBufferUpload); i++)
        {
            device->CreateBuffer(&meshSubsetBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Mesh subset upload buffer %i", i);
            device->SetName(&meshSubsetBufferUpload[i], name);
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&meshSubsetBuffer, desc, 0);
        device->SetName(&meshSubsetBuffer, "Mesh subset buffer");

        // Material buffer
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(materialBufferUpload); i++)
        {
            device->CreateBuffer(&materialBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Material upload buffer %i", i);
            device->SetName(&materialBufferUpload[i], name);
        }

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&materialBuffer, desc, 0);
        device->SetName(&materialBuffer, "Material buffer");

        // Indirect buffer
#if 0
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(meshIndirectBufferUpload); i++)
        {
            device->CreateBuffer(&meshIndirectBufferUpload[i], desc, 0);
            string name = PushStr8F(temp.arena, "Indirect upload buffer %i", i);
            device->SetName(&meshIndirectBufferUpload[i], name);
        }
#endif

        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::StorageBuffer | ResourceUsage::IndirectBuffer;
        device->CreateBuffer(&meshIndirectBuffer, desc, 0);
        device->SetName(&meshIndirectBuffer, "Mesh indirect buffer");

        // Mesh index buffer
        desc                = {};
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::StorageBuffer | ResourceUsage::IndexBuffer;
        device->CreateBuffer(&meshIndexBuffer, desc, 0);
        device->SetName(&meshIndexBuffer, "Mesh index buffer");

        // instance to draw id buffer
        // desc        = {};
        // desc.mSize  = kilobytes(4);
        // desc.mUsage = MemoryUsage::GPU_ONLY;
    }

    // Initialize render targets/depth buffers
    {
        TextureDesc desc;
        desc.mWidth        = swapchain.mDesc.width;
        desc.mHeight       = swapchain.mDesc.height;
        desc.mInitialUsage = ResourceUsage::DepthStencil;
        desc.mFormat       = Format::D32_SFLOAT_S8_UINT;

        device->CreateTexture(&depthBufferMain, desc, 0);

        // Shadows
        desc.mWidth        = 1024;
        desc.mHeight       = 1024;
        desc.mInitialUsage = ResourceUsage::SampledImage; // TODO: this is weird.
        desc.mFutureUsages = ResourceUsage::DepthStencil;
        desc.mFormat       = Format::D32_SFLOAT;
        desc.mNumLayers    = cNumCascades;
        desc.mSampler      = TextureDesc::DefaultSampler::Nearest;
        desc.mTextureType  = TextureDesc::TextureType::Texture2DArray;

        device->CreateTexture(&shadowDepthBuffer, desc, 0);

        for (u32 i = 0; i < shadowDepthBuffer.mDesc.mNumLayers; i++)
        {
            shadowSlices.push_back(device->CreateSubresource(&shadowDepthBuffer, i, 1));
        }
    }

    // Initialize rasterization state
    {
        rasterizers[RasterType_CCW_CullBack].mCullMode  = RasterizationState::CullMode::Back;
        rasterizers[RasterType_CCW_CullFront].mCullMode = RasterizationState::CullMode::Front;
        rasterizers[RasterType_CCW_CullNone].mCullMode  = RasterizationState::CullMode::None;
    }

    // Initialize shaders
    {
        shaders[ShaderType_Mesh_VS].mName          = "mesh_vs.hlsl";
        shaders[ShaderType_Mesh_VS].stage          = ShaderStage::Vertex;
        shaders[ShaderType_Mesh_FS].mName          = "mesh_fs.hlsl";
        shaders[ShaderType_Mesh_FS].stage          = ShaderStage::Fragment;
        shaders[ShaderType_ShadowMap_VS].mName     = "depth_vs.hlsl";
        shaders[ShaderType_ShadowMap_VS].stage     = ShaderStage::Vertex;
        shaders[ShaderType_BC1_CS].mName           = "blockcompress_cs.hlsl";
        shaders[ShaderType_BC1_CS].stage           = ShaderStage::Compute;
        shaders[ShaderType_Skin_CS].mName          = "skinning_cs.hlsl";
        shaders[ShaderType_Skin_CS].stage          = ShaderStage::Compute;
        shaders[ShaderType_TriangleCull_CS].mName  = "cull_triangle_cs.hlsl";
        shaders[ShaderType_TriangleCull_CS].stage  = ShaderStage::Compute;
        shaders[ShaderType_ClearIndirect_CS].mName = "clear_indirect_cs.hlsl";
        shaders[ShaderType_ClearIndirect_CS].stage = ShaderStage::Compute;
    }

    // Compile shaders
    {
        shadercompiler::InitShaderCompiler();
        for (u32 i = 0; i < ShaderType_Count; i++)
        {
            shadercompiler::CompileInput input;
            input.shaderName = shaders[i].mName;
            input.stage      = shaders[i].stage;

            shadercompiler::CompileOutput output;

            u64 pos = ArenaPos(arena);
            shadercompiler::CompileShader(arena, &input, &output);
            device->CreateShader(&shaders[i], output.shaderData);
            ArenaPopTo(arena, pos);
        }
    }

    // Initialize pipelines
    {
        InputLayout &inputLayout = inputLayouts[IL_Type_MeshVertex];
        inputLayout.mElements    = {
            Format::R32G32B32_SFLOAT, Format::R32G32B32_SFLOAT, Format::R32G32_SFLOAT, Format::R32G32B32_SFLOAT,
            Format::R32G32B32A32_UINT, Format::R32G32B32A32_SFLOAT};

        inputLayout.mBinding = 0;
        inputLayout.mStride  = sizeof(MeshVertex);
        inputLayout.mRate    = InputRate::Vertex;

        // Main
        PipelineStateDesc desc      = {};
        desc.mDepthStencilFormat    = Format::D32_SFLOAT_S8_UINT;
        desc.mColorAttachmentFormat = Format::R8G8B8A8_SRGB;
        desc.vs                     = &shaders[ShaderType_Mesh_VS];
        desc.fs                     = &shaders[ShaderType_Mesh_FS];
        desc.mRasterState           = &rasterizers[RasterType_CCW_CullBack];
        device->CreatePipeline(&desc, &pipelineState, "Main pass");

        // Shadows
        desc                     = {};
        desc.mDepthStencilFormat = Format::D32_SFLOAT;
        desc.vs                  = &shaders[ShaderType_ShadowMap_VS];
        desc.mRasterState        = &rasterizers[RasterType_CCW_CullNone];
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

        // Clera indirect
        desc.compute = &shaders[ShaderType_ClearIndirect_CS];
        device->CreateComputePipeline(&desc, &clearIndirectPipeline, "Clear indirect compute");
    }
}

enum RenderPassType
{
    RenderPassType_Main,
    RenderPassType_Shadow,
};

internal void CullMeshBatches(CommandList cmdList)
{
    TriangleCullPushConstant pc;
    pc.meshBatchDescriptor    = device->GetDescriptorIndex(&meshBatchBuffer, ResourceType::SRV);
    pc.meshGeometryDescriptor = device->GetDescriptorIndex(&meshGeometryBuffer, ResourceType::SRV);
    pc.meshParamsDescriptor   = device->GetDescriptorIndex(&meshParamsBuffer, ResourceType::SRV);
    device->PushConstants(cmdList, sizeof(pc), &pc);
    device->BindCompute(&triangleCullPipeline, cmdList);
    device->BindResource(&meshIndirectBuffer, ResourceType::UAV, 0, cmdList);
    device->BindResource(&meshIndexBuffer, ResourceType::UAV, 1, cmdList);
    device->UpdateDescriptorSet(cmdList);
    device->Dispatch(cmdList, meshBatchCount, 1, 1);
}

internal void RenderMeshes(CommandList cmdList, RenderPassType type, i32 cascadeNum = -1)
{
    b8 shadowPass = type == RenderPassType_Shadow;
    b8 mainPass   = type == RenderPassType_Main;

    PushConstant pc;
    pc.meshParamsDescriptor = device->GetDescriptorIndex(&meshParamsBuffer, ResourceType::SRV);
    pc.subsetDescriptor     = device->GetDescriptorIndex(&meshSubsetBuffer, ResourceType::SRV);
    pc.materialDescriptor   = device->GetDescriptorIndex(&materialBuffer, ResourceType::SRV);
    pc.geometryDescriptor   = device->GetDescriptorIndex(&meshGeometryBuffer, ResourceType::SRV);

    if (shadowPass)
    {
        pc.cascadeNum = cascadeNum;
    }

    device->PushConstants(cmdList, sizeof(pc), &pc);
    // TODO: generate global index buffer
    device->BindIndexBuffer(cmdList, &meshIndexBuffer);
    device->DrawIndexedIndirect(cmdList, &meshIndirectBuffer, drawCount);

    // u32 subsetCount = 0;
    // for (MeshIter iter = gameScene->BeginMeshIter(); !gameScene->End(&iter); gameScene->Next(&iter))
    // {
    //     Mesh *mesh = gameScene->Get(&iter);
    //     if (mesh->meshIndex == -1) continue;
    //
    //     for (u32 subsetIndex = 0; subsetIndex < mesh->numSubsets; subsetIndex++)
    //     {
    //         Mesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
    //
    //         // TODO: remove this for indirect
    //         pc.drawID = subsetCount++;
    //
    //         device->PushConstants(cmdList, sizeof(pc), &pc);
    //         device->BindIndexBuffer(cmdList, &mesh->buffer, mesh->indexView.offset);
    //         device->DrawIndexed(cmdList, subset->indexCount, subset->indexStart, 0);
    //     }
    // }
}

internal void Render()
{
    TIMED_FUNCTION();
    // TODO: eventually, this should not be accessed from here
    G_State *g_state         = engine->GetGameState();
    RenderState *renderState = engine->GetRenderState();

    // Read through deferred block compress commands
    CommandList cmd;
    cmd = device->BeginCommandList(graphics::QueueType_Compute);

    // Upload frame allocations
    {
        // NOTE: WAR, only need execution dependency
        {
            GPUBarrier barriers[] = {
                GPUBarrier::Memory(PipelineFlag_AllCommands, PipelineFlag_Transfer),
                GPUBarrier::Buffer(&meshIndirectBuffer, PipelineFlag_Indirect, PipelineFlag_Compute,
                                   AccessFlag_IndirectRead, AccessFlag_ShaderWrite),
            };
            device->Barrier(cmd, barriers, ArrayLength(barriers));
        }
        device->BindCompute(&clearIndirectPipeline, cmd);
        device->BindResource(&meshIndirectBuffer, ResourceType::UAV, 0, cmd);
        device->UpdateDescriptorSet(cmd);
        device->Dispatch(cmd, (meshBatchCount + BATCH_SIZE - 1) / BATCH_SIZE, 1, 1);

        GPUBuffer *currentSkinBufUpload   = &skinningBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMeshParamUpload = &meshParamsBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMeshGeoUpload   = &meshGeometryBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentSubsetUpload    = &meshSubsetBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentBatchUpload     = &meshBatchBufferUpload[device->GetCurrentBuffer()];
        GPUBuffer *currentMaterialUpload  = &materialBufferUpload[device->GetCurrentBuffer()];

        // GPUBuffer *currentIndirectUpload  = &meshIndirectBufferUpload[device->GetCurrentBuffer()];
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
        if (meshSubsetBufferSize)
        {
            device->CopyBuffer(cmd, &meshSubsetBuffer, currentSubsetUpload, meshSubsetBufferSize);
        }
        if (meshBatchBufferSize)
        {
            device->CopyBuffer(cmd, &meshBatchBuffer, currentBatchUpload, meshBatchBufferSize);
        }
        if (materialBufferSize)
        {
            device->CopyBuffer(cmd, &materialBuffer, currentMaterialUpload, materialBufferSize);
        }

        {
            GPUBarrier barriers[] = {
                GPUBarrier::Memory(PipelineFlag_Transfer, PipelineFlag_Compute,
                                   AccessFlag_TransferWrite, AccessFlag_ShaderWrite | AccessFlag_ShaderRead),
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
        device->BindCompute(&skinPipeline, cmd);
        SkinningPushConstants pc;
        pc.skinningBuffer = device->GetDescriptorIndex(&skinningBuffer, ResourceType::SRV);

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
    }
    // triangle culling
    {
        GPUBarrier barrier = GPUBarrier::Memory(PipelineFlag_Compute, PipelineFlag_Compute,
                                                AccessFlag_ShaderWrite, AccessFlag_ShaderRead);
        device->Barrier(cmd, &barrier, 1);

        CullMeshBatches(cmd);
    }

    CommandList cmdList = device->BeginCommandList(QueueType_Graphics);
    device->Wait(cmd, cmdList);
    TIMED_GPU(cmdList);
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
    }

    {
        // GPUBarrier barriers[] = {
        //     GPUBarrier::Memory(PipelineFlag_Transfer, PipelineFlag_VertexShader,
        //                        AccessFlag_TransferWrite, AccessFlag_ShaderRead | AccessFlag_UniformRead),
        //     GPUBarrier::Buffer(&meshIndirectBuffer, PipelineFlag_Transfer, PipelineFlag_Indirect,
        //                        AccessFlag_TransferWrite, AccessFlag_IndirectRead),
        // };
        // device->Barrier(cmdList, barriers, ArrayLength(barriers));
    }

    // Shadow pass :)
    {
        for (u32 shadowSlice = 0; shadowSlice < shadowDepthBuffer.mDesc.mNumLayers; shadowSlice++)
        {
            RenderPassImage images[] = {
                RenderPassImage::DepthStencil(&shadowDepthBuffer,
                                              ResourceUsage::SampledImage, ResourceUsage::SampledImage, shadowSlice),
            };

            device->BeginRenderPass(images, ArrayLength(images), cmdList);
            device->BindPipeline(&shadowMapPipeline, cmdList);
            device->BindResource(&cascadeParamsBuffer, ResourceType::SRV, CASCADE_PARAMS_BIND, cmdList);
            device->UpdateDescriptorSet(cmdList);

            Viewport viewport;
            viewport.width  = (f32)shadowDepthBuffer.mDesc.mWidth;
            viewport.height = (f32)shadowDepthBuffer.mDesc.mHeight;

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

    {
        // TIMED_GPU(cmdList);
        // RenderPassImage images[] = {
        //     RenderPassImage::DepthStencil(&depthBufferMain,
        //                                   ResourceUsage::None, ResourceUsage::DepthStencil),
        // };

        RenderPassImage images[] = {
            {
                RenderPassImage::RenderImageType::Depth,
                &depthBufferMain,
            },
        };

        device->BeginRenderPass(&swapchain, images, ArrayLength(images), cmdList);
        device->BindPipeline(&pipelineState, cmdList);
        device->BindResource(&cascadeParamsBuffer, ResourceType::SRV, CASCADE_PARAMS_BIND, cmdList);
        device->BindResource(&shadowDepthBuffer, ResourceType::SRV, SHADOW_MAP_BIND, cmdList);
        device->UpdateDescriptorSet(cmdList);

        Viewport viewport;
        viewport.width  = (f32)swapchain.GetDesc().width;
        viewport.height = (f32)swapchain.GetDesc().height;

        device->SetViewport(cmdList, &viewport);
        Rect2 scissor;
        scissor.minP = {0, 0};
        scissor.maxP = {65536, 65536};

        device->SetScissor(cmdList, scissor);

        PushConstant pc;
        pc.meshParamsDescriptor = device->GetDescriptorIndex(&meshParamsBuffer, ResourceType::SRV);

        RenderMeshes(cmdList, RenderPassType_Main);
        device->EndRenderPass(cmdList);
    }
    debugState.EndTriangleCount(cmdList);
    debugState.EndFrame(cmdList);
    device->SubmitCommandLists();

    currentFrame++;
    currentBuffer = currentFrame % ArrayLength(frameData);

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
    u32 blockSize = GetBlockSize(output->mDesc.mFormat);
    static thread_global Texture bc1Uav;
    TextureDesc desc;
    desc.mWidth        = input->mDesc.mWidth / blockSize;
    desc.mHeight       = input->mDesc.mHeight / blockSize;
    desc.mFormat       = Format::R32G32_UINT;
    desc.mInitialUsage = ResourceUsage::StorageImage;
    desc.mFutureUsages = ResourceUsage::TransferSrc;
    desc.mTextureType  = TextureDesc::TextureType::Texture2D;

    if (!bc1Uav.IsValid() || bc1Uav.mDesc.mWidth < desc.mWidth || bc1Uav.mDesc.mHeight < desc.mHeight)
    {
        TextureDesc bcDesc = desc;
        bcDesc.mWidth      = (u32)GetNextPowerOfTwo(desc.mWidth);
        bcDesc.mHeight     = (u32)GetNextPowerOfTwo(desc.mHeight);

        device->CreateTexture(&bc1Uav, bcDesc, 0);
        device->SetName(&bc1Uav, "BC1 UAV");
    }

    // Output block compression to intermediate texture
    Shader *shader = &shaders[ShaderType_BC1_CS];
    device->BindCompute(&blockCompressPipeline, cmd);
    device->BindResource(&bc1Uav, ResourceType::UAV, 0, cmd);
    device->BindResource(input, ResourceType::SRV, 0, cmd);

    device->UpdateDescriptorSet(cmd);
    device->Dispatch(cmd, (desc.mWidth + 7) / 8, (desc.mHeight + 7) / 8, 1);

    // Copy from uav to output
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&bc1Uav, PipelineFlag_Compute, PipelineFlag_Transfer,
                              AccessFlag_ShaderWrite, AccessFlag_TransferRead,
                              ImageLayout_General, ImageLayout_TransferSrc),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
    device->CopyTexture(cmd, output, &bc1Uav);
    device->DeleteTexture(input);

    // Transfer the block compressed texture to its initial format
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&bc1Uav, PipelineFlag_Transfer, PipelineFlag_Compute,
                              AccessFlag_TransferRead, AccessFlag_ShaderWrite,
                              ImageLayout_TransferSrc, ImageLayout_General),
            GPUBarrier::Image(output, PipelineFlag_Transfer, PipelineFlag_FragmentShader,
                              AccessFlag_TransferWrite, AccessFlag_ShaderRead,
                              ImageLayout_TransferDst, ImageLayout_ShaderRead),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
}

} // namespace render
