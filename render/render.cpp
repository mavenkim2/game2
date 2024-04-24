#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../keepmovingforward_math.h"
#include <algorithm>
#include "../asset.h"
#include "../asset_cache.h"
#include "render.h"
#include "render_core.h"
#include "../font.h"
#include "../font.cpp"
#include "vertex_cache.h"
#endif

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

                    u32 cubeIndices[] = {
                        3,
                        0,
                        0,
                        4,
                        4,
                        7,
                        7,
                        3,
                        1,
                        2,
                        2,
                        6,
                        6,
                        5,
                        5,
                        1,
                        0,
                        1,
                        2,
                        3,
                        4,
                        5,
                        6,
                        7,
                    };
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

    state->vertexCache.VC_Init();
    RenderFrameDataInit();

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

internal void D_EndFrame()
{
    RenderState *renderState = engine->GetRenderState();
    renderState->drawParams  = D_PrepareMeshes();
    R_CascadedShadowMap(&renderState->light, renderState->shadowMapMatrices, renderState->cascadeDistances);
    renderState->vertexCache.VC_BeginGPUSubmit();
    R_SwapFrameData();
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

internal void D_PushHeightmap(Heightmap heightmap)
{
    R_CommandHeightMap *command = (R_CommandHeightMap *)R_CreateCommand(sizeof(*command));
    command->type               = R_CommandType_Heightmap;
    command->heightmap          = heightmap;
}

// so i want to have a ring buffer between the game and the renderer. simple enough. let's start with single
// threaded, similar to what we have now. right now the way the renderer works is that there's a permanent render
// state global, and each frame the simulation/game sends data to the render pass to use, etc etc
// yadda yadda.
internal void D_PushModel(VC_Handle vertexBuffer, VC_Handle indexBuffer, Mat4 transform)
{
    D_State *d_state       = engine->GetDrawState();
    R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
    R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

    pass->list.mTotalSurfaceCount += 1;

    node->val.numSurfaces = 1;
    D_Surface *surface    = (D_Surface *)R_FrameAlloc(sizeof(*surface));
    node->val.surfaces    = surface;

    surface->vertexBuffer = vertexBuffer;
    surface->indexBuffer  = indexBuffer;

    node->val.transform = transform;
    QueuePush(pass->list.first, pass->list.last, node);
}

internal void D_PushModel(AS_Handle loadedModel, Mat4 transform, Mat4 &mvp, Mat4 *skinningMatrices = 0,
                          u32 skinningMatricesCount = 0)
{
    D_State *d_state         = engine->GetDrawState();
    RenderState *renderState = engine->GetRenderState();
    if (!IsModelHandleNil(loadedModel))
    {
        LoadedModel *model = GetModel(loadedModel);

        // Frustum cull;
        // TODO: the bounds for the model are currently wrong because of gltf
        // (for some reason the model is massive and then shrunk down by a skinning matrix :)
        Rect3 bounds = model->bounds;
        bounds.minP  = transform * bounds.minP;
        bounds.maxP  = transform * bounds.maxP;
        DrawBox(bounds, {1, 0, 0, 1});
        if (!D_IsInBounds(model->bounds, mvp))
        {
            return;
        }

        // Skinned mesh
        R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
        R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

        pass->list.mTotalSurfaceCount += model->numMeshes;

        node->val.numSurfaces = model->numMeshes;
        D_Surface *surfaces   = (D_Surface *)R_FrameAlloc(node->val.numSurfaces * sizeof(*surfaces));
        node->val.surfaces    = surfaces;

        for (u32 i = 0; i < model->numMeshes; i++)
        {
            Mesh *mesh         = &model->meshes[i];
            D_Surface *surface = &surfaces[i];

            surface->vertexBuffer = mesh->surface.vertexBuffer;
            surface->indexBuffer  = mesh->surface.indexBuffer;

            // TODO: ptr or copy?
            surface->material = &mesh->material;
        }

        node->val.transform = transform;

        if (skinningMatricesCount)
        {
            node->val.jointHandle = renderState->vertexCache.VC_AllocateBuffer(BufferType_Uniform, BufferUsage_Dynamic, skinningMatrices,
                                                                               sizeof(skinningMatrices[0]), skinningMatricesCount);
        }
        QueuePush(pass->list.first, pass->list.last, node);
    }
}

// prepare to submit to gpu
internal R_MeshPreparedDrawParams *D_PrepareMeshes()
{
    RenderState *renderState             = engine->GetRenderState();
    R_PassMesh *pass                     = renderState->passes[R_PassType_Mesh].passMesh;
    R_MeshPreparedDrawParams *drawParams = (R_MeshPreparedDrawParams *)R_FrameAlloc(sizeof(R_MeshPreparedDrawParams));
    drawParams->mIndirectBuffers         = (R_IndirectCmd *)R_FrameAlloc(sizeof(R_IndirectCmd) * pass->list.mTotalSurfaceCount);
    drawParams->mPerMeshDrawParams       = (R_MeshPerDrawParams *)R_FrameAlloc(sizeof(R_MeshPerDrawParams) * pass->list.mTotalSurfaceCount);

    i32 drawCount = 0;
    // Per mesh
    for (R_MeshParamsNode *node = pass->list.first; node != 0; node = node->next)
    {
        R_MeshParams *params = &node->val;

        i32 jointOffset = -1;
        if (params->jointHandle != 0)
        {
            if (!renderState->vertexCache.CheckCurrent(params->jointHandle))
            {
                continue;
            }
            jointOffset = (i32)(renderState->vertexCache.GetOffset(params->jointHandle) / sizeof(Mat4));
        }

        // Per material
        for (u32 surfaceCount = 0; surfaceCount < params->numSurfaces; surfaceCount++)
        {
            R_IndirectCmd *indirectBuffer     = &drawParams->mIndirectBuffers[drawCount];
            R_MeshPerDrawParams *perMeshParam = &drawParams->mPerMeshDrawParams[drawCount];
            D_Surface *surface                = &params->surfaces[surfaceCount];

            perMeshParam->mTransform   = params->transform;
            perMeshParam->mJointOffset = jointOffset;
            perMeshParam->mIsPBR       = true;

            indirectBuffer->mCount         = (u32)(renderState->vertexCache.GetSize(surface->indexBuffer) / sizeof(u32));
            indirectBuffer->mInstanceCount = 1;
            indirectBuffer->mFirstIndex    = (u32)(renderState->vertexCache.GetOffset(surface->indexBuffer) / sizeof(u32));
            // TODO: this could change
            indirectBuffer->mBaseVertex   = (u32)(renderState->vertexCache.GetOffset(surface->vertexBuffer) / sizeof(MeshVertex));
            indirectBuffer->mBaseInstance = 0;

            drawCount++;
            if (!surface->material)
            {
                continue;
            }
            for (i32 textureIndex = 0; textureIndex < TextureType_Count; textureIndex++)
            {
                R_Handle textureHandle = GetTextureRenderHandle(surface->material->textureHandles[textureIndex]);
                if (textureIndex == TextureType_MR && textureHandle.u64[0] == 0)
                {
                    perMeshParam->mIsPBR = false;
                }
                perMeshParam->mIndex[textureIndex] = (u64)textureHandle.u32[0];
                // TODO: change the shader to reflect that this is a float :)
                perMeshParam->mSlice[textureIndex] = textureHandle.u32[1];
            }
        }
    }
    return drawParams;
}

internal void D_PushLight(Light *light)
{
    RenderState *renderState = engine->GetRenderState();
    renderState->light       = *light;
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

internal void DebugDrawSkeleton(AS_Handle model, Mat4 transform, Mat4 *skinningMatrices, b32 showAxes = 0)
{
    LoadedSkeleton *skeleton = GetSkeletonFromModel(model);
    loopi(0, skeleton->count)
    {
        u32 parentId = skeleton->parents[i];
        if (parentId != -1)
        {
            Mat4 bindPoseMatrix = Inverse(skeleton->inverseBindPoses[i]);
            V3 childTranslation = GetTranslation(skinningMatrices[i] * bindPoseMatrix);
            V3 childPoint       = transform * childTranslation;

            V3 parentTranslation =
                GetTranslation(skinningMatrices[parentId] * Inverse(skeleton->inverseBindPoses[parentId]));
            V3 parentPoint = transform * parentTranslation;
            DrawLine(childPoint, parentPoint, Color_Green);

            if (showAxes)
            {
                V3 axisPoint = Normalize(bindPoseMatrix.columns[0].xyz) + childPoint;
                DrawArrow(childPoint, axisPoint, Color_Red, .2f);

                axisPoint = Normalize(bindPoseMatrix.columns[1].xyz) + childPoint;
                DrawArrow(childPoint, axisPoint, Color_Green, .2f);

                axisPoint = Normalize(bindPoseMatrix.columns[2].xyz) + childPoint;
                DrawArrow(childPoint, axisPoint, Color_Blue, .2f);
            }
        }
    }
}

// interesting trick: having "nodes" all start with the same header, and just casting between them based on
// what the id is. kinda like inheritance except more powerful
//
// also, have one massive buffer for both indices and vertices, instead of allocating separate buffer
// objects for them. then, handles can just be 64 bit numbers that contain the offset, size, and frame

// global R_FrameState *gRenderFrameState;

internal void RenderFrameDataInit()
{
    RenderState *state            = engine->GetRenderState();
    Arena *arena                  = ArenaAlloc(megabytes(256));
    state->renderFrameState.arena = arena;

    for (i32 i = 0; i < cFrameDataNumFrames; i++)
    {
        R_FrameData *frameData = &state->renderFrameState.frameData[i];
        frameData->memory      = PushArray(arena, u8, cFrameBufferSize);
    }
    state->renderFrameState.currentFrame     = 0;
    state->renderFrameState.currentFrameData = &state->renderFrameState.frameData[state->renderFrameState.currentFrame];

    R_FrameData *data = state->renderFrameState.currentFrameData;
    data->head = data->tail = (R_Command *)R_FrameAlloc(sizeof(R_Command));
    data->head->type        = R_CommandType_Null;
    data->head->next        = 0;
}

internal void *R_FrameAlloc(const i32 inSize)
{
    RenderState *state = engine->GetRenderState();
    i32 alignedSize    = AlignPow2(inSize, 16);
    i32 offset         = AtomicAddI32(&state->renderFrameState.currentFrameData->allocated, alignedSize);

    void *out = state->renderFrameState.currentFrameData->memory + offset;
    MemoryZero(out, inSize);

    return out;
}

internal b32 IsAligned(u8 *ptr, i32 alignment)
{
    b32 result = (u64)ptr == AlignPow2((u64)ptr, alignment);
    return result;
}

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

//////////////////////////////
// Lights
//
// Render to depth buffer from the perspective of the light, test vertices against this depth buffer
// to see whether object is in shadow.
// Returns splits cascade distances, and splits + 1 matrices
internal void R_ShadowMapFrusta(i32 splits, f32 splitWeight, Mat4 *outMatrices, f32 *outSplits)
{
    RenderState *renderState = engine->GetRenderState();
    f32 nearZStart           = renderState->nearZ;
    f32 farZEnd              = renderState->farZ;
    f32 nearZ                = nearZStart;
    f32 farZ                 = farZEnd;
    f32 lambda               = splitWeight;
    f32 ratio                = farZEnd / nearZStart;

    for (i32 i = 0; i < splits + 1; i++)
    {
        f32 si = (i + 1) / (f32)(splits + 1);
        if (i > 0)
        {
            nearZ = farZ - (farZ * 0.005f);
        }
        // NOTE: ???
        farZ = 1.005f * lambda * (nearZStart * Powf(ratio, si)) +
               (1 - lambda) * (nearZStart + (farZEnd - nearZStart) * si);

        Mat4 matrix    = Perspective4(renderState->fov, renderState->aspectRatio, nearZ, farZ);
        Mat4 result    = matrix * renderState->viewMatrix;
        outMatrices[i] = Inverse(result);
        if (i <= splits)
        {
            outSplits[i] = farZ;
        }
    }
}

// TODO: frustum culling cuts out fragments that cast shadows. use the light's frustum to cull
// also restrict the bounds to more tightly enclose the bounds of the objects (for better quality shadows)
internal void R_CascadedShadowMap(const Light *inLight, Mat4 *outLightViewProjectionMatrices,
                                  f32 *outCascadeDistances)
{
    RenderState *renderState = engine->GetRenderState();
    Mat4 mvpMatrices[cNumCascades];
    f32 cascadeDistances[cNumCascades];
    R_ShadowMapFrusta(cNumSplits, .9f, mvpMatrices, cascadeDistances);
    Assert(inLight->type == LightType_Directional);

    // Step 0. Set up the mvp matrix for each frusta.

    // Step 1. Get the corners of each of the view frusta in world space.
    V3 frustumVertices[cNumCascades][8];
    for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    {
        for (i32 x = 0; x < 2; x++)
        {
            for (i32 y = 0; y < 2; y++)
            {
                for (i32 z = 0; z < 2; z++)
                {
                    V4 p = {2.f * x - 1, 2.f * y - 1, 2.f * z - 1.f, 1.f};
                    p    = mvpMatrices[cascadeIndex] * p;

                    // Why this?
                    f32 oneOverW                                                  = 1 / p.w;
                    frustumVertices[cascadeIndex][(z << 2) | (y << 1) | (x << 0)] = p.xyz * oneOverW;

                    // test
                    V4 result = Inverse(mvpMatrices[cascadeIndex]) * MakeV4(p.xyz * oneOverW, 1);
                    int asdf  = 0;
                }
            }
        }
    }

    // Step 2. Find light world to view matrix (first get center point of frusta)

    // Light direction is specified from surface -> light origin
    V3 worldUp               = renderState->camera.right;
    V3 centers[cNumCascades] = {};
    Mat4 lightViewMatrices[cNumCascades];
    for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    {
        for (i32 i = 0; i < 8; i++)
        {
            centers[cascadeIndex] += frustumVertices[cascadeIndex][i];
        }
        centers[cascadeIndex] /= 8;
        lightViewMatrices[cascadeIndex] = LookAt4(centers[cascadeIndex] + inLight->dir, centers[cascadeIndex], worldUp);
    }

    Rect3 bounds[cNumCascades];
    for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    {
        Init(&bounds[cascadeIndex]);
        // Loop over each corner of each frusta
        for (i32 i = 0; i < 8; i++)
        {
            V4 result = Transform(lightViewMatrices[cascadeIndex], frustumVertices[cascadeIndex][i]);
            AddBounds(bounds[cascadeIndex], result.xyz);
        }
    }

    // TODO: instead of tightly fitting the frusta, the light's box could be tighter

    for (i32 i = 0; i < cNumCascades; i++)
    {
        // When viewing down the -z axis, the max is the near plane and the min is the far plane.
        Rect3 *currentBounds = &bounds[i];
        // The orthographic projection expects 0 < n < f
        // TODO: use the bounds of the light instead
        f32 zNear = -currentBounds->maxZ;
        f32 zFar  = -currentBounds->minZ;

        f32 extent = zFar - zNear;

        V3 shadowCameraPos = centers[i] - inLight->dir * zNear;
        Mat4 fixedLookAt   = LookAt4(shadowCameraPos, centers[i], worldUp);

        outLightViewProjectionMatrices[i] = Orthographic4(currentBounds->minX, currentBounds->maxX, currentBounds->minY, currentBounds->maxY, 0, extent) * fixedLookAt;
        outCascadeDistances[i]            = cascadeDistances[i];
    }
}

// Constants
// #if 0
//
// struct R_TempMemoryNode
// {
//     u8 *buffer;
//     u32 size;
// };
//
// internal R_Handle R_GetTempMemHandle(R_TempMemoryNode *node)
// {
//     R_Handle result;
//     result.u64[0] = (u64)node;
//     return result;
// }
// internal R_TempMemoryNode *R_GetTempMemFromHandle(R_Handle handle)
// {
//     R_TempMemoryNode *node = (R_TempMemoryNode *)handle.u64[0];
//     return node;
// }
//
// internal R_Handle R_AllocateTemp(u64 size, void **out)
// {
//     // TODO: maybe something better than this
//     R_TempMemoryNode *node = r_state->nodes;
//     if (node != 0)
//     {
//         StackPop(r_state->nodes);
//     }
//     else
//     {
//         node         = PushStruct(r_state->arena, R_TempMemoryNode);
//         node->size   = megabytes(4);
//         node->buffer = PushArray(r_state->arena, u8, node->size);
//         StackPush(r_state->tempMemNodes, node);
//     }
//
//     *out = node->buffer;
//
//     R_Handle result = R_GetTempMemHandle(node);
//     return result;
// }
//
// internal void R_FreeTemp(R_Handle temp)
// {
//     R_TempMemoryNode *node = R_GetTempMemFromHandle(temp);
//     StackPush(r_state->nodes, node);
// }
//
// //////////////////////////////
// //  not sure yet below
// //
//
// // takes data from skinning matrices/game state/dynamic shit or something and then
// //
// // map individual data to a render feature or something, where the render feature defines entry points for
// // A. what data is taken from the game state
// // B. what data is put in the atomic ring buffer
// // C. how data is shoved into the renderer and how it is rendererd :)
//
// internal void R_SubmitRenderPass(RenderState *state)
// {
//     // TODO: maybe also sort here some how
//     AtomicRing *ring = &shared->g2rRing;
//
//     R_Pass3D *pass3D = state->passes[R_PassType_3D];
//     for (u32 i = 0; i < pass3D->groups.numGroups; i++)
//     {
//         R_Batch3DParams *params = &pass3D->groups[i].params;
//
//         params->
//     }
//
//     for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
//     {
//         state->passes[type];
//     }
//     pass->
// }
//
// internal u8 *R_Interpolate(Arena *arena, RenderState *oldState, RenderState *newState, f32 dt)
// {
//     u8 *pushBuffer = PushArray(arena, u8, newState->size);
//     u64 cursor     = 0;
//
//     f32 oldTimestamp = oldState->timestamp;
//     f32 newTimestamp = newState->timestamp;
//
//     for (u32 i = 0; i < ArrayLength(state->objects); i++)
//     {
//         RenderData *oldData = &oldState->objects[i].data;
//         RenderData *newData = &newState->objects[i].data;
//
//         // but this might not be true and for good reason. what if a thing gains attributes or something?
//         // who even knows
//         Assert(oldData->numPieces == newData->numPieces);
//         for (u32 datumIndex = 0; datumIndex < oldData->numPieces; datumIndex++)
//         {
//             RenderDatum *oldDatum = &oldData->piece[datumIndex];
//             RenderDatum *newDatum = &newData->piece[datumIndex];
//
//             u64 datumSize = 0;
//             switch (newDatum->type)
//             {
//                 case DatumType_AnimationTransform:
//                 {
//                     AnimationTransform *oldTform = (AnimationTransform *)oldDatum;
//                     AnimationTransform *newTform = (AnimationTransform *)newDatum;
//
//                     AnimationTransform result = Lerp(oldTform, newTform, dt);
//
//                     MemoryCopy(pushBuffer + cursor, &result, sizeof(result));
//                     cursor += AlignPow2(sizeof(result), 8);
//
//                     break;
//                 }
//                 case DatumType_Position:
//                 {
//                     V3 *oldPosition = (V3 *)oldDatum;
//                     V3 *newPosition = (V3 *)newDatum;
//
//                     V3 result = Lerp(oldPosition, newPosition, dt);
//                     datumSize = sizeof(AnimationTransform);
//
//                     MemoryCopy(pushBuffer + cursor, &result, sizeof(result));
//                     cursor += AlignPow2(sizeof(result), 8);
//                     break;
//                 }
//                 default: Assert(!"Not valid data");
//             }
//         }
//     }
//     return pushBuffer;
// }
//
// internal void R_SubmitFrame(RenderState *state, f32 accumulator, f32 dt)
// {
//     f32 alpha = accumulator / dt;
//
//     R_Interpolate(lastState, newState, alpha);
//     // TODO: Ideally this is just a memcopy.
// }
//
// internal void R_EntryPoint(void *p)
// {
//     for (; shared->running;)
//     {
//         // StartAtomicRead(&shared->g2rRing, sizeof(u64));
//         // u64 readPos = shared->readPos;
//         // u64 size;
//         // RingReadStruct(
//         // R_EndFrame(state);
//     }
// }
//
// // STEPS:
// // 1. extract
// // 2. prepare
// // 3. submit
//
// // this contains static data used in the game. this doesn't change from frame to frame
// enum R_RenderFeatureType
// {
//     R_RenderFeatureType_SkinnedMesh,
//     R_RenderFeatureType_Count,
// };
//
// // contains data for skinned models
// struct R_RenderData
// {
// };
//
// struct R_FrameNode
// {
//     R_FrameNodeUniforms uniforms;
//     R_BatchList batchList;
//     R_FrameNode *next;
//
//     // void *data;
//     // u32 numInstances;
//     // u32 sizeOfInstance;
// };
//
// // cap is 16
// struct R_FrameNodeChunk
// {
//     R_FrameNode *first;
//     R_FrameNode *last;
//     u32 count;
//     u32 cap;
// };
//
// struct R_FrameNodeChunkList
// {
//     R_FrameNodeChunk *first;
//     R_FrameNodeChunk *last;
// };
//
// typedef void *R_Params;
//
// struct R_SubmitNode
// {
//     // uniforms
//     R_Params *params;
//
//     // render buffer handles
//     R_Handle renderObject;
//
//     // instance data
//     void *data;
//     u32 numInstances;
//     u32 sizeOfInstance;
// };
//
// // :)
// struct R_FramePacket
// {
//     Arena *arena;
//
//     R_FrameNodeChunkList frameNodeLists[R_RenderFeatureType_Count];
//     // i would like each of these to just be a big dumb block of memory
//     R_SubmitNode submitNodeArrays[R_RenderFeatureType_Count];
// };
//
// // typedef R_FrameNode* R_ExtractData();
//
// #define R_SUBMIT(name) void name(R_SubmitNodeList *list)
// typedef R_SUBMIT(r_submit);
//
// struct R_RenderFeature
// {
//     r_submit *R_Submit;
// };
//
// // and these tables can use different prepare/submit functions based on the version of OPENGL or vulkan.
// // i think the extract should be the same regardless of the renderer backend use
//
// global R_RenderFeature R_RenderFeatureTable[R_RenderFeatureType_Count];
//
// internal void R_InitializeRenderFeatureTable()
// {
//     R_RenderFeatureTable[R_RenderFeatureType_SkinnedMesh] = R_SubmitSkinnedMesh;
// }
//
// internal void R_Submit(R_FramePacket *packet)
// {
//     for (R_RenderFeatureType type = (R_RenderFeatureType)0; type < R_RenderFeatureType_Count;
//          type                     = (R_RenderFeatureType)(type + 1))
//     {
//         R_SubmitNodeList *list = &packet->submitNodeList[type];
//         (R_SubmitTable[type])(list);
//     }
// }
//
// // each submit list uses the same shader.
// R_SUBMIT(R_SubmitSkinnedMesh)
// {
//     GLuint modelProgramId = openGL->shaders[R_ShaderType_SkinnedMesh].id;
//     openGL->glUseProgram(modelProgramId);
//
//     for (R_SubmitNode *node = list->first; node != 0; node = node->next)
//     {
//         R_SkinnedMeshParams *params = (R_SkinnedMeshParams *)node->params;
//         node->bufferData            = ;
//         R_SubmitNodeList *node      = 0;
//     }
// }
//
// struct R_FramePacketRing
// {
//     R_FramePacket framePackets[4];
//
//     // position of the last frame state, so framePacket[readPos] is the last frame, framePacket[readPos + 1]
//     is the
//     // new frame
//     u32 readPos;
//     // position the game can write to
//     u32 writePos;
// };
//
// internal R_FramePacket *R_StartFrame()
// {
//     R_FramePacket *result = 0;
//
//     R_FramePacketRing *ring = &r_state->framePacketRing;
//     u32 readPos             = ring->readPos;
//     u32 writePos            = ring->readPos;
//
//     u32 length = ArrayLength(ring->framePackets);
//     // check if power of 2
//     Assert(length & (length - 1) == 0);
//     u32 modulo = length - 1;
//
//     u32 spotsBetween = (writePos + length - readPos) & (modulo);
//     // u32 availableSpots = length - (writePos - readPos);
//     if (spotsBetween < length)
//     {
//         result = ring->framePackets[writePos];
//     }
//     return result;
// }
//
// internal void R_SubmitFrame()
// {
//     WriteBarrier();
//     r_state->framePacketRing.writePos =
//         (r_state->framePacketRing.writePos + 1) & (ArrayLength(r_state->framePackets) - 1);
// }
//
// const u32 cNumFrames = 2;
//
// internal R_FramePacket *R_ReceiveFrame()
// {
//     R_FramePacket *result   = 0;
//     R_FramePacketRing *ring = &r_state->framePacketRing;
//
//     u32 readPos  = ring->readPos;
//     u32 writePos = ring->readPos;
//
//     u32 length = ArrayLength(ring->framePackets);
//     u32 modulo = length - 1;
//
//     // the read position should be cNumFrames behind the write position
//     u32 spotsBetween = ((writePos + length - readPos) & (modulo)); // - cNumFrames;
//     if (spotsBetween > 2)
//     {
//         u32 spotsToMove = spotsBetween - cNumFrames;
//         WriteBarrier();
//         ring->readPos = (readPos + spotsToMove) & (modulo);
//     }
//     R_FramePacket *packet = &ring->framePackets[ring->readPos];
//
//     return packet;
// }
// #endif

//////////////////////////////
// UI
//

// internal void DrawRectangle(RenderState* renderState, V2 min, V2 max) {
//     renderState->
//         DrawRectangle(&renderState, V2(0, 0), V2(renderState.width / 10, renderState.height / 10));
// }

/* internal RenderGroup BeginRenderGroup(OpenGL* openGL) {
    RenderGroup* group = &openGL->group;
    // TODO: if the number is any higher, then u16 index cannot represent
    u32 maxQuadCountPerFrame = 1 << 14;
    group.maxVertexCount = maxQuadCountPerFrame * 4;
    group.maxIndexCount = maxQuadCountPerFrame * 6;
    group.indexCount = 0;
    group.vertexCount = 0;

    return group;
} */

// internal void PushCube(RenderCommand* commands, V3 pos, V3 radius, V4 color)
// {
//     f32 minX = pos.x - radius.x;
//     f32 maxX = pos.x + radius.x;
//     f32 minY = pos.y - radius.y;
//     f32 maxY = pos.y + radius.y;
//     f32 minZ = pos.z - radius.z;
//     f32 maxZ = pos.z + radius.z;
//
//     V3 p0 = {minX, minY, minZ};
//     V3 p1 = {maxX, minY, minZ};
//     V3 p2 = {maxX, maxY, minZ};
//     V3 p3 = {minX, maxY, minZ};
//     V3 p4 = {minX, minY, maxZ};
//     V3 p5 = {maxX, minY, maxZ};
//     V3 p6 = {maxX, maxY, maxZ};
//     V3 p7 = {minX, maxY, maxZ};
//
//     Mesh mesh;
//
//     // NOTE: winding must be counterclockwise if it's a front face
//     //  -X
//     PushQuad(commands, p3, p0, p4, p7, V3{-1, 0, 0}, color);
//     // +X
//     PushQuad(commands, p1, p2, p6, p5, V3{1, 0, 0}, color);
//     // -Y
//     PushQuad(commands, p0, p1, p5, p4, V3{0, -1, 0}, color);
//     // +Y
//     PushQuad(commands, p2, p3, p7, p6, V3{0, 1, 0}, color);
//     // -Z
//     PushQuad(commands, p3, p2, p1, p0, V3{0, 0, -1}, color);
//     // +Z
//     PushQuad(commands, p4, p5, p6, p7, V3{0, 0, 1}, color);
//
// }
//
// internal void PushQuad(RenderCommand *commands, V3 p0, V3 p1, V3 p2, V3 p3, V3 n, V4 color)
// {
//     // NOTE: per quad there is 4 new vertices, 6 indices, but the 6 indices are
//     // 4 different numbers.
//     Mesh
//     u32 vertexIndex = group->vertexCount;
//     u32 indexIndex  = group->indexCount;
//     group->vertexCount += 4;
//     group->indexCount += 6;
//     Assert(group->vertexCount <= group->maxVertexCount);
//     Assert(group->indexCount <= group->maxIndexCount);
//
//     RenderVertex *vertex = group->vertexArray + vertexIndex;
//     u16 *index           = group->indexArray + indexIndex;
//
//     vertex[0].p     = MakeV4(p0, 1.f);
//     vertex[0].color = color.rgb;
//     vertex[0].n     = n;
//
//     vertex[1].p     = MakeV4(p1, 1.f);
//     vertex[1].color = color.rgb;
//     vertex[1].n     = n;
//
//     vertex[2].p     = MakeV4(p2, 1.f);
//     vertex[2].color = color.rgb;
//     vertex[2].n     = n;
//
//     vertex[3].p     = MakeV4(p3, 1.f);
//     vertex[3].color = color.rgb;
//     vertex[3].n     = n;
//
//     // TODO: this will blow out the vertex count fast
//     u16 baseIndex = (u16)vertexIndex;
//     Assert((u32)baseIndex == vertexIndex);
//     index[0] = baseIndex + 0;
//     index[1] = baseIndex + 1;
//     index[2] = baseIndex + 2;
//     index[3] = baseIndex + 0;
//     index[4] = baseIndex + 2;
//     index[5] = baseIndex + 3;
//
//     group->quadCount += 1;
// }
