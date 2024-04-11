const i32 cNumFrames = 3;
enum BufferType
{
    // BufferType_Pos,
    // BufferType_Skinned,
    BufferType_Vertex,
    BufferType_Index,
};

enum BufferUsageType
{
    BufferUsage_Static,
    BufferUsage_Dynamic,
};

struct GPUBuffer
{
    R_BufferHandle handle;
    u8 *mappedBufferBase;
    i32 size;
    i32 offset;
    BufferType type;
};

struct VertexCache
{
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    BufferUsageType type;
};

struct VertexCacheState
{
    VertexCache staticData;
    VertexCache frameData[cNumFrames];
    i32 currentFrame;
    i32 currentIndex;
    i32 currentDrawIndex;
};

static VertexCacheState gVertexCache;

internal VC_Handle VC_AllocateBuffer(BufferType bufferType, BufferUsageType usageType, void *data, i32 size);
