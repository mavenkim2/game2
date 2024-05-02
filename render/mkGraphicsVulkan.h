#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "../mkCommon.h"
#endif

#ifdef WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VK_NO_PROTOTYPES
#include "../third_party/vulkan/vulkan.h"
#include "../third_party/vulkan/volk.h"
#include "mkGraphics.h"

namespace graphics
{

struct mkGraphicsVulkan : mkGraphics
{
    Arena *mArena;

    //////////////////////////////
    // API State
    //
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    list<VkQueueFamilyProperties2> mQueueFamilyProperties;
    u32 mGraphicsFamily = VK_QUEUE_FAMILY_IGNORED;
    u32 mComputeFamily  = VK_QUEUE_FAMILY_IGNORED;
    u32 mCopyFamily     = VK_QUEUE_FAMILY_IGNORED;

    VkPhysicalDeviceProperties2 mDeviceProperties;
    // VkPhysicalDeviceVulkan11Properties mProperties11;
    // VkPhysicalDeviceVulkan12Properties mProperties12;
    // VkPhysicalDeviceVulkan13Properties mProperties13;

    VkPhysicalDeviceFeatures2 mDeviceFeatures;
    VkPhysicalDeviceVulkan11Features mFeatures11;
    VkPhysicalDeviceVulkan12Features mFeatures12;
    VkPhysicalDeviceVulkan13Features mFeatures13;

    //////////////////////////////
    // Queues, command pools & buffers, fences
    //
    struct CommandQueue
    {
        VkQueue mQueue;
    } mQueues[QueueType_Count];

    VkFence mFrameFences[cNumBuffers][QueueType_Count] = {};

    // TODO: rethink this orientation?
    struct CommandListVulkan
    {
        VkCommandPool mCommandPools[cNumBuffers][QueueType_Count]     = {};
        VkCommandBuffer mCommandBuffers[cNumBuffers][QueueType_Count] = {};
        u32 mCurrentQueue                                             = 0;
        u32 mCurrentBuffer                                            = 0;

        const VkCommandBuffer GetCommandBuffer() const
        {
            return mCommandBuffers[mCurrentBuffer][mCurrentQueue];
        }

        const VkCommandPool GetCommandPool() const
        {
            return mCommandPools[mCurrentBuffer][mCurrentQueue];
        }
    };
    list<CommandListVulkan> mCommandLists;
    TicketMutex mCommandMutex = {};
    u32 mCmdCount;

    CommandListVulkan &GetCommandList(CommandList cmd)
    {
        Assert(cmd.IsValid());
        return *(CommandListVulkan *)(cmd.internalState);
    }

    u32 GetCurrentBuffer()
    {
        return mFrameCount % cNumBuffers;
    }

    b32 mDebugUtils = false;

    // Shaders
    // VkPipelineShaderStage
    struct SwapchainVulkan
    {
        SwapchainDesc mDesc;
        VkSwapchainKHR mSwapchain;
        VkSurfaceKHR mSurface;
        VkExtent2D mExtent;

        list<VkImage> mImages;
        list<VkImageView> mImageViews;

        list<VkSemaphore> mAcquireSemaphores;
        VkSemaphore mReleaseSemaphore = 0;

        u32 mAcquireSemaphoreIndex;
        u32 mImageIndex;
    };

    //////////////////////////////
    // Pipelines
    //
    list<VkDynamicState> mDynamicStates;
    VkPipelineDynamicStateCreateInfo mDynamicStateInfo;
    VkPipeline mPipeline;

    // Functions
    SwapchainVulkan *ToInternal(Swapchain *swapchain)
    {
        return (SwapchainVulkan *)(swapchain->internalState);
    }

    CommandListVulkan *ToInternal(CommandList *commandlist)
    {
        return (CommandListVulkan *)(commandlist->internalState);
    }

    mkGraphicsVulkan(OS_Handle window, ValidationMode validationMode, GPUDevicePreference preference);
    b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain) override;
    void CreateShader() override;
    virtual CommandList BeginCommandList(QueueType queue) override;
    void BeginRenderPass(Swapchain *inSwapchain, CommandList *inCommandList) override;
};
} // namespace graphics
