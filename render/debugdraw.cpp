internal void DrawLine(DebugRenderer *debug, V3 from, V3 to, V4 color)
{
    DebugVertex v1;
    v1.pos   = from;
    v1.color = color;
    DebugVertex v2;
    v2.pos   = to;
    v2.color = color;

    ArrayPush(&debug->vertices, v1);
    ArrayPush(&debug->vertices, v2);
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
