global V4 Color_Red   = {1, 0, 0, 1};
global V4 Color_Green = {0, 1, 0, 1};
global V4 Color_Blue  = {0, 0, 1, 1};

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

internal void DrawSphere(DebugRenderer *debug, Sphere* sphere) {
    
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
