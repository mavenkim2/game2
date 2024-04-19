const i32 cVertCacheNumFrames = 3;
enum BufferType
{
    BufferType_Vertex,
    BufferType_Index,
    BufferType_Uniform,
};

enum BufferUsageType
{
    BufferUsage_Static,
    BufferUsage_Dynamic,
};

struct GPUBuffer
{
    R_BufferHandle mHandle;
    u8 *mMappedBufferBase;
    i32 mSize;
    i32 mOffset;
    BufferType mType;
};

struct VertexCache
{
    GPUBuffer mVertexBuffer;
    GPUBuffer mIndexBuffer;
    GPUBuffer mUniformBuffer;
    BufferUsageType mType;
};

struct VertexCacheState
{
public:
    b32 CheckStatic(VC_Handle handle);
    b32 CheckSubmitted(VC_Handle handle);
    b32 CheckCurrent(VC_Handle handle);
    inline u64 GetOffset(VC_Handle handle);
    inline u64 GetSize(VC_Handle handle);

    VertexCache mStaticData;
    VertexCache mFrameData[cVertCacheNumFrames];
    i32 mCurrentFrame;
    i32 mCurrentIndex;
    i32 mCurrentDrawIndex;
};

static VertexCacheState gVertexCache;

internal VC_Handle VC_AllocateBuffer(BufferType bufferType, BufferUsageType usageType, void *data, i32 elementSize,
                                     i32 count);
