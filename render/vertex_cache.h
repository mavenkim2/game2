const i32 cVertCacheNumFrames       = 3;
const i32 VERTEX_CACHE_STATIC_MASK  = 1;
const i32 VERTEX_CACHE_OFFSET_SHIFT = 1;
const i32 VERTEX_CACHE_OFFSET_MASK  = 0x1ffffff;
const i32 VERTEX_CACHE_SIZE_SHIFT   = 26;
const i32 VERTEX_CACHE_SIZE_MASK    = 0x1ffffff;
const i32 VERTEX_CACHE_FRAME_SHIFT  = 51;
const i32 VERTEX_CACHE_FRAME_MASK   = 0x1fff;

// must be smaller than 2^25 - 1
const i32 VERTEX_CACHE_BUFFER_SIZE = 31 * 1024 * 1024;

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
    void VC_Init();
    void VC_BeginGPUSubmit();
    GPUBuffer *VC_GetBufferFromHandle(VC_Handle handle, BufferType type);
    VC_Handle VC_AllocateBuffer(BufferType bufferType, BufferUsageType usageType, void *data, i32 elementSize,
                                i32 count);

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
