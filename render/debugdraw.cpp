global V4 Color_Red   = {1, 0, 0, 1};
global V4 Color_Green = {0, 1, 0, 1};
global V4 Color_Blue  = {0, 0, 1, 1};
global V4 Color_Black = {0, 0, 0, 1};
#define DEFAULT_SECTORS 12
#define DEFAULT_STACKS  12

internal void InitializeDebug(Arena* arena, DebugRenderer *debug)
{
    ArrayInit(arena, debug->lines, DebugVertex, MAX_DEBUG_VERTICES);
    ArrayInit(arena, debug->points, DebugVertex, MAX_DEBUG_VERTICES);
    ArrayInit(arena, debug->indexLines, DebugVertex, MAX_DEBUG_VERTICES);
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
            DebugVertex vertex;
            vertex.pos   = MakeV3(x, y, z);
            vertex.color = Color_Blue;

            ArrayPush(&debug->indexLines, vertex);

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

    Primitive p;
    p.vertexCount = debug->indexLines.count;
    p.indexCount = debug->indices.count;
    ArrayPush2(debug->primitives, p);
    // ArrayPush2(debug->baseVertex, debug->indexLines.count);
    // ArrayPush2(debug->indexCount, debug->indices.count);
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

// TODO: initialize a debug sphere vbo at the start, have draw sphere take in a transform, then
// call glDrawElementsInstanced() : )
// Also LOD. the spheres look trippy when you get far enough from them
internal void DrawSphere(DebugRenderer *debug, ConvexShape *sphere, V4 color)
{
    // TODO: hard code 
    Mat4 transform = Translate(Scale(sphere->radius), sphere->center);
    ArrayPush2(debug->primitives[0].transforms, transform);
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
