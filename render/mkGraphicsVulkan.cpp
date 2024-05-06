#include "../mkCrack.h"
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

        case Format::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case Format::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        default: return VK_FORMAT_UNDEFINED;
    }
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

VkDescriptorType ConvertDescriptorType(DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        default: Assert(0); return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

VkShaderStageFlags ConvertShaderStage(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: Assert(0); return VK_SHADER_STAGE_ALL;
    }
}

VkAccessFlags2 ConvertResourceStateToAccessFlag(ResourceState state)
{
    VkAccessFlags2 flags = VK_ACCESS_2_NONE;
    if (HasFlags(state, ResourceState_VertexBuffer))
    {
        flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (HasFlags(state, ResourceState_IndexBuffer))
    {
        flags |= VK_ACCESS_2_INDEX_READ_BIT;
    }
    if (HasFlags(state, ResourceState_UniformBuffer))
    {
        flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
    }
    if (HasFlags(state, ResourceState_TransferSrc))
    {
        flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    }
    return flags;
}

VkPipelineStageFlags2 ConvertResourceToPipelineStage(ResourceState state)
{
    VkPipelineStageFlags2 flags = VK_PIPELINE_STAGE_2_NONE;
    if (HasFlags(state, ResourceState_VertexBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if (HasFlags(state, ResourceState_IndexBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
    // TODO: this is likely going to need to change
    if (HasFlags(state, ResourceState_UniformBuffer))
    {
        flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    if (HasFlags(state, ResourceState_TransferSrc))
    {
        flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    return flags;
}

} // namespace vulkan

using namespace vulkan;

void mkGraphicsVulkan::Cleanup()
{
    MutexScope(&mCleanupMutex)
    {
        for (auto &semaphore : mCleanupSemaphores)
        {
            vkDestroySemaphore(mDevice, semaphore, 0);
        }
        mCleanupSemaphores.clear();
        for (auto &swapchain : mCleanupSwapchains)
        {
            vkDestroySwapchainKHR(mDevice, swapchain, 0);
        }
        mCleanupSwapchains.clear();
        for (auto &imageview : mCleanupImageViews)
        {
            vkDestroyImageView(mDevice, imageview, 0);
        }
        mCleanupImageViews.clear();
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
    desc.mUsage = MemoryUsage::CPU_TO_GPU;
    desc.mSize  = megabytes(32);
    desc.mFlags = BindFlag_Vertex | BindFlag_Index | BindFlag_Uniform;
    CreateBuffer(&mFrameAllocator[0].mBuffer, desc, 0);
    CreateBuffer(&mFrameAllocator[1].mBuffer, desc, 0);
    mFrameAllocator[0].mAlignment = 8;
    mFrameAllocator[1].mAlignment = 8;

    // Init descriptor pool
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type                 = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount      = cNumBuffers;

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.poolSizeCount              = 1;
        createInfo.pPoolSizes                 = &poolSize;
        createInfo.maxSets                    = cNumBuffers;

        res = vkCreateDescriptorPool(mDevice, &createInfo, 0, &mPool);
        Assert(res == VK_SUCCESS);
    }
}

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
            inSwapchain->mDesc.format = Format::B8G8R8_SRGB;
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
            MutexScope(&mCleanupMutex)
            {
                mCleanupSwapchains.push_back(swapchainCreateInfo.oldSwapchain);
                for (u32 i = 0; i < (u32)swapchain->mImageViews.size(); i++)
                {
                    mCleanupImageViews.push_back(swapchain->mImageViews[i]);
                }
                for (u32 i = 0; i < (u32)swapchain->mAcquireSemaphores.size(); i++)
                {
                    mCleanupSemaphores.push_back(swapchain->mAcquireSemaphores[i]);
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
    PipelineStateVulkan *ps = new PipelineStateVulkan();
    outPS->internalState    = ps;
    outPS->mDesc            = *inDesc;

    VkShaderModule vertShaderModule = {};
    VkShaderModule fragShaderModule = {};

    string vert = {};
    string frag = {};

    if (inDesc->mVS)
    {
        vert = platform.OS_ReadEntireFile(mArena, inDesc->mVS->mName);
    }
    if (inDesc->mFS)
    {
        frag = platform.OS_ReadEntireFile(mArena, inDesc->mFS->mName);
    }

    // Create shader modules
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize                 = vert.size;
    createInfo.pCode                    = (u32 *)vert.str;
    VkResult res                        = vkCreateShaderModule(mDevice, &createInfo, 0, &vertShaderModule);
    Assert(res == VK_SUCCESS);

    createInfo.codeSize = frag.size;
    createInfo.pCode    = (u32 *)frag.str;
    res                 = vkCreateShaderModule(mDevice, &createInfo, 0, &fragShaderModule);
    Assert(res == VK_SUCCESS);

    // Pipeline create shader stage info
    VkPipelineShaderStageCreateInfo pipelineShaderStageInfo[2] = {};

    pipelineShaderStageInfo[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageInfo[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineShaderStageInfo[0].module = vertShaderModule;
    pipelineShaderStageInfo[0].pName  = "main";

    pipelineShaderStageInfo[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageInfo[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineShaderStageInfo[1].module = fragShaderModule;
    pipelineShaderStageInfo[1].pName  = "main";

    // Vertex inputs

    list<VkVertexInputBindingDescription> bindings;
    list<VkVertexInputAttributeDescription> attributes;

    // Create vertex binding

    for (auto &il : outPS->mDesc.mInputLayouts)
    {
        VkVertexInputBindingDescription bind;

        bind.stride    = il.mStride;
        bind.inputRate = il.mRate == InputRate::Vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        bind.binding   = il.mBinding;

        bindings.push_back(bind);
    }
    // Create vertx attribs
    u32 currentOffset = 0;
    u32 loc           = 0;
    for (auto &il : outPS->mDesc.mInputLayouts)
    {
        u32 currentBinding = il.mBinding;
        for (auto &format : il.mElements)
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
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable                        = VK_FALSE;
    rasterizer.depthBiasConstantFactor                = 0.f;
    rasterizer.depthBiasClamp                         = 0.f;
    rasterizer.depthBiasSlopeFactor                   = 0.f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable                  = VK_FALSE;
    multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading                     = 1.f;
    multisampling.pSampleMask                          = 0;
    multisampling.alphaToCoverageEnable                = VK_FALSE;
    multisampling.alphaToOneEnable                     = VK_FALSE;

    // VkPipelineDepthStencilStateCreateInfo depthStencil = {};
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
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding                      = 0;
        uboLayoutBinding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount              = 1;
        uboLayoutBinding.stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers           = 0;

        VkDescriptorSetLayoutCreateInfo descriptorCreateInfo = {};

        descriptorCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorCreateInfo.bindingCount = 1;
        descriptorCreateInfo.pBindings    = &uboLayoutBinding;

        res = vkCreateDescriptorSetLayout(mDevice, &descriptorCreateInfo, 0, &descriptorLayout);
        Assert(res == VK_SUCCESS);
        ps->mDescriptorSetLayouts.push_back(descriptorLayout);

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = mPool;
        allocInfo.descriptorSetCount          = (u32)ps->mDescriptorSetLayouts.size();
        allocInfo.pSetLayouts                 = ps->mDescriptorSetLayouts.data();

        ps->mDescriptorSets.resize(ps->mDescriptorSetLayouts.size());
        res = vkAllocateDescriptorSets(mDevice, &allocInfo, ps->mDescriptorSets.data());
        Assert(res == VK_SUCCESS);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &descriptorLayout;
    // Push constants are kind of like compile time constants for shaders? except they don't have to be
    // specified at shader creation, instead pipeline creation
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = 0;

    res = vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, 0, &ps->mPipelineLayout);
    Assert(res == VK_SUCCESS);

    // Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};

    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.layout              = ps->mPipelineLayout;
    pipelineInfo.stageCount          = ArrayLength(pipelineShaderStageInfo);
    pipelineInfo.pStages             = pipelineShaderStageInfo;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = 0;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &mDynamicStateInfo;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = 0;
    pipelineInfo.renderPass          = VK_NULL_HANDLE;

    // Dynamic rendering :)
    VkFormat format                             = VK_FORMAT_B8G8R8A8_UNORM; // TODO: get from the swap chain
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType                         = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.viewMask                      = 0;
    renderingInfo.colorAttachmentCount          = 1;
    renderingInfo.pColorAttachmentFormats       = &format;

    // TODO: depth and stencil
    // renderingInfo.depthAttachmentFormat

    pipelineInfo.pNext = &renderingInfo;

    // VkPipelineRenderingCreateInfo
    res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &ps->mPipeline);
    Assert(res == VK_SUCCESS);
}

void mkGraphicsVulkan::Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)
{
    CommandListVulkan *command = ToInternal(cmd);

    for (u32 i = 0; i < count; i++)
    {
        GPUBarrier *barrier = &barriers[i];
        switch (barrier->mType)
        {
            case GPUBarrier::Type::Buffer:
            {
                GPUBuffer *buffer             = (GPUBuffer *)barrier->mResource;
                GPUBufferVulkan *bufferVulkan = ToInternal(buffer);

                VkBufferMemoryBarrier2 bufferBarrier = {};
                bufferBarrier.sType                  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                bufferBarrier.buffer                 = bufferVulkan->mBuffer;
                bufferBarrier.offset                 = 0;
                bufferBarrier.size                   = buffer->mDesc.mSize;
                bufferBarrier.srcStageMask           = ConvertResourceToPipelineStage(barrier->mBefore);
                bufferBarrier.srcAccessMask          = ConvertResourceStateToAccessFlag(barrier->mBefore);
                bufferBarrier.dstStageMask           = ConvertResourceToPipelineStage(barrier->mAfter);
                bufferBarrier.dstAccessMask          = ConvertResourceStateToAccessFlag(barrier->mAfter);
                bufferBarrier.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;

                VkDependencyInfo dependencyInfo         = {};
                dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.bufferMemoryBarrierCount = 1;
                dependencyInfo.pBufferMemoryBarriers    = &bufferBarrier;

                vkCmdPipelineBarrier2(command->GetCommandBuffer(), &dependencyInfo);
            };
            break;
        }
    }
}

void mkGraphicsVulkan::CreateBuffer(GPUBuffer *inBuffer, GPUBufferDesc inDesc, void *inData)
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

    VkBufferCreateInfo createInfo = {};
    createInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size               = inBuffer->mDesc.mSize;

    if (HasFlags(inDesc.mFlags, BindFlag_Vertex))
    {
        createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mFlags, BindFlag_Index))
    {
        createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mFlags, BindFlag_Uniform))
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

    if (inData)
    {
        UpdateBuffer(inBuffer, inData);
    }

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

void mkGraphicsVulkan::UpdateDescriptorSet(CommandList cmd, GPUBuffer *buffer)
{
    CommandListVulkan *command = ToInternal(cmd);
    Assert(command);

    GPUBufferVulkan *bufferVulkan = ToInternal(buffer);

    const PipelineState *pipeline = command->mCurrentPipeline;
    Assert(pipeline);
    PipelineStateVulkan *pipelineVulkan = ToInternal(pipeline);
    Assert(pipelineVulkan);

    VkDescriptorBufferInfo bufferInfo    = {};
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    for (auto &descriptorSet : pipelineVulkan->mDescriptorSets)
    {
        bufferInfo.buffer = bufferVulkan->mBuffer;
        bufferInfo.offset = 0;            //? ? ? ? ;
        bufferInfo.range  = sizeof(Mat4); // VK_WHOLE_SIZE; //? ? ? ? ; // VK_WHOLE_SIZE

        descriptorWrite.dstSet     = descriptorSet;
        descriptorWrite.dstBinding = 0;

        // These next two are for descriptor arrays?
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorCount = 1;

        descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.pBufferInfo      = &bufferInfo;
        descriptorWrite.pImageInfo       = 0;
        descriptorWrite.pTexelBufferView = 0;

        vkUpdateDescriptorSets(mDevice, 1, &descriptorWrite, 0, 0);
    }

    vkCmdBindDescriptorSets(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineVulkan->mPipelineLayout, 0, 1, pipelineVulkan->mDescriptorSets.data(), 0, 0);
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
    copy.dstOffset    = 0;
    copy.size         = size;

    vkCmdCopyBuffer(command->GetCommandBuffer(), ToInternal(&currentFrameData->mBuffer)->mBuffer, bufVulk->mBuffer, 1, &copy);
}

// Updates the buffer
void mkGraphicsVulkan::UpdateBuffer(GPUBuffer *inBuffer, void *inData)
{
    GPUBufferVulkan *buffer = ToInternal(inBuffer);
    Assert(buffer);

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

    MemoryCopy(mappedData, inData, inBuffer->mDesc.mSize);

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

        if (HasFlags(inBuffer->mDesc.mFlags, BindFlag_Vertex))
        {
            bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
            bufferBarrier.dstAccessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            // bufferBarrier.dstAccessMask
        }
        if (HasFlags(inBuffer->mDesc.mFlags, BindFlag_Index))
        {
            bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
            bufferBarrier.dstAccessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        }

        VkDependencyInfo dependencyInfo         = {};
        dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = 1;
        dependencyInfo.pBufferMemoryBarriers    = &bufferBarrier;

        // this command buffer has to be on the graphics queue :(
        vkCmdPipelineBarrier2(cmd.mTransitionBuffer, &dependencyInfo);

        Submit(cmd);
    }
}

// Uses transfer queue for allocations.
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
        desc.mSize         = size;
        desc.mUsage        = MemoryUsage::CPU_TO_GPU;
        CreateBuffer(&cmd.mUploadBuffer, desc, 0);
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
        submitSemInfo[0].stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

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
    // TODO: compute
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

void mkGraphicsVulkan::BeginRenderPass(Swapchain *inSwapchain, CommandList inCommandList)
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
                BeginRenderPass(inSwapchain, inCommandList);
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

    info.colorAttachmentCount = 1;
    info.pColorAttachments    = &colorAttachment;

    // TODO: I have no idea wht this is
    VkImageMemoryBarrier2 barrier           = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image                           = swapchain->mImages[swapchain->mImageIndex];
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
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

    // I think this replaces the sub pass dependency you would have to specify in the render pass?
    VkDependencyInfo dependencyInfo        = {};
    dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;
    vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // barrier.dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    vkCmdBeginRendering(commandList->GetCommandBuffer(), &info);

    commandList->mEndPassImageMemoryBarriers.push_back(barrier);
    commandList->mUpdateSwapchains.push_back(*inSwapchain);
}

void mkGraphicsVulkan::BindVertexBuffer(CommandList cmd, GPUBuffer *buffer)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    Assert(HasFlags(buffer->mDesc.mFlags, BindFlag_Vertex));
    GPUBufferVulkan *bufferVulk = ToInternal(buffer);
    Assert(bufferVulk);

    VkDeviceSize offsets[16];
    offsets[0] = 0;

    vkCmdBindVertexBuffers(commandList->GetCommandBuffer(), 0, 1, &bufferVulk->mBuffer, offsets);
}

void mkGraphicsVulkan::BindIndexBuffer(CommandList cmd, GPUBuffer *buffer)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    Assert(HasFlags(buffer->mDesc.mFlags, BindFlag_Index));
    GPUBufferVulkan *bufferVulk = ToInternal(buffer);
    Assert(bufferVulk);

    vkCmdBindIndexBuffer(commandList->GetCommandBuffer(), bufferVulk->mBuffer, 0, VK_INDEX_TYPE_UINT32);
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
    VkResult res;
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);

    vkCmdEndRendering(commandList->GetCommandBuffer());

    // Barrier between end of rendering and submit.
    VkDependencyInfo dependencyInfo        = {};
    dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = (u32)commandList->mEndPassImageMemoryBarriers.size();
    dependencyInfo.pImageMemoryBarriers    = commandList->mEndPassImageMemoryBarriers.data();
    vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

    mCmdCount = 0;
    vkEndCommandBuffer(commandList->GetCommandBuffer());

    list<VkSemaphoreSubmitInfo> waitSemaphores;
    list<VkSemaphoreSubmitInfo> signalSemaphores;

    // Passed to vkQueuePresent
    list<VkSemaphore> submitSemaphores; // signaled when cmd list is submitted to queue

    // TODO: I'm not sure if this is ever more than one. also, when this code is extended to multiple queues,
    // compute/transfer don't have these
    list<VkSwapchainKHR> presentSwapchains; // swapchains to present
    list<u32> swapchainImageIndices;        // swapchain image to present
    for (auto &sc : commandList->mUpdateSwapchains)
    {
        SwapchainVulkan *swapchain = ToInternal(&sc);

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
        signalSemaphore.stageMask             = VK_PIPELINE_STAGE_2_NONE;
        signalSemaphores.push_back(signalSemaphore);

        submitSemaphores.push_back(swapchain->mReleaseSemaphore);

        presentSwapchains.push_back(swapchain->mSwapchain);
        swapchainImageIndices.push_back(swapchain->mImageIndex);
    }

    VkCommandBufferSubmitInfo bufferInfo = {};
    bufferInfo.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    bufferInfo.commandBuffer             = commandList->GetCommandBuffer();

    // The queue submission call waits on the image to be available.
    VkSubmitInfo2 submitInfo            = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount   = (u32)waitSemaphores.size();
    submitInfo.pWaitSemaphoreInfos      = waitSemaphores.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphores.size();
    submitInfo.pSignalSemaphoreInfos    = signalSemaphores.data();
    submitInfo.commandBufferInfoCount   = 1;
    submitInfo.pCommandBufferInfos      = &bufferInfo;

    // Submit the command buffers to the graphics queue
    MutexScope(&mQueues[QueueType_Graphics].mLock)
    {
        vkQueueSubmit2(mQueues[QueueType_Graphics].mQueue, 1, &submitInfo, mFrameFences[GetCurrentBuffer()][QueueType_Graphics]);
    }

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

    // Handles swap chain invalidation.
    {
        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
            {
                for (auto &swapchain : commandList->mUpdateSwapchains)
                {
                    b32 result = CreateSwapchain(&swapchain);
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
    }

    // Reset the next frame
    mFrameAllocator->mOffset.store(0);
    commandList->mEndPassImageMemoryBarriers.clear();
    commandList->mUpdateSwapchains.clear();
    Cleanup();
}

void mkGraphicsVulkan::BindPipeline(const PipelineState *ps, CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);
    command->mCurrentPipeline  = ps;

    PipelineStateVulkan *psVulkan = ToInternal(ps);
    vkCmdBindPipeline(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, psVulkan->mPipeline);
}

void mkGraphicsVulkan::WaitForGPU()
{
    VkResult res = vkDeviceWaitIdle(mDevice);
    Assert(res == VK_SUCCESS);
}

} // namespace graphics
