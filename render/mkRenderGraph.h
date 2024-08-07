#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include "mkGraphics.h"
namespace rendergraph
{

typedef u32 PassHandle;
// typedef u32 BufferHandle;
// typedef u32 TextureHandle;
// const BufferHandle INVALID_BUFFER_HANDLE     = INVALID_HANDLE;
// const TextureHandle INVALID_TEXTURE_HANDLE   = INVALID_HANDLE;
typedef u32 ResourceHandle;
const ResourceHandle NULL_HANDLE     = 0;
const PassHandle INVALID_PASS_HANDLE = 0xffffffff;

struct PassResource;
struct BufferView;
struct TextureView;
struct ResourceView;
using ExecuteFunction           = std::function<void(graphics::CommandList cmd)>;
using ComputeFunction           = std::function<void(void)>;
using ResourceEnumerateFunction = std::function<void(const PassResource *resource, ResourceView *views)>;
using BufferEnumerateFunction   = std::function<void(const PassResource *resource, BufferView *views)>;
using TextureEnumerateFunction  = std::function<void(const PassResource *resource, TextureView *views)>;

inline b32 IsValidResourceHandle(ResourceHandle handle)
{
    return handle != NULL_HANDLE;
}

inline b32 IsValidPassHandle(PassHandle handle)
{
    return handle != INVALID_PASS_HANDLE;
}

enum class HLSLType : u32
{
    StructuredBuffer,
    RWStructuredBuffer,

    Texture2D,
    Texture2DArray,
    RWTexture2D,

    // Buffer,
    // ByteAddressBuffer,
};

enum class ShaderParamFlags : u32
{
    None     = 0,
    Header   = 1 << 0,
    Compute  = 1 << 1,
    Graphics = 1 << 2,

    Push = 1 << 3,
};
ENUM_CLASS_FLAGS(ShaderParamFlags)

inline b8 IsTexture(HLSLType type)
{
    switch (type)
    {
        case HLSLType::Texture2D:
        case HLSLType::Texture2DArray:
        case HLSLType::RWTexture2D:
            return 1;
        default: return 0;
    }
}

inline b8 IsBuffer(HLSLType type)
{
    switch (type)
    {
        case HLSLType::StructuredBuffer:
        case HLSLType::RWStructuredBuffer:
            return 1;
        default: return 0;
    }
}

inline string ConvertHLSLTypeToName(HLSLType type)
{
    switch (type)
    {
        case HLSLType::StructuredBuffer: return Str8Lit("HLSLType::StructuredBuffer");
        case HLSLType::RWStructuredBuffer: return Str8Lit("HLSLType::RWStructuredBuffer");
        case HLSLType::Texture2D: return Str8Lit("HLSLType::Texture2D");
        case HLSLType::Texture2DArray: return Str8Lit("HLSLType::Texture2DArray");
        case HLSLType::RWTexture2D: return Str8Lit("HLSLType::RWTexture2D");
        default: Assert(0); return Str8Lit("Invalid");
    }
}

inline string ConvertResourceViewTypeToName(graphics::ResourceViewType type)
{
    switch (type)
    {
        case graphics::ResourceViewType::CBV: return Str8Lit("graphics::ResourceViewType::CBV");
        case graphics::ResourceViewType::UAV: return Str8Lit("graphics::ResourceViewType::UAV");
        case graphics::ResourceViewType::SRV: return Str8Lit("graphics::ResourceViewType::SRV");
        case graphics::ResourceViewType::SAM: return Str8Lit("graphics::ResourceViewType::SAM");
        default: Assert(0); return Str8Lit("Invalid case");
    }
}

inline string ConvertShaderParamFlagsToString(ShaderParamFlags flags, b32 hasPush)
{
    string result;
    switch (flags)
    {
        case ShaderParamFlags::Header:
        {
            result = hasPush ? Str8Lit("ShaderParamFlags::Header | ShaderParamFlags::Push") : Str8Lit("ShaderParamFlags::Header");
        }
        break;
        case ShaderParamFlags::Compute:
        {
            result = hasPush ? Str8Lit("ShaderParamFlags::Compute | ShaderParamFlags::Push") : Str8Lit("ShaderParamFlags::Compute");
        }
        break;
        case ShaderParamFlags::Graphics:
        {
            result = hasPush ? Str8Lit("ShaderParamFlags::Graphics | ShaderParamFlags::Push") : Str8Lit(" ShaderParamFlags::Graphics ");
        }
        break;
        case ShaderParamFlags::None:
        {
            result = hasPush ? Str8Lit("ShaderParamFlags::None | ShaderParamFlags::Push") : Str8Lit("ShaderParamFlags::None");
        }
        break;
        default: Assert(0); return Str8Lit("Invalid");
    }
    return result;
}

struct PassResource
{
    graphics::ResourceViewType viewType;
    HLSLType objectType;
    i32 binding;
};

struct BufferDesc
{
    u32 size;
};

enum class ResourceType
{
    Texture,
    Buffer,
};

struct ResourceView
{
    ResourceType type;
    graphics::ResourceAccess access;
    ResourceHandle handle;
};

struct BufferView : ResourceView
{
};

struct TextureView : ResourceView
{
    i32 subresourceIndex;
};

// u32 numElements;
// u32 bytesPerElement;
//
// static BufferView CreateStructured(u32 numElements, u32 bytesPerElement)
// {
//     BufferView view;
//     view.numElements     = numElements;
//     view.bytesPerElement = bytesPerElement;
// }

struct BaseShaderParamType
{
    string name;
    u32 numResources;
    u32 numBuffers;
    u32 numTextures;
    ShaderParamFlags flags;
};

enum class PassFlags
{
    None = 0,
    // AsyncCompute       = 1 << 0,
    NotCulled = 1 << 0,

    Compute  = 1 << 1,
    Indirect = 1 << 2,
    Graphics = 1 << 3,
};
ENUM_CLASS_FLAGS(PassFlags)

struct RenderPass
{
    string name;
    PassFlags flags;
    BaseShaderParamType *parameters;
    ExecuteFunction func;
    graphics::PipelineState *pipelineState;
    // TODO: going to need additional resources here (e.g. ColorAttachments, DepthStencil, Indirect Buffers)
    Array<ResourceHandle> nonShaderBoundResources;
};

struct BufferRange
{
    graphics::GPUBuffer *transientBackingBuffer;
    u32 transientBufferId;
    u32 offset;
    u32 size;
};

enum class ResourceFlags
{
    None         = 0,
    Buffer       = 1 << 0,
    Texture      = 1 << 1,
    Import       = 1 << 2,
    Export       = 1 << 3,
    NotTransient = Import | Export,
};
ENUM_CLASS_FLAGS(ResourceFlags)

struct RenderGraphResource
{
    u32 numUses;

    ResourceFlags flags;

    graphics::ResourceAccess lastAccess;
    PassHandle lastPassWriteHandle;

    // TODO: these two may not be used
    graphics::PipelineStage lastPipelineStage;
    PassFlags lastPassFlags;

    PassHandle firstPass;
    PassHandle lastPass;

    // Initialization data (provided by user)

    // Buffer size
    u32 bufferSize;
    // Texture desc
    graphics::TextureDesc textureDesc;

    // Backing resource information
    union
    {
        // Transient buffer
        BufferRange bufferRange;

        // Transient texture
        graphics::Texture *texture;
    };

    void Init(ResourceFlags inFlags)
    {
        flags |= inFlags;
        lastAccess          = graphics::ResourceAccess::None;
        lastPassFlags       = PassFlags::None;
        lastPipelineStage   = graphics::PipelineStage::None;
        firstPass           = INVALID_PASS_HANDLE;
        lastPass            = INVALID_PASS_HANDLE;
        lastPassWriteHandle = INVALID_PASS_HANDLE;
    }
};

struct BarrierBatch
{
    Array<graphics::GPUBarrier> barriers;
    inline void Emplace();
    inline graphics::GPUBarrier &Back();
    inline graphics::GPUBarrier *Data();
    inline u32 Length();
};

// Transient resource allocator
struct TransientResourceAllocator
{
    const u32 pageSize = megabytes(2);
    Array<u32> offsets;
    Array<graphics::GPUBuffer> backingBuffers;

    Array<u32> textureInUseMasks;
    Array<graphics::Texture> backingTextures;
    void Init();
    // TODO: save the buffer range so it can be aliased
    BufferRange CreateBuffer(u32 size);
    graphics::Texture *CreateTexture(graphics::TextureDesc desc);
    void Reset();
};

struct RenderGraph
{
    Arena *arena;
    u64 arenaBeginFramePos; // reset to this pos at end of frame

    AtomicFixedHashTable<512, 512> resourceNameHashTable;
    RenderGraphResource resources[256];
    u32 resourceStringHashes[256];
    u32 numResources;

    AtomicFixedHashTable<64, 64> renderPassHashTable;
    u32 renderPassStringHashes[32];
    RenderPass passes[32];
    PassHandle passCount;

    // Pass dependencies
    Array<PassHandle> passDependencies[32];
    u32 numPasses;

    // Barrier batches
    Array<BarrierBatch> barrierBatches;

    // Transient resource allocator
    TransientResourceAllocator transientResourceAllocator;

    void Init();
    void Compile();
    graphics::CommandList Execute();
    void BeginFrame();
    void EndFrame();

    template <typename ParameterType>
    ParameterType *AllocParameters();

    PassHandle AddPassInternal(string passName, void *params, graphics::PipelineState *state, const ExecuteFunction &func, PassFlags flags);
    PassHandle AddPass(string passName, void *params, graphics::PipelineState *state, const ExecuteFunction &func);
    PassHandle AddPass(string passName, void *params, graphics::PipelineState *state, PassFlags flags, const ExecuteFunction &func);
    ResourceHandle CreateBuffer(string name, u32 size);
    ResourceHandle CreateTexture(string name, graphics::TextureDesc desc);
    ResourceHandle CreateResourceInternal(string name, ResourceFlags flags, graphics::TextureDesc desc = {}, u32 size = 0);
    void ExtendLifetime(ResourceHandle handle, ResourceFlags lifetime);
    ResourceHandle Import(string name, graphics::Texture *texture);
    ResourceHandle AddComputeIndirectPass(string passName,
                                          void *params,
                                          graphics::PipelineState *state,
                                          graphics::GPUBuffer *indirectBuffer,
                                          u32 offset,
                                          const ComputeFunction &func = ComputeFunction());
    ResourceHandle AddComputePass(string passName,
                                  void *params,
                                  graphics::PipelineState *state,
                                  V3U32 groupCounts,
                                  const ComputeFunction &func = ComputeFunction());
};

} // namespace rendergraph

#endif
