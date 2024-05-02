#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "../mkList.h"
#include "../mkPlatformInc.h"
#include "mkGraphicsVulkan.h"
#endif

// #include "mkGraphicsVulkan.h"
#include "../third_party/vulkan/volk.c"

// Namespace only used in this file
namespace vulkan
{
using namespace graphics;

VkFormat ConvertFormat(Format value)
{
    switch (value)
    {
        case Format::B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case Format::B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
        case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
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

} // namespace vulkan

namespace graphics
{
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
            for (auto& layer : validationLayers)
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
}

b32 mkGraphicsVulkan::CreateSwapchain(Window window, Instance instance, SwapchainDesc *desc, Swapchain *inSwapchain)
{
    SwapchainVulkan *swapchain = ToInternal(inSwapchain);

    if (swapchain == 0)
    {
        swapchain = new SwapchainVulkan();
    }
    swapchain->mDesc           = *desc;
    inSwapchain->internalState = swapchain;
    VkResult res;
    // Create surface
#if WINDOWS
    VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
    win32SurfaceCreateInfo.sType                       = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    win32SurfaceCreateInfo.hwnd                        = window;
    win32SurfaceCreateInfo.hinstance                   = instance;

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

    CreateSwapchain(swapchain);

    return true;
}

// Recreates the swap chain if it becomes invalid
b32 mkGraphicsVulkan::CreateSwapchain(SwapchainVulkan *swapchain)
{
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
        surfaceFormat.format     = ConvertFormat(swapchain->mDesc.format);
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        VkFormat requestedFormat = ConvertFormat(swapchain->mDesc.format);

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
            swapchain->mDesc.format = Format::B8G8R8_SRGB;
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
            swapchain->mExtent        = {swapchain->mDesc.width, swapchain->mDesc.height};
            swapchain->mExtent.width  = Clamp(swapchain->mDesc.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            swapchain->mExtent.height = Clamp(swapchain->mDesc.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
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

void mkGraphicsVulkan::CreateShader()
{
    VkShaderModule vertShaderModule = {};
    VkShaderModule fragShaderModule = {};

    string vert = platform.OS_ReadEntireFile(mArena, "src/shaders/triangle_test_vert.spv");
    string frag = platform.OS_ReadEntireFile(mArena, "src/shaders/triangle_test_frag.spv");

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
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount        = 0;
    vertexInputInfo.pVertexBindingDescriptions           = 0;
    vertexInputInfo.vertexAttributeDescriptionCount      = 0;
    vertexInputInfo.pVertexAttributeDescriptions         = 0;

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

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount             = 0;
    pipelineLayoutInfo.pSetLayouts                = 0;
    // Push constants are kind of like compile time constants for shaders? except they don't have to be
    // specified at shader creation, instead pipeline creation
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = 0;

    res = vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, 0, &pipelineLayout);
    Assert(res == VK_SUCCESS);

    // Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};

    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.layout              = pipelineLayout;
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
    res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &mPipeline);
    Assert(res == VK_SUCCESS);
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
    }

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

void mkGraphicsVulkan::BeginRenderPass(Swapchain *inSwapchain, CommandList *inCommandList)
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
            if (CreateSwapchain(swapchain))
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
    info.renderArea.extent.width  = Min(swapchain->mDesc.width, swapchain->mExtent.width);
    info.renderArea.extent.height = Min(swapchain->mDesc.height, swapchain->mExtent.height);
    info.layerCount               = 1;

    VkRenderingAttachmentInfo colorAttachment   = {};
    colorAttachment.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView                   = swapchain->mImageViews[swapchain->mImageIndex];
    colorAttachment.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp                     = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = 0.f;
    colorAttachment.clearValue.color.float32[1] = 0.f;
    colorAttachment.clearValue.color.float32[2] = 0.f;
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
    barrier.srcAccessMask                   = VK_ACCESS_2_NONE;
    barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
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

    barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    vkCmdBeginRendering(commandList->GetCommandBuffer(), &info);
    vkCmdBindPipeline(commandList->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    VkViewport viewport = {};
    viewport.x          = 0.f;
    viewport.y          = 0.f;
    viewport.width      = (f32)swapchain->mExtent.width;
    viewport.height     = (f32)swapchain->mExtent.height;
    viewport.minDepth   = 0.f;
    viewport.maxDepth   = 1.f;

    VkRect2D scissor      = {};
    scissor.extent.width  = 65536;
    scissor.extent.height = 65536;

    vkCmdSetViewport(commandList->GetCommandBuffer(), 0, 1, &viewport);
    vkCmdSetScissor(commandList->GetCommandBuffer(), 0, 1, &scissor);

    // vertex count, instance count, first vertex, first instance
    vkCmdDraw(commandList->GetCommandBuffer(), 3, 1, 0, 0);

    vkCmdEndRendering(commandList->GetCommandBuffer());

    // Barrier between end of rendering and submit. TODO: understand these image memory barriers better
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;
    vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

    mCmdCount = 0;
    vkEndCommandBuffer(commandList->GetCommandBuffer());

    VkSemaphoreSubmitInfo waitSemaphore = {};
    waitSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphore.semaphore             = swapchain->mAcquireSemaphores[swapchain->mAcquireSemaphoreIndex];
    waitSemaphore.value                 = 0;
    waitSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signalSemaphore = {};
    signalSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphore.semaphore             = swapchain->mReleaseSemaphore;
    signalSemaphore.value                 = 0;
    signalSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo bufferInfo = {};
    bufferInfo.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    bufferInfo.commandBuffer             = commandList->GetCommandBuffer();

    // The queue submission call waits on the image to be available.
    VkSubmitInfo2 submitInfo            = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount   = 1;
    submitInfo.pWaitSemaphoreInfos      = &waitSemaphore;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos    = &signalSemaphore;
    submitInfo.commandBufferInfoCount   = 1;
    submitInfo.pCommandBufferInfos      = &bufferInfo;

    // Submit the command buffers to the graphics queue
    vkQueueSubmit2(mQueues[QueueType_Graphics].mQueue, 1, &submitInfo, mFrameFences[GetCurrentBuffer()][QueueType_Graphics]);

    // Present the swap chain image
    // This waits for the queue submission to finish.
    VkPresentInfoKHR presentInfo   = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &swapchain->mReleaseSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain->mSwapchain;
    presentInfo.pImageIndices      = &swapchain->mImageIndex;
    res                            = vkQueuePresentKHR(mQueues[QueueType_Graphics].mQueue, &presentInfo);

    if (res != VK_SUCCESS)
    {
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        {
            b32 result = CreateSwapchain(swapchain);
            Assert(result);
        }
    }

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

    Cleanup();
}

void mkGraphicsVulkan::WaitForGPU()
{
    VkResult res = vkDeviceWaitIdle(mDevice);
    Assert(res == VK_SUCCESS);
}

} // namespace graphics
