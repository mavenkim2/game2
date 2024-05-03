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

struct PipelineStateDesc
{
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

struct GPUBuffer : GraphicsObject
{
    GPUBufferDesc mDesc;
    void *mMappedData;
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

    virtual b32 CreateSwapchain(Window window, Instance instance, SwapchainDesc *desc, Swapchain *swapchain) = 0;
    virtual void CreateShader(PipelineStateDesc *inDesc, PipelineState *outPS)                               = 0;
    virtual void CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData)                       = 0;
    virtual CommandList BeginCommandList(QueueType queue)                                                    = 0;
    virtual void BeginRenderPass(Swapchain *inSwapchain, CommandList *inCommandList)                         = 0;
    virtual void Draw(CommandList *cmd, u32 vertexCount, u32 firstVertex)                                    = 0;
    virtual void SetViewport(CommandList *cmd, Viewport *viewport)                                           = 0;
    virtual void SetScissor(CommandList *cmd, Rect2 scissor)                                                 = 0;
    virtual void EndRenderPass(CommandList *cmd)                                                             = 0;
    virtual void BindPipeline(const PipelineState *ps, CommandList *cmd)                                     = 0;
    virtual void WaitForGPU()                                                                                = 0;
};
} // namespace graphics
