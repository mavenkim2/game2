struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct DebugRenderer
{
    ArrayDef(DebugVertex) vertices;
    u32 vbo;
};
