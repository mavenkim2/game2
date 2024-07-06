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
};

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

inline string ConvertShaderParamFlagsToString(ShaderParamFlags flags)
{
    switch (flags)
    {
        case ShaderParamFlags::Header: return Str8Lit("ShaderParamFlags::Header");
        case ShaderParamFlags::Compute: return Str8Lit("ShaderParamFlags::Compute");
        case ShaderParamFlags::Graphics: return Str8Lit("ShaderParamFlags::Graphics");
        case ShaderParamFlags::None: return Str8Lit("ShaderParamFlags::None");
        default: Assert(0); return Str8Lit("Invalid");
    }
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
};

struct RenderGraphResource
{
    u32 numUses;

    graphics::ResourceAccess lastAccess;
    graphics::PipelineStage lastPipelineStage;
    PassFlags lastPassFlags;
    PassHandle lastPassWriteHandle;

    PassHandle firstPass;
    PassHandle lastPass;

    graphics::GPUResource resource;
};

/*
 */
struct RenderGraph
{
    Arena *arena;
    u32 arenaBeginFramePos; // reset to this pos at end of frame

    AtomicFixedHashTable<512, 512> resourceNameHashTable;
    RenderGraphResource resources[512];
    u32 resourceStringHashes[512];
    u32 numResources;

    AtomicFixedHashTable<64, 64> renderPassHashTable;
    u32 renderPassStringHashes[32];
    RenderPass passes[32];
    PassHandle passCount;

    // Pass dependencies
    Array<PassHandle> passDependencies[32];
    u32 numPasses;

    // Transitions

    graphics::GPUBuffer transientResourceBuffer;

    void Init();
    void Compile();
    void Execute();

    template <typename ParameterType>
    ParameterType *AllocParameters();

    PassHandle AddPassInternal(string passName, void *params, const ExecuteFunction &func, PassFlags flags);
    PassHandle AddPass(string passName, void *params, const ExecuteFunction &func);
    PassHandle AddPass(string passName, void *params, PassFlags flags, const ExecuteFunction &func);
    ResourceHandle CreateBuffer(string name);
};

} // namespace rendergraph

#endif
