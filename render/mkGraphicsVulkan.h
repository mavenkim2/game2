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

#include <queue>

#define VK_CHECK(check)                \
    do                                 \
    {                                  \
        VkResult result_ = check;      \
        Assert(result_ == VK_SUCCESS); \
    } while (0);

namespace graphics
{

struct mkGraphicsVulkan : mkGraphics
{
    Arena *mArena;
    Mutex mArenaMutex = {};
    DeviceCapabilities capabilities;

    //////////////////////////////
    // API State
    //
    VkInstance mInstance;
    VkPhysicalDevice physicalDevice;
    VkDevice mDevice;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    list<VkQueueFamilyProperties2> mQueueFamilyProperties;
    list<u32> mFamilies;
    u32 mGraphicsFamily = VK_QUEUE_FAMILY_IGNORED;
    u32 mComputeFamily  = VK_QUEUE_FAMILY_IGNORED;
    u32 mCopyFamily     = VK_QUEUE_FAMILY_IGNORED;

    VkPhysicalDeviceProperties2 mDeviceProperties;
    VkPhysicalDeviceVulkan11Properties mProperties11;
    VkPhysicalDeviceVulkan12Properties mProperties12;
    VkPhysicalDeviceVulkan13Properties mProperties13;
    VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties;
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR variableShadingRateProperties;

    VkPhysicalDeviceFeatures2 mDeviceFeatures;
    VkPhysicalDeviceVulkan11Features mFeatures11;
    VkPhysicalDeviceVulkan12Features mFeatures12;
    VkPhysicalDeviceVulkan13Features mFeatures13;
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR variableShadingRateFeatures;

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

    struct CommandListVulkan
    {
        QueueType type;
        VkCommandPool commandPools[cNumBuffers]     = {};
        VkCommandBuffer commandBuffers[cNumBuffers] = {};
        u32 currentBuffer                           = 0;
        PipelineState *currentPipeline              = 0;
        VkSemaphore semaphore;
        list<CommandList> waitForCmds;
        std::atomic_bool waitedOn{false};

        list<VkImageMemoryBarrier2> endPassImageMemoryBarriers;
        list<Swapchain> updateSwapchains;

        // Descriptor bindings

        BindedResource srvTable[cMaxBindings] = {};
        BindedResource uavTable[cMaxBindings] = {};

        // Descriptor set
        // VkDescriptorSet mDescriptorSets[cNumBuffers][QueueType_Count];
        list<VkDescriptorSet> descriptorSets[cNumBuffers];
        u32 currentSet = 0;
        // b32 mIsDirty[cNumBuffers][QueueType_Count] = {};

        CommandListVulkan *next;

        const VkCommandBuffer GetCommandBuffer() const
        {
            return commandBuffers[currentBuffer];
        }

        const VkCommandPool GetCommandPool() const
        {
            return commandPools[currentBuffer];
        }
    };

    // TODO: consider using buckets?
    u32 numCommandLists = 0;
    list<CommandListVulkan *> commandLists;
    // list<CommandListVulkan *> computeCommandLists;
    TicketMutex mCommandMutex = {};

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

        SwapchainVulkan *next;
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
        list<VkDescriptorSetLayout> mDescriptorSetLayouts;
        list<VkDescriptorSet> mDescriptorSets;
        // u32 mCurrentSet = 0;
        VkPipelineLayout mPipelineLayout;

        VkPushConstantRange mPushConstantRange = {};
        PipelineStateVulkan *next;
    };

    struct ShaderVulkan
    {
        VkShaderModule module;
        VkPipelineShaderStageCreateInfo pipelineStageInfo;
        list<VkDescriptorSetLayoutBinding> layoutBindings;
        VkPushConstantRange pushConstantRange;

        ShaderVulkan *next;
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
            DescriptorType type;
            VkDescriptorBufferInfo info;
            VkBufferView view   = VK_NULL_HANDLE;
            i32 descriptorIndex = -1;

            b32 IsBindless()
            {
                return descriptorIndex != -1;
            }
        };
        i32 subresourceSrv;
        i32 subresourceUav;
        list<Subresource> subresources;
        GPUBufferVulkan *next;
    };

    //////////////////////////////
    // Fences
    //
    struct FenceVulkan
    {
        u32 count;
        VkFence fence;
        FenceVulkan *next;
    };

    //////////////////////////////
    // Textures/Samplers
    //

    struct TextureVulkan
    {
        VkImage mImage            = VK_NULL_HANDLE;
        VkBuffer stagingBuffer    = VK_NULL_HANDLE;
        VmaAllocation mAllocation = VK_NULL_HANDLE;

        struct Subresource
        {
            VkImageView mImageView = VK_NULL_HANDLE;
            u32 mBaseLayer;
            u32 mNumLayers;
            i32 descriptorIndex;

            b32 IsValid()
            {
                return mImageView != VK_NULL_HANDLE;
            }
        };
        Subresource mSubresource;        // whole view
        list<Subresource> mSubresources; // sub views
        TextureVulkan *next;
    };

    struct SamplerVulkan
    {
        VkSampler mSampler;
        SamplerVulkan *next;
    };

    //////////////////////////////
    // Allocation/Deferred cleanup
    //

    VmaAllocator mAllocator;
    Mutex mCleanupMutex = {};
    list<VkSemaphore> mCleanupSemaphores[cNumBuffers];
    list<VkSwapchainKHR> mCleanupSwapchains[cNumBuffers];
    list<VkImageView> mCleanupImageViews[cNumBuffers];
    list<VkBufferView> cleanupBufferViews[cNumBuffers];

    struct CleanupBuffer
    {
        VkBuffer mBuffer;
        VmaAllocation mAllocation;
    };
    list<CleanupBuffer> mCleanupBuffers[cNumBuffers];
    struct CleanupTexture
    {
        VkImage image;
        VmaAllocation allocation;
    };
    list<CleanupTexture> cleanupTextures[cNumBuffers];
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
    SwapchainVulkan *freeSwapchain     = 0;
    CommandListVulkan *freeCommandList = 0;
    PipelineStateVulkan *freePipeline  = 0;
    GPUBufferVulkan *freeBuffer        = 0;
    TextureVulkan *freeTexture         = 0;
    SamplerVulkan *freeSampler         = 0;
    ShaderVulkan *freeShader           = 0;
    FenceVulkan *freeFence             = 0;

    SwapchainVulkan *ToInternal(Swapchain *swapchain)
    {
        Assert(swapchain->IsValid());
        return (SwapchainVulkan *)(swapchain->internalState);
    }

    CommandListVulkan *ToInternal(CommandList commandlist)
    {
        Assert(commandlist.IsValid());
        return (CommandListVulkan *)(commandlist.internalState);
    }

    PipelineStateVulkan *ToInternal(PipelineState *ps)
    {
        Assert(ps->IsValid());
        return (PipelineStateVulkan *)(ps->internalState);
    }

    GPUBufferVulkan *ToInternal(GPUBuffer *gb)
    {
        Assert(gb->IsValid());
        return (GPUBufferVulkan *)(gb->internalState);
    }

    TextureVulkan *ToInternal(Texture *texture)
    {
        Assert(texture->IsValid());
        return (TextureVulkan *)(texture->internalState);
    }

    ShaderVulkan *ToInternal(Shader *shader)
    {
        Assert(shader->IsValid());
        return (ShaderVulkan *)(shader->internalState);
    }

    // VkFence ToInternal(Fence fence)
    // {
    //     Assert(fence.IsValid());
    //     return (VkFence)(fence.internalState);
    // }

    FenceVulkan *ToInternal(Fence *fence)
    {
        Assert(fence->IsValid());
        return (FenceVulkan *)(fence->internalState);
    }

    VkQueryPool ToInternal(QueryPool *pool)
    {
        Assert(pool->IsValid());
        return (VkQueryPool)(pool->internalState);
    }

    mkGraphicsVulkan(ValidationMode validationMode, GPUDevicePreference preference);
    u64 GetMinAlignment(GPUBufferDesc *inDesc) override;
    b32 CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *swapchain) override;
    void CreatePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name) override;
    void CreateComputePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name) override;
    void CreateShader(Shader *shader, string shaderData) override;
    void CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback) override;
    void CopyBuffer(CommandList cmd, GPUBuffer *dest, GPUBuffer *src, u32 size) override;
    void CopyTexture(CommandList cmd, Texture *dst, Texture *src, Rect3U32 *rect = 0) override;
    void DeleteBuffer(GPUBuffer *buffer) override;
    void CreateTexture(Texture *outTexture, TextureDesc desc, void *inData) override;
    void DeleteTexture(Texture *texture) override;
    void CreateSampler(Sampler *sampler, SamplerDesc desc) override;
    void BindResource(GPUResource *resource, ResourceType type, u32 slot, CommandList cmd, i32 subresource = -1) override;
    i32 GetDescriptorIndex(GPUResource *resource, ResourceType type, i32 subresourceIndex = -1) override;
    i32 CreateSubresource(GPUBuffer *buffer, ResourceType type, u64 offset = 0ull, u64 size = ~0ull, Format format = Format::Null,
                          const char *name = 0) override;
    i32 CreateSubresource(Texture *texture, u32 baseLayer = 0, u32 numLayers = ~0u) override;
    void UpdateDescriptorSet(CommandList cmd);
    CommandList BeginCommandList(QueueType queue) override;
    void BeginRenderPass(Swapchain *inSwapchain, RenderPassImage *images, u32 count, CommandList inCommandList) override;
    void BeginRenderPass(RenderPassImage *images, u32 count, CommandList cmd) override;
    void Draw(CommandList cmd, u32 vertexCount, u32 firstVertex) override;
    void DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex) override;
    void DrawIndexedIndirect(CommandList cmd, GPUBuffer *indirectBuffer, u32 drawCount, u32 offset = 0, u32 stride = 20) override;
    void DrawIndexedIndirectCount(CommandList cmd, GPUBuffer *indirectBuffer, GPUBuffer *countBuffer,
                                  u32 maxDrawCount, u32 indirectOffset = 0, u32 countOffset = 0, u32 stride = 20) override;
    void BindVertexBuffer(CommandList cmd, GPUBuffer **buffers, u32 count = 1, u32 *offsets = 0) override;
    void BindIndexBuffer(CommandList cmd, GPUBuffer *buffer, u64 offset = 0) override;
    void Dispatch(CommandList cmd, u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;
    void SetViewport(CommandList cmd, Viewport *viewport) override;
    void SetScissor(CommandList cmd, Rect2 scissor) override;
    void EndRenderPass(CommandList cmd) override;
    void SubmitCommandLists() override;
    void BindPipeline(PipelineState *ps, CommandList cmd) override;
    void BindCompute(PipelineState *ps, CommandList cmd) override;
    void PushConstants(CommandList cmd, u32 size, void *data, u32 offset = 0) override;
    void WaitForGPU() override;
    void Wait(CommandList waitFor, CommandList cmd) override;
    void Wait(CommandList wait) override;
    void Barrier(CommandList cmd, GPUBarrier *barriers, u32 count) override;
    b32 IsSignaled(FenceTicket ticket) override;
    b32 IsLoaded(GPUResource *resource) override;

    // Query pool
    void CreateQueryPool(QueryPool *pool, QueryType type, u32 queryCount) override;
    void BeginQuery(QueryPool *pool, CommandList cmd, u32 queryIndex) override;
    void EndQuery(QueryPool *pool, CommandList cmd, u32 queryIndex) override;
    void ResolveQuery(QueryPool *pool, CommandList cmd, GPUBuffer *buffer, u32 queryIndex, u32 count, u32 destOffset) override;
    void ResetQuery(QueryPool *pool, CommandList cmd, u32 index, u32 count) override;
    u32 GetCount(Fence f) override;

    // Debug
    void SetName(GPUResource *resource, const char *name) override;
    void SetName(GPUResource *resource, string name) override;

private:
    const i32 cPoolSize = 64;
    b32 CreateSwapchain(Swapchain *inSwapchain);

    //////////////////////////////
    // Dedicated transfer queue
    //
    struct RingAllocation
    {
        void *mappedData;
        u64 size;
        u32 offset;
        u32 ringId;
        b8 freed;
    };
    Mutex mTransferMutex = {};
    struct TransferCommand
    {
        VkCommandPool mCmdPool            = VK_NULL_HANDLE; // command pool to issue transfer request
        VkCommandBuffer mCmdBuffer        = VK_NULL_HANDLE;
        VkCommandPool mTransitionPool     = VK_NULL_HANDLE; // command pool to issue transfer request
        VkCommandBuffer mTransitionBuffer = VK_NULL_HANDLE;
        // VkFence mFence                               = VK_NULL_HANDLE; // signals cpu that transfer is complete
        Fence fence;
        VkSemaphore mSemaphores[QueueType_Count - 1] = {}; // graphics, compute
        RingAllocation *ringAllocation;

        const b32 IsValid()
        {
            return mCmdPool != VK_NULL_HANDLE;
        }
    };
    list<TransferCommand> mTransferFreeList;

    TransferCommand Stage(u64 size);

    void Submit(TransferCommand cmd);

    struct RingAllocator
    {
        TicketMutex lock;
        GPUBuffer transferRingBuffer;
        u64 ringBufferSize;
        u32 writePos;
        u32 readPos;
        u32 alignment;

        RingAllocation allocations[256];
        u16 allocationReadPos;
        u16 allocationWritePos;

    } stagingRingAllocators[4];

    // NOTE: there is a potential case where the allocation has transferred, but the fence isn't signaled (when command buffer
    // is being reused). current solution is to just not care, since it doesn't impact anything yet.
    RingAllocation *RingAlloc(u64 size);
    RingAllocation *RingAllocInternal(u32 ringId, u64 size);
    void RingFree(RingAllocation *allocation);

    //////////////////////////////
    // Bindless resources
    //
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

    // Linear wrap, nearest wrap, cmp > clamp to edge
    list<VkSampler> immutableSamplers;
    // VkSampler mLinearSampler;
    // VkSampler mNearestSampler;

    VkImage mNullImage2D;
    VmaAllocation mNullImage2DAllocation;
    VkImageView mNullImageView2D;
    VkImageView mNullImageView2DArray;

    VkBuffer mNullBuffer;
    VmaAllocation mNullBufferAllocation;

    //////////////////////////////
    // Debug
    //
    void SetName(u64 handle, VkObjectType type, const char *name);
    void SetName(VkDescriptorSetLayout handle, const char *name);
    void SetName(VkDescriptorSet handle, const char *name);
    void SetName(VkShaderModule handle, const char *name);
    void SetName(VkPipeline handle, const char *name);
    void SetName(VkQueue handle, const char *name);
};

} // namespace graphics

#endif
