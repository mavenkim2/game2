global V4 Color_Red   = {1, 0, 0, 1};
global V4 Color_Green = {0, 1, 0, 1};
global V4 Color_Blue  = {0, 0, 1, 1};
global V4 Color_Black = {0, 0, 0, 1};
#define DEFAULT_SECTORS 12
#define DEFAULT_STACKS  12

internal void InitializeRenderer(Arena *arena, RenderState *state)
{
    ArrayInit(arena, state->commands, RenderCommand, MAX_COMMANDS);
    DebugRenderer *debug = &state->debugRenderer;

    // Create sphere primitive
    ArrayInit(arena, debug->lines, DebugVertex, MAX_DEBUG_VERTICES);
    ArrayInit(arena, debug->points, DebugVertex, MAX_DEBUG_VERTICES);
    ArrayInit(arena, debug->indexLines, V3, MAX_DEBUG_VERTICES);
    ArrayInit(arena, debug->indices, u32, MAX_DEBUG_VERTICES * 5);

    u32 sectors = DEFAULT_SECTORS;
    u32 stacks  = DEFAULT_STACKS;

    f32 x, y, z;
    f32 sectorStep = 2 * PI / sectors;
    f32 stackStep  = PI / stacks;

    f32 theta = 0;

    // Vertices
    loopi(0, sectors + 1)
    {
        f32 phi = -PI / 2;
        loopj(0, stacks + 1)
        {
            x = Cos(phi) * Cos(theta);
            y = Cos(phi) * Sin(theta);
            z = Sin(phi);

            ArrayPush(&debug->indexLines, MakeV3(x, y, z));

            phi += stackStep;
        }
        theta += sectorStep;
    }

    // Indices
    loopi(0, sectors)
    {
        u32 i1 = i * (stacks + 1);
        u32 i2 = i1 + (stacks + 1);
        loopj(0, stacks)
        {
            ArrayPush(&debug->indices, i1);
            ArrayPush(&debug->indices, i2);
            ArrayPush(&debug->indices, i1);
            ArrayPush(&debug->indices, i1 + 1);
            i1++;
            i2++;
        }
    }

    // Cube
    Primitive p;
    p.vertexCount = debug->indexLines.count;
    p.indexCount  = debug->indices.count;
    ArrayPut(debug->primitives, p);

    // Create cube primitive
    f32 point = 1;
    V3 p0     = {-point, -point, -point};
    V3 p1     = {point, -point, -point};
    V3 p2     = {point, point, -point};
    V3 p3     = {-point, point, -point};
    V3 p4     = {-point, -point, point};
    V3 p5     = {point, -point, point};
    V3 p6     = {point, point, point};
    V3 p7     = {-point, point, point};

    // TODO: be able to change topology
    ArrayPush(&debug->indexLines, p0);
    ArrayPush(&debug->indexLines, p1);
    ArrayPush(&debug->indexLines, p2);
    ArrayPush(&debug->indexLines, p3);
    ArrayPush(&debug->indexLines, p4);
    ArrayPush(&debug->indexLines, p5);
    ArrayPush(&debug->indexLines, p6);
    ArrayPush(&debug->indexLines, p7);

    // -X
    ArrayPush(&debug->indices, 3);
    ArrayPush(&debug->indices, 0);
    ArrayPush(&debug->indices, 0);
    ArrayPush(&debug->indices, 4);
    ArrayPush(&debug->indices, 4);
    ArrayPush(&debug->indices, 7);
    ArrayPush(&debug->indices, 7);
    ArrayPush(&debug->indices, 3);
    // +X
    ArrayPush(&debug->indices, 1);
    ArrayPush(&debug->indices, 2);
    ArrayPush(&debug->indices, 2);
    ArrayPush(&debug->indices, 6);
    ArrayPush(&debug->indices, 6);
    ArrayPush(&debug->indices, 5);
    ArrayPush(&debug->indices, 5);
    ArrayPush(&debug->indices, 1);

    // -Z
    ArrayPush(&debug->indices, 0);
    ArrayPush(&debug->indices, 1);
    ArrayPush(&debug->indices, 2);
    ArrayPush(&debug->indices, 3);

    // +Z
    ArrayPush(&debug->indices, 4);
    ArrayPush(&debug->indices, 5);
    ArrayPush(&debug->indices, 6);
    ArrayPush(&debug->indices, 7);

    p.vertexCount = 8;
    p.indexCount  = 24;
    ArrayPut(debug->primitives, p);
}

internal void BeginRenderFrame(RenderState *state)
{
    state->commands.count             = 0;
    state->debugRenderer.lines.count  = 0;
    state->debugRenderer.points.count = 0;
    Primitive *primitive;
    forEach(state->debugRenderer.primitives, primitive)
    {
        ArraySetLen(primitive->transforms, 0);
        ArraySetLen(primitive->colors, 0);
    }
}

// internal void PushTexture(Texture texture, Model *model)
// {
//     ArrayPush(&model->textures, texture);
// }

internal void PushTexture(Model *model, u32 id)
{
    ArrayPush(&model->textureHandles, id);
}

internal void PushModel(RenderState *state, Model *model, Mat4 *finalTransforms = 0)
{
    RenderCommand command;
    command.model     = model;
    command.transform = Identity();
    if (finalTransforms)
    {
        command.finalBoneTransforms = finalTransforms;
    }
    ArrayPush(&state->commands, command);
}

internal void DrawLine(DebugRenderer *debug, V3 from, V3 to, V4 color)
{
    DebugVertex v1;
    v1.pos   = from;
    v1.color = color;
    DebugVertex v2;
    v2.pos   = to;
    v2.color = color;

    ArrayPush(&debug->lines, v1);
    ArrayPush(&debug->lines, v2);
}

internal void DrawPoint(DebugRenderer *debug, V3 point, V4 color)
{
    DebugVertex p;
    p.pos   = point;
    p.color = color;
    ArrayPush(&debug->points, p);
}

internal void DrawArrow(DebugRenderer *debug, V3 from, V3 to, V4 color, f32 size)
{
    DrawLine(debug, from, to, color);

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

        DrawLine(debug, to - dir + perp, to, color);
        DrawLine(debug, to - dir - perp, to, color);
    }
}

enum PrimitiveType
{
    Primitive_Sphere,
    Primitive_Box,
    Primitive_MAX,
};

internal void DrawBox(DebugRenderer *debug, V3 offset, V3 scale, V4 color)
{
    Mat4 transform = Translate(Scale(scale), offset);
    ArrayPut(debug->primitives[Primitive_Box].transforms, transform);
    ArrayPut(debug->primitives[Primitive_Box].colors, color);
}

// TODO: LOD
internal void DrawSphere(DebugRenderer *debug, V3 offset, f32 radius, V4 color)
{
    // TODO: hard code
    Mat4 transform = Translate(Scale(radius), offset);
    ArrayPut(debug->primitives[Primitive_Sphere].transforms, transform);
    ArrayPut(debug->primitives[Primitive_Sphere].colors, color);
}

internal void DebugDrawSkeleton(DebugRenderer *debug, Model *model, Mat4 *finalTransform)
{
    Skeleton *skeleton = &model->skeleton;
    loopi(0, skeleton->count)
    {
        u32 parentId = skeleton->parents.items[i];
        if (parentId != -1)
        {
            V3 childPoint = model->transform *
                            GetTranslation(finalTransform[i] * Inverse(skeleton->inverseBindPoses.items[i]));
            V3 parentPoint =
                model->transform *
                GetTranslation(finalTransform[parentId] * Inverse(skeleton->inverseBindPoses.items[parentId]));
            DrawLine(debug, parentPoint, childPoint, Color_Green);
        }
    }
}
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
