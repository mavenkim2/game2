struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct DebugRenderer
{
    ArrayDef(DebugVertex) lines;
    ArrayDef(DebugVertex) points;

    ArrayDef(DebugVertex) indexLines;
    ArrayDef(u32) indices;

    u32 vbo;
    u32 ebo;
};
