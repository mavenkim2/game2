#ifndef MK_GRAPHICS_H
#define MK_GRAPHICS_H

#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkTypes.h"
#endif

#include <atomic>
#include <functional>

namespace graphics
{

using CopyFunction = std::function<void(void *)>;

static const i32 cMaxBindings = 16;

#if WINDOWS
typedef HWND Window;
typedef HINSTANCE Instance;
#else
#error not supported
#endif

enum class ValidationMode
{
    Disabled,
    Enabled,
    Verbose,
};

enum class GPUDevicePreference
{
    Discrete,
    Integrated,
};

enum QueueType
{
    QueueType_Graphics,
    QueueType_Compute,
    QueueType_Copy,

    QueueType_Count,
};

typedef u32 DeviceCapabilities;
enum
{
    DeviceCapabilities_MeshShader,
    DeviceCapabilities_VariableShading,
};

typedef u32 ResourceUsage;
enum : u32
{
    ResourceUsage_None     = 0,
    ResourceUsage_Compute  = 1 << 0,
    ResourceUsage_Graphics = 1 << 1,
    ResourceUsage_Read     = 1 << 2, // read in the graphics pipeline
    ResourceUsage_Write    = 1 << 3, // write in the graphics pipeline
    ResourceUsage_Depth    = 1 << 4,
    ResourceUsage_Stencil  = 1 << 5,
    // ResourceUsage_SRV          = 1 << 7,
    // ResourceUsage_UAV          = 1 << 8,
    ResourceUsage_Buffer  = 1 << 6,
    ResourceUsage_Texture = 1 << 7,

    // Pipeline stages
    ResourceUsage_Indirect       = 1 << 8,
    ResourceUsage_Vertex         = 1 << 9,
    ResourceUsage_Fragment       = 1 << 10,
    ResourceUsage_Index          = 1 << 11,
    ResourceUsage_Input          = 1 << 12,
    ResourceUsage_Shader         = 1 << 13,
    PipelineStage_VertexInput    = ResourceUsage_Vertex | ResourceUsage_Input,
    PipelineStage_IndexInput     = ResourceUsage_Index | ResourceUsage_Input,
    PipelineStage_VertexShader   = ResourceUsage_Vertex | ResourceUsage_Shader,
    PipelineStage_FragmentShader = ResourceUsage_Fragment | ResourceUsage_Shader,

    // Transfer
    ResourceUsage_TransferSrc = 1 << 14,
    ResourceUsage_TransferDst = 1 << 15,

    // Bindless
    ResourceUsage_Bindless = (1 << 16) | ResourceUsage_TransferDst,

    // Attachments
    ResourceUsage_ColorAttachment = 1 << 17,

    ResourceUsage_ComputeRead  = ResourceUsage_Compute | ResourceUsage_Read,
    ResourceUsage_ComputeWrite = ResourceUsage_Compute | ResourceUsage_Write,

    ResourceUsage_VertexBuffer = ResourceUsage_Vertex | ResourceUsage_Buffer,
    ResourceUsage_IndexBuffer  = ResourceUsage_Index | ResourceUsage_Buffer,

    ResourceUsage_DepthStencil = ResourceUsage_Depth | ResourceUsage_Stencil | ResourceUsage_Read | ResourceUsage_Write,

    ResourceUsage_UniformBuffer = (1 << 18) | ResourceUsage_Buffer,
    ResourceUsage_UniformTexel  = (1 << 19) | ResourceUsage_Buffer,

    ResourceUsage_StorageBufferRead = (1 << 20) | ResourceUsage_Buffer, // read only
    ResourceUsage_StorageBuffer     = (1 << 21) | ResourceUsage_Buffer, // read write
    ResourceUsage_StorageTexel      = (1 << 22) | ResourceUsage_Buffer,

    ResourceUsage_SampledImage = (1 << 23) | ResourceUsage_Texture,
    ResourceUsage_StorageImage = (1 << 24) | ResourceUsage_Texture,

    ResourceUsage_ShaderGlobals = ResourceUsage_StorageBufferRead | ResourceUsage_Bindless,
    ResourceUsage_Reset         = 0xffffffff,
};

enum class Format
{
    Null,
    B8G8R8_UNORM,
    B8G8R8_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,

    R32_SFLOAT,
    R32_UINT,
    R32G32_UINT,
    R8G8_UNORM,
    R8G8B8A8_SRGB,
    R8G8B8A8_UNORM,

    R32G32_SFLOAT,
    R32G32B32_SFLOAT,
    R32G32B32A32_SFLOAT,
    R32G32B32A32_UINT,

    D32_SFLOAT_S8_UINT,
    D32_SFLOAT,
    D24_UNORM_S8_UINT,

    BC1_RGB_UNORM,

    Count,
};

enum class ResourceType
{
    // Used for resource bindings
    SRV, // shader resource view (read only)
    UAV, // unordered access view (write + read)
    SAM, // sampler
    CBV, // constant buffer view (read only)
};

//	https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html
// template <typename E>
// struct enable_bitmask_operators
// {
//     static constexpr bool enable = false;
// };
// template <typename E>
// constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator|(E lhs, E rhs)
// {
//     typedef typename std::underlying_type<E>::type underlying;
//     return static_cast<E>(
//         static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
// }
// template <typename E>
// constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E &>::type operator|=(E &lhs, E rhs)
// {
//     typedef typename std::underlying_type<E>::type underlying;
//     lhs = static_cast<E>(
//         static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
//     return lhs;
// }
// template <typename E>
// constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator&(E lhs, E rhs)
// {
//     typedef typename std::underlying_type<E>::type underlying;
//     return static_cast<E>(
//         static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
// }
// template <typename E>
// constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E &>::type operator&=(E &lhs, E rhs)
// {
//     typedef typename std::underlying_type<E>::type underlying;
//     lhs = static_cast<E>(
//         static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
//     return lhs;
// }
// template <typename E>
// constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator~(E rhs)
// {
//     typedef typename std::underlying_type<E>::type underlying;
//     rhs = static_cast<E>(
//         ~static_cast<underlying>(rhs));
//     return rhs;
// }
//
// template <typename E>
// inline b32 HasFlags(E lhs, E rhs)
// {
//     return (lhs & rhs) == rhs;
// }

enum class MemoryUsage
{
    GPU_ONLY,
    CPU_ONLY,
    CPU_TO_GPU,
    GPU_TO_CPU,
};

enum class InputRate
{
    Vertex,
    Instance,
};

struct InputLayout
{
    list<Format> elements;
    u32 binding;
    u32 stride;
    InputRate rate;
};

struct Viewport
{
    f32 x        = 0;
    f32 y        = 0;
    f32 width    = 0;
    f32 height   = 0;
    f32 minDepth = 0;
    f32 maxDepth = 1.f;
};

// typedef u32 ResourceUsage;
// enum class ResourceUsage
// {
//     None               = 0,
//     UniformBuffer      = 1 << 1,
//     UniformTexelBuffer = 1 << 2,
//     StorageBuffer      = 1 << 3,
//     StorageTexelBuffer = 1 << 4,
//
//     VertexBuffer = 1 << 5,
//     IndexBuffer  = 1 << 6,
//
//     TransferSrc = 1 << 7,
//     TransferDst = 1 << 8,
//
//     SampledImage = 1 << 9,
//     StorageImage = 1 << 10,
//
//     DepthStencil = 1 << 11,
//
//     IndirectBuffer = 1 << 12,
//
//     MegaBuffer  = 1 << 13, // e.g. subviews contain meaningful data, not the whole buffer itself
//     NotBindless = 1 << 14,
// };

enum ImageUsage
{
    ImageUsage_None         = 0,
    ImageUsage_Sampled      = 1 << 0,
    ImageUsage_Storage      = 1 << 1,
    ImageUsage_TransferSrc  = 1 << 2,
    ImageUsage_TransferDst  = 1 << 3,
    ImageUsage_DepthStencil = 1 << 4,
};

typedef u32 PipelineFlag;
enum : u32
{
    PipelineFlag_None                 = 0,
    PipelineFlag_Indirect             = 1 << 0,
    PipelineFlag_IndexInput           = 1 << 1,
    PipelineFlag_VertexAttributeInput = 1 << 2,
    PipelineFlag_VertexShader         = 1 << 3,
    PipelineFlag_Transfer             = 1 << 4,
    PipelineFlag_Compute              = 1 << 5,
    PipelineFlag_FragmentShader       = 1 << 6,
    PipelineFlag_Depth                = 1 << 7,
    PipelineFlag_AllCommands          = 1U << 31,
};

typedef u32 AccessFlag;
enum
{
    AccessFlag_None                = 0,
    AccessFlag_VertexAttributeRead = 1 << 0,
    AccessFlag_IndexRead           = 1 << 1,
    AccessFlag_UniformRead         = 1 << 2,
    AccessFlag_TransferRead        = 1 << 3,
    AccessFlag_TransferWrite       = 1 << 4,
    AccessFlag_ShaderRead          = 1 << 5,
    AccessFlag_ShaderWrite         = 1 << 6,
    AccessFlag_IndirectRead        = 1 << 7,
    AccessFlag_DepthStencil        = 1 << 8,
};

// inline b32 HasFlags(u32 lhs, u32 rhs)
// {
//     return (lhs & rhs) == rhs;
// }

enum class Filter
{
    Nearest,
    Linear,
};

enum class SamplerMode
{
    Wrap,
    ClampToEdge,
    ClampToBorder,
};

enum class CompareOp
{
    None,
    Less,
};

enum class BorderColor
{
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

//////////////////////////////
// Graphics primitives
//
struct SwapchainDesc
{
    u32 width;
    u32 height;
    Format format;
    // Colorspace mColorspace;
};

enum class ShaderStage
{
    Vertex,
    Geometry,
    Fragment,
    Compute,
    Count,
};
ENUM_CLASS_FLAGS(ShaderStage)

enum ShaderType
{
    ShaderType_Mesh_VS,
    ShaderType_Mesh_FS,
    ShaderType_ShadowMap_VS,
    ShaderType_BC1_CS,
    ShaderType_Skin_CS,
    ShaderType_TriangleCull_CS,
    ShaderType_ClearIndirect_CS,
    ShaderType_DrawCompaction_CS,
    ShaderType_InstanceCullPass1_CS,
    ShaderType_InstanceCullPass2_CS,
    ShaderType_ClusterCull_CS,
    ShaderType_DispatchPrep_CS,
    ShaderType_GenerateMips_CS,
    ShaderType_Count,
};

enum QueryType
{
    QueryType_Occlusion,
    QueryType_Timestamp,
    QueryType_PipelineStatistics,
};

enum RasterType
{
    RasterType_CCW_CullBack,
    RasterType_CCW_CullFront,
    RasterType_CCW_CullNone,
    RasterType_Count,
};

enum DescriptorType
{
    DescriptorType_SampledImage,
    DescriptorType_UniformTexel,
    DescriptorType_StorageBuffer,
    DescriptorType_StorageTexelBuffer,
    DescriptorType_Count,
};

struct GraphicsObject
{
    void *internalState = 0;

    b32 IsValid()
    {
        return internalState != 0;
    }
};

struct QueryPool : GraphicsObject
{
    QueryType type;
    u32 queryCount;
};

struct Fence : GraphicsObject
{
    // u32 count;
};

struct FenceTicket
{
    Fence fence;
    u32 ticket;

    // void Create(Fence f);
};

struct GPUResource : GraphicsObject
{
    enum class ResourceType
    {
        Null,
        Buffer,
        Image,
    } resourceType;

    FenceTicket ticket;

    inline b32 IsTexture()
    {
        return resourceType == ResourceType::Image;
    }

    inline b32 IsBuffer()
    {
        return resourceType == ResourceType::Buffer;
    }
};

struct BindedResource
{
    GPUResource *resource = 0;
    i32 subresourceIndex  = 0;

    b8 IsValid()
    {
        return (resource && resource->IsValid());
    }
};

struct Swapchain : GraphicsObject
{
    SwapchainDesc desc;
    const SwapchainDesc GetDesc()
    {
        return desc;
    }
};

struct Shader : GraphicsObject
{
    string name;
    ShaderStage stage;
    string outName = {};
};

struct CommandList : GraphicsObject
{
};

//////////////////////////////
// Pipelines
//

struct RasterizationState
{
    enum class CullMode
    {
        None,
        Back,
        Front,
        FrontAndBack,
    } cullMode         = CullMode::None;
    b32 isFrontFaceCCW = 1;
    // TODO: depth bias
};

// struct DescriptorBinding
// {
//     u32 mBinding;
//     u32 mArraySize = 1;
//     ResourceUsage mUsage;
//     ShaderStage mStage;
//
//     DescriptorBinding(u32 binding, ResourceUsage usage, ShaderStage stage, u32 arraySize = 1)
//         : mBinding(binding), mUsage(usage), mStage(stage), mArraySize(arraySize) {}
// };

// struct PushConstantRange
// {
//     u32 mOffset;
//     u32 size = 0;
// };

struct PipelineStateDesc
{
    Format depthStencilFormat    = Format::Null;
    Format colorAttachmentFormat = Format::Null;
    union
    {
        Shader *shaders[ShaderStage::Count];
        struct
        {
            Shader *vs;
            Shader *fs;
            Shader *gs;
            Shader *compute;
        };
    };
    RasterizationState *rasterState;
    list<InputLayout *> inputLayouts;
};

struct PipelineState : GraphicsObject
{
    PipelineStateDesc desc;
};

struct GPUBufferDesc
{
    u64 size;
    // BindFlag mFlags    = 0;
    MemoryUsage usage = MemoryUsage::GPU_ONLY;
    ResourceUsage resourceUsage;
};

struct GPUBuffer : GPUResource
{
    GPUBufferDesc desc;
    void *mappedData;
};

struct GPUBarrier
{
    enum class Type
    {
        Buffer,
        Image,
        Memory,
    } type = Type::Memory;

    GPUResource *resource;

    ResourceUsage usageBefore;
    ResourceUsage usageAfter;
    i32 subresource;

    static inline GPUBarrier Buffer(GPUResource *resource, ResourceUsage inUsageBefore, ResourceUsage inUsageAfter)
    {
        GPUBarrier barrier;
        Assert(resource->resourceType == GPUResource::ResourceType::Buffer);

        barrier.type        = Type::Buffer;
        barrier.resource    = resource;
        barrier.usageBefore = inUsageBefore;
        barrier.usageAfter  = inUsageAfter;

        return barrier;
    }

    static inline GPUBarrier ComputeWriteToRead(GPUResource *resource, ResourceUsage additionalBefore = ResourceUsage_None,
                                                ResourceUsage additionalAfter = ResourceUsage_None)
    {
        switch (resource->resourceType)
        {
            case GPUResource::ResourceType::Buffer: return Buffer(resource, ResourceUsage_ComputeWrite | additionalBefore, ResourceUsage_ComputeRead | additionalAfter);
            case GPUResource::ResourceType::Image: return Image(resource, ResourceUsage_ComputeWrite | additionalBefore, ResourceUsage_ComputeRead | additionalAfter);
            default: Assert(0); return Buffer(resource, ResourceUsage_ComputeRead, ResourceUsage_ComputeWrite);
        }
    }

    static inline GPUBarrier ComputeReadToWrite(GPUResource *resource)
    {
        switch (resource->resourceType)
        {
            case GPUResource::ResourceType::Buffer: return Buffer(resource, ResourceUsage_ComputeRead, ResourceUsage_ComputeWrite);
            case GPUResource::ResourceType::Image: return Image(resource, ResourceUsage_ComputeRead, ResourceUsage_ComputeWrite);
            default: Assert(0); return Buffer(resource, ResourceUsage_ComputeRead, ResourceUsage_ComputeWrite);
        }
    }

    static GPUBarrier Memory(ResourceUsage inUsageBefore, ResourceUsage inUsageAfter)
    {
        GPUBarrier barrier;
        barrier.type        = Type::Memory;
        barrier.resource    = 0;
        barrier.usageBefore = inUsageBefore;
        barrier.usageAfter  = inUsageAfter;
        return barrier;
    }

    static GPUBarrier Image(GPUResource *resource, ResourceUsage inUsageBefore, ResourceUsage inUsageAfter, i32 inSubresource = -1)
    {
        GPUBarrier barrier;
        Assert(resource->resourceType == GPUResource::ResourceType::Image);
        barrier.type        = Type::Image;
        barrier.resource    = resource;
        barrier.usageBefore = inUsageBefore;
        barrier.usageAfter  = inUsageAfter;
        barrier.subresource = inSubresource;
        return barrier;
    }
};

struct TextureDesc
{
    enum class TextureType
    {
        Texture2D,
        Texture2DArray,
        // Texture3D,
        Cubemap,
    } textureType              = TextureType::Texture2D;
    u32 width                  = 1;
    u32 height                 = 1;
    u32 depth                  = 1;
    u32 numMips                = 1;
    u32 numLayers              = 1;
    Format format              = Format::Null;
    MemoryUsage usage          = MemoryUsage::GPU_ONLY;
    ResourceUsage initialUsage = ResourceUsage_None;
    ResourceUsage futureUsages = ResourceUsage_None;
    enum class DefaultSampler
    {
        None,
        Nearest,
        Linear,
    } sampler = DefaultSampler::None;
};

struct TextureMappedData
{
    void *mappedData;
    u32 size;
};

struct Texture : GPUResource
{
    TextureDesc desc;
    TextureMappedData mappedData;
};

enum class ReductionMode
{
    None,
    Min,
    Max,
};

struct SamplerDesc
{
    Filter mag                  = Filter::Nearest;
    Filter min                  = Filter::Nearest;
    Filter mipMode              = Filter::Nearest;
    SamplerMode mode            = SamplerMode::Wrap;
    ReductionMode reductionMode = ReductionMode::None;
    BorderColor borderColor     = BorderColor::TransparentBlack;
    CompareOp compareOp         = CompareOp::None;
    u32 maxAnisotropy           = 0;
    f32 minLod                  = 0;
    f32 maxLod                  = FLT_MAX;
};

struct Sampler : GraphicsObject
{
    SamplerDesc desc;
};

struct FrameAllocation
{
    void *ptr;
    u64 offset;
    u64 size;
};

enum class LoadOp
{
    Load,
    Clear,
    DontCare,
};

enum class StoreOp
{
    Store,
    DontCare,
    None,
};

struct RenderPassImage
{
    enum class RenderImageType
    {
        Depth,
        Color,
    } imageType;
    Texture *texture;

    ResourceUsage layoutBefore = ResourceUsage_None;
    ResourceUsage layout       = ResourceUsage_None;
    // ResourceUsage layoutAfter  = ResourceUsage_None;
    LoadOp loadOp;
    StoreOp storeOp;
    i32 subresource = -1;

    static RenderPassImage DepthStencil(Texture *texture, ResourceUsage layoutBefore, i32 subresource = -1,
                                        LoadOp inLoadOp = LoadOp::Clear, StoreOp inStoreOp = StoreOp::Store)
    {
        RenderPassImage image;
        image.imageType    = RenderImageType::Depth;
        image.texture      = texture;
        image.layoutBefore = layoutBefore;
        image.layout       = ResourceUsage_DepthStencil;
        // image.layoutAfter  = layoutAfter;
        image.loadOp      = inLoadOp;
        image.storeOp     = inStoreOp;
        image.subresource = subresource;
        return image;
    }

    static RenderPassImage Color(Texture *texture, ResourceUsage layoutBefore, i32 subresource = -1,
                                 LoadOp inLoadOp = LoadOp::Clear, StoreOp inStoreOp = StoreOp::Store)
    {
        RenderPassImage image;
        image.imageType    = RenderImageType::Color;
        image.texture      = texture;
        image.layoutBefore = layoutBefore;
        image.layout       = ResourceUsage_ColorAttachment;
        image.loadOp       = inLoadOp;
        image.storeOp      = inStoreOp;
        image.subresource  = subresource;
        return image;
    }
};

inline u32 GetFormatSize(Format format)
{
    switch (format)
    {
        case Format::BC1_RGB_UNORM:
        case Format::R32G32_UINT:
        case Format::R32G32_SFLOAT:
        case Format::D32_SFLOAT_S8_UINT:
            return 8;
        case Format::R32G32B32_SFLOAT:
            return 12;
        case Format::R32G32B32A32_SFLOAT:
        case Format::R32G32B32A32_UINT:
            return 16;
        case Format::B8G8R8_UNORM:
        case Format::B8G8R8_SRGB:
            return 3;
        case Format::R32_UINT:
        case Format::B8G8R8A8_UNORM:
        case Format::R8G8B8A8_UNORM:
        case Format::B8G8R8A8_SRGB:
        case Format::R8G8B8A8_SRGB:
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT:
            return 4;
        case Format::R8G8_UNORM:
            return 2;

        default:
            Assert(0);
            return 16;
    }
}

inline u32 GetBlockSize(Format format)
{
    switch (format)
    {
        case Format::BC1_RGB_UNORM:
            return 4;
        default: return 1;
    }
}

inline u32 GetTextureSize(TextureDesc desc)
{
    u32 size      = 0;
    u32 blockSize = GetBlockSize(desc.format);
    u32 stride    = GetFormatSize(desc.format);
    const u32 x   = desc.width / blockSize;
    const u32 y   = desc.height / blockSize;

    Assert(desc.numMips == 1);
    Assert(desc.numLayers == 1);
    Assert(desc.depth == 1);
    size += x * y * stride;

    return size;
}

struct mkGraphics
{
    static const i32 cNumBuffers = 2;
    f64 cTimestampPeriod;
    u64 frameCount = 0;

    inline f64 GetTimestampPeriod() { return cTimestampPeriod; }
    virtual u64 GetMinAlignment(GPUBufferDesc *inDesc)                                                                   = 0;
    virtual FrameAllocation FrameAllocate(u64 size)                                                                      = 0;
    virtual void FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize = ~0, u64 inOffset = 0)       = 0;
    virtual void CommitFrameAllocation(CommandList cmd, FrameAllocation &alloc, GPUBuffer *dstBuffer, u64 dstOffset = 0) = 0;

    virtual b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain)               = 0;
    virtual void CreatePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)           = 0;
    virtual void CreateComputePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)    = 0;
    virtual void CreateShader(Shader *shader, string shaderData)                                        = 0;
    virtual void AddPCTemp(Shader *shader, u32 offset, u32 size)                                        = 0;
    virtual void CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback) = 0;

    void CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData)
    {
        if (inData == 0)
        {
            CreateBufferCopy(inBuffer, inDesc, 0);
        }
        else
        {
            CopyFunction func = [&](void *dest) { MemoryCopy(dest, inData, inDesc.size); };
            CreateBufferCopy(inBuffer, inDesc, func);
        }
    }

    void ResizeBuffer(GPUBuffer *buffer, u32 newSize)
    {
        GPUBufferDesc desc = buffer->desc;
        desc.size          = newSize;
        DeleteBuffer(buffer);
        CreateBuffer(buffer, desc, 0);
    }

    virtual void ClearBuffer(CommandList cmd, GPUBuffer *dst)                                                                      = 0;
    virtual void CopyBuffer(CommandList cmd, GPUBuffer *dest, GPUBuffer *source, u32 size)                                         = 0;
    virtual void CopyTexture(CommandList cmd, Texture *dst, Texture *src, Rect3U32 *rect = 0)                                      = 0;
    virtual void CopyImage(CommandList cmd, Swapchain *dst, Texture *src)                                                          = 0;
    virtual void DeleteBuffer(GPUBuffer *buffer)                                                                                   = 0;
    virtual void CreateTexture(Texture *outTexture, TextureDesc desc, void *inData)                                                = 0;
    virtual void DeleteTexture(Texture *texture)                                                                                   = 0;
    virtual void CreateSampler(Sampler *sampler, SamplerDesc desc)                                                                 = 0;
    virtual void BindSampler(CommandList cmd, Sampler *sampler, u32 slot)                                                          = 0;
    virtual void BindResource(GPUResource *resource, ResourceType type, u32 slot, CommandList cmd, i32 subresource = -1)           = 0;
    virtual i32 GetDescriptorIndex(GPUResource *resource, ResourceType type, i32 subresourceIndex = -1)                            = 0;
    virtual i32 CreateSubresource(GPUBuffer *buffer, ResourceType type, u64 offset = 0ull, u64 size = ~0ull,
                                  Format format = Format::Null, const char *name = 0)                                              = 0;
    virtual i32 CreateSubresource(Texture *texture, u32 baseLayer = 0, u32 numLayers = ~0u,
                                  u32 baseMip = 0, u32 numMips = ~0u)                                                              = 0;
    virtual void UpdateDescriptorSet(CommandList cmd, b8 isCompute = 0)                                                            = 0;
    virtual CommandList BeginCommandList(QueueType queue)                                                                          = 0;
    virtual void BeginRenderPass(Swapchain *inSwapchain, CommandList inCommandList)                                                = 0;
    virtual void BeginRenderPass(RenderPassImage *images, u32 count, CommandList cmd)                                              = 0;
    virtual void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex)                                                           = 0;
    virtual void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex)                                     = 0;
    virtual void DrawIndexedIndirect(CommandList cmd, GPUBuffer *indirectBuffer, u32 drawCount,
                                     u32 offset = 0, u32 stride = 20)                                                              = 0;
    virtual void DrawIndexedIndirectCount(CommandList cmd, GPUBuffer *indirectBuffer, GPUBuffer *countBuffer,
                                          u32 maxDrawCount, u32 indirectOffset = 0, u32 countOffset = 0, u32 stride = 20)          = 0;
    virtual void BindVertexBuffer(CommandList cmd, GPUBuffer **buffers, u32 count = 1, u32 *offsets = 0)                           = 0;
    virtual void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer, u64 offset = 0)                                               = 0;
    virtual void Dispatch(CommandList cmd, u32 groupCountX, u32 groupCountY, u32 groupCountZ)                                      = 0;
    virtual void DispatchIndirect(CommandList cmd, GPUBuffer *buffer, u32 offset = 0)                                              = 0;
    virtual void SetViewport(CommandList cmd, Viewport *viewport)                                                                  = 0;
    virtual void SetScissor(CommandList cmd, Rect2 scissor)                                                                        = 0;
    virtual void EndRenderPass(CommandList cmd)                                                                                    = 0;
    virtual void EndRenderPass(Swapchain *swapchain, CommandList cmd)                                                              = 0;
    virtual void SubmitCommandLists()                                                                                              = 0;
    virtual void BindPipeline(PipelineState *ps, CommandList cmd)                                                                  = 0;
    virtual void BindCompute(PipelineState *ps, CommandList cmd)                                                                   = 0;
    virtual void PushConstants(CommandList cmd, u32 size, void *data, u32 offset = 0)                                              = 0;
    virtual void WaitForGPU()                                                                                                      = 0;
    virtual void Wait(CommandList waitFor, CommandList cmd)                                                                        = 0;
    virtual void Wait(CommandList wait)                                                                                            = 0;
    virtual void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)                                                         = 0;
    virtual b32 IsSignaled(FenceTicket ticket)                                                                                     = 0;
    virtual b32 IsLoaded(GPUResource *resource)                                                                                    = 0;
    virtual void CreateQueryPool(QueryPool *queryPool, QueryType type, u32 queryCount)                                             = 0;
    virtual void BeginQuery(QueryPool *queryPool, CommandList cmd, u32 queryIndex)                                                 = 0;
    virtual void EndQuery(QueryPool *queryPool, CommandList cmd, u32 queryIndex)                                                   = 0;
    virtual void ResolveQuery(QueryPool *queryPool, CommandList cmd, GPUBuffer *buffer, u32 queryIndex, u32 count, u32 destOffset) = 0;
    virtual void ResetQuery(QueryPool *queryPool, CommandList cmd, u32 index, u32 count)                                           = 0;
    virtual u32 GetCount(Fence f)                                                                                                  = 0;
    virtual void SetName(GPUResource *resource, const char *name)                                                                  = 0;
    virtual void SetName(GPUResource *resource, string name)                                                                       = 0;
    virtual void BeginEvent(CommandList cmd, string name)                                                                          = 0;
    virtual void EndEvent(CommandList cmd)                                                                                         = 0;

    virtual u32 GetCurrentBuffer() = 0;
};

// template <>
// struct enable_bitmask_operators<graphics::ResourceUsage>
// {
//     static const bool enable = true;
// };
// template <>
// struct enable_bitmask_operators<graphics::ShaderStage>
// {
//     static const bool enable = true;
// };

} // namespace graphics

#endif
