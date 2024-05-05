#ifndef MK_GRAPHICS_H
#define MK_GRAPHICS_H

namespace graphics
{

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
    QueueType_Copy,
    QueueType_Compute,

    QueueType_Count,
};

enum class Format
{
    Null,
    B8G8R8_UNORM,
    B8G8R8_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,

    R32G32_SFLOAT,
    R32G32B32_SFLOAT,
    R32G32B32A32_SFLOAT,

    R32G32B32A32_UINT,
};

template <typename E>
inline b32 HasFlags(E lhs, E rhs)
{
    return (lhs & rhs) == rhs;
}

typedef u32 BindFlag;
enum
{
    BingFlag_None    = 0,
    BindFlag_Vertex  = 1 << 0,
    BindFlag_Index   = 1 << 1,
    BindFlag_Uniform = 1 << 2,
};

enum class MemoryUsage
{
    GPU_ONLY,
    CPU_ONLY,
    CPU_TO_GPU,
    GPU_TO_CPU,
};

inline b32 HasFlags(BindFlag lhs, BindFlag rhs)
{
    return (lhs & rhs) == rhs;
}

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
    Vertex,   // Vertex shader
    Geometry, // Geometry shader
    Fragment, // Fragment shader
    Compute,  // Compute shader
    Count,
};

enum class DescriptorType
{
    Uniform,
};

struct Descriptor
{
    DescriptorType mDescType;
    ShaderStage mShaderStage;
};

struct Shader
{
    string mName;
};

enum ShaderType
{
    VS_TEST,
    FS_TEST,

    ShaderType_Count,
};

struct PipelineStateDesc
{
    Shader *mVS;
    Shader *mFS;
    list<InputLayout> mInputLayouts;
};

struct GraphicsObject
{
    void *internalState = 0;

    b32 IsValid()
    {
        return internalState != 0;
    }
};

struct GPUResource : GraphicsObject
{
    enum class ResourceType
    {
        Buffer,
        Image,
    } mResourceType;
};

struct Swapchain : GraphicsObject
{
    SwapchainDesc mDesc;
    const SwapchainDesc GetDesc()
    {
        return mDesc;
    }
};

struct CommandList : GraphicsObject
{
};

struct PipelineState : GraphicsObject
{
    PipelineStateDesc mDesc;
};

struct GPUBufferDesc
{
    u64 mSize;
    BindFlag mFlags    = 0;
    MemoryUsage mUsage = MemoryUsage::GPU_ONLY;
};

struct GPUBuffer : GPUResource
{
    GPUBufferDesc mDesc;
    void *mMappedData;
};

typedef u32 ResourceState;
enum
{
    ResourceState_None          = 0,
    ResourceState_UniformBuffer = 1 << 1,
    // ShaderWrite = 1 << 1,

    ResourceState_VertexBuffer = 1 << 2,
    ResourceState_IndexBuffer  = 1 << 3,

    ResourceState_TransferSrc = 1 << 4,
    ResourceState_TransferDst = 1 << 5,
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

    ResourceState mBefore;
    ResourceState mAfter;
};

u32 GetFormatSize(Format format)
{
    switch (format)
    {
        case Format::R32G32_SFLOAT:
            return 8;
        case Format::R32G32B32_SFLOAT:
            return 12;
        case Format::R32G32B32A32_SFLOAT:
        case Format::R32G32B32A32_UINT:
            return 16;
        default:
            Assert(0);
            return 16;
    }
}

struct mkGraphics
{
    static const i32 cNumBuffers = 2;
    u64 mFrameCount              = 0;

    virtual void FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize = ~0, u64 inOffset = 0) = 0;

    virtual b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain)      = 0;
    virtual void CreateShader(PipelineStateDesc *inDesc, PipelineState *outPS)                 = 0;
    virtual void CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData)         = 0;
    virtual void UpdateBuffer(GPUBuffer *inBuffer, void *inData)                               = 0;
    virtual void UpdateDescriptorSet(CommandList cmd, GPUBuffer *buffer)                       = 0;
    virtual CommandList BeginCommandList(QueueType queue)                                      = 0;
    virtual void BeginRenderPass(Swapchain *inSwapchain, CommandList inCommandList)            = 0;
    virtual void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex)                       = 0;
    virtual void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex) = 0;
    virtual void BindVertexBuffer(CommandList cmd, GPUBuffer *buffer)                          = 0;
    virtual void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer)                           = 0;
    virtual void SetViewport(CommandList cmd, Viewport *viewport)                              = 0;
    virtual void SetScissor(CommandList cmd, Rect2 scissor)                                    = 0;
    virtual void EndRenderPass(CommandList cmd)                                                = 0;
    virtual void BindPipeline(const PipelineState *ps, CommandList cmd)                        = 0;
    virtual void WaitForGPU()                                                                  = 0;
    virtual void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)                     = 0;
};
} // namespace graphics

#endif
