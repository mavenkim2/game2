#ifndef MK_GRAPHICS_VULKAN_H
#define MK_GRAPHICS_VULKAN_H

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

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "../third_party/vulkan/vk_mem_alloc.h"

namespace graphics
{

struct mkGraphicsVulkan : mkGraphics
{
    Arena *mArena;
    Mutex mArenaMutex = {};

    //////////////////////////////
    // API State
    //
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    list<VkQueueFamilyProperties2> mQueueFamilyProperties;
    list<u32> mFamilies;
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

    VkPhysicalDeviceMemoryProperties2 mMemProperties;

    //////////////////////////////
    // Queues, command pools & buffers, fences
    //
    struct CommandQueue
    {
        VkQueue mQueue;
        Mutex mLock = {};
    } mQueues[QueueType_Count];

    VkFence mFrameFences[cNumBuffers][QueueType_Count] = {};

    // TODO: rethink this orientation?
    struct CommandListVulkan
    {
        VkCommandPool mCommandPools[cNumBuffers][QueueType_Count]     = {};
        VkCommandBuffer mCommandBuffers[cNumBuffers][QueueType_Count] = {};
        u32 mCurrentQueue                                             = 0;
        u32 mCurrentBuffer                                            = 0;
        const PipelineState *mCurrentPipeline                         = 0;

        list<VkImageMemoryBarrier2> mEndPassImageMemoryBarriers;
        list<Swapchain> mUpdateSwapchains;

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
    u32 mCmdCount             = 0;

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
        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        VkSurfaceKHR mSurface;
        VkExtent2D mExtent;

        list<VkImage> mImages;
        list<VkImageView> mImageViews;

        list<VkSemaphore> mAcquireSemaphores;
        VkSemaphore mReleaseSemaphore = VK_NULL_HANDLE;

        u32 mAcquireSemaphoreIndex = 0;
        u32 mImageIndex;
    };

    //////////////////////////////
    // Descriptors
    //
    VkDescriptorPool mPool;

    //////////////////////////////
    // Pipelines
    //
    list<VkDynamicState> mDynamicStates;
    VkPipelineDynamicStateCreateInfo mDynamicStateInfo;
    struct PipelineStateVulkan
    {
        VkPipeline mPipeline;
        list<VkDescriptorSetLayout> mDescriptorSetLayouts;
        list<VkDescriptorSet> mDescriptorSets;
        VkPipelineLayout mPipelineLayout;
    };

    //////////////////////////////
    // Buffers
    //
    struct GPUBufferVulkan
    {
        VkBuffer mBuffer;
        VmaAllocation mAllocation;
    };

    // Frame allocators
    struct FrameData
    {
        GPUBuffer mBuffer;
        std::atomic<u64> mOffset = 0;
        u64 mTotalSize           = 0;
        u32 mAlignment           = 0;
    } mFrameAllocator[cNumBuffers];

    // The gpu buffer should already be created
    void FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize = ~0, u64 inOffset = 0);

    //////////////////////////////
    // Allocation/Deferred cleanup
    //

    VmaAllocator mAllocator;
    Mutex mCleanupMutex = {};
    list<VkSemaphore> mCleanupSemaphores;
    list<VkSwapchainKHR> mCleanupSwapchains;
    list<VkImageView> mCleanupImageViews;

    void Cleanup();

    // Functions
    SwapchainVulkan *ToInternal(Swapchain *swapchain)
    {
        return (SwapchainVulkan *)(swapchain->internalState);
    }

    CommandListVulkan *ToInternal(CommandList commandlist)
    {
        return (CommandListVulkan *)(commandlist.internalState);
    }

    PipelineStateVulkan *ToInternal(const PipelineState *ps)
    {
        return (PipelineStateVulkan *)(ps->internalState);
    }

    GPUBufferVulkan *ToInternal(GPUBuffer *gb)
    {
        return (GPUBufferVulkan *)(gb->internalState);
    }

    mkGraphicsVulkan(OS_Handle window, ValidationMode validationMode, GPUDevicePreference preference);
    b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain) override;
    void CreateShader(PipelineStateDesc *inDesc, PipelineState *outPS) override;
    void CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData) override;
    void UpdateBuffer(GPUBuffer *inBuffer, void *inData);
    void UpdateDescriptorSet(CommandList cmd, GPUBuffer *buffer);
    CommandList BeginCommandList(QueueType queue) override;
    void BeginRenderPass(Swapchain *inSwapchain, CommandList inCommandList) override;
    void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex) override;
    void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex);

    // TODO: specify these in the command list or something
    void BindVertexBuffer(CommandList cmd, GPUBuffer *buffer) override;
    void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer) override;
    void SetViewport(CommandList cmd, Viewport *viewport) override;
    void SetScissor(CommandList cmd, Rect2 scissor) override;
    void EndRenderPass(CommandList cmd) override;
    void BindPipeline(const PipelineState *ps, CommandList cmd) override;
    void WaitForGPU() override;

    void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count) override;

private:
    b32 CreateSwapchain(Swapchain *inSwapchain);

    //////////////////////////////
    // Dedicated transfer queue
    //
    Mutex mTransferMutex = {};
    struct TransferCommand
    {
        VkCommandPool mCmdPool                       = VK_NULL_HANDLE; // command pool to issue transfer request
        VkCommandBuffer mCmdBuffer                   = VK_NULL_HANDLE;
        VkCommandPool mTransitionPool                = VK_NULL_HANDLE; // command pool to issue transfer request
        VkCommandBuffer mTransitionBuffer            = VK_NULL_HANDLE;
        VkFence mFence                               = VK_NULL_HANDLE; // signals cpu that transfer is complete
        VkSemaphore mSemaphores[QueueType_Count - 1] = {};             // graphics, compute
        GPUBuffer mUploadBuffer;

        const b32 IsValid()
        {
            return mCmdPool != VK_NULL_HANDLE;
        }
    };
    list<TransferCommand> mTransferFreeList;

    TransferCommand Stage(u64 size);
    void Submit(TransferCommand cmd);
};

} // namespace graphics

#endif
