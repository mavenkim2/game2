struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct Primitive
{
    u32 vertexCount;
    u32 indexCount;
    // For instancing
    Mat4 *transforms = 0;
};

struct DebugRenderer
{
    Array(DebugVertex) lines;
    Array(DebugVertex) points;

    Array(DebugVertex) indexLines;
    Array(u32) indices;

    Primitive* primitives = 0;

    u32 vbo;
    u32 ebo;
};
