#ifndef MK_GRAPHICS_H
#define MK_GRAPHICS_H

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

enum class Format
{
    Null,
    B8G8R8_UNORM,
    B8G8R8_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,

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
template <typename E>
struct enable_bitmask_operators
{
    static constexpr bool enable = false;
};
template <typename E>
constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator|(E lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}
template <typename E>
constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E &>::type operator|=(E &lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}
template <typename E>
constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator&(E lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}
template <typename E>
constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E &>::type operator&=(E &lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    lhs = static_cast<E>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}
template <typename E>
constexpr typename std::enable_if<enable_bitmask_operators<E>::enable, E>::type operator~(E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    rhs = static_cast<E>(
        ~static_cast<underlying>(rhs));
    return rhs;
}

template <typename E>
inline b32 HasFlags(E lhs, E rhs)
{
    return (lhs & rhs) == rhs;
}

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
    list<Format> mElements;
    u32 mBinding;
    u32 mStride;
    InputRate mRate;
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
enum class ResourceUsage
{
    None               = 0,
    UniformBuffer      = 1 << 1,
    UniformTexelBuffer = 1 << 2,
    StorageBuffer      = 1 << 3,
    StorageTexelBuffer = 1 << 4,

    VertexBuffer = 1 << 5,
    IndexBuffer  = 1 << 6,

    TransferSrc = 1 << 7,
    TransferDst = 1 << 8,

    SampledImage = 1 << 9,
    StorageImage = 1 << 10,

    DepthStencil = 1 << 11,

    MegaBuffer  = 1 << 12, // e.g. subviews contain meaningful data, not the whole buffer itself
    NotBindless = 1 << 13,
};

// inline b32 HasFlags(ResourceUsage lhs, ResourceUsage rhs)
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

enum ShaderType
{
    ShaderType_Mesh_VS,
    ShaderType_Mesh_FS,
    ShaderType_ShadowMap_VS,
    ShaderType_BC1_CS,
    ShaderType_Skin_CS,
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
    b8 signaled;
};

struct GPUResource : GraphicsObject
{
    enum class ResourceType
    {
        Null,
        Buffer,
        Image,
    } mResourceType;

    Fence fence;

    inline b32 IsTexture()
    {
        return mResourceType == ResourceType::Image;
    }

    inline b32 IsBuffer()
    {
        return mResourceType == ResourceType::Buffer;
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
    SwapchainDesc mDesc;
    const SwapchainDesc GetDesc()
    {
        return mDesc;
    }
};

struct Shader : GraphicsObject
{
    string mName;
    ShaderStage stage;
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
    } mCullMode       = CullMode::None;
    b32 mFrontFaceCCW = 1;
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
//     u32 mSize = 0;
// };

struct PipelineStateDesc
{
    Format mDepthStencilFormat    = Format::Null;
    Format mColorAttachmentFormat = Format::Null;
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
    RasterizationState *mRasterState;
    list<InputLayout *> mInputLayouts;
};

struct PipelineState : GraphicsObject
{
    PipelineStateDesc mDesc;
};

struct GPUBufferDesc
{
    u64 mSize;
    // BindFlag mFlags    = 0;
    MemoryUsage mUsage = MemoryUsage::GPU_ONLY;
    ResourceUsage mResourceUsage;
};

struct GPUBuffer : GPUResource
{
    GPUBufferDesc mDesc;
    void *mMappedData;
};

struct GPUBarrier
{
    enum class Type
    {
        Buffer,
        Image,
        Memory,
    } mType = Type::Memory;

    GPUResource *mResource;

    ResourceUsage mBefore;
    ResourceUsage mAfter;

    static GPUBarrier Buffer(GPUResource *resource, ResourceUsage before, ResourceUsage after)
    {
        GPUBarrier barrier;
        Assert(resource->mResourceType == GPUResource::ResourceType::Buffer);
        barrier.mType     = Type::Buffer;
        barrier.mResource = resource;
        barrier.mBefore   = before;
        barrier.mAfter    = after;
        return barrier;
    }

    static GPUBarrier Memory(ResourceUsage before, ResourceUsage after)
    {
        GPUBarrier barrier;
        barrier.mType     = Type::Memory;
        barrier.mResource = 0;
        barrier.mBefore   = before;
        barrier.mAfter    = after;
        return barrier;
    }

    static GPUBarrier Image(GPUResource *resource, ResourceUsage before, ResourceUsage after)
    {
        GPUBarrier barrier;
        Assert(resource->mResourceType == GPUResource::ResourceType::Image);
        barrier.mType     = Type::Image;
        barrier.mResource = resource;
        barrier.mBefore   = before;
        barrier.mAfter    = after;
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
    } mTextureType              = TextureType::Texture2D;
    u32 mWidth                  = 1;
    u32 mHeight                 = 1;
    u32 mDepth                  = 1;
    u32 mNumMips                = 1;
    u32 mNumLayers              = 1;
    Format mFormat              = Format::Null;
    MemoryUsage mUsage          = MemoryUsage::GPU_ONLY;
    ResourceUsage mInitialUsage = ResourceUsage::None;
    ResourceUsage mFutureUsages = ResourceUsage::None;
    enum class DefaultSampler
    {
        None,
        Nearest,
        Linear,
    } mSampler = DefaultSampler::None;
};

struct TextureMappedData
{
    void *mappedData;
    u32 size;
};

struct Texture : GPUResource
{
    TextureDesc mDesc;
    TextureMappedData mappedData;
};

struct SamplerDesc
{
    Filter mMag              = Filter::Nearest;
    Filter mMin              = Filter::Nearest;
    Filter mMipMode          = Filter::Nearest;
    SamplerMode mMode        = SamplerMode::Wrap;
    BorderColor mBorderColor = BorderColor::TransparentBlack;
    CompareOp mCompareOp     = CompareOp::None;
    u32 mMaxAnisotropy       = 0;
};

struct Sampler : GraphicsObject
{
    SamplerDesc mDesc;
};

struct RenderPassImage
{
    enum class RenderImageType
    {
        Depth,
    } mImageType;
    Texture *mTexture;

    ResourceUsage mLayoutBefore = ResourceUsage::None;
    ResourceUsage mLayout       = ResourceUsage::None;
    ResourceUsage mLayoutAfter  = ResourceUsage::None;
    i32 mSubresource            = -1;

    static RenderPassImage DepthStencil(Texture *texture, ResourceUsage layoutBefore, ResourceUsage layoutAfter, i32 subresource = -1)
    {
        RenderPassImage image;
        image.mImageType    = RenderImageType::Depth;
        image.mTexture      = texture;
        image.mLayoutBefore = layoutBefore;
        image.mLayout       = ResourceUsage::DepthStencil;
        image.mLayoutAfter  = layoutAfter;
        image.mSubresource  = subresource;
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
    u32 blockSize = GetBlockSize(desc.mFormat);
    u32 stride    = GetFormatSize(desc.mFormat);
    const u32 x   = desc.mWidth / blockSize;
    const u32 y   = desc.mHeight / blockSize;

    Assert(desc.mNumMips == 1);
    Assert(desc.mNumLayers == 1);
    Assert(desc.mDepth == 1);
    size += x * y * stride;

    return size;
}

struct mkGraphics
{
    static const i32 cNumBuffers = 2;
    f64 cTimestampPeriod;
    u64 mFrameCount = 0;

    inline f64 GetTimestampPeriod() { return cTimestampPeriod; }
    virtual u64 GetMinAlignment(GPUBufferDesc *inDesc)                                                             = 0;
    virtual void FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize = ~0, u64 inOffset = 0) = 0;

    virtual b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain)               = 0;
    virtual void CreatePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)           = 0;
    virtual void CreateComputePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)    = 0;
    virtual void CreateShader(Shader *shader, string shaderData)                                        = 0;
    virtual void CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback) = 0;

    void CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData)
    {
        if (inData == 0)
        {
            CreateBufferCopy(inBuffer, inDesc, 0);
        }
        else
        {
            CopyFunction func = [&](void *dest) { MemoryCopy(dest, inData, inDesc.mSize); };
            CreateBufferCopy(inBuffer, inDesc, func);
        }
    }

    void ResizeBuffer(GPUBuffer *buffer, u32 newSize)
    {
        GPUBufferDesc desc = buffer->mDesc;
        desc.mSize         = newSize;
        DeleteBuffer(buffer);
        CreateBuffer(buffer, desc, 0);
    }

    virtual void CopyBuffer(CommandList cmd, GPUBuffer *dest, GPUBuffer *source, u32 size)                               = 0;
    virtual void CopyTexture(CommandList cmd, Texture *dst, Texture *src, Rect3U32 *rect = 0)                            = 0;
    virtual void DeleteBuffer(GPUBuffer *buffer)                                                                         = 0;
    virtual void CreateTexture(Texture *outTexture, TextureDesc desc, void *inData)                                      = 0;
    virtual void DeleteTexture(Texture *texture)                                                                         = 0;
    virtual void CreateSampler(Sampler *sampler, SamplerDesc desc)                                                       = 0;
    virtual void BindResource(GPUResource *resource, ResourceType type, u32 slot, CommandList cmd, i32 subresource = -1) = 0;
    virtual i32 GetDescriptorIndex(GPUResource *resource, ResourceType type, i32 subresourceIndex = -1)                  = 0;

    virtual i32 CreateSubresource(GPUBuffer *buffer, ResourceType type, u64 offset = 0ull, u64 size = ~0ull, Format format = Format::Null, const char *name = 0) = 0;

    virtual i32 CreateSubresource(Texture *texture, u32 baseLayer = 0, u32 numLayers = ~0u)                                   = 0;
    virtual void UpdateDescriptorSet(CommandList cmd)                                                                         = 0;
    virtual CommandList BeginCommandList(QueueType queue)                                                                     = 0;
    virtual void BeginRenderPass(Swapchain *inSwapchain, RenderPassImage *images, u32 count, CommandList inCommandList)       = 0;
    virtual void BeginRenderPass(RenderPassImage *images, u32 count, CommandList cmd)                                         = 0;
    virtual void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex)                                                      = 0;
    virtual void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex)                                = 0;
    virtual void BindVertexBuffer(CommandList cmd, GPUBuffer **buffers, u32 count = 1, u32 *offsets = 0)                      = 0;
    virtual void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer, u64 offset = 0)                                          = 0;
    virtual void Dispatch(CommandList cmd, u32 groupCountX, u32 groupCountY, u32 groupCountZ)                                 = 0;
    virtual void SetViewport(CommandList cmd, Viewport *viewport)                                                             = 0;
    virtual void SetScissor(CommandList cmd, Rect2 scissor)                                                                   = 0;
    virtual void EndRenderPass(CommandList cmd)                                                                               = 0;
    virtual void SubmitCommandLists()                                                                                         = 0;
    virtual void BindPipeline(PipelineState *ps, CommandList cmd)                                                             = 0;
    virtual void BindCompute(PipelineState *ps, CommandList cmd)                                                              = 0;
    virtual void PushConstants(CommandList cmd, u32 size, void *data, u32 offset = 0)                                         = 0;
    virtual void WaitForGPU()                                                                                                 = 0;
    virtual void Wait(CommandList waitFor, CommandList cmd)                                                                   = 0;
    virtual void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)                                                    = 0;
    virtual b32 IsSignaled(Fence fence)                                                                                       = 0;
    virtual b32 IsLoaded(GPUResource *resource)                                                                               = 0;
    virtual void CreateQueryPool(QueryPool *pool, QueryType type, u32 queryCount)                                             = 0;
    virtual void BeginQuery(QueryPool *pool, CommandList cmd, u32 queryIndex)                                                 = 0;
    virtual void EndQuery(QueryPool *pool, CommandList cmd, u32 queryIndex)                                                   = 0;
    virtual void ResolveQuery(QueryPool *pool, CommandList cmd, GPUBuffer *buffer, u32 queryIndex, u32 count, u32 destOffset) = 0;
    virtual void ResetQuery(QueryPool *pool, CommandList cmd, u32 index, u32 count)                                           = 0;
    virtual void SetName(GPUResource *resource, const char *name)                                                             = 0;

    virtual u32 GetCurrentBuffer() = 0;
};

template <>
struct enable_bitmask_operators<graphics::ResourceUsage>
{
    static const bool enable = true;
};
template <>
struct enable_bitmask_operators<graphics::ShaderStage>
{
    static const bool enable = true;
};

} // namespace graphics

#endif
