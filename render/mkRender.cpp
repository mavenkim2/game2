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

    // state->vertexCache.VC_Init();
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

#if 0
internal void D_EndFrame()
{
    RenderState *renderState = engine->GetRenderState();
    R_PassMesh *pass         = R_GetPassFromKind(R_PassType_Mesh)->passMesh;

    // TODO: these need to go somewhere else
    {
        R_SetupViewFrustum();
        for (ViewLight *light = pass->viewLight; light != 0; light = light->next)
        {
            R_CullModelsToLight(light);
            if (light->type == LightType_Directional)
            {
                R_CascadedShadowMap(light, renderState->shadowMapMatrices, renderState->cascadeDistances);
            }
        }

        renderState->drawParams = D_PrepareMeshes(pass->list.first, pass->list.mTotalSurfaceCount);

        // for each light, prepare the meshes that are ONLY SHADOWS for that light. in the future, will
        // need to consider models that cast shadows into the view frustum from multiple lights.
        for (ViewLight *light = pass->viewLight; light != 0; light = light->next)
        {
            light->drawParams = D_PrepareMeshes(light->modelNodes, light->mNumShadowSurfaces);
        }
    }
    renderState->vertexCache.VC_BeginGPUSubmit();
    R_SwapFrameData();
}
#endif

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
#if 0
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

        Rect3 bounds = model->bounds;
        bounds.minP  = transform * bounds.minP;
        bounds.maxP  = transform * bounds.maxP;

        if (bounds.minP.x > bounds.maxP.x)
        {
            Swap(f32, bounds.minP.x, bounds.maxP.x);
        }
        if (bounds.minP.y > bounds.maxP.y)
        {
            Swap(f32, bounds.minP.y, bounds.maxP.y);
        }
        if (bounds.minP.z > bounds.maxP.z)
        {
            Swap(f32, bounds.minP.z, bounds.maxP.z);
        }
        DrawBox(bounds, {1, 0, 0, 1});

        R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
        R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

        node->val.mIsDirectlyVisible = D_IsInBounds(model->bounds, mvp);
        node->val.mBounds            = bounds;
        pass->list.mTotalSurfaceCount += model->numMeshes;

        node->val.numSurfaces = model->numMeshes;
        D_Surface *surfaces   = (D_Surface *)R_FrameAlloc(node->val.numSurfaces * sizeof(*surfaces));
        node->val.surfaces    = surfaces;

        for (u32 i = 0; i < model->numMeshes; i++)
        {
            Mesh *mesh         = &model->meshes[i];
            D_Surface *surface = &surfaces[i];

            surface->vertexBuffer = mesh->surface.mVertexBuffer;
            surface->indexBuffer  = mesh->surface.mIndexBuffer;

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
internal R_MeshPreparedDrawParams *D_PrepareMeshes(R_MeshParamsNode *head, i32 inCount)
{
    RenderState *renderState = engine->GetRenderState();
    // R_PassMesh *pass         = renderState->passes[R_PassType_Mesh].passMesh;

    // Per mesh
    i32 drawCount                        = 0;
    R_MeshPreparedDrawParams *drawParams = (R_MeshPreparedDrawParams *)R_FrameAlloc(sizeof(R_MeshPreparedDrawParams));
    drawParams->mIndirectBuffers         = (R_IndirectCmd *)R_FrameAlloc(sizeof(R_IndirectCmd) * inCount);
    drawParams->mPerMeshDrawParams       = (R_MeshPerDrawParams *)R_FrameAlloc(sizeof(R_MeshPerDrawParams) * inCount);
    for (R_MeshParamsNode *node = head; node != 0; node = node->next)
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
#endif

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
internal void D_PushLight(Light *light)
{
    R_PassMesh *pass = R_GetPassFromKind(R_PassType_Mesh)->passMesh;

    ViewLight *viewLight = (ViewLight *)R_FrameAlloc(sizeof(ViewLight));
    viewLight->type      = light->type;
    viewLight->dir       = light->dir;

    viewLight->next = pass->viewLight;
    pass->viewLight = viewLight;
}

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
        //
        // Increase the resolution by bounding to the frustum
        // Rect2 bounds;
        // bounds.minP = {FLT_MAX, FLT_MAX};
        // bounds.maxP = {-FLT_MAX, -FLT_MAX};
        // for (i32 i = 0; i < ArrayLength(frustumVertices); i++)
        // {
        //     V4 ndc = outLightViewProjectionMatrices[cascadeIndex] * MakeV4(frustumVertices[i], 1.0);
        //     // Assert(ndc.x >= -ndc.w && ndc.x <= ndc.w);
        //     // Assert(ndc.y >= -ndc.w && ndc.y <= ndc.w);
        //     // Assert(ndc.z >= -ndc.w && ndc.z <= ndc.w);
        //     ndc.xyz /= ndc.w;
        //     bounds.minX = Min(bounds.minX, ndc.x);
        //     bounds.minY = Min(bounds.minY, ndc.y);
        //
        //     bounds.maxX = Max(bounds.maxX, ndc.x);
        //     bounds.maxY = Max(bounds.maxX, ndc.y);
        // }
        // V2 boundCenter  = (bounds.minP + bounds.maxP) / 2.f;
        // V2 boundExtents = bounds.maxP - bounds.minP;
        // Mat4 grow       = Scale(MakeV3(1.f / boundExtents.x, 1.f / boundExtents.y, 1.f)) * Translate4(-MakeV3(boundCenter, 0.f));
        //
        // outLightViewProjectionMatrices[cascadeIndex] = grow * outLightViewProjectionMatrices[cascadeIndex];
    }

    // V3 frustumVertices[cNumCascades][8];
    // for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    // {
    //     R_GetFrustumCorners(mvpMatrices[cascadeIndex], BoundsZeroToOneCube, frustumVertices[cascadeIndex]);
    // }
    //
    // // Step 2. Find light world to view matrix (first get center point of frusta)
    //
    // V3 centers[cNumCascades];
    // // Light direction is specified from surface->light origin
    // V3 worldUp = renderState->camera.right;
    // Mat4 lightViewMatrices[cNumCascades];
    // for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    // {
    //     for (i32 i = 0; i < 8; i++)
    //     {
    //         centers[cascadeIndex] += frustumVertices[cascadeIndex][i];
    //     }
    //     centers[cascadeIndex] /= 8;
    //     lightViewMatrices[cascadeIndex] = LookAt4(centers[cascadeIndex] + inLight->dir, centers[cascadeIndex], worldUp);
    // }
    //
    // Rect3 bounds[cNumCascades];
    // for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
    // {
    //     Init(&bounds[cascadeIndex]);
    //     // Loop over each corner of each frusta
    //     for (i32 i = 0; i < 8; i++)
    //     {
    //         V4 result = Transform(lightViewMatrices[cascadeIndex], frustumVertices[cascadeIndex][i]);
    //         AddBounds(bounds[cascadeIndex], result.xyz);
    //     }
    // }
    //
    // // TODO: instead of tightly fitting the frusta, the light's box could be tighter
    //
    // for (i32 i = 0; i < cNumCascades; i++)
    // {
    //     // When viewing down the -z axis, the max is the near plane and the min is the far plane.
    //     Rect3 *currentBounds = &bounds[i];
    //     // The orthographic projection expects 0 < n < f
    //
    //     // TODO: use the bounds of the light instead
    //     f32 zNear = -currentBounds->maxZ - 50;
    //     f32 zFar  = -currentBounds->minZ;
    //
    //     f32 extent = zFar - zNear;
    //
    //     V3 shadowCameraPos = centers[i] - inLight->dir * zNear;
    //     Mat4 fixedLookAt   = LookAt4(shadowCameraPos, centers[i], worldUp);
    //
    //     outLightViewProjectionMatrices[i] = Orthographic4(currentBounds->minX, currentBounds->maxX, currentBounds->minY, currentBounds->maxY, 0, extent) * fixedLookAt;
    //     outCascadeDistances[i]            = cascadeDistances[i];
    // }
}

using namespace graphics;
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

graphics::GPUBuffer ubo;
graphics::GPUBuffer vbo;
graphics::GPUBuffer ibo;

graphics::GPUBuffer skinningBuffer[device->cNumBuffers];
graphics::GPUBuffer skinningBufferUpload[device->cNumBuffers];

InputLayout inputLayouts[IL_Type_Count];
Shader shaders[ShaderType_Count];
RasterizationState rasterizers[RasterType_Count];

Texture depthBufferMain;
Texture shadowDepthBuffer;
list<i32> shadowSlices;

struct DeferredBlockCompressCmd
{
    Texture in;
    Texture *out;
};
DeferredBlockCompressCmd blockCompressRing[256];
std::atomic<u64> volatile blockCompressWrite       = 0;
std::atomic<u64> volatile blockCompressCommitWrite = 0;
u64 blockCompressRead                              = 0;

internal void Initialize()
{
    arena = ArenaAlloc();
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
        GPUBufferDesc desc;
        desc.mSize          = kilobytes(64);
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        device->CreateBuffer(&ubo, desc, 0);

        // Skinning
        desc.mSize          = kilobytes(4);
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mResourceUsage = ResourceUsage::TransferSrc;
        for (u32 i = 0; i < ArrayLength(skinningBufferUpload); i++)
        {
            device->CreateBuffer(&skinningBufferUpload[i], desc, 0);
        }

        desc.mUsage         = MemoryUsage::GPU_ONLY;
        desc.mResourceUsage = ResourceUsage::UniformBuffer;
        for (u32 i = 0; i < ArrayLength(skinningBuffer); i++)
        {
            device->CreateBuffer(&skinningBuffer[i], desc, 0);
        }
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
    shaders[ShaderType_Mesh_VS].mName      = "mesh_vs.hlsl";
    shaders[ShaderType_Mesh_VS].stage      = ShaderStage::Vertex;
    shaders[ShaderType_Mesh_FS].mName      = "mesh_fs.hlsl";
    shaders[ShaderType_Mesh_FS].stage      = ShaderStage::Fragment;
    shaders[ShaderType_ShadowMap_VS].mName = "depth_vs.hlsl";
    shaders[ShaderType_ShadowMap_VS].stage = ShaderStage::Vertex;
    shaders[ShaderType_BC1_CS].mName       = "blockcompress_cs.hlsl";
    shaders[ShaderType_BC1_CS].stage       = ShaderStage::Compute;

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
    }
}

internal void Render()
{
    // TODO: eventually, this should not be accessed from here
    G_State *g_state         = engine->GetGameState();
    RenderState *renderState = engine->GetRenderState();

    // Read through deferred block compress commands
    CommandList cmd;
    {
        u64 readPos    = blockCompressRead;
        u64 endPos     = blockCompressCommitWrite.load();
        const u64 size = ArrayLength(blockCompressRing);
        cmd            = device->BeginCommandList(graphics::QueueType_Compute);
        for (u64 i = readPos; i < endPos; i++)
        {
            DeferredBlockCompressCmd *deferCmd = &blockCompressRing[(i & (size - 1))];
            BlockCompressImage(&deferCmd->in, deferCmd->out, cmd);
            blockCompressRead++;
        }
        std::atomic_thread_fence(std::memory_order_release);
    }

    CommandList cmdList = device->BeginCommandList(QueueType_Graphics);
    device->Wait(cmd, cmdList);
    GPUBuffer *currentSkinBufUpload = &skinningBufferUpload[device->GetCurrentBuffer()];
    GPUBuffer *currentSkinBuf       = &skinningBuffer[device->GetCurrentBuffer()];

    // GPUBarrier barrier = CreateBarrier(currentSkinBufUpload, ResourceUsage::TransferSrc, ResourceUsage::None);
    // device->Barrier(cmdList, &barrier, 1);
    if (g_state->mSkinningBufferSize)
    {
        device->CopyBuffer(cmdList, currentSkinBuf, currentSkinBufUpload, g_state->mSkinningBufferSize);
    }

    // Setup uniform buffer
    {
        Ubo uniforms;
        for (u32 i = 0; i < g_state->mEntityCount; i++)
        {
            ModelParams *modelParams     = &uniforms.rParams[i];
            modelParams->transform       = renderState->transform * g_state->mTransforms[i];
            modelParams->modelViewMatrix = renderState->viewMatrix * g_state->mTransforms[i];
            modelParams->modelMatrix     = g_state->mTransforms[i];
        }
        ViewLight testLight = {};
        testLight.dir       = {0, .5, .5};
        testLight.dir       = Normalize(testLight.dir);
        uniforms.rLightDir  = MakeV4(testLight.dir, 1.0);
        uniforms.rViewPos   = MakeV4(renderState->camera.position, 1.0);

        R_CascadedShadowMap(&testLight, uniforms.rLightViewProjectionMatrices, uniforms.rCascadeDistances.elements);
        device->FrameAllocate(&ubo, &uniforms, cmdList, sizeof(uniforms));
    }

    GPUBarrier barriers[] = {
        GPUBarrier::Buffer(&ubo, ResourceUsage::TransferSrc, ResourceUsage::UniformBuffer),
        GPUBarrier::Buffer(currentSkinBuf, ResourceUsage::TransferDst, ResourceUsage::UniformBuffer),
    };
    device->Barrier(cmdList, barriers, ArrayLength(barriers));

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
            device->BindResource(&ubo, ResourceType::SRV, MODEL_PARAMS_BIND, cmdList);
            device->BindResource(currentSkinBuf, ResourceType::SRV, SKINNING_BIND, cmdList);
            device->UpdateDescriptorSet(cmdList);

            Viewport viewport;
            viewport.width  = (f32)shadowDepthBuffer.mDesc.mWidth;
            viewport.height = (f32)shadowDepthBuffer.mDesc.mHeight;

            device->SetViewport(cmdList, &viewport);
            Rect2 scissor;
            scissor.minP = {0, 0};
            scissor.maxP = {65536, 65536};

            device->SetScissor(cmdList, scissor);

            PushConstant pc;
            pc.cascadeNum = shadowSlice;

            for (u32 entityIndex = 0; entityIndex < g_state->mEntityCount; entityIndex++)
            {
                game::Entity *entity = &g_state->mEntities[entityIndex];
                LoadedModel *model   = GetModel(entity->mAssetHandle);
                pc.skinningOffset    = entity->mSkinningOffset;
                pc.modelIndex        = entityIndex;

                for (u32 meshIndex = 0; meshIndex < model->numMeshes; meshIndex++)
                {
                    Mesh *mesh = &model->meshes[meshIndex];

                    pc.vertexPos        = mesh->vertexPosView.subresourceDescriptor;
                    pc.vertexBoneId     = mesh->vertexBoneIdView.subresourceDescriptor;
                    pc.vertexBoneWeight = mesh->vertexBoneWeightView.subresourceDescriptor;

                    device->PushConstants(cmdList, sizeof(pc), &pc);
                    device->BindIndexBuffer(cmdList, &mesh->buffer, mesh->indexView.offset);

                    device->DrawIndexed(cmdList, mesh->indexCount, 0, 0);
                }
            }
            device->EndRenderPass(cmdList);
        }
    }

    // Main geo pass

    {
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
        device->BindResource(&ubo, ResourceType::SRV, MODEL_PARAMS_BIND, cmdList);
        device->BindResource(currentSkinBuf, ResourceType::SRV, SKINNING_BIND, cmdList);
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

        for (u32 entityIndex = 0; entityIndex < g_state->mEntityCount; entityIndex++)
        {
            game::Entity *entity = &g_state->mEntities[entityIndex];
            LoadedModel *model   = GetModel(entity->mAssetHandle);
            PushConstant pc;
            pc.modelIndex     = entityIndex;
            pc.skinningOffset = entity->mSkinningOffset;

            for (u32 meshIndex = 0; meshIndex < model->numMeshes; meshIndex++)
            {
                Mesh *mesh = &model->meshes[meshIndex];
                // TODO: multi draw indirect somehow
                u32 baseVertex = 0;
                for (u32 subsetIndex = 0; subsetIndex < mesh->numSubsets; subsetIndex++)
                {
                    Mesh::MeshSubset *subset           = &mesh->subsets[subsetIndex];
                    scene::MaterialComponent &material = gameScene.materials[subset->materialIndex];

                    graphics::Texture *texture = GetTexture(material.textures[TextureType_Diffuse]);
                    i32 descriptorIndex        = device->GetDescriptorIndex(texture);
                    pc.albedo                  = descriptorIndex;

                    texture         = GetTexture(material.textures[TextureType_Normal]);
                    descriptorIndex = device->GetDescriptorIndex(texture);
                    pc.normal       = descriptorIndex;

                    pc.vertexPos        = mesh->vertexPosView.subresourceDescriptor;
                    pc.vertexNor        = mesh->vertexNorView.subresourceDescriptor;
                    pc.vertexTan        = mesh->vertexTanView.subresourceDescriptor;
                    pc.vertexUv         = mesh->vertexUvView.subresourceDescriptor;
                    pc.vertexBoneId     = mesh->vertexBoneIdView.subresourceDescriptor;
                    pc.vertexBoneWeight = mesh->vertexBoneWeightView.subresourceDescriptor;
                    pc.meshTransform    = mesh->transform;

                    device->PushConstants(cmdList, sizeof(pc), &pc);

                    device->BindIndexBuffer(cmdList, &mesh->buffer, mesh->indexView.offset);

                    device->DrawIndexed(cmdList, subset->indexCount, subset->indexStart, 0);
                    baseVertex += subset->indexCount;
                }
            }
        }
        device->EndRenderPass(cmdList);
    }
    device->SubmitCommandLists();
}

void DeferBlockCompress(graphics::Texture input, graphics::Texture *output)
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
    static Texture bc1Uav;
    TextureDesc desc;
    desc.mWidth        = input->mDesc.mWidth / blockSize;
    desc.mHeight       = input->mDesc.mHeight / blockSize;
    desc.mFormat       = Format::R32G32_UINT;
    desc.mInitialUsage = ResourceUsage::StorageImage;
    desc.mFutureUsages = ResourceUsage::TransferSrc;
    desc.mTextureType  = TextureDesc::TextureType::Texture2D;

    if (!bc1Uav.IsValid() || bc1Uav.mDesc.mWidth < output->mDesc.mWidth || bc1Uav.mDesc.mHeight < output->mDesc.mHeight)
    {
        TextureDesc bcDesc = desc;
        bcDesc.mWidth      = (u32)GetNextPowerOfTwo(output->mDesc.mWidth);
        bcDesc.mHeight     = (u32)GetNextPowerOfTwo(output->mDesc.mHeight);

        device->CreateTexture(&bc1Uav, desc, 0);
        device->SetName(&bc1Uav, "BC1 Tex");
    }

    // Output block compression to intermediate texture
    Shader *shader = &shaders[ShaderType_BC1_CS];
    device->BindCompute(&blockCompressPipeline, cmd);
    device->BindResource(&bc1Uav, ResourceType::UAV, 0, cmd);
    device->BindResource(input, ResourceType::SRV, 0, cmd);

    device->UpdateDescriptorSet(cmd);
    device->Dispatch(cmd, (bc1Uav.mDesc.mWidth + 7) / 8, (bc1Uav.mDesc.mHeight + 7) / 8, 1);

    // Copy from uav to output
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&bc1Uav, ResourceUsage::StorageImage, ResourceUsage::TransferSrc),
            // GPUBarrier::Image(output, ResourceUsage::SampledImage, ResourceUsage::TransferDst),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
    device->CopyTexture(cmd, output, &bc1Uav);
    device->DeleteTexture(input);

    // Transfer the block compressed texture to its initial format
    {
        GPUBarrier barriers[] = {
            GPUBarrier::Image(&bc1Uav, ResourceUsage::TransferSrc, ResourceUsage::StorageImage),
            GPUBarrier::Image(output, ResourceUsage::TransferDst, ResourceUsage::SampledImage),
        };
        device->Barrier(cmd, barriers, ArrayLength(barriers));
    }
}

} // namespace render
