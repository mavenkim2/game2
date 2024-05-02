namespace graphics
{

#if WINDOWS
typedef HWND Window;
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
};
struct SwapchainDesc
{
    u32 width;
    u32 height;
    Format format;
    // Colorspace mColorspace;
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
};

struct CommandList : GraphicsObject
{
};

struct mkGraphics
{
    static const i32 cNumBuffers = 2;
    u64 mFrameCount              = 0;

    virtual b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain) = 0;
    virtual void CreateShader()                                                           = 0;
    virtual CommandList BeginCommandList(QueueType queue)                                 = 0;
    virtual void BeginRenderPass(Swapchain *inSwapchain, CommandList *inCommandList)      = 0;
};
} // namespace graphics
