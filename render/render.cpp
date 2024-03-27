#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../asset.h"
#include "../asset_cache.h"
#include "render.h"
#include "render_core.h"
#endif

global const V4 Color_Red               = {1, 0, 0, 1};
global const V4 Color_Green             = {0, 1, 0, 1};
global const V4 Color_Blue              = {0, 0, 1, 1};
global const V4 Color_Black             = {0, 0, 0, 1};
global const u32 r_primitiveSizeTable[] = {sizeof(R_LineInst), sizeof(DebugVertex), sizeof(R_PrimitiveInst),
                                           sizeof(R_PrimitiveInst)};
#define DEFAULT_SECTORS 12
#define DEFAULT_STACKS  12

struct D_State
{
    Arena *arena;
    u64 frameStartPos;
    RenderState *state;
};
global D_State *d_state;

internal void D_Init(RenderState *state)
{
    Arena *arena   = ArenaAlloc();
    d_state        = PushStruct(arena, D_State);
    d_state->arena = arena;
    d_state->state = state;

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
            case R_PassType_StaticMesh:
            {
                state->passes[type].passStatic = PushStruct(arena, R_PassStaticMesh);
                break;
            }
            case R_PassType_SkinnedMesh:
            {
                state->passes[type].passSkinned = PushStruct(arena, R_PassSkinnedMesh);
                break;
            }
            default: Assert(!"Invalid default case");
        }
    }
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
                    {-point, -point, -point}, {point, -point, -point}, {point, point, -point},
                    {-point, point, -point},  {-point, -point, point}, {point, -point, point},
                    {point, point, point},    {-point, point, point},
                };

                u32 cubeIndices[] = {
                    3, 0, 0, 4, 4, 7, 7, 3, 1, 2, 2, 6, 6, 5, 5, 1, 0, 1, 2, 3, 4, 5, 6, 7,
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
    d_state->frameStartPos = ArenaPos(arena);
}

internal void D_BeginFrame()
{
    RenderState *state = d_state->state;
    ArenaPopTo(d_state->arena, d_state->frameStartPos);
    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        switch (type)
        {
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
            case R_PassType_SkinnedMesh:
            {
                R_PassSkinnedMesh *pass = state->passes[type].passSkinned;
                pass->list.first = pass->list.last = 0;
            }
            default:
            {
                break;
            }
        }
    }
}

internal void D_PushModel(AS_Handle loadedModel, Mat4 transform, Mat4 *skinningMatrices = 0,
                          u32 skinningMatricesCount = 0)
{
    RenderState *state = d_state->state;
    if (!IsModelHandleNil(loadedModel))
    {
        if (skinningMatricesCount)
        {
            R_PassSkinnedMesh *pass         = R_GetPassFromKind(R_PassType_SkinnedMesh)->passSkinned;
            R_SkinnedMeshParamsNode *node   = PushStruct(d_state->arena, R_SkinnedMeshParamsNode);
            node->val.transform             = transform;
            node->val.model                 = loadedModel;
            node->val.skinningMatrices      = skinningMatrices;
            node->val.skinningMatricesCount = skinningMatricesCount;
            QueuePush(pass->list.first, pass->list.last, node);
        }
        else
        {
            R_PassStaticMesh *pass       = R_GetPassFromKind(R_PassType_StaticMesh)->passStatic;
            R_StaticMeshParamsNode *node = PushStruct(d_state->arena, R_StaticMeshParamsNode);
            node->val.transform          = transform;
            node->val.mesh               = loadedModel;
            QueuePush(pass->list.first, pass->list.last, node);
        }
    }
}

struct D_FontAlignment
{
    V2 start;
    V2 advance;
};

struct R_RectInst
{
    V2 pos;
    f32 scale;
    R_Handle handle;
};

// in this case the handle will be a pair of indices into an array of sampler2DArrays
internal void D_PushRect(Rect2 rect, R_Handle img)
{
    f32 scale = (rect.maxP - rect.minP) / 2;
    V2 pos    = rec.minP;

    R_RectInst *inst = (R_RectInst *)R_BatchListPush(&group->batchList, 256);
    inst->pos        = pos;
    inst->scale      = scale;
    inst->handle     = img;
}

// ideal is to do all of this in oe draw call
internal void D_PushText(string line)
{
    static D_FontAlignment d_fontAlignment;
    d_fontAlignment.advance = {50, 50};
    RenderState *state      = d_state->state;
    R_PassUI *pass          = R_GetPassFromKind(R_PassType_UI)->passUI;

    // alignment, some starting spot, some context? of where to start?
    // how do I get the bitmap I need? it should NOT be a filename
    f32 startX = d_fontAlignment.start.x;
    f32 startY = d_fontAlignment.start.y;
    for (u32 i = 0; i < line.size; i++)
    {
        R_Handle font = AS_GetCharacter(line.str[i]);
        V2 size       = {50, 50};
        Rect2 rect    = CreateRectFromBottomLeft({startX, startY}, size);
        D_PushRect(rect, font);
        startX += d_fontAlignment.advance.x;
    }
    d_fontAlignment.start.y = d_fontAlignment.start.y - d_fontAlignment.advance.y;
}

inline R_Pass *R_GetPassFromKind(R_PassType type)
{
    RenderState *state = d_state->state;
    R_Pass *result     = &state->passes[type];
    return result;
}

internal u8 *R_BatchListPush(R_BatchList *list, u32 instCap)
{
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
        V3 perp = Normalize(Cross(Cross(dir, up), dir));

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

internal void DrawSphere(V3 offset, f32 radius, V4 color)
{
    Mat4 transform        = Translate(Scale(radius), offset);
    R_Pass3D *pass3D      = R_GetPassFromKind(R_PassType_3D)->pass3D;
    R_Batch3DGroup *group = &pass3D->groups[R_Primitive_Sphere];
    R_PrimitiveInst *inst = (R_PrimitiveInst *)R_BatchListPush(&group->batchList, 256);
    inst->color           = color;
    inst->transform       = transform;
}

internal void DebugDrawSkeleton(AS_Handle model, Mat4 transform, Mat4 *skinningMatrices)
{
    LoadedSkeleton *skeleton = GetSkeletonFromModel(model);
    loopi(0, skeleton->count)
    {
        u32 parentId = skeleton->parents[i];
        if (parentId != -1)
        {
            V3 childTranslation = GetTranslation(skinningMatrices[i] * Inverse(skeleton->inverseBindPoses[i]));
            V3 childPoint       = transform * childTranslation;
            V3 parentTranslation =
                GetTranslation(skinningMatrices[parentId] * Inverse(skeleton->inverseBindPoses[parentId]));
            V3 parentPoint = transform * parentTranslation;
            DrawLine(parentPoint, childPoint, Color_Green);
        }
    }
}

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
