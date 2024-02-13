struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct DebugRenderer
{
    ArrayDef(DebugVertex) lines;
    ArrayDef(DebugVertex) points;

    u32 vbo;
};
