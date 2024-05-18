#include "../mkCrack.h"
#include "mkGraphics.h"
#ifdef LSP_INCLUDE
#include "../mkList.h"
#include "../mkPlatformInc.h"
#include "mkGraphicsVulkan.h"
#endif

// #include "mkGraphicsVulkan.h"
#include "../third_party/vulkan/volk.c"

// Namespace only used in this file
namespace graphics
{
namespace vulkan
{

VkFormat ConvertFormat(Format value)
{
    switch (value)
    {
        case Format::B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case Format::B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
        case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

        case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;

        case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;

        case Format::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case Format::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        default: Assert(0); return VK_FORMAT_UNDEFINED;
    }
}

VkImageLayout ConvertResourceUsageToImageLayout(ResourceUsage usage)
{
    switch (usage)
    {
        case ResourceUsage::DepthStencil: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ResourceUsage::SampledTexture: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        default: Assert(0); return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

b32 HasFlags(TextureDesc *desc, ResourceUsage usage)
{
    return HasFlags(desc->mInitialUsage, usage) || HasFlags(desc->mFutureUsages, usage);
}

b32 IsFormatDepthSupported(Format format)
{
    b32 result = 0;
    switch (format)
    {
        case Format::D32_SFLOAT:
        case Format::D32_SFLOAT_S8_UINT:
        case Format::D24_UNORM_S8_UINT:
        {
            result = 1;
        }
        break;
    }
    return result;
}

b32 IsFormatStencilSupported(Format format)
{
    b32 result = 0;
    switch (format)
    {
        case Format::D32_SFLOAT_S8_UINT:
        case Format::D24_UNORM_S8_UINT:
        {
            result = 1;
        }
        break;
    }
    return result;
}

// Debug utils callback
VkBool32 DebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                                     void *userData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        Printf("[Vulkan Warning]: %s\n", callbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        Printf("[Vulkan Error]: %s\n", callbackData->pMessage);
    }

    return VK_FALSE;
}

VkShaderStageFlags ConvertShaderStage(ShaderStage stage)
{
    VkShaderStageFlags flags = 0;
    if (HasFlags(stage, ShaderStage::Vertex))
    {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (HasFlags(stage, ShaderStage::Fragment))
    {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (HasFlags(stage, ShaderStage::Geometry))
    {
        flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    if (HasFlags(stage, ShaderStage::Compute))
    {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    return flags;
}

VkAccessFlags2 ConvertResourceUsageToAccessFlag(ResourceUsage state)
{
    VkAccessFlags2 flags = VK_ACCESS_2_NONE;
    if (HasFlags(state, ResourceUsage::VertexBuffer))
    {
        flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::IndexBuffer))
    {
        flags |= VK_ACCESS_2_INDEX_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::UniformBuffer))
    {
        flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::TransferSrc))
    {
        flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::TransferDst))
    {
        flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    }
    if (HasFlags(state, ResourceUsage::SampledTexture))
    {
        flags |= VK_ACCESS_2_SHADER_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::DepthStencil))
    {
        flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    return flags;
}

VkAccessFlags2 ConvertResourceUsageToAccessFlag(TextureDesc *desc)
{
    return ConvertResourceUsageToAccessFlag(desc->mInitialUsage | desc->mFutureUsages);
}

VkPipelineStageFlags2 ConvertResourceToPipelineStage(ResourceUsage state)
{
    VkPipelineStageFlags2 flags = VK_PIPELINE_STAGE_2_NONE;
    if (HasFlags(state, ResourceUsage::VertexBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if (HasFlags(state, ResourceUsage::IndexBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
    // TODO: this is likely going to need to change
    if (HasFlags(state, ResourceUsage::UniformBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    if (HasFlags(state, ResourceUsage::TransferSrc) || HasFlags(state, ResourceUsage::TransferDst))
    {
        flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    if (HasFlags(state, ResourceUsage::SampledTexture))
    {
        flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // TODO: textures could be sampled earlier maybe
    }
    if (HasFlags(state, ResourceUsage::DepthStencil))
    {
        flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }
    return flags;
}

VkPipelineStageFlags2 ConvertResourceToPipelineStage(TextureDesc *desc)
{
    return ConvertResourceToPipelineStage(desc->mInitialUsage | desc->mFutureUsages);
}

VkFilter ConvertFilter(Filter filter)
{
    switch (filter)
    {
        case Filter::Nearest: return VK_FILTER_NEAREST;
        case Filter::Linear: return VK_FILTER_LINEAR;
        default: Assert(0); return VK_FILTER_NEAREST;
    }
}

VkSamplerAddressMode ConvertAddressMode(SamplerMode mode)
{
    switch (mode)
    {
        case SamplerMode::Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: Assert(0); return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

} // namespace vulkan

using namespace vulkan;

void mkGraphicsVulkan::Cleanup()
{
    u32 currentBuffer = GetCurrentBuffer();
    MutexScope(&mCleanupMutex)
    {
        for (auto &semaphore : mCleanupSemaphores[currentBuffer])
        {
            vkDestroySemaphore(mDevice, semaphore, 0);
        }
        mCleanupSemaphores[currentBuffer].clear();
        for (auto &swapchain : mCleanupSwapchains[currentBuffer])
        {
            vkDestroySwapchainKHR(mDevice, swapchain, 0);
        }
        mCleanupSwapchains[currentBuffer].clear();
        for (auto &imageview : mCleanupImageViews[currentBuffer])
        {
            vkDestroyImageView(mDevice, imageview, 0);
        }
        mCleanupImageViews[currentBuffer].clear();
        for (auto &buffer : mCleanupBuffers[currentBuffer])
        {
            vmaDestroyBuffer(mAllocator, buffer.mBuffer, buffer.mAllocation);
        }
        mCleanupBuffers[currentBuffer].clear();
    }
}

mkGraphicsVulkan::mkGraphicsVulkan(OS_Handle window, ValidationMode validationMode, GPUDevicePreference preference)
{
    mArena          = ArenaAlloc();
    const i32 major = 0;
    const i32 minor = 0;
    const i32 patch = 1;

    VkResult res;

    res = volkInitialize();
    Assert(res == VK_SUCCESS);

    // Create the application
    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "MK Engine Application";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName        = "MK Engine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, major, minor, patch);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // Load available layers
    u32 layerCount = 0;
    res            = vkEnumerateInstanceLayerProperties(&layerCount, 0);
    Assert(res == VK_SUCCESS);
    list<VkLayerProperties> availableLayers(layerCount);
    res = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    Assert(res == VK_SUCCESS);

    // Load extension info
    u32 extensionCount = 0;
    res                = vkEnumerateInstanceExtensionProperties(0, &extensionCount, 0);
    Assert(res == VK_SUCCESS);
    list<VkExtensionProperties> extensionProperties(extensionCount);
    res = vkEnumerateInstanceExtensionProperties(0, &extensionCount, extensionProperties.data());
    Assert(res == VK_SUCCESS);

    list<const char *> instanceExtensions;
    list<const char *> instanceLayers;
    // Add extensions
    for (auto &availableExtension : extensionProperties)
    {
        if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            mDebugUtils = true;
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }
    instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef WINDOWS
    instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
#error not supported
#endif

    // Add layers
    if (validationMode != ValidationMode::Disabled)
    {
        static const list<const char *> validationPriorityList[] = {
            // Preferred
            {"VK_LAYER_KHRONOS_validation"},
            // Fallback
            {"VK_LAYER_LUNARG_standard_validation"},
            // Individual
            {
                "VK_LAYER_GOOGLE_threading",
                "VK_LAYER_LUNARG_parameter_validation",
                "VK_LAYER_LUNARG_object_tracker",
                "VK_LAYER_LUNARG_core_validation",
                "VK_LAYER_GOOGLE_unique_objects",
            },
            // Last resort
            {
                "VK_LAYER_LUNARG_core_validation",
            },
        };
        for (auto &validationLayers : validationPriorityList)
        {
            bool validated = true;
            for (auto &layer : validationLayers)
            {
                bool found = false;
                for (auto &availableLayer : availableLayers)
                {
                    if (strcmp(availableLayer.layerName, layer) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    validated = false;
                    break;
                }
            }

            if (validated)
            {
                for (auto &c : validationLayers)
                {
                    instanceLayers.push_back(c);
                }
                break;
            }
        }
    }

    // Create instance
    {
        VkInstanceCreateInfo instInfo    = {};
        instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pApplicationInfo        = &appInfo;
        instInfo.enabledLayerCount       = (u32)instanceLayers.size();
        instInfo.ppEnabledLayerNames     = instanceLayers.data();
        instInfo.enabledExtensionCount   = (u32)instanceExtensions.size();
        instInfo.ppEnabledExtensionNames = instanceExtensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {};

        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        if (validationMode != ValidationMode::Disabled && mDebugUtils)
        {
            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugUtilsCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            if (validationMode == ValidationMode::Verbose)
            {
                debugUtilsCreateInfo.messageSeverity |= (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT);
            }

            debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
            instInfo.pNext                       = &debugUtilsCreateInfo;
        }

        res = vkCreateInstance(&instInfo, 0, &mInstance);
        Assert(res == VK_SUCCESS);

        volkLoadInstance(mInstance);

        if (validationMode != ValidationMode::Disabled && mDebugUtils)
        {
            res = vkCreateDebugUtilsMessengerEXT(mInstance, &debugUtilsCreateInfo, 0, &mDebugMessenger);
            Assert(res == VK_SUCCESS);
        }
    }

    // Enumerate physical devices
    {
        u32 deviceCount = 0;
        res             = vkEnumeratePhysicalDevices(mInstance, &deviceCount, 0);
        Assert(res == VK_SUCCESS);
        Assert(deviceCount != 0);

        list<VkPhysicalDevice> devices(deviceCount);
        res = vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
        Assert(res == VK_SUCCESS);

        list<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        for (auto &device : devices)
        {
            b32 suitable       = false;
            u32 deviceExtCount = 0;
            res                = vkEnumerateDeviceExtensionProperties(device, 0, &deviceExtCount, 0);
            Assert(res == VK_SUCCESS);
            list<VkExtensionProperties> availableDevExt(deviceExtCount);
            res = vkEnumerateDeviceExtensionProperties(device, 0, &deviceExtCount, availableDevExt.data());
            Assert(res == VK_SUCCESS);

            b32 hasRequiredExtensions = 1;
            for (auto &requiredExtension : deviceExtensions)
            {
                b32 hasExtension = 0;
                for (auto &extension : availableDevExt)
                {
                    if (strcmp(extension.extensionName, requiredExtension) == 0)
                    {
                        hasExtension = 1;
                        break;
                    }
                }
                if (!hasExtension)
                {
                    hasRequiredExtensions = 0;
                    break;
                }
            }

            if (!hasRequiredExtensions)
            {
                continue;
            }

            mDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            mDeviceProperties.pNext = 0;
            vkGetPhysicalDeviceProperties2(device, &mDeviceProperties);

            suitable = mDeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (preference == GPUDevicePreference::Integrated)
            {
                suitable = mDeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            }
            if (suitable)
            {
                mPhysicalDevice = device;
                break;
            }
        }

        mDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        mFeatures11.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        mFeatures12.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        mFeatures13.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        mDeviceFeatures.pNext = &mFeatures11;
        mFeatures11.pNext     = &mFeatures12;
        mFeatures12.pNext     = &mFeatures13;
        void **featuresChain  = &mFeatures13.pNext;
        *featuresChain        = 0;

        vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &mDeviceFeatures);

        Assert(mFeatures13.dynamicRendering == VK_TRUE);
        Assert(mFeatures12.descriptorIndexing == VK_TRUE);

        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(mPhysicalDevice, &queueFamilyCount, 0);

        mQueueFamilyProperties.resize(queueFamilyCount);

        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            mQueueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        }
        vkGetPhysicalDeviceQueueFamilyProperties2(mPhysicalDevice, &queueFamilyCount, mQueueFamilyProperties.data());

        // Device exposes 1+ queue families, queue families have 1+ queues. Each family supports a combination
        // of the below:
        // 1. Graphics
        // 2. Compute
        // 3. Transfer
        // 4. Sparse Memory Management

        // Find queues in queue family
        for (u32 i = 0; i < mQueueFamilyProperties.size(); i++)
        {
            auto &queueFamily = mQueueFamilyProperties[i];
            if (queueFamily.queueFamilyProperties.queueCount > 0)
            {
                if (mGraphicsFamily == VK_QUEUE_FAMILY_IGNORED &&
                    queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    mGraphicsFamily = i;
                }
                if ((queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    (mCopyFamily == VK_QUEUE_FAMILY_IGNORED ||
                     (!(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT))))

                {
                    mCopyFamily = i;
                }
                if ((queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                    (mComputeFamily == VK_QUEUE_FAMILY_IGNORED ||
                     !(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)))

                {
                    mComputeFamily = i;
                }
            }
        }

        // Create the device queues
        list<VkDeviceQueueCreateInfo> queueCreateInfos;
        f32 queuePriority = 1.f;
        for (u32 i = 0; i < 3; i++)
        {
            u32 queueFamily = 0;
            if (i == 0)
            {
                queueFamily = mGraphicsFamily;
            }
            else if (i == 1)
            {
                if (mGraphicsFamily == mComputeFamily)
                {
                    continue;
                }
                queueFamily = mComputeFamily;
            }
            else if (i == 2)
            {
                if (mGraphicsFamily == mCopyFamily || mComputeFamily == mCopyFamily)
                {
                    continue;
                }
                queueFamily = mCopyFamily;
            }
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex        = queueFamily;
            queueCreateInfo.queueCount              = 1;
            queueCreateInfo.pQueuePriorities        = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);

            mFamilies.push_back(queueFamily);
        }
        VkDeviceCreateInfo createInfo      = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount    = (u32)queueCreateInfos.size();
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.pEnabledFeatures        = 0;
        createInfo.pNext                   = &mDeviceFeatures;
        createInfo.enabledExtensionCount   = (u32)deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        res = vkCreateDevice(mPhysicalDevice, &createInfo, 0, &mDevice);
        Assert(res == VK_SUCCESS);

        volkLoadDevice(mDevice);
    }

    // Get the device queues
    vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mQueues[QueueType_Graphics].mQueue);
    vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mQueues[QueueType_Compute].mQueue);
    vkGetDeviceQueue(mDevice, mCopyFamily, 0, &mQueues[QueueType_Copy].mQueue);

    SetName((u64)mQueues[QueueType_Graphics].mQueue, GraphicsObjectType::Queue, "Graphics Queue");
    SetName((u64)mQueues[QueueType_Copy].mQueue, GraphicsObjectType::Queue, "Transfer Queue");

    // TODO: unified memory access architectures
    mMemProperties       = {};
    mMemProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    vkGetPhysicalDeviceMemoryProperties2(mPhysicalDevice, &mMemProperties);

    VmaAllocatorCreateInfo allocCreateInfo = {};
    allocCreateInfo.physicalDevice         = mPhysicalDevice;
    allocCreateInfo.device                 = mDevice;
    allocCreateInfo.instance               = mInstance;
    allocCreateInfo.vulkanApiVersion       = VK_API_VERSION_1_3;
    // these are promoted to core, so this doesn't do anything
    allocCreateInfo.flags = VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

#if VMA_DYNAMIC_VULKAN_FUNCTIONS
    VmaVulkanFunctions vulkanFunctions    = {};
    vulkanFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    allocCreateInfo.pVulkanFunctions      = &vulkanFunctions;
#else
#error
#endif

    res = vmaCreateAllocator(&allocCreateInfo, &mAllocator);
    Assert(res == VK_SUCCESS);

    // Set up dynamic pso
    mDynamicStates = {
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_VIEWPORT,
    };

    // Set up frame fences
    for (u32 buffer = 0; buffer < cNumBuffers; buffer++)
    {
        for (u32 queue = 0; queue < QueueType_Count; queue++)
        {
            if (mQueues[queue].mQueue == VK_NULL_HANDLE)
            {
                continue;
            }
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            res                         = vkCreateFence(mDevice, &fenceInfo, 0, &mFrameFences[buffer][queue]);
            Assert(res == VK_SUCCESS);
        }
    }

    mDynamicStateInfo                   = {};
    mDynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    mDynamicStateInfo.dynamicStateCount = (u32)mDynamicStates.size();
    mDynamicStateInfo.pDynamicStates    = mDynamicStates.data();

    // Init frame allocators
    GPUBufferDesc desc;
    desc.mUsage         = MemoryUsage::CPU_TO_GPU;
    desc.mSize          = megabytes(32);
    desc.mResourceUsage = ResourceUsage::VertexBuffer | ResourceUsage::IndexBuffer | ResourceUsage::UniformBuffer;
    for (u32 i = 0; i < cNumBuffers; i++)
    {
        CreateBuffer(&mFrameAllocator[i].mBuffer, desc, 0);
        mFrameAllocator[i].mAlignment = 8;
    }

    // Init descriptor pool
    {
        VkDescriptorPoolSize poolSizes[2];

        u32 count = 0;
        // Uniform buffers
        poolSizes[count].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[count].descriptorCount = cPoolSize;
        count++;

        // Combined samplers
        poolSizes[count].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[count].descriptorCount = cPoolSize;

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.poolSizeCount              = count;
        createInfo.pPoolSizes                 = poolSizes;
        createInfo.maxSets                    = cPoolSize;

        res = vkCreateDescriptorPool(mDevice, &createInfo, 0, &mPool);
        Assert(res == VK_SUCCESS);
    }

    // Default samplers
    {
        // Null sampler
        VkSamplerCreateInfo samplerCreate = {};
        samplerCreate.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        res = vkCreateSampler(mDevice, &samplerCreate, 0, &mNullSampler);
        Assert(res == VK_SUCCESS);

        // Default sampler
        samplerCreate.anisotropyEnable        = VK_FALSE;
        samplerCreate.maxAnisotropy           = 0;
        samplerCreate.minLod                  = 0;
        samplerCreate.maxLod                  = FLT_MAX;
        samplerCreate.mipLodBias              = 0;
        samplerCreate.unnormalizedCoordinates = VK_FALSE;
        samplerCreate.compareEnable           = VK_FALSE;
        samplerCreate.compareOp               = VK_COMPARE_OP_NEVER;

        samplerCreate.minFilter    = VK_FILTER_LINEAR;
        samplerCreate.magFilter    = VK_FILTER_LINEAR;
        samplerCreate.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreate.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreate.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreate.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        res = vkCreateSampler(mDevice, &samplerCreate, 0, &mLinearSampler);
        Assert(res == VK_SUCCESS);

        samplerCreate.minFilter  = VK_FILTER_NEAREST;
        samplerCreate.magFilter  = VK_FILTER_NEAREST;
        samplerCreate.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        res                      = vkCreateSampler(mDevice, &samplerCreate, 0, &mNearestSampler);
        Assert(res == VK_SUCCESS);
    }

    // Default views
    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width      = 1;
        imageInfo.extent.height     = 1;
        imageInfo.extent.depth      = 1;
        imageInfo.mipLevels         = 1;
        imageInfo.arrayLayers       = 1;
        imageInfo.format            = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
        res                               = vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &mNullImage2D, &mNullImage2DAllocation, 0);

        VkImageViewCreateInfo createInfo           = {};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;

        createInfo.image    = mNullImage2D;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        res = vkCreateImageView(mDevice, &createInfo, 0, &mNullImageView2D);
        Assert(res == VK_SUCCESS);

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        res                 = vkCreateImageView(mDevice, &createInfo, 0, &mNullImageView2DArray);
        Assert(res == VK_SUCCESS);

        // Transitions

        TransferCommand cmd = Stage(0);

        VkImageMemoryBarrier2 imageBarrier           = {};
        imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.image                           = mNullImage2D;
        imageBarrier.oldLayout                       = imageInfo.initialLayout;
        imageBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
        imageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        imageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.baseMipLevel   = 0;
        imageBarrier.subresourceRange.layerCount     = 1;
        imageBarrier.subresourceRange.levelCount     = 1;
        imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo dependencyInfo        = {};
        dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers    = &imageBarrier;

        vkCmdPipelineBarrier2(cmd.mTransitionBuffer, &dependencyInfo);

        Submit(cmd);
    }

    // Null buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size               = 4;
        bufferInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.preferredFlags          = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        res = vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo, &mNullBuffer, &mNullBufferAllocation, 0);
        Assert(res == VK_SUCCESS);
    }

    // Bindless
    {
        for (DescriptorType type = (DescriptorType)0; type < DescriptorType_Count; type = (DescriptorType)(type + 1))
        {
            VkDescriptorType descriptorType;
            switch (descriptorType)
            {
                case DescriptorType_Storage: descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
                case DescriptorType_CombinedSampler: descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
                default: Assert(0);
            }

            BindlessDescriptorPool &bindlessDescriptorPool = bindlessDescriptorPools[type];
            VkDescriptorPoolSize poolSize                  = {};
            poolSize.type                                  = descriptorType;
            if (type == DescriptorType_Storage)
            {
                poolSize.descriptorCount = Min(100, mDeviceProperties.properties.limits.maxDescriptorSetStorageBuffers / 4);
            }
            else if (type == DescriptorType_CombinedSampler)
            {
                poolSize.descriptorCount = Min(100, mDeviceProperties.properties.limits.maxDescriptorSetSampledImages / 4);
            }

            VkDescriptorPoolCreateInfo createInfo = {};
            createInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            createInfo.poolSizeCount              = 1;
            createInfo.pPoolSizes                 = &poolSize;
            createInfo.maxSets                    = 1;
            createInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            res                                   = vkCreateDescriptorPool(mDevice, &createInfo, 0, &bindlessDescriptorPool.pool);
            Assert(res == VK_SUCCESS);

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding                      = 0;
            binding.pImmutableSamplers           = 0;
            binding.stageFlags                   = VK_SHADER_STAGE_ALL;
            binding.descriptorCount              = poolSize.descriptorCount;
            binding.descriptorType               = descriptorType;

            // These flags enable bindless: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorBindingFlagBits.html
            VkDescriptorBindingFlags bindingFlags =
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreate = {};
            bindingFlagsCreate.sType                                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            bindingFlagsCreate.bindingCount                                = 1;
            bindingFlagsCreate.pBindingFlags                               = &bindingFlags;

            VkDescriptorSetLayoutCreateInfo createSetLayout = {};
            createSetLayout.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            createSetLayout.bindingCount                    = 1;
            createSetLayout.pBindings                       = &binding;
            createSetLayout.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            createSetLayout.pNext                           = &bindingFlagsCreate;

            res = vkCreateDescriptorSetLayout(mDevice, &createSetLayout, 0, &bindlessDescriptorPool.layout);
            Assert(res == VK_SUCCESS);

            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool              = bindlessDescriptorPool.pool;
            allocInfo.descriptorSetCount          = 1;
            allocInfo.pSetLayouts                 = &bindlessDescriptorPool.layout;
            res                                   = vkAllocateDescriptorSets(mDevice, &allocInfo, &bindlessDescriptorPool.set);
            Assert(res == VK_SUCCESS);

            for (u32 i = 0; i < poolSize.descriptorCount; i++)
            {
                bindlessDescriptorPool.freeList.push_back(i);
            }
        }
    }
} // namespace graphics

b32 mkGraphicsVulkan::CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *inSwapchain)
{
    SwapchainVulkan *swapchain = ToInternal(inSwapchain);

    if (swapchain == 0)
    {
        swapchain = new SwapchainVulkan();
    }
    inSwapchain->mDesc         = *desc;
    inSwapchain->internalState = swapchain;
    VkResult res;
    // Create surface
#if WINDOWS
    VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
    win32SurfaceCreateInfo.sType                       = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    win32SurfaceCreateInfo.hwnd                        = window;
    win32SurfaceCreateInfo.hinstance                   = GetModuleHandleW(0);

    res = vkCreateWin32SurfaceKHR(mInstance, &win32SurfaceCreateInfo, 0, &swapchain->mSurface);
    Assert(res == VK_SUCCESS);
#else
#error not supported
#endif

    // Check whether physical device has a queue family that supports presenting to the surface
    u32 presentFamily = VK_QUEUE_FAMILY_IGNORED;
    for (u32 familyIndex = 0; familyIndex < mQueueFamilyProperties.size(); familyIndex++)
    {
        VkBool32 supported = false;
        res                = vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, familyIndex, swapchain->mSurface, &supported);
        Assert(res == VK_SUCCESS);

        if (mQueueFamilyProperties[familyIndex].queueFamilyProperties.queueCount > 0 && supported)
        {
            presentFamily = familyIndex;
            break;
        }
    }
    if (presentFamily == VK_QUEUE_FAMILY_IGNORED)
    {
        return false;
    }

    CreateSwapchain(inSwapchain);

    return true;
}

// Recreates the swap chain if it becomes invalid
b32 mkGraphicsVulkan::CreateSwapchain(Swapchain *inSwapchain)
{
    SwapchainVulkan *swapchain = ToInternal(inSwapchain);
    Assert(swapchain);

    VkResult res;

    u32 formatCount = 0;
    res             = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, swapchain->mSurface, &formatCount, 0);
    Assert(res == VK_SUCCESS);
    list<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, swapchain->mSurface, &formatCount, surfaceFormats.data());
    Assert(res == VK_SUCCESS);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, swapchain->mSurface, &surfaceCapabilities);
    Assert(res == VK_SUCCESS);

    u32 presentCount = 0;
    res              = vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, swapchain->mSurface, &presentCount, 0);
    Assert(res == VK_SUCCESS);
    list<VkPresentModeKHR> surfacePresentModes;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, swapchain->mSurface, &presentCount, surfacePresentModes.data());
    Assert(res == VK_SUCCESS);

    // Pick one of the supported formats
    VkSurfaceFormatKHR surfaceFormat = {};
    {
        surfaceFormat.format     = ConvertFormat(inSwapchain->mDesc.format);
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        VkFormat requestedFormat = ConvertFormat(inSwapchain->mDesc.format);

        b32 valid = false;
        for (auto &checkedFormat : surfaceFormats)
        {
            if (requestedFormat == checkedFormat.format)
            {
                surfaceFormat = checkedFormat;
                valid         = true;
                break;
            }
        }
        if (!valid)
        {
            inSwapchain->mDesc.format = Format::B8G8R8A8_UNORM;
        }
    }

    // Pick the extent (size)
    {
        if (surfaceCapabilities.currentExtent.width != 0xFFFFFFFF && surfaceCapabilities.currentExtent.height != 0xFFFFFFFF)
        {
            swapchain->mExtent = surfaceCapabilities.currentExtent;
        }
        else
        {
            swapchain->mExtent        = {inSwapchain->mDesc.width, inSwapchain->mDesc.height};
            swapchain->mExtent.width  = Clamp(inSwapchain->mDesc.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            swapchain->mExtent.height = Clamp(inSwapchain->mDesc.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        }
    }
    u32 imageCount = max(2, surfaceCapabilities.minImageCount);
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
    {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    {
        swapchainCreateInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface          = swapchain->mSurface;
        swapchainCreateInfo.minImageCount    = imageCount;
        swapchainCreateInfo.imageFormat      = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace  = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent      = swapchain->mExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // Choose present mode. Mailbox allows old images in swapchain queue to be replaced if the queue is full.
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto &presentMode : surfacePresentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
        swapchainCreateInfo.clipped      = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = swapchain->mSwapchain;

        res = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, 0, &swapchain->mSwapchain);
        Assert(res == VK_SUCCESS);

        // Clean up the old swap chain, if it exists
        if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
        {
            u32 nextBuffer = GetNextBuffer();
            MutexScope(&mCleanupMutex)
            {
                mCleanupSwapchains[nextBuffer].push_back(swapchainCreateInfo.oldSwapchain);
                for (u32 i = 0; i < (u32)swapchain->mImageViews.size(); i++)
                {
                    mCleanupImageViews[nextBuffer].push_back(swapchain->mImageViews[i]);
                }
                for (u32 i = 0; i < (u32)swapchain->mAcquireSemaphores.size(); i++)
                {
                    mCleanupSemaphores[nextBuffer].push_back(swapchain->mAcquireSemaphores[i]);
                }
                swapchain->mAcquireSemaphores.clear();
            }
        }

        // Get swapchain images
        res = vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapchain, &imageCount, 0);
        Assert(res == VK_SUCCESS);
        swapchain->mImages.resize(imageCount);
        res = vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapchain, &imageCount, swapchain->mImages.data());
        Assert(res == VK_SUCCESS);

        // Create swap chain image views (determine how images are accessed)
        swapchain->mImageViews.resize(imageCount);
        for (u32 i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo createInfo           = {};
            createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image                           = swapchain->mImages[i];
            createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format                          = surfaceFormat.format;
            createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel   = 0;
            createInfo.subresourceRange.levelCount     = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount     = 1;

            // TODO: delete old image view
            res = vkCreateImageView(mDevice, &createInfo, 0, &swapchain->mImageViews[i]);
            Assert(res == VK_SUCCESS);
        }

        // Create swap chain semaphores
        {
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            if (swapchain->mAcquireSemaphores.empty())
            {
                u32 size = (u32)swapchain->mImages.size();
                swapchain->mAcquireSemaphores.resize(size);
                for (u32 i = 0; i < size; i++)
                {
                    res = vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &swapchain->mAcquireSemaphores[i]);
                    Assert(res == VK_SUCCESS);
                }
            }
            if (swapchain->mReleaseSemaphore == VK_NULL_HANDLE)
            {
                res = vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &swapchain->mReleaseSemaphore);
                Assert(res == VK_SUCCESS);
            }
        }
    }
    return true;
}

void mkGraphicsVulkan::CreateShader(PipelineStateDesc *inDesc, PipelineState *outPS)
{
    VkResult res;
    PipelineStateVulkan *ps = new PipelineStateVulkan();
    outPS->internalState    = ps;
    outPS->mDesc            = *inDesc;

    string vert = {};
    string frag = {};

    if (inDesc->mVS)
    {
        vert = platform.ReadEntireFile(mArena, inDesc->mVS->mName);
    }
    if (inDesc->mFS)
    {
        frag = platform.ReadEntireFile(mArena, inDesc->mFS->mName);
    }

    list<VkShaderModule> shaderModules;

    VkShaderModule vertShaderModule = {};
    VkShaderModule fragShaderModule = {};

    // Pipeline create shader stage info
    list<VkPipelineShaderStageCreateInfo> pipelineShaderStageInfo;

    // Create shader modules and pipeline shader stages
    if (inDesc->mVS)
    {
        shaderModules.emplace_back();

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize                 = vert.size;
        createInfo.pCode                    = (u32 *)vert.str;
        res                                 = vkCreateShaderModule(mDevice, &createInfo, 0, &shaderModules.back());
        Assert(res == VK_SUCCESS);

        pipelineShaderStageInfo.emplace_back();
        VkPipelineShaderStageCreateInfo &shaderInfo = pipelineShaderStageInfo.back();
        shaderInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderInfo.stage                            = VK_SHADER_STAGE_VERTEX_BIT;
        shaderInfo.module                           = shaderModules.back();
        shaderInfo.pName                            = "main";
    }
    if (inDesc->mFS)
    {
        shaderModules.emplace_back();

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize                 = frag.size;
        createInfo.pCode                    = (u32 *)frag.str;
        res                                 = vkCreateShaderModule(mDevice, &createInfo, 0, &shaderModules.back());
        Assert(res == VK_SUCCESS);

        pipelineShaderStageInfo.emplace_back();
        VkPipelineShaderStageCreateInfo &shaderInfo = pipelineShaderStageInfo.back();
        shaderInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderInfo.stage                            = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderInfo.module                           = shaderModules.back();
        shaderInfo.pName                            = "main";
    }
    // Vertex inputs

    list<VkVertexInputBindingDescription> bindings;
    list<VkVertexInputAttributeDescription> attributes;

    // Create vertex binding

    for (auto &il : outPS->mDesc.mInputLayouts)
    {
        VkVertexInputBindingDescription bind;

        bind.stride    = il->mStride;
        bind.inputRate = il->mRate == InputRate::Vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        bind.binding   = il->mBinding;

        bindings.push_back(bind);
    }
    // Create vertx attribs
    u32 currentOffset = 0;
    u32 loc           = 0;
    for (auto &il : outPS->mDesc.mInputLayouts)
    {
        u32 currentBinding = il->mBinding;
        for (auto &format : il->mElements)
        {
            VkVertexInputAttributeDescription attrib;
            attrib.binding  = currentBinding;
            attrib.location = loc++;
            attrib.format   = ConvertFormat(format);
            attrib.offset   = currentOffset;

            attributes.push_back(attrib);

            currentOffset += GetFormatSize(format);
        }
        currentOffset = 0;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount        = (u32)bindings.size();
    vertexInputInfo.pVertexBindingDescriptions           = bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount      = (u32)attributes.size();
    vertexInputInfo.pVertexAttributeDescriptions         = attributes.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable                 = VK_FALSE;

    // Viewport/scissor
    VkViewport viewport = {};
    viewport.x          = 0.f;
    viewport.y          = 0.f;
    viewport.width      = 65536;
    viewport.height     = 65536;
    viewport.minDepth   = 0.f;
    viewport.maxDepth   = 1.f;

    VkRect2D scissor      = {};
    scissor.extent.width  = 65536;
    scissor.extent.height = 65536;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount                     = 1;
    viewportState.scissorCount                      = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.pScissors                         = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable                       = VK_FALSE;
    rasterizer.rasterizerDiscardEnable                = VK_FALSE;
    rasterizer.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth                              = 1.f;
    switch (inDesc->mRasterState->mCullMode)
    {
        case RasterizationState::CullMode::Back:
        {
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        }
        break;
        case RasterizationState::CullMode::Front:
        {
            rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        }
        break;
        case RasterizationState::CullMode::None:
        {
            rasterizer.cullMode = VK_CULL_MODE_NONE;
        }
        break;
        default: Assert(0);
    }

    rasterizer.frontFace               = inDesc->mRasterState->mFrontFaceCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.f;
    rasterizer.depthBiasClamp          = 0.f;
    rasterizer.depthBiasSlopeFactor    = 0.f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable                  = VK_FALSE;
    multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading                     = 1.f;
    multisampling.pSampleMask                          = 0;
    multisampling.alphaToCoverageEnable                = VK_FALSE;
    multisampling.alphaToOneEnable                     = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthCompareOp                        = VK_COMPARE_OP_LESS;
    depthStencil.depthTestEnable                       = VK_TRUE;
    depthStencil.depthWriteEnable                      = VK_TRUE;
    depthStencil.depthBoundsTestEnable                 = VK_FALSE;
    depthStencil.minDepthBounds                        = 0.f;
    depthStencil.maxDepthBounds                        = 1.f;

    // depthStencil.

    // Blending
    // Mixes old and new value
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    // Combines old and new value using a bitwise operation
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable                       = VK_FALSE;
    colorBlending.logicOp                             = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount                     = 1;
    colorBlending.pAttachments                        = &colorBlendAttachment;
    colorBlending.blendConstants[0]                   = 0.f;
    colorBlending.blendConstants[1]                   = 0.f;
    colorBlending.blendConstants[2]                   = 0.f;
    colorBlending.blendConstants[3]                   = 0.f;

    // Descriptor sets
    VkDescriptorSetLayout descriptorLayout;
    {
        ps->mLayoutBindings.resize(inDesc->mDescriptorBindings.size());

        for (size_t i = 0; i < inDesc->mDescriptorBindings.size(); i++)
        {
            VkDescriptorSetLayoutBinding *binding = &ps->mLayoutBindings[i];
            DescriptorBinding *inputBinding       = &inDesc->mDescriptorBindings[i];

            binding->binding = inputBinding->mBinding;
            switch (inputBinding->mUsage)
            {
                case ResourceUsage::UniformBuffer:
                    binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case ResourceUsage::SampledTexture:
                    binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    break;
                default: Assert(0);
            }
            binding->stageFlags         = ConvertShaderStage(inputBinding->mStage);
            binding->descriptorCount    = inputBinding->mArraySize;
            binding->pImmutableSamplers = 0;
        }

        VkDescriptorSetLayoutCreateInfo descriptorCreateInfo = {};

        descriptorCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorCreateInfo.bindingCount = (u32)ps->mLayoutBindings.size();
        descriptorCreateInfo.pBindings    = ps->mLayoutBindings.data();

        res = vkCreateDescriptorSetLayout(mDevice, &descriptorCreateInfo, 0, &descriptorLayout);

        Assert(res == VK_SUCCESS);
        ps->mDescriptorSetLayouts.push_back(descriptorLayout);
    } // namespace graphics

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &descriptorLayout;
    // Push constants are kind of like compile time constants for shaders? except they don't have to be
    // specified at shader creation, instead pipeline creation
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = 0;

    VkPushConstantRange &range = ps->mPushConstantRange;
    if (inDesc->mPushConstantRange.mSize > 0)
    {
        range.size                                = inDesc->mPushConstantRange.mSize;
        range.offset                              = inDesc->mPushConstantRange.mOffset;
        range.stageFlags                          = ConvertShaderStage(inDesc->mPushConstantRange.mStageFlags);
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &range;
    }
    else
    {
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges    = 0;
    }

    res = vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, 0, &ps->mPipelineLayout);
    Assert(res == VK_SUCCESS);

    // Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};

    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.layout              = ps->mPipelineLayout;
    pipelineInfo.stageCount          = (u32)pipelineShaderStageInfo.size();
    pipelineInfo.pStages             = pipelineShaderStageInfo.data();
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &mDynamicStateInfo;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = 0;
    pipelineInfo.renderPass          = VK_NULL_HANDLE;

    // Dynamic rendering :)
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType                         = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.viewMask                      = 0;
    if (inDesc->mColorAttachmentFormat != Format::Null)
    {
        VkFormat format                       = ConvertFormat(inDesc->mColorAttachmentFormat);
        renderingInfo.colorAttachmentCount    = 1;
        renderingInfo.pColorAttachmentFormats = &format;
    }
    if (inDesc->mDepthStencilFormat != Format::Null)
    {
        renderingInfo.depthAttachmentFormat = ConvertFormat(inDesc->mDepthStencilFormat);
        if (IsFormatStencilSupported(inDesc->mDepthStencilFormat))
        {
            renderingInfo.stencilAttachmentFormat = renderingInfo.depthAttachmentFormat;
        }
    }

    pipelineInfo.pNext = &renderingInfo;

    // VkPipelineRenderingCreateInfo
    res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &ps->mPipeline);
    Assert(res == VK_SUCCESS);
}

void mkGraphicsVulkan::Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)
{
    CommandListVulkan *command = ToInternal(cmd);

    list<VkBufferMemoryBarrier2> bufferBarriers;
    list<VkMemoryBarrier2> memoryBarriers;
    for (u32 i = 0; i < count; i++)
    {
        GPUBarrier *barrier = &barriers[i];
        switch (barrier->mType)
        {
            case GPUBarrier::Type::Buffer:
            {
                GPUBuffer *buffer             = (GPUBuffer *)barrier->mResource;
                GPUBufferVulkan *bufferVulkan = ToInternal(buffer);

                bufferBarriers.emplace_back();
                VkBufferMemoryBarrier2 &bufferBarrier = bufferBarriers.back();
                bufferBarrier.sType                   = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                bufferBarrier.buffer                  = bufferVulkan->mBuffer;
                bufferBarrier.offset                  = 0;
                bufferBarrier.size                    = buffer->mDesc.mSize;
                bufferBarrier.srcStageMask            = ConvertResourceToPipelineStage(barrier->mBefore);
                bufferBarrier.srcAccessMask           = ConvertResourceUsageToAccessFlag(barrier->mBefore);
                bufferBarrier.dstStageMask            = ConvertResourceToPipelineStage(barrier->mAfter);
                bufferBarrier.dstAccessMask           = ConvertResourceUsageToAccessFlag(barrier->mAfter);
                bufferBarrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
            };
            break;
            case GPUBarrier::Type::Memory:
            {
                memoryBarriers.emplace_back();
                VkMemoryBarrier2 &memoryBarrier = memoryBarriers.back();
                memoryBarrier.sType             = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                memoryBarrier.srcStageMask      = ConvertResourceToPipelineStage(barrier->mBefore);
                memoryBarrier.srcAccessMask     = ConvertResourceUsageToAccessFlag(barrier->mBefore);
                memoryBarrier.dstStageMask      = ConvertResourceToPipelineStage(barrier->mAfter);
                memoryBarrier.dstAccessMask     = ConvertResourceUsageToAccessFlag(barrier->mAfter);
            }
            break;
        }
    }
    VkDependencyInfo dependencyInfo         = {};
    dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = (u32)bufferBarriers.size();
    dependencyInfo.pBufferMemoryBarriers    = bufferBarriers.data();
    dependencyInfo.memoryBarrierCount       = (u32)memoryBarriers.size();
    dependencyInfo.pMemoryBarriers          = memoryBarriers.data();

    vkCmdPipelineBarrier2(command->GetCommandBuffer(), &dependencyInfo);
}

u64 mkGraphicsVulkan::GetMinAlignment(GPUBufferDesc *inDesc)
{
    u64 alignment = 1;
    if (HasFlags(inDesc->mResourceUsage, ResourceUsage::UniformBuffer))
    {
        alignment = Max(alignment, mDeviceProperties.properties.limits.minUniformBufferOffsetAlignment);
    }
    if (HasFlags(inDesc->mResourceUsage, ResourceUsage::StorageBuffer))
    {
        alignment = Max(alignment, mDeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
    }
    return alignment;
}

void mkGraphicsVulkan::CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback)
{
    VkResult res;
    GPUBufferVulkan *buffer = ToInternal(inBuffer);
    Assert(!buffer);

    MutexScope(&mArenaMutex)
    {
        buffer = PushStruct(mArena, GPUBufferVulkan);
    }

    inBuffer->internalState = buffer;
    inBuffer->mDesc         = inDesc;
    inBuffer->mMappedData   = 0;
    inBuffer->mResourceType = GPUResource::ResourceType::Buffer;

    VkBufferCreateInfo createInfo = {};
    createInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size               = inBuffer->mDesc.mSize;

    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::VertexBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::IndexBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::UniformBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    // Sharing
    if (mFamilies.size() > 1)
    {
        createInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = (u32)mFamilies.size();
        createInfo.pQueueFamilyIndices   = mFamilies.data();
    }
    else
    {
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage                   = VMA_MEMORY_USAGE_AUTO;

    if (inDesc.mUsage == MemoryUsage::CPU_TO_GPU)
    {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    // Buffers only on GPU must be copied to using a staging buffer
    else if (inDesc.mUsage == MemoryUsage::GPU_ONLY)
    {
        createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    res = vmaCreateBuffer(mAllocator, &createInfo, &allocCreateInfo, &buffer->mBuffer, &buffer->mAllocation, 0);
    Assert(res == VK_SUCCESS);

    // Map the buffer if it's a staging buffer
    if (inDesc.mUsage == MemoryUsage::CPU_TO_GPU)
    {
        inBuffer->mMappedData = buffer->mAllocation->GetMappedData();
        inBuffer->mDesc.mSize = buffer->mAllocation->GetSize();
    }

    if (initCallback != 0)
    {
        TransferCommand cmd;
        void *mappedData = 0;
        if (inBuffer->mDesc.mUsage == MemoryUsage::CPU_TO_GPU)
        {
            mappedData = inBuffer->mMappedData;
        }
        else
        {
            cmd        = Stage(inBuffer->mDesc.mSize);
            mappedData = cmd.mUploadBuffer.mMappedData;
        }

        initCallback(mappedData);

        if (cmd.IsValid())
        {
            // Memory copy data to the staging buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.srcOffset    = 0;
            bufferCopy.dstOffset    = 0;
            bufferCopy.size         = inBuffer->mDesc.mSize;

            // Copy from the staging buffer to the allocated buffer
            vkCmdCopyBuffer(cmd.mCmdBuffer, ToInternal(&cmd.mUploadBuffer)->mBuffer, buffer->mBuffer, 1, &bufferCopy);

            // Create a barrier on the graphics queue. Not needed on the transfer queue, since it doesn't
            // ever use the data after copying.

            // TODO: I think this barrier is useless, because the semaphore should signal the graphics queue
            // when the staging buffer transfer is complete.
            VkBufferMemoryBarrier2 bufferBarrier = {};
            bufferBarrier.sType                  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            bufferBarrier.buffer                 = buffer->mBuffer;
            bufferBarrier.offset                 = 0;
            bufferBarrier.size                   = VK_WHOLE_SIZE;
            bufferBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            bufferBarrier.srcAccessMask          = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            bufferBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            bufferBarrier.dstAccessMask          = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            bufferBarrier.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;

            if (HasFlags(inBuffer->mDesc.mResourceUsage, ResourceUsage::VertexBuffer))
            {
                bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
                bufferBarrier.dstAccessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
                // bufferBarrier.dstAccessMask
            }
            if (HasFlags(inBuffer->mDesc.mResourceUsage, ResourceUsage::IndexBuffer))
            {
                bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
                bufferBarrier.dstAccessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            }

            VkDependencyInfo dependencyInfo         = {};
            dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.bufferMemoryBarrierCount = 1;
            dependencyInfo.pBufferMemoryBarriers    = &bufferBarrier;

            vkCmdPipelineBarrier2(cmd.mTransitionBuffer, &dependencyInfo);

            Submit(cmd);
        }
    }

    CreateSubresource(inBuffer, SubresourceType::SRV);

    // If data is provided, do the transfer

    // Allocate:
#if 0
    vkCreateBuffer(mDevice, &createInfo, 0, &buffer->mBuffer);
    VkMemoryRequirements memoryRequirement;
    vkGetBufferMemoryRequirements(mDevice, buffer->mBuffer, &memoryRequirement);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize       = memoryRequirement.size;

    u32 memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (u32 i = 0; i < mMemProperties.memoryProperties.memoryTypeCount; i++)
    {
        if (memoryRequirement.memoryTypeBits & (1 << i) &&
            HasFlags(mMemProperties.memoryProperties.memoryTypes[i].propertyFlags, memoryPropertyFlags))
        {
            allocateInfo.memoryTypeIndex = i;
            break;
        }
    }

    VkDeviceMemory memory;
    res = vkAllocateMemory(mDevice, &allocateInfo, 0, &memory);

    res = vkBindBufferMemory(mDevice, buffer->mBuffer, memory, 0); // memoryRequirement.alignment);
    Assert(res == VK_SUCCESS);
#endif
}

void mkGraphicsVulkan::CreateTexture(Texture *outTexture, TextureDesc desc, void *inData)
{
    TextureVulkan *texVulk = 0;
    MutexScope(&mArenaMutex)
    {
        texVulk = PushStruct(mArena, TextureVulkan);
    }

    outTexture->internalState = texVulk;
    outTexture->mDesc         = desc;
    outTexture->mResourceType = GPUResource::ResourceType::Image;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // Get the image type
    {
        switch (desc.mTextureType)
        {
            case TextureDesc::TextureType::Texture2D:
            case TextureDesc::TextureType::Texture2DArray:
            {
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
            }
            break;
                break;
            case TextureDesc::TextureType::Cubemap:
            {
                imageInfo.imageType = VK_IMAGE_TYPE_3D;
            }
            break;
        }
    }

    imageInfo.extent.width  = desc.mWidth;
    imageInfo.extent.height = desc.mHeight;
    imageInfo.extent.depth  = desc.mDepth;
    imageInfo.mipLevels     = desc.mNumMips;
    imageInfo.arrayLayers   = desc.mNumLayers;
    imageInfo.format        = ConvertFormat(desc.mFormat);
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (HasFlags(&desc, ResourceUsage::SampledTexture))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (HasFlags(&desc, ResourceUsage::DepthStencil))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (desc.mUsage == MemoryUsage::GPU_ONLY)
    {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (mFamilies.size() > 1)
    {
        imageInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
        imageInfo.queueFamilyIndexCount = (u32)mFamilies.size();
        imageInfo.pQueueFamilyIndices   = mFamilies.data();
    }
    else
    {
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags   = 0;

    if (desc.mTextureType == TextureDesc::TextureType::Cubemap)
    {
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;

    VmaAllocationInfo info = {};
    VkResult res           = vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &texVulk->mImage, &texVulk->mAllocation, &info);
    Assert(res == VK_SUCCESS);

    // TODO: handle 3d texture creation
    if (inData)
    {
        TransferCommand cmd;
        void *mappedData = 0;
        cmd              = Stage(texVulk->mAllocation->GetSize());
        mappedData       = cmd.mUploadBuffer.mMappedData;

        MemoryCopy(mappedData, inData, texVulk->mAllocation->GetSize());

        if (cmd.IsValid())
        {
            // Copy the contents of the staging buffer to the image
            VkBufferImageCopy imageCopy               = {};
            imageCopy.bufferOffset                    = 0;
            imageCopy.bufferRowLength                 = 0;
            imageCopy.bufferImageHeight               = 0;
            imageCopy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            imageCopy.imageSubresource.mipLevel       = 0;
            imageCopy.imageSubresource.baseArrayLayer = 0;
            imageCopy.imageSubresource.layerCount     = 1;
            imageCopy.imageOffset                     = {0, 0, 0};
            imageCopy.imageExtent                     = {desc.mWidth, desc.mHeight, 1};

            // Layout transition to transfer destination before copying from the staging buffer
            VkImageMemoryBarrier2 barrier           = {};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.image                           = texVulk->mImage;
            barrier.oldLayout                       = imageInfo.initialLayout;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE; // ?
            barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask                   = VK_ACCESS_2_NONE;
            barrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;

            VkDependencyInfo dependencyInfo        = {};
            dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers    = &barrier;
            vkCmdPipelineBarrier2(cmd.mCmdBuffer, &dependencyInfo);

            vkCmdCopyBufferToImage(cmd.mCmdBuffer, ToInternal(&cmd.mUploadBuffer)->mBuffer, texVulk->mImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

            // Transition to layout used in pipeline
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstStageMask  = ConvertResourceToPipelineStage(&desc);
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = ConvertResourceUsageToAccessFlag(&desc);

            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers    = &barrier;
            vkCmdPipelineBarrier2(cmd.mTransitionBuffer, &dependencyInfo);

            Submit(cmd);
        }
    }
    // Transfer the image layout of the image to its initial layout
    else if (desc.mInitialUsage != ResourceUsage::None)
    {
        TransferCommand cmd           = Stage(0);
        VkImageMemoryBarrier2 barrier = {};
        barrier.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image                 = texVulk->mImage;
        barrier.oldLayout             = imageInfo.initialLayout;
        barrier.newLayout             = ConvertResourceUsageToImageLayout(desc.mInitialUsage);
        barrier.srcStageMask          = VK_PIPELINE_STAGE_2_NONE;
        barrier.dstStageMask          = ConvertResourceToPipelineStage(desc.mInitialUsage);
        barrier.srcAccessMask         = VK_ACCESS_2_NONE;
        barrier.dstAccessMask         = ConvertResourceUsageToAccessFlag(desc.mInitialUsage);
        if (IsFormatDepthSupported(desc.mFormat))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

            if (IsFormatStencilSupported(desc.mFormat))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo dependencyInfo        = {};
        dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers    = &barrier;
        vkCmdPipelineBarrier2(cmd.mTransitionBuffer, &dependencyInfo);

        Submit(cmd);
    }

    CreateSubresource(outTexture);
}

void mkGraphicsVulkan::CreateSampler(Sampler *sampler, SamplerDesc desc)
{
    SamplerVulkan *samplerVulk = 0;
    MutexScope(&mArenaMutex)
    {
        samplerVulk = PushStruct(mArena, SamplerVulkan);
    }

    sampler->internalState = samplerVulk;

    VkSamplerCreateInfo samplerCreate = {};
    samplerCreate.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreate.magFilter           = ConvertFilter(desc.mMag);
    samplerCreate.minFilter           = ConvertFilter(desc.mMin);
    samplerCreate.mipmapMode          = desc.mMipMode == Filter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreate.addressModeU        = ConvertAddressMode(desc.mMode); // TODO: these could potentially be different per coord
    samplerCreate.addressModeV        = ConvertAddressMode(desc.mMode);
    samplerCreate.addressModeW        = ConvertAddressMode(desc.mMode);
    samplerCreate.anisotropyEnable    = desc.mMaxAnisotropy > 0 ? VK_FALSE : VK_TRUE;
    samplerCreate.maxAnisotropy       = Min(mDeviceProperties.properties.limits.maxSamplerAnisotropy, desc.mMaxAnisotropy);
    switch (desc.mBorderColor)
    {
        case BorderColor::TransparentBlack: samplerCreate.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK; break;
        case BorderColor::OpaqueBlack: samplerCreate.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; break;
        case BorderColor::OpaqueWhite: samplerCreate.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE; break;
    }
    samplerCreate.unnormalizedCoordinates = VK_FALSE;
    samplerCreate.compareEnable           = VK_FALSE;
    samplerCreate.compareOp               = desc.mCompareOp == CompareOp::None ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS;
    samplerCreate.mipLodBias              = 0;
    samplerCreate.minLod                  = 0;
    samplerCreate.maxLod                  = 0;

    VkResult res = vkCreateSampler(mDevice, &samplerCreate, 0, &samplerVulk->mSampler);
    Assert(res == VK_SUCCESS);
}

void mkGraphicsVulkan::BindResource(GPUResource *resource, u32 slot, CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);
    Assert(command);
    Assert(slot < cMaxBindings);

    if (resource)
    {
        if (command->mResourceTable[slot].internalState != resource->internalState)
        {
            command->mResourceTable[slot] = *resource;

            // for (u32 i = 0; i < cNumBuffers; i++)
            // {
            //     command->mIsDirty[i][QueueType_Graphics] = true;
            // }
        }
    }
    // adds to a table in the command list that
}

i32 mkGraphicsVulkan::GetDescriptorIndex(GPUResource *resource, i32 subresourceIndex)
{
    i32 descriptorIndex = -1;
    Assert(resource->IsBuffer());
    GPUBufferVulkan *buffer = ToInternal((GPUBuffer *)resource);
    if (subresourceIndex == -1)
    {
        descriptorIndex = buffer->subresource.descriptorIndex;
    }
    else
    {
        descriptorIndex = buffer->subresources[subresourceIndex].descriptorIndex;
    }
    return descriptorIndex;
}

i32 mkGraphicsVulkan::CreateSubresource(GPUBuffer *buffer, SubresourceType type, u64 offset, u64 size, Format format)
{
    i32 subresourceIndex     = -1;
    GPUBufferVulkan *bufVulk = ToInternal(buffer);

    Assert(type == SubresourceType::SRV);

    GPUBufferVulkan::Subresource subresource;

    // Storage buffer
    if (format == Format::Null)
    {
        BindlessDescriptorPool &pool   = bindlessDescriptorPools[DescriptorType_Storage];
        i32 subresourceDescriptorIndex = pool.Allocate();
        subresource.descriptorIndex    = subresourceDescriptorIndex;

        VkWriteDescriptorSet write = {};
        write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet               = pool.set;
        write.dstBinding           = 0;
        write.descriptorCount      = 1;
        write.dstArrayElement      = subresourceDescriptorIndex;
        write.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pTexelBufferView     = 0;

        subresource.info.buffer = bufVulk->mBuffer;
        subresource.info.offset = offset;
        subresource.info.range  = size;

        write.pBufferInfo = &subresource.info;

        vkUpdateDescriptorSets(mDevice, 1, &write, 0, 0);

        if (!bufVulk->subresource.IsValid())
        {
            bufVulk->subresource = subresource;
        }
        else
        {
            bufVulk->subresources.push_back(subresource);
            subresourceIndex = (i32)bufVulk->subresources.size() - 1;
        }
    }
    // Texel buffers, not supported yet
    else
    {
        Assert(0);
    }
    return subresourceIndex;
}

// Creates image views
i32 mkGraphicsVulkan::CreateSubresource(Texture *texture, u32 baseLayer, u32 numLayers)
{
    TextureVulkan *textureVulk = ToInternal(texture);

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.format                = ConvertFormat(texture->mDesc.mFormat);
    createInfo.image                 = textureVulk->mImage;
    switch (texture->mDesc.mTextureType)
    {
        case TextureDesc::TextureType::Texture2D:
        {
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        }
        break;
        case TextureDesc::TextureType::Texture2DArray:
        {
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        break;
        case TextureDesc::TextureType::Cubemap:
        {
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        break;
    }
    VkImageAspectFlags flags = 0;
    if (HasFlags(&texture->mDesc, ResourceUsage::DepthStencil))
    {
        flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (IsFormatStencilSupported(texture->mDesc.mFormat))
        {
            flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        flags |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    createInfo.subresourceRange.aspectMask     = flags;
    createInfo.subresourceRange.baseArrayLayer = baseLayer;
    createInfo.subresourceRange.layerCount     = numLayers;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;

    i32 result = -1;
    if (baseLayer == 0 && numLayers == VK_REMAINING_ARRAY_LAYERS)
    {
        textureVulk->mSubresource.mBaseLayer = 0;
        textureVulk->mSubresource.mNumLayers = VK_REMAINING_ARRAY_LAYERS;
        VkResult res                         = vkCreateImageView(mDevice, &createInfo, 0, &textureVulk->mSubresource.mImageView);
        Assert(res == VK_SUCCESS);
    }
    else
    {
        textureVulk->mSubresources.emplace_back();
        TextureVulkan::Subresource *subresource = &textureVulk->mSubresources.back();
        subresource->mBaseLayer                 = baseLayer;
        subresource->mNumLayers                 = numLayers;

        VkResult res = vkCreateImageView(mDevice, &createInfo, 0, &subresource->mImageView);
        Assert(res == VK_SUCCESS);
        result = (i32)(textureVulk->mSubresources.size() - 1);
    }
    return result;
}

void mkGraphicsVulkan::UpdateDescriptorSet(CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);
    Assert(command);

    const PipelineState *pipeline = command->mCurrentPipeline;
    Assert(pipeline);
    PipelineStateVulkan *pipelineVulkan = ToInternal(pipeline);
    Assert(pipelineVulkan);

    list<VkWriteDescriptorSet> descriptorWrites;
    list<VkDescriptorBufferInfo> bufferInfos;
    list<VkDescriptorImageInfo> imageInfos;

    descriptorWrites.reserve(32);
    bufferInfos.reserve(32);
    imageInfos.reserve(32);

    // u32 currentSet = pipelineVulkan->mCurrentSet++;
    u32 currentSet = command->mCurrentSet++;
    VkDescriptorSet *descriptorSet;
    if (currentSet >= command->mDescriptorSets[GetCurrentBuffer()].size())
    {
        command->mDescriptorSets[GetCurrentBuffer()].emplace_back();
        descriptorSet = &command->mDescriptorSets[GetCurrentBuffer()].back();

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = mPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = pipelineVulkan->mDescriptorSetLayouts.data();
        VkResult res                          = vkAllocateDescriptorSets(mDevice, &allocInfo, descriptorSet);
        Assert(res == VK_SUCCESS); // TODO: this will run out
    }
    else
    {
        descriptorSet = &command->mDescriptorSets[GetCurrentBuffer()][currentSet];
    }

    for (auto &layoutBinding : pipelineVulkan->mLayoutBindings)
    {
        for (u32 descriptorIndex = 0; descriptorIndex < layoutBinding.descriptorCount; descriptorIndex++)
        {
            descriptorWrites.emplace_back();
            VkWriteDescriptorSet &descriptorWrite = descriptorWrites.back();
            descriptorWrite                       = {};
            descriptorWrite.dstSet                = *descriptorSet;
            descriptorWrite.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding            = layoutBinding.binding;
            descriptorWrite.descriptorCount       = 1;
            descriptorWrite.descriptorType        = layoutBinding.descriptorType;
            // These next two are for descriptor arrays?
            descriptorWrite.dstArrayElement = 0;

            switch (layoutBinding.descriptorType)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                {
                    GPUResource &resource = command->mResourceTable[layoutBinding.binding];

                    bufferInfos.emplace_back();
                    if (!resource.IsValid() || !resource.IsBuffer())
                    {
                        VkDescriptorBufferInfo &info = bufferInfos.back();
                        info.buffer                  = mNullBuffer;
                        info.offset                  = 0;
                        info.range                   = VK_WHOLE_SIZE;
                    }
                    else
                    {
                        GPUBufferVulkan *bufferVulkan = ToInternal((GPUBuffer *)&resource);

                        // TODO: not right, use a table?
                        bufferInfos.back() = bufferVulkan->subresource.info;
                    }

                    descriptorWrite.pBufferInfo = &bufferInfos.back();
                }
                break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                {
                    GPUResource &resource = command->mResourceTable[layoutBinding.binding];

                    VkImageView view;
                    VkSampler sampler = mNullSampler;
                    if (!resource.IsValid() || !resource.IsTexture())
                    {
                        view = mNullImageView2D;
                    }
                    else
                    {
                        Texture *tex           = (Texture *)(&resource);
                        TextureVulkan *texture = ToInternal(tex);
                        view                   = texture->mSubresource.mImageView;
                        switch (tex->mDesc.mSampler)
                        {
                            case TextureDesc::DefaultSampler::Nearest:
                            {
                                sampler = mNearestSampler;
                            }
                            break;
                            case TextureDesc::DefaultSampler::Linear:
                            {
                                sampler = mLinearSampler;
                            }
                            break;
                        }
                    }
                    imageInfos.emplace_back();
                    VkDescriptorImageInfo &imageInfo = imageInfos.back();
                    imageInfo.sampler                = sampler;
                    imageInfo.imageView              = view;
                    imageInfo.imageLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    descriptorWrite.pImageInfo = &imageInfo;
                }
                break;
                default:
                    Assert(!"Not implemented");
            }
        }
    }

    vkUpdateDescriptorSets(mDevice, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, 0);

    vkCmdBindDescriptorSets(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineVulkan->mPipelineLayout, 0, 1, descriptorSet, 0, 0);
}

void mkGraphicsVulkan::FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize, u64 inOffset)
{
    CommandListVulkan *command  = ToInternal(cmd);
    FrameData *currentFrameData = &mFrameAllocator[GetCurrentBuffer()];

    GPUBufferVulkan *bufVulk = ToInternal(inBuf);
    // Is power of 2
    Assert((currentFrameData->mAlignment & (currentFrameData->mAlignment - 1)) == 0);

    u64 size        = Min(inSize, inBuf->mDesc.mSize);
    u64 alignedSize = AlignPow2(size, (u64)currentFrameData->mAlignment);
    u64 offset      = currentFrameData->mOffset.fetch_add(alignedSize);

    MemoryCopy((void *)((size_t)currentFrameData->mBuffer.mMappedData + offset), inData, size);

    VkBufferCopy copy = {};
    copy.srcOffset    = offset;
    copy.dstOffset    = inOffset;
    copy.size         = size;

    vkCmdCopyBuffer(command->GetCommandBuffer(), ToInternal(&currentFrameData->mBuffer)->mBuffer, bufVulk->mBuffer, 1, &copy);
}

// Uses transfer queue for allocations.
// TODO: use a ring instead?
mkGraphicsVulkan::TransferCommand mkGraphicsVulkan::Stage(u64 size)
{
    VkResult res;
    BeginMutex(&mTransferMutex);

    TransferCommand cmd;
    for (u32 i = 0; i < (u32)mTransferFreeList.size(); i++)
    {
        if (mTransferFreeList[i].mUploadBuffer.mDesc.mSize > size)
        {
            // Submission is done, can reuse cmd pool
            if (vkGetFenceStatus(mDevice, mTransferFreeList[i].mFence) == VK_SUCCESS)
            {
                cmd = mTransferFreeList[i];
                Swap(TransferCommand, mTransferFreeList[mTransferFreeList.size() - 1], mTransferFreeList[i]);
                mTransferFreeList.pop_back();
            }
            break;
        }
    }

    EndMutex(&mTransferMutex);

    if (!cmd.IsValid())
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex        = mCopyFamily;
        res                              = vkCreateCommandPool(mDevice, &poolInfo, 0, &cmd.mCmdPool);
        Assert(res == VK_SUCCESS);
        poolInfo.queueFamilyIndex = mGraphicsFamily;
        res                       = vkCreateCommandPool(mDevice, &poolInfo, 0, &cmd.mTransitionPool);
        Assert(res == VK_SUCCESS);

        VkCommandBufferAllocateInfo bufferInfo = {};
        bufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandPool                 = cmd.mCmdPool;
        bufferInfo.commandBufferCount          = 1;
        bufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        res                                    = vkAllocateCommandBuffers(mDevice, &bufferInfo, &cmd.mCmdBuffer);
        Assert(res == VK_SUCCESS);
        bufferInfo.commandPool = cmd.mTransitionPool;
        res                    = vkAllocateCommandBuffers(mDevice, &bufferInfo, &cmd.mTransitionBuffer);
        Assert(res == VK_SUCCESS);

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        res                         = vkCreateFence(mDevice, &fenceInfo, 0, &cmd.mFence);
        Assert(res == VK_SUCCESS);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (u32 i = 0; i < ArrayLength(cmd.mSemaphores); i++)
        {
            res = vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &cmd.mSemaphores[i]);
            Assert(res == VK_SUCCESS);
        }

        GPUBufferDesc desc = {};
        desc.mSize         = GetNextPowerOfTwo(size);
        desc.mSize         = Max(desc.mSize, kilobytes(64));
        desc.mUsage        = MemoryUsage::CPU_TO_GPU;
        CreateBuffer(&cmd.mUploadBuffer, desc, 0);
        SetName(&cmd.mUploadBuffer, "Transfer Staging Buffer");
    }

    res = vkResetCommandPool(mDevice, cmd.mCmdPool, 0);
    Assert(res == VK_SUCCESS);
    res = vkResetCommandPool(mDevice, cmd.mTransitionPool, 0);
    Assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = 0;
    res                                = vkBeginCommandBuffer(cmd.mCmdBuffer, &beginInfo);
    Assert(res == VK_SUCCESS);
    res = vkBeginCommandBuffer(cmd.mTransitionBuffer, &beginInfo);
    Assert(res == VK_SUCCESS);

    res = vkResetFences(mDevice, 1, &cmd.mFence);
    Assert(res == VK_SUCCESS);

    return cmd;
}

void mkGraphicsVulkan::Submit(TransferCommand cmd)
{
    VkResult res = vkEndCommandBuffer(cmd.mCmdBuffer);
    Assert(res == VK_SUCCESS);
    res = vkEndCommandBuffer(cmd.mTransitionBuffer);
    Assert(res == VK_SUCCESS);

    VkCommandBufferSubmitInfo bufSubmitInfo = {};
    bufSubmitInfo.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    VkSemaphoreSubmitInfo waitSemInfo = {};
    waitSemInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

    VkSemaphoreSubmitInfo submitSemInfo[2] = {};
    submitSemInfo[0].sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submitSemInfo[1].sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    // Submit the copy command to the transfer queue.
    {
        bufSubmitInfo.commandBuffer = cmd.mCmdBuffer;

        submitSemInfo[0].semaphore = cmd.mSemaphores[0];
        submitSemInfo[0].value     = 0;
        submitSemInfo[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos    = &bufSubmitInfo;

        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos    = submitSemInfo;

        MutexScope(&mQueues[QueueType_Copy].mLock)
        {
            res = vkQueueSubmit2(mQueues[QueueType_Copy].mQueue, 1, &submitInfo, VK_NULL_HANDLE);
            Assert(res == VK_SUCCESS);
        }
    }
    // Insert the execution dependency (semaphores) and memory dependency (barrier) on the graphics queue
    {
        bufSubmitInfo.commandBuffer = cmd.mTransitionBuffer;

        waitSemInfo.semaphore = cmd.mSemaphores[0];
        waitSemInfo.value     = 0;
        waitSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitInfo.commandBufferInfoCount   = 1;
        submitInfo.pCommandBufferInfos      = &bufSubmitInfo;
        submitInfo.waitSemaphoreInfoCount   = 1;
        submitInfo.pWaitSemaphoreInfos      = &waitSemInfo;
        submitInfo.signalSemaphoreInfoCount = 0;
        submitInfo.pSignalSemaphoreInfos    = 0;

        MutexScope(&mQueues[QueueType_Graphics].mLock)
        {
            res = vkQueueSubmit2(mQueues[QueueType_Graphics].mQueue, 1, &submitInfo, cmd.mFence);
            Assert(res == VK_SUCCESS);
        }
    }
    MutexScope(&mTransferMutex)
    {
        mTransferFreeList.push_back(cmd);
    }
    // TODO: compute
}

void mkGraphicsVulkan::DeleteBuffer(GPUBuffer *buffer)
{
    GPUBufferVulkan *bufferVulkan = ToInternal(buffer);

    buffer->internalState = 0;
    u32 nextBuffer        = GetNextBuffer();

    MutexScope(&mCleanupMutex)
    {
        mCleanupBuffers[nextBuffer].emplace_back();
        CleanupBuffer &cleanup = mCleanupBuffers[nextBuffer].back();
        cleanup.mBuffer        = bufferVulkan->mBuffer;
        cleanup.mAllocation    = bufferVulkan->mAllocation;
    }
}

void mkGraphicsVulkan::CopyBuffer(CommandList cmd, GPUBuffer *dst, GPUBuffer *src, u32 size)
{
    CommandListVulkan *command = ToInternal(cmd);

    VkBufferCopy copy = {};
    copy.size         = size;
    copy.dstOffset    = 0;
    copy.srcOffset    = 0;

    vkCmdCopyBuffer(command->GetCommandBuffer(),
                    ToInternal(src)->mBuffer,
                    ToInternal(dst)->mBuffer,
                    1,
                    &copy);
}

// So from my understanding, the command list contains buffer * queue_type command pools, each with 1
// command buffer. If the command buffer isn't initialize, the pool/command buffer/semaphore is created for it.
CommandList mkGraphicsVulkan::BeginCommandList(QueueType queue)
{
    VkResult res;
    BeginTicketMutex(&mCommandMutex);
    u32 currentCmd = mCmdCount++;
    if (currentCmd >= mCommandLists.size())
    {
        mCommandLists.emplace_back();
    }

    CommandList cmd;
    cmd.internalState = &mCommandLists[currentCmd];
    EndTicketMutex(&mCommandMutex);

    CommandListVulkan &command = GetCommandList(cmd);
    command.mCurrentQueue      = queue;
    command.mCurrentBuffer     = GetCurrentBuffer();

    // Create new command pool
    if (command.GetCommandBuffer() == VK_NULL_HANDLE)
    {
        for (u32 buffer = 0; buffer < cNumBuffers; buffer++)
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            switch (queue)
            {
                case QueueType_Graphics:
                    poolInfo.queueFamilyIndex = mGraphicsFamily;
                    break;
                case QueueType_Compute:
                    poolInfo.queueFamilyIndex = mComputeFamily;
                    break;
                case QueueType_Copy:
                    poolInfo.queueFamilyIndex = mCopyFamily;
                    break;
                default:
                    Assert(!"Invalid queue type");
                    break;
            }

            res = vkCreateCommandPool(mDevice, &poolInfo, 0, &command.mCommandPools[buffer][queue]);
            Assert(res == VK_SUCCESS);

            VkCommandBufferAllocateInfo bufferInfo = {};
            bufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            bufferInfo.commandPool                 = command.mCommandPools[buffer][queue];
            bufferInfo.commandBufferCount          = 1;
            bufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            res = vkAllocateCommandBuffers(mDevice, &bufferInfo, &command.mCommandBuffers[buffer][queue]);
            Assert(res == VK_SUCCESS);
        }
    } // namespace graphics

    // Reset command pool
    res = vkResetCommandPool(mDevice, command.GetCommandPool(), 0);
    Assert(res == VK_SUCCESS);

    // Start command buffer recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = 0; // for secondary command buffers
    res                                = vkBeginCommandBuffer(command.GetCommandBuffer(), &beginInfo);
    Assert(res == VK_SUCCESS);

    return cmd;
}

void mkGraphicsVulkan::BeginRenderPass(Swapchain *inSwapchain, RenderPassImage *images, u32 count, CommandList inCommandList)
{
    // Assume the vulkan swapchain struct is valid
    SwapchainVulkan *swapchain     = ToInternal(inSwapchain);
    CommandListVulkan *commandList = ToInternal(inCommandList);

    swapchain->mAcquireSemaphoreIndex = (swapchain->mAcquireSemaphoreIndex + 1) % (swapchain->mAcquireSemaphores.size());

    // TODO: this also has to be mutexed, because this is async
    VkResult res = vkAcquireNextImageKHR(mDevice, swapchain->mSwapchain, UINT64_MAX,
                                         swapchain->mAcquireSemaphores[swapchain->mAcquireSemaphoreIndex],
                                         VK_NULL_HANDLE, &swapchain->mImageIndex);
    if (res != VK_SUCCESS)
    {
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        {
            if (CreateSwapchain(inSwapchain))
            {
                BeginRenderPass(inSwapchain, images, count, inCommandList);
                return;
            }
        }
        Assert(0);
    }

    // TODO: handle swap chain becoming suboptimal
    Assert(res == VK_SUCCESS);

    // NOTE: this is usually done during the pipeline creation phase. however, with dynamic rendering,
    // the attachments don't have to be added until the render pass is started.
    VkRenderingInfo info          = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.offset.x      = 0;
    info.renderArea.offset.y      = 0;
    info.renderArea.extent.width  = Min(inSwapchain->mDesc.width, swapchain->mExtent.width);
    info.renderArea.extent.height = Min(inSwapchain->mDesc.height, swapchain->mExtent.height);
    info.layerCount               = 1;

    VkRenderingAttachmentInfo colorAttachment   = {};
    colorAttachment.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView                   = swapchain->mImageViews[swapchain->mImageIndex];
    colorAttachment.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp                     = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = 0.5f;
    colorAttachment.clearValue.color.float32[1] = 0.5f;
    colorAttachment.clearValue.color.float32[2] = 0.5f;
    colorAttachment.clearValue.color.float32[3] = 1.f;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    VkRenderingAttachmentInfo stencilAttachment = {};
    stencilAttachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    list<VkImageMemoryBarrier2> beginPassImageMemoryBarriers;

    // TODO: remove from here
    for (u32 i = 0; i < count; i++)
    {
        RenderPassImage *image     = &images[i];
        Texture *texture           = image->mTexture;
        TextureVulkan *textureVulk = ToInternal(texture);
        switch (image->mImageType)
        {
            case RenderPassImage::RenderImageType::Depth:
            {
                depthAttachment.imageView                     = textureVulk->mSubresource.mImageView;
                depthAttachment.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp                       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachment.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depthAttachment.clearValue.depthStencil.depth = 1.f;
                if (IsFormatStencilSupported(texture->mDesc.mFormat))
                {
                    stencilAttachment.imageView                       = textureVulk->mSubresource.mImageView;
                    stencilAttachment.loadOp                          = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    stencilAttachment.storeOp                         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    stencilAttachment.imageLayout                     = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
                    stencilAttachment.clearValue.depthStencil.stencil = 0;
                }

                beginPassImageMemoryBarriers.emplace_back();

                VkImageMemoryBarrier2 &barrier      = beginPassImageMemoryBarriers.back();
                barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.image                       = textureVulk->mImage;
                barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout                   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barrier.srcStageMask                = VK_PIPELINE_STAGE_2_NONE;
                barrier.dstStageMask                = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
                barrier.srcAccessMask               = VK_ACCESS_2_NONE;
                barrier.dstAccessMask               = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if (IsFormatStencilSupported(texture->mDesc.mFormat))
                {
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                barrier.subresourceRange.baseMipLevel   = 0;
                barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
                barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            }
            break;
            default: Assert(0); break;
        }
    }

    info.colorAttachmentCount = 1;
    info.pColorAttachments    = &colorAttachment;
    info.pDepthAttachment     = &depthAttachment;
    info.pStencilAttachment   = &stencilAttachment;

    VkImageMemoryBarrier2 barrier           = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image                           = swapchain->mImages[swapchain->mImageIndex];
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask                   = VK_ACCESS_2_NONE;
    barrier.dstAccessMask                   = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
    // Queue family transfer
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    beginPassImageMemoryBarriers.push_back(barrier);

    // I think this replaces the sub pass dependency you would have to specify in the render pass?
    VkDependencyInfo dependencyInfo        = {};
    dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = (u32)beginPassImageMemoryBarriers.size();
    dependencyInfo.pImageMemoryBarriers    = beginPassImageMemoryBarriers.data();
    vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    vkCmdBeginRendering(commandList->GetCommandBuffer(), &info);

    commandList->mEndPassImageMemoryBarriers.push_back(barrier);
    commandList->mUpdateSwapchains.push_back(*inSwapchain);
}

void mkGraphicsVulkan::BeginRenderPass(RenderPassImage *images, u32 count, CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);

    VkRenderingInfo info     = {};
    info.sType               = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.layerCount          = 1;

    VkRenderingAttachmentInfo colorAttachments[8] = {};
    u32 colorAttachmentCount                      = 0;

    // colorAttachment.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // colorAttachment.imageView                   = swapchain->mImageViews[swapchain->mImageIndex];
    // colorAttachment.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // colorAttachment.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // colorAttachment.storeOp                     = VK_ATTACHMENT_STORE_OP_STORE;
    // colorAttachment.clearValue.color.float32[0] = 0.5f;
    // colorAttachment.clearValue.color.float32[1] = 0.5f;
    // colorAttachment.clearValue.color.float32[2] = 0.5f;
    // colorAttachment.clearValue.color.float32[3] = 1.f;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    VkRenderingAttachmentInfo stencilAttachment = {};
    stencilAttachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    list<VkImageMemoryBarrier2> beginPassImageMemoryBarriers;

    for (u32 i = 0; i < count; i++)
    {
        RenderPassImage *image     = &images[i];
        Texture *texture           = image->mTexture;
        TextureVulkan *textureVulk = ToInternal(texture);

        info.renderArea.extent.width  = Max(info.renderArea.extent.width, texture->mDesc.mWidth);
        info.renderArea.extent.height = Max(info.renderArea.extent.height, texture->mDesc.mHeight);

        TextureVulkan::Subresource subresource;
        subresource = image->mSubresource < 0 ? textureVulk->mSubresource : textureVulk->mSubresources[image->mSubresource];
        switch (image->mImageType)
        {
            case RenderPassImage::RenderImageType::Depth:
            {
                depthAttachment.imageView                     = subresource.mImageView;
                depthAttachment.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
                depthAttachment.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depthAttachment.clearValue.depthStencil.depth = 1.f;
                if (IsFormatStencilSupported(texture->mDesc.mFormat))
                {
                    stencilAttachment.imageView                       = subresource.mImageView;
                    stencilAttachment.loadOp                          = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    stencilAttachment.storeOp                         = VK_ATTACHMENT_STORE_OP_STORE;
                    stencilAttachment.imageLayout                     = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
                    stencilAttachment.clearValue.depthStencil.stencil = 0;
                }
            }
            break;
            default: Assert(0); break;
        }

        if (image->mLayout != image->mLayoutBefore)
        {
            beginPassImageMemoryBarriers.emplace_back();

            VkImageMemoryBarrier2 &barrier      = beginPassImageMemoryBarriers.back();
            barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.image                       = textureVulk->mImage;
            barrier.oldLayout                   = ConvertResourceUsageToImageLayout(image->mLayoutBefore);
            barrier.newLayout                   = ConvertResourceUsageToImageLayout(image->mLayout);
            barrier.srcStageMask                = ConvertResourceToPipelineStage(image->mLayoutBefore);
            barrier.dstStageMask                = ConvertResourceToPipelineStage(image->mLayout);
            barrier.srcAccessMask               = ConvertResourceUsageToAccessFlag(image->mLayoutBefore);
            barrier.dstAccessMask               = ConvertResourceUsageToAccessFlag(image->mLayout);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (IsFormatStencilSupported(texture->mDesc.mFormat))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = subresource.mBaseLayer;
            barrier.subresourceRange.layerCount     = subresource.mNumLayers;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        }
        if (image->mLayout != image->mLayoutAfter)
        {
            command->mEndPassImageMemoryBarriers.emplace_back();

            VkImageMemoryBarrier2 &barrier      = command->mEndPassImageMemoryBarriers.back();
            barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.image                       = textureVulk->mImage;
            barrier.oldLayout                   = ConvertResourceUsageToImageLayout(image->mLayout);
            barrier.newLayout                   = ConvertResourceUsageToImageLayout(image->mLayoutAfter);
            barrier.srcStageMask                = ConvertResourceToPipelineStage(image->mLayout);
            barrier.dstStageMask                = ConvertResourceToPipelineStage(image->mLayoutAfter);
            barrier.srcAccessMask               = ConvertResourceUsageToAccessFlag(image->mLayout);
            barrier.dstAccessMask               = ConvertResourceUsageToAccessFlag(image->mLayoutAfter);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (IsFormatStencilSupported(texture->mDesc.mFormat))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = subresource.mBaseLayer;
            barrier.subresourceRange.layerCount     = subresource.mNumLayers;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        }
    }

    info.colorAttachmentCount = colorAttachmentCount;
    info.pColorAttachments    = colorAttachments;
    info.pDepthAttachment     = &depthAttachment;
    info.pStencilAttachment   = &stencilAttachment;

    VkDependencyInfo dependencyInfo        = {};
    dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = (u32)beginPassImageMemoryBarriers.size();
    dependencyInfo.pImageMemoryBarriers    = beginPassImageMemoryBarriers.data();
    vkCmdPipelineBarrier2(command->GetCommandBuffer(), &dependencyInfo);

    vkCmdBeginRendering(command->GetCommandBuffer(), &info);
}

void mkGraphicsVulkan::BindVertexBuffer(CommandList cmd, GPUBuffer **buffers, u32 count, u32 *offsets)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    VkBuffer vBuffers[8];
    VkDeviceSize vOffsets[8] = {};
    Assert(count < 8);

    for (u32 i = 0; i < count; i++)
    {
        GPUBuffer *buffer = buffers[i];
        Assert(HasFlags(buffer->mDesc.mResourceUsage, ResourceUsage::VertexBuffer));
        if (buffer == 0 || !buffer->IsValid())
        {
            vBuffers[i] = mNullBuffer;
        }
        else
        {
            GPUBufferVulkan *bufferVulk = ToInternal(buffer);
            vBuffers[i]                 = bufferVulk->mBuffer;
            if (offsets)
            {
                vOffsets[i] = offsets[i];
            }
        }
    }

    vkCmdBindVertexBuffers(commandList->GetCommandBuffer(), 0, count, vBuffers, vOffsets);
}

void mkGraphicsVulkan::BindIndexBuffer(CommandList cmd, GPUBuffer *buffer, u64 offset)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    Assert(HasFlags(buffer->mDesc.mResourceUsage, ResourceUsage::IndexBuffer));
    GPUBufferVulkan *bufferVulk = ToInternal(buffer);
    Assert(bufferVulk);

    vkCmdBindIndexBuffer(commandList->GetCommandBuffer(), bufferVulk->mBuffer, offset, VK_INDEX_TYPE_UINT32);
}

void mkGraphicsVulkan::Draw(CommandList cmd, u32 vertexCount, u32 firstVertex)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    vkCmdDraw(commandList->GetCommandBuffer(), vertexCount, 1, firstVertex, 0);
}

void mkGraphicsVulkan::DrawIndexed(CommandList cmd, u32 indexCount, u32 firstVertex, u32 baseVertex)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    vkCmdDrawIndexed(commandList->GetCommandBuffer(), indexCount, 1, firstVertex, baseVertex, 0);
}

void mkGraphicsVulkan::SetViewport(CommandList cmd, Viewport *viewport)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    VkViewport view;
    view.x        = viewport->x;
    view.y        = viewport->y;
    view.width    = viewport->width;
    view.height   = viewport->height;
    view.minDepth = viewport->minDepth;
    view.maxDepth = viewport->maxDepth;

    vkCmdSetViewport(commandList->GetCommandBuffer(), 0, 1, &view);
}

void mkGraphicsVulkan::SetScissor(CommandList cmd, Rect2 scissor)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    VkRect2D s      = {};
    s.offset.x      = (i32)scissor.minP.x;
    s.offset.y      = (i32)scissor.minP.y;
    s.extent.width  = (u32)(scissor.maxP.x - scissor.minP.x);
    s.extent.height = (u32)(scissor.maxP.y - scissor.minP.y);

    vkCmdSetScissor(commandList->GetCommandBuffer(), 0, 1, &s);
}

void mkGraphicsVulkan::EndRenderPass(CommandList cmd)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    vkCmdEndRendering(commandList->GetCommandBuffer());

    // Barrier between end of rendering and submit.
    if (!commandList->mEndPassImageMemoryBarriers.empty())
    {
        VkDependencyInfo dependencyInfo        = {};
        dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = (u32)commandList->mEndPassImageMemoryBarriers.size();
        dependencyInfo.pImageMemoryBarriers    = commandList->mEndPassImageMemoryBarriers.data();
        vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

        commandList->mEndPassImageMemoryBarriers.clear();
    }
}

void mkGraphicsVulkan::SubmitCommandLists()
{
    VkResult res;
    list<VkCommandBufferSubmitInfo> bufferSubmitInfo;
    list<VkSemaphoreSubmitInfo> waitSemaphores;
    list<VkSemaphoreSubmitInfo> signalSemaphores;

    // Passed to vkQueuePresent
    list<VkSemaphore> submitSemaphores; // signaled when cmd list is submitted to queue
    list<Swapchain *> previousSwapchains;
    list<VkSwapchainKHR> presentSwapchains; // swapchains to present
    list<u32> swapchainImageIndices;        // swapchain image to present

    for (u32 i = 0; i < mCmdCount; i++)
    {
        CommandListVulkan *commandList = &mCommandLists[i];
        vkEndCommandBuffer(commandList->GetCommandBuffer());

        bufferSubmitInfo.emplace_back();
        VkCommandBufferSubmitInfo &bufferInfo = bufferSubmitInfo.back();
        bufferInfo.sType                      = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        bufferInfo.commandBuffer              = commandList->GetCommandBuffer();

        // TODO: I'm not sure if this is ever more than one. also, when this code is extended to multiple queues,
        // compute/transfer don't have these
        for (auto &sc : commandList->mUpdateSwapchains)
        {
            SwapchainVulkan *swapchain = ToInternal(&sc);
            previousSwapchains.push_back(&sc);

            VkSemaphoreSubmitInfo waitSemaphore = {};
            waitSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            waitSemaphore.semaphore             = swapchain->mAcquireSemaphores[swapchain->mAcquireSemaphoreIndex];
            waitSemaphore.value                 = 0;
            waitSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            waitSemaphores.push_back(waitSemaphore);

            VkSemaphoreSubmitInfo signalSemaphore = {};
            signalSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signalSemaphore.semaphore             = swapchain->mReleaseSemaphore;
            signalSemaphore.value                 = 0;
            signalSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            signalSemaphores.push_back(signalSemaphore);

            submitSemaphores.push_back(swapchain->mReleaseSemaphore);

            presentSwapchains.push_back(swapchain->mSwapchain);
            swapchainImageIndices.push_back(swapchain->mImageIndex);
        }

        // commandList->mEndPassImageMemoryBarriers.clear();
        commandList->mUpdateSwapchains.clear();
        commandList->mCurrentSet = 0;
    }

    // The queue submission call waits on the image to be available.
    VkSubmitInfo2 submitInfo            = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount   = (u32)waitSemaphores.size();
    submitInfo.pWaitSemaphoreInfos      = waitSemaphores.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphores.size();
    submitInfo.pSignalSemaphoreInfos    = signalSemaphores.data();
    submitInfo.commandBufferInfoCount   = (u32)bufferSubmitInfo.size();
    submitInfo.pCommandBufferInfos      = bufferSubmitInfo.data();

    // Submit the command buffers to the graphics queue
    MutexScope(&mQueues[QueueType_Graphics].mLock)
    {
        vkQueueSubmit2(mQueues[QueueType_Graphics].mQueue, 1, &submitInfo, mFrameFences[GetCurrentBuffer()][QueueType_Graphics]);

        // Present the swap chain image. This waits for the queue submission to finish.
        {
            VkPresentInfoKHR presentInfo   = {};
            presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = (u32)submitSemaphores.size();
            presentInfo.pWaitSemaphores    = submitSemaphores.data();
            presentInfo.swapchainCount     = (u32)presentSwapchains.size();
            presentInfo.pSwapchains        = presentSwapchains.data();
            presentInfo.pImageIndices      = swapchainImageIndices.data();
            res                            = vkQueuePresentKHR(mQueues[QueueType_Graphics].mQueue, &presentInfo);
        }
    }

    // Handles swap chain invalidation.
    {
        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
            {
                for (auto &swapchain : previousSwapchains)
                {
                    b32 result = CreateSwapchain(swapchain);
                    Assert(result);
                }
            }
            else
            {
                Assert(0)
            }
        }
    }

    // Wait for the queue submission of the previous frame to resolve before continuing.
    {
        // Changes GetCurrentBuffer()
        mCmdCount = 0;
        mFrameCount++;
        // Waits for previous previous frame
        if (mFrameCount >= cNumBuffers)
        {
            u32 currentBuffer = GetCurrentBuffer();
            if (mFrameFences[currentBuffer][QueueType_Graphics] == VK_NULL_HANDLE)
            {
            }
            else
            {
                res = vkWaitForFences(mDevice, 1, &mFrameFences[currentBuffer][QueueType_Graphics], VK_TRUE, UINT64_MAX);
                Assert(res == VK_SUCCESS);
                res = vkResetFences(mDevice, 1, &mFrameFences[currentBuffer][QueueType_Graphics]);
                Assert(res == VK_SUCCESS);
            }
        }
        mFrameAllocator[GetCurrentBuffer()].mOffset.store(0);
    }

    // Reset the next frame
    // PipelineStateVulkan *ps = ToInternal(commandList->mCurrentPipeline);

    Cleanup();
}

void mkGraphicsVulkan::PushConstants(CommandList cmd, u32 size, void *data, u32 offset)
{
    CommandListVulkan *command = ToInternal(cmd);
    if (command->mCurrentPipeline)
    {
        PipelineStateVulkan *pipeline = ToInternal(command->mCurrentPipeline);
        Assert(pipeline->mPushConstantRange.size > 0);

        vkCmdPushConstants(command->GetCommandBuffer(), pipeline->mPipelineLayout,
                           pipeline->mPushConstantRange.stageFlags, offset, size, data);
    }
}

void mkGraphicsVulkan::BindPipeline(const PipelineState *ps, CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);
    if (command->mCurrentPipeline != ps)
    {
        command->mCurrentPipeline = ps;

        PipelineStateVulkan *psVulkan = ToInternal(ps);
        vkCmdBindPipeline(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, psVulkan->mPipeline);
    }
}

void mkGraphicsVulkan::WaitForGPU()
{
    VkResult res = vkDeviceWaitIdle(mDevice);
    Assert(res == VK_SUCCESS);
}

void mkGraphicsVulkan::SetName(GPUResource *resource, const char *name)
{
    if (!mDebugUtils || resource == 0 || !resource->IsValid())
    {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.pObjectName                   = name;
    if (resource->IsTexture())
    {
        info.objectType   = VK_OBJECT_TYPE_IMAGE;
        info.objectHandle = (u64)ToInternal((Texture *)resource)->mImage;
    }
    else if (resource->IsBuffer())
    {
        info.objectType   = VK_OBJECT_TYPE_BUFFER;
        info.objectHandle = (u64)ToInternal((GPUBuffer *)resource)->mBuffer;
    }
    if (info.objectHandle == 0)
    {
        return;
    }
    VkResult res = vkSetDebugUtilsObjectNameEXT(mDevice, &info);
    Assert(res == VK_SUCCESS);
}

void mkGraphicsVulkan::SetName(u64 handle, GraphicsObjectType type, const char *name)
{
    if (!mDebugUtils || handle == 0)
    {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.pObjectName                   = name;
    switch (type)
    {
        case GraphicsObjectType::Queue: info.objectType = VK_OBJECT_TYPE_QUEUE; break;
    }
    info.objectHandle = handle;
    VkResult res      = vkSetDebugUtilsObjectNameEXT(mDevice, &info);
    Assert(res == VK_SUCCESS);
}

// void mkGraphicsVulkan::BeginEvent()
// {
//     VkDebugUtilsLabelEXT debugLabel;
//     debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
// }

} // namespace graphics
