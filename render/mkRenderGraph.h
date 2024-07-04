#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

namespace rendergraph
{

typedef u32 PassHandle;
typedef u32 BufferHandle;
typedef u32 TextureHandle;
const u32 INVALID_HANDLE = 0xffffffff;
const BufferHandle INVALID_BUFFER_HANDLE   = INVALID_HANDLE;
const TextureHandle INVALID_TEXTURE_HANDLE = INVALID_HANDLE;
const PassHandle INVALID_PASS_HANDLE       = INVALID_HANDLE;

struct PassResource;
struct BufferView;
struct TextureView;
using ExecuteFunction          = std::function<void(graphics::CommandList cmd)>;
using BufferEnumerateFunction  = std::function<void(PassResource *resources, BufferView *views)>;
using TextureEnumerateFunction = std::function<void(PassResource *resources, TextureView *views)>;

inline b32 IsValidHandle(u32 handle)
{
    return handle != INVALID_HANDLE;
}

enum class ResourceType : u32
{
    StructuredBuffer,
    RWStructuredBuffer,

    Texture2D,
    Texture2DArray,
    RWTexture2D,

    // Buffer,
    // ByteAddressBuffer,
};

enum class ResourceUsage
{
    CBV, // constant buffer view
    SRV, // shader resource view
    UAV, // unordered access view
    SAM, // sampler
};

enum class ShaderParamFlags : u32
{
    None     = 0,
    Header   = 1 << 0,
    Compute  = 1 << 1,
    Graphics = 1 << 2,
};

inline b8 IsTexture(ResourceType type)
{
    switch (type)
    {
        case ResourceType::Texture2D:
        case ResourceType::Texture2DArray:
        case ResourceType::RWTexture2D:
            return 1;
        default: return 0;
    }
}

inline b8 IsBuffer(ResourceType type)
{
    switch (type)
    {
        case ResourceType::StructuredBuffer:
        case ResourceType::RWStructuredBuffer:
            return 1;
        default: return 0;
    }
}

inline string ConvertResourceTypeToName(ResourceType type)
{
    switch (type)
    {
        case ResourceType::StructuredBuffer: return Str8Lit("ResourceType::StructuredBuffer");
        case ResourceType::RWStructuredBuffer: return Str8Lit("ResourceType::RWStructuredBuffer");
        case ResourceType::Texture2D: return Str8Lit("ResourceType::Texture2D");
        case ResourceType::Texture2DArray: return Str8Lit("ResourceType::Texture2DArray");
        case ResourceType::RWTexture2D: return Str8Lit("ResourceType::RWTexture2D");
        default: Assert(0); return Str8Lit("Invalid");
    }
}

inline string ConvertResourceUsageToName(ResourceUsage usage)
{
    switch (usage)
    {
        case ResourceUsage::CBV: return Str8Lit("ResourceUsage::CBV");
        case ResourceUsage::UAV: return Str8Lit("ResourceUsage::UAV");
        case ResourceUsage::SRV: return Str8Lit("ResourceUsage::SRV");
        case ResourceUsage::SAM: return Str8Lit("ResourceUsage::SAM");
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
    ResourceUsage usage;
    ResourceType type;
    i32 binding;
};

struct BufferDesc
{
    u32 size;
};

enum class ViewAccess
{
    ComputeSRV,
    ComputeUAV,
    GraphicsSRV,
    GraphicsUAV,
};
ENUM_CLASS_FLAGS(ViewAccess)

enum class ResourceViewType
{
    Texture,
    Buffer,
};

struct ResourceView
{
    ResourceViewType type;
    ViewAccess access;
};

struct BufferView : ResourceView
{
    BufferHandle handle;
};

struct TextureView : ResourceView
{
    TextureHandle handle;
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

// enum class PassFlags
// {
//     Uninitialized = 0,
//     Initialized   = 1 << 0,
// };
// ENUM_CLASS_FLAGS(PassFlags)

struct RenderPass
{
    string name;
    void *parameters;
    ExecuteFunction func;
    u32 size;
};

struct RenderGraphBuffer
{
    // u32 numElements;
    // u32 bytesPerElement;
    // graphics::Format format;

    u32 numUses; // a reference count

    PassHandle firstPass;
    PassHandle lastPass;
    PassHandle lastPassWriteHandle;
    PassHandle lastPassReadHandle;
};

/*
 */
struct RenderGraph
{
    Arena *arena;
    u32 arenaBeginFramePos; // reset to this pos at end of frame

    // Per frame data reset every frame

    // Persistent (>1 frame)
    AtomicFixedHashTable<512, 512> bufferNameHashTable;
    RenderGraphBuffer buffers[512];
    u32 bufferStringHashes[512];
    u32 numBuffers = 0;

    AtomicFixedHashTable<512, 512> textureNameHashTable;
    u32 textureStringHashes[512];
    u32 numTextures = 0;

    AtomicFixedHashTable<64, 64> renderPassHashTable;
    u32 renderPassStringHashes[32];
    RenderPass passes[32];
    PassHandle passCount;

    // Pass dependencies
    PassHandle *passDependencies[32];
    u32 passDependencyCounts[32];
    u32 numPasses;

    graphics::GPUBuffer transientResourceBuffer;

    void Init();
    void Compile();
    PassHandle AddPassInternal(string passName, void *params, u32 size, const ExecuteFunction &func);
    BufferHandle CreateBuffer(string name);
};

#define AddPass(graph, name, params, func) (graph)->AddPassInternal((name), (params), sizeof(*(params)), (func))
} // namespace rendergraph

#endif
