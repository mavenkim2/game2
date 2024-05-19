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

        // Descriptor bindings
        GPUResource mResourceTable[cMaxBindings];

        // Descriptor set
        // VkDescriptorSet mDescriptorSets[cNumBuffers][QueueType_Count];
        list<VkDescriptorSet> mDescriptorSets[cNumBuffers];
        u32 mCurrentSet = 0;
        // b32 mIsDirty[cNumBuffers][QueueType_Count] = {};

        const VkCommandBuffer
        GetCommandBuffer() const
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

    u32 GetCurrentBuffer() override
    {
        return mFrameCount % cNumBuffers;
    }

    u32 GetNextBuffer()
    {
        return (mFrameCount + 1) % cNumBuffers;
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

    // TODO: the descriptor sets shouldn't be here.
    struct PipelineStateVulkan
    {
        VkPipeline mPipeline;
        list<VkDescriptorSetLayoutBinding> mLayoutBindings;
        list<VkDescriptorSetLayout> mDescriptorSetLayouts; // TODO: this is just one
        list<VkDescriptorSet> mDescriptorSets;
        // u32 mCurrentSet = 0;
        VkPipelineLayout mPipelineLayout;

        VkPushConstantRange mPushConstantRange = {};
    };

    //////////////////////////////
    // Buffers
    //
    struct GPUBufferVulkan
    {
        VkBuffer mBuffer;
        VmaAllocation mAllocation;

        struct Subresource
        {
            VkDescriptorBufferInfo info;
            i32 descriptorIndex = -1;

            b32 IsValid()
            {
                return descriptorIndex != -1;
            }
        };
        Subresource subresource;
        list<Subresource> subresources;
    };

    //////////////////////////////
    // Textures/Samplers
    //

    struct TextureVulkan
    {
        VkImage mImage            = VK_NULL_HANDLE;
        VmaAllocation mAllocation = VK_NULL_HANDLE;

        struct Subresource
        {
            VkImageView mImageView = VK_NULL_HANDLE;
            u32 mBaseLayer;
            u32 mNumLayers;
            i32 descriptorIndex;
        };
        Subresource mSubresource;        // whole view
        list<Subresource> mSubresources; // sub views
    };

    struct SamplerVulkan
    {
        VkSampler mSampler;
    };

    //////////////////////////////
    // Allocation/Deferred cleanup
    //

    VmaAllocator mAllocator;
    Mutex mCleanupMutex = {};
    list<VkSemaphore> mCleanupSemaphores[cNumBuffers];
    list<VkSwapchainKHR> mCleanupSwapchains[cNumBuffers];
    list<VkImageView> mCleanupImageViews[cNumBuffers];

    struct CleanupBuffer
    {
        VkBuffer mBuffer;
        VmaAllocation mAllocation;
    };
    list<CleanupBuffer> mCleanupBuffers[cNumBuffers];
    // list<VmaAllocation> mCleanupAllocations[cNumBuffers];

    void Cleanup();

    // Frame allocators
    struct FrameData
    {
        GPUBuffer mBuffer;
        std::atomic<u64> mOffset = 0;
        // u64 mTotalSize           = 0;
        u32 mAlignment = 0;
    } mFrameAllocator[cNumBuffers];

    // The gpu buffer should already be created
    void FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize = ~0, u64 inOffset = 0);

    //////////////////////////////
    // Functions
    //
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

    TextureVulkan *ToInternal(Texture *texture)
    {
        return (TextureVulkan *)(texture->internalState);
    }

    mkGraphicsVulkan(OS_Handle window, ValidationMode validationMode, GPUDevicePreference preference);
    u64 GetMinAlignment(GPUBufferDesc *inDesc) override;
    b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain) override;
    void CreateShader(PipelineStateDesc *inDesc, PipelineState *outPS, string name) override;
    void CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback) override;
    void CopyBuffer(CommandList cmd, GPUBuffer *dest, GPUBuffer *src, u32 size) override;
    void DeleteBuffer(GPUBuffer *buffer) override;
    void CreateTexture(Texture *outTexture, TextureDesc desc, void *inData) override;
    void CreateSampler(Sampler *sampler, SamplerDesc desc) override;
    void BindResource(GPUResource *resource, u32 slot, CommandList cmd) override;
    i32 GetDescriptorIndex(Texture *resource, i32 subresourceIndex = -1) override;
    i32 GetDescriptorIndex(GPUBuffer *buffer, i32 subresourceIndex = -1) override;
    i32 CreateSubresource(GPUBuffer *buffer, SubresourceType type, u64 offset = 0ull, u64 size = ~0ull, Format format = Format::Null) override;
    i32 CreateSubresource(Texture *texture, u32 baseLayer = 0, u32 numLayers = ~0u) override;
    void UpdateDescriptorSet(CommandList cmd);
    CommandList BeginCommandList(QueueType queue) override;
    void BeginRenderPass(Swapchain *inSwapchain, RenderPassImage *images, u32 count, CommandList inCommandList) override;
    void BeginRenderPass(RenderPassImage *images, u32 count, CommandList cmd) override;
    void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex) override;
    void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex);

    void BindVertexBuffer(CommandList cmd, GPUBuffer **buffers, u32 count = 1, u32 *offsets = 0) override;
    void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer, u64 offset = 0) override;
    void SetViewport(CommandList cmd, Viewport *viewport) override;
    void SetScissor(CommandList cmd, Rect2 scissor) override;
    void EndRenderPass(CommandList cmd) override;
    void SubmitCommandLists() override;
    void BindPipeline(const PipelineState *ps, CommandList cmd) override;
    void PushConstants(CommandList cmd, u32 size, void *data, u32 offset = 0) override;
    void WaitForGPU() override;

    void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count) override;

    void SetName(GPUResource *resource, const char *name) override;
    void SetName(u64 handle, GraphicsObjectType type, const char *name) override;

private:
    const i32 cPoolSize = 64;
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

    //////////////////////////////
    // Bindless resources
    //
    enum DescriptorType
    {
        // DescriptorType_Uniform,
        DescriptorType_CombinedSampler,
        DescriptorType_Storage,
        DescriptorType_Count,
    };

    struct BindlessDescriptorPool
    {
        VkDescriptorPool pool        = VK_NULL_HANDLE;
        VkDescriptorSet set          = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;

        u32 descriptorCount;
        list<i32> freeList;

        Mutex mutex = {};

        i32 Allocate()
        {
            i32 result = -1;
            MutexScope(&mutex)
            {
                if (freeList.size() != 0)
                {
                    result = freeList.back();
                    freeList.pop_back();
                }
            }
            return result;
        }

        void Free(i32 i)
        {
            if (i >= 0)
            {
                MutexScope(&mutex)
                {
                    freeList.push_back(i);
                }
            }
        }
    };

    BindlessDescriptorPool bindlessDescriptorPools[DescriptorType_Count];

    list<VkDescriptorSet> bindlessDescriptorSets;
    list<VkDescriptorSetLayout> bindlessDescriptorSetLayouts;

    //////////////////////////////
    // Default samplers
    //
    VkSampler mNullSampler;
    VkSampler mLinearSampler;
    VkSampler mNearestSampler;

    VkImage mNullImage2D;
    VmaAllocation mNullImage2DAllocation;
    VkImageView mNullImageView2D;
    VkImageView mNullImageView2DArray;

    VkBuffer mNullBuffer;
    VmaAllocation mNullBufferAllocation;
};

} // namespace graphics

#endif
