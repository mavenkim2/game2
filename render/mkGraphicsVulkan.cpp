#include "../mkCrack.h"
#include "mkGraphics.h"
#ifdef LSP_INCLUDE
#include "../mkList.h"
#include "../mkPlatformInc.h"
#include "mkGraphicsVulkan.h"
#include "../mkShaderCompiler.h"
#endif

// #include "mkGraphicsVulkan.h"
#include "../third_party/vulkan/volk.c"
// #include "../third_party/spirv_reflect.h"

namespace spirv_reflect
{
#define SPV_ENABLE_UTILITY_CODE
#include "../third_party/spirv_reflect.cpp"
} // namespace spirv_reflect

// Namespace only used in this file
namespace graphics
{

namespace vulkan
{

const i32 VK_BINDING_SHIFT_S      = 100;
const i32 VK_BINDING_SHIFT_T      = 200;
const i32 VK_BINDING_SHIFT_U      = 300;
const i32 IMMUTABLE_SAMPLER_START = 50;

VkFormat ConvertFormat(Format value)
{
    switch (value)
    {
        case Format::B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case Format::B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
        case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

        case Format::R32_UINT: return VK_FORMAT_R32_UINT;
        case Format::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case Format::R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;

        case Format::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case Format::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;

        case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;

        case Format::BC1_RGB_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        default: Assert(0); return VK_FORMAT_UNDEFINED;
    }
}

VkImageLayout ConvertImageLayout(ImageLayout layout)
{
    switch (layout)
    {
        case ImageLayout_None: return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout_DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout_ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout_General: return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout_TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout_TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        default: Assert(0); return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkImageLayout ConvertResourceUsageToImageLayout(ResourceUsage usage)
{
    switch (usage)
    {
        case ResourceUsage::None: return VK_IMAGE_LAYOUT_UNDEFINED;
        case ResourceUsage::DepthStencil: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ResourceUsage::SampledImage: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ResourceUsage::StorageImage: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceUsage::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ResourceUsage::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
    switch (stage)
    {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: Assert(0); return VK_SHADER_STAGE_ALL;
    }
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
    if (HasFlags(state, ResourceUsage::UniformBuffer) || HasFlags(state, ResourceUsage::UniformTexelBuffer))
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
    if (HasFlags(state, ResourceUsage::SampledImage))
    {
        flags |= VK_ACCESS_2_SHADER_READ_BIT;
    }
    if (HasFlags(state, ResourceUsage::StorageImage) || HasFlags(state, ResourceUsage::StorageBuffer))
    {
        flags |= VK_ACCESS_2_SHADER_READ_BIT;
        flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
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
    if (HasFlags(state, ResourceUsage::UniformBuffer) || HasFlags(state, ResourceUsage::StorageBuffer) || HasFlags(state, ResourceUsage::UniformTexelBuffer) || HasFlags(state, ResourceUsage::SampledImage) || HasFlags(state, ResourceUsage::StorageImage))
    {
        flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    if (HasFlags(state, ResourceUsage::TransferSrc) || HasFlags(state, ResourceUsage::TransferDst))
    {
        flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
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

} // namespace vulkan
} // namespace graphics

VkPipelineStageFlags2 ConvertPipelineFlags(graphics::PipelineFlag flags)
{
    VkPipelineStageFlags2 outFlags = VK_PIPELINE_STAGE_2_NONE;
    if (HasFlags(flags, graphics::PipelineFlag_IndexInput))
    {
        outFlags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_VertexAttributeInput))
    {
        outFlags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_VertexShader))
    {
        outFlags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_Transfer))
    {
        outFlags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_Compute))
    {
        outFlags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_FragmentShader))
    {
        outFlags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    if (HasFlags(flags, graphics::PipelineFlag_AllCommands))
    {
        outFlags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
    return outFlags;
}

VkAccessFlags2 ConvertAccessFlags(graphics::AccessFlag flags)
{
    VkAccessFlags2 outFlags = VK_ACCESS_2_NONE;
    if (HasFlags(flags, graphics::AccessFlag_VertexAttributeRead))
    {
        outFlags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_IndexRead))
    {
        outFlags |= VK_ACCESS_2_INDEX_READ_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_UniformRead))
    {
        outFlags |= VK_ACCESS_2_UNIFORM_READ_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_TransferRead))
    {
        outFlags |= VK_ACCESS_2_TRANSFER_READ_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_TransferWrite))
    {
        outFlags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_ShaderRead))
    {
        outFlags |= VK_ACCESS_2_SHADER_READ_BIT;
    }
    if (HasFlags(flags, graphics::AccessFlag_ShaderWrite))
    {
        outFlags |= VK_ACCESS_2_SHADER_WRITE_BIT;
    }
    return outFlags;
}

namespace graphics
{
namespace vulkan
{

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
        for (auto &bufferView : cleanupBufferViews[currentBuffer])
        {
            vkDestroyBufferView(mDevice, bufferView, 0);
        }
        cleanupBufferViews[currentBuffer].clear();
        for (auto &buffer : mCleanupBuffers[currentBuffer])
        {
            vmaDestroyBuffer(mAllocator, buffer.mBuffer, buffer.mAllocation);
        }
        mCleanupBuffers[currentBuffer].clear();
        for (auto &texture : cleanupTextures[currentBuffer])
        {
            vmaDestroyImage(mAllocator, texture.image, texture.allocation);
        }
        cleanupTextures[currentBuffer].clear();
    }
}

mkGraphicsVulkan::mkGraphicsVulkan(ValidationMode validationMode, GPUDevicePreference preference)
{
    mArena          = ArenaAlloc();
    const i32 major = 0;
    const i32 minor = 0;
    const i32 patch = 1;

    VK_CHECK(volkInitialize());

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
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, 0));
    list<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

    // Load extension info
    u32 extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(0, &extensionCount, 0));
    list<VkExtensionProperties> extensionProperties(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(0, &extensionCount, extensionProperties.data()));

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
        Assert(volkGetInstanceVersion() >= VK_API_VERSION_1_3);
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

        VK_CHECK(vkCreateInstance(&instInfo, 0, &mInstance));
        volkLoadInstanceOnly(mInstance);

        if (validationMode != ValidationMode::Disabled && mDebugUtils)
        {
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &debugUtilsCreateInfo, 0, &mDebugMessenger));
        }
    }

    // Enumerate physical devices
    {
        u32 deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &deviceCount, 0));
        Assert(deviceCount != 0);

        list<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data()));

        list<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        VkPhysicalDevice preferred = VK_NULL_HANDLE;
        VkPhysicalDevice fallback  = VK_NULL_HANDLE;

        for (auto &device : devices)
        {
            VkPhysicalDeviceProperties2 props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(device, &props);
            if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;

            u32 queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, 0);

            list<VkQueueFamilyProperties2> queueFamilyProps;
            queueFamilyProps.resize(queueFamilyCount);
            for (u32 i = 0; i < queueFamilyCount; i++)
            {
                queueFamilyProps[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            }

            vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilyProps.data());

            u32 graphicsIndex = VK_QUEUE_FAMILY_IGNORED;
            for (u32 i = 0; i < queueFamilyCount; i++)
            {
                if (queueFamilyProps[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    graphicsIndex = i;
                    break;
                }
            }
            if (graphicsIndex == VK_QUEUE_FAMILY_IGNORED) continue;

#if WINDOWS
            if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(device, graphicsIndex)) continue;
#endif
            if (props.properties.apiVersion < VK_API_VERSION_1_3) continue;

            b32 suitable = props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (preference == GPUDevicePreference::Integrated)
            {
                suitable = props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            }
            if (!preferred && suitable)
            {
                preferred = device;
            }
            if (!fallback)
            {
                fallback = device;
            }
        }
        physicalDevice = preferred ? preferred : fallback;
        if (!physicalDevice)
        {
            Printf("Error: No GPU selected\n");
            Assert(0);
        }
        // Printf("Selected GPU: %s\n", mDeviceProperties.properties.deviceName);

        mDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        mFeatures11.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        mFeatures12.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        mFeatures13.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        mDeviceFeatures.pNext = &mFeatures11;
        mFeatures11.pNext     = &mFeatures12;
        mFeatures12.pNext     = &mFeatures13;
        void **featuresChain  = &mFeatures13.pNext;
        *featuresChain        = 0;

        mDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        mProperties11.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        mProperties12.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
        mProperties13.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
        mDeviceProperties.pNext = &mProperties11;
        mProperties11.pNext     = &mProperties12;
        mProperties12.pNext     = &mProperties13;
        void **propertiesChain  = &mProperties13.pNext;

        u32 deviceExtCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &deviceExtCount, 0));
        list<VkExtensionProperties> availableDevExt(deviceExtCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &deviceExtCount, availableDevExt.data()));

        auto checkExtension = [&availableDevExt](const char *extName) {
            for (auto &extension : availableDevExt)
            {
                if (strcmp(extension.extensionName, extName) == 0)
                {
                    return true;
                }
            }
            return false;
        };

        if (checkExtension(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME))
        {
            deviceExtensions.push_back(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME);
        }
        if (checkExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME))
        {
            deviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            capabilities |= DeviceCapabilities_MeshShader;
            meshShaderProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
            *propertiesChain     = &meshShaderProperties;
            propertiesChain      = &meshShaderProperties.pNext;
            meshShaderFeatures   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
            *featuresChain       = &meshShaderFeatures;
            featuresChain        = &meshShaderFeatures.pNext;
        }
        if (checkExtension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
        {
            deviceExtensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
            capabilities |= DeviceCapabilities_VariableShading;
            variableShadingRateProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR};
            *propertiesChain              = &variableShadingRateProperties;
            propertiesChain               = &variableShadingRateProperties.pNext;
            variableShadingRateFeatures   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
            *featuresChain                = &variableShadingRateFeatures;
            featuresChain                 = &variableShadingRateFeatures.pNext;
        }

        vkGetPhysicalDeviceFeatures2(physicalDevice, &mDeviceFeatures);

        Assert(mDeviceFeatures.features.multiDrawIndirect == VK_TRUE);
        Assert(mDeviceFeatures.features.pipelineStatisticsQuery == VK_TRUE);
        Assert(mFeatures13.dynamicRendering == VK_TRUE);
        Assert(mFeatures12.descriptorIndexing == VK_TRUE);
        if (capabilities & DeviceCapabilities_MeshShader)
        {
            Assert(meshShaderFeatures.meshShader == VK_TRUE);
            Assert(meshShaderFeatures.taskShader == VK_TRUE);
        }

        vkGetPhysicalDeviceProperties2(physicalDevice, &mDeviceProperties);
        cTimestampPeriod = (f64)mDeviceProperties.properties.limits.timestampPeriod * 1e-9;

        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, 0);
        mQueueFamilyProperties.resize(queueFamilyCount);
        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            mQueueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        }
        vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, mQueueFamilyProperties.data());

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

        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &mDevice));

        volkLoadDevice(mDevice);
    }

    // Get the device queues
    vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mQueues[QueueType_Graphics].mQueue);
    vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mQueues[QueueType_Compute].mQueue);
    vkGetDeviceQueue(mDevice, mCopyFamily, 0, &mQueues[QueueType_Copy].mQueue);

    SetName(mQueues[QueueType_Graphics].mQueue, "Graphics Queue");
    SetName(mQueues[QueueType_Copy].mQueue, "Transfer Queue");

    // TODO: unified memory access architectures
    mMemProperties       = {};
    mMemProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &mMemProperties);

    VmaAllocatorCreateInfo allocCreateInfo = {};
    allocCreateInfo.physicalDevice         = physicalDevice;
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

    VK_CHECK(vmaCreateAllocator(&allocCreateInfo, &mAllocator));

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
            VK_CHECK(vkCreateFence(mDevice, &fenceInfo, 0, &mFrameFences[buffer][queue]));
        }
    }

    mDynamicStateInfo                   = {};
    mDynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    mDynamicStateInfo.dynamicStateCount = (u32)mDynamicStates.size();
    mDynamicStateInfo.pDynamicStates    = mDynamicStates.data();

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

        VK_CHECK(vkCreateDescriptorPool(mDevice, &createInfo, 0, &mPool));
    }

    // Bindless descriptor pools
    {
        for (DescriptorType type = (DescriptorType)0; type < DescriptorType_Count; type = (DescriptorType)(type + 1))
        {
            VkDescriptorType descriptorType;
            switch (type)
            {
                case DescriptorType_SampledImage: descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
                case DescriptorType_UniformTexel: descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER; break;
                case DescriptorType_StorageBuffer: descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
                case DescriptorType_StorageTexelBuffer: descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER; break;
                default: Assert(0);
            }

            BindlessDescriptorPool &bindlessDescriptorPool = bindlessDescriptorPools[type];
            VkDescriptorPoolSize poolSize                  = {};
            poolSize.type                                  = descriptorType;
            if (type == DescriptorType_StorageBuffer || type == DescriptorType_StorageTexelBuffer)
            {
                poolSize.descriptorCount = Min(10000, mDeviceProperties.properties.limits.maxDescriptorSetStorageBuffers / 4);
            }
            else if (type == DescriptorType_SampledImage)
            {
                poolSize.descriptorCount = Min(10000, mDeviceProperties.properties.limits.maxDescriptorSetSampledImages / 4);
            }
            else if (type == DescriptorType_UniformTexel)
            {
                poolSize.descriptorCount = Min(10000, mDeviceProperties.properties.limits.maxDescriptorSetUniformBuffers / 4);
            }
            bindlessDescriptorPool.descriptorCount = poolSize.descriptorCount;

            VkDescriptorPoolCreateInfo createInfo = {};
            createInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            createInfo.poolSizeCount              = 1;
            createInfo.pPoolSizes                 = &poolSize;
            createInfo.maxSets                    = 1;
            createInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            VK_CHECK(vkCreateDescriptorPool(mDevice, &createInfo, 0, &bindlessDescriptorPool.pool));

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding                      = 0;
            binding.pImmutableSamplers           = 0;
            binding.stageFlags                   = VK_SHADER_STAGE_ALL;
            binding.descriptorType               = descriptorType;
            binding.descriptorCount              = bindlessDescriptorPool.descriptorCount;

            // These flags enable bindless: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorBindingFlagBits.html
            VkDescriptorBindingFlags bindingFlags =
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
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

            VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &createSetLayout, 0, &bindlessDescriptorPool.layout));

            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool              = bindlessDescriptorPool.pool;
            allocInfo.descriptorSetCount          = 1;
            allocInfo.pSetLayouts                 = &bindlessDescriptorPool.layout;
            VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo, &bindlessDescriptorPool.set));

            for (u32 i = 0; i < poolSize.descriptorCount; i++)
            {
                bindlessDescriptorPool.freeList.push_back(poolSize.descriptorCount - i - 1);
            }
            bindlessDescriptorSets.push_back(bindlessDescriptorPool.set);
            bindlessDescriptorSetLayouts.push_back(bindlessDescriptorPool.layout);

            // Set debug names
            TempArena temp = ScratchStart(0, 0);
            string typeName;
            switch (type)
            {
                case DescriptorType_SampledImage: typeName = "Sampled Image"; break;
                case DescriptorType_StorageBuffer: typeName = "Storage Buffer"; break;
                case DescriptorType_UniformTexel: typeName = "Uniform Texel Buffer"; break;
                case DescriptorType_StorageTexelBuffer: typeName = "Storage Texel Buffer"; break;
            }
            string name = PushStr8F(temp.arena, "Bindless Descriptor Set Layout: %S", typeName);
            SetName(bindlessDescriptorPool.layout, (const char *)name.str);

            name = PushStr8F(temp.arena, "Bindless Descriptor Set: %S", typeName);
            SetName(bindlessDescriptorPool.set, (const char *)name.str);
            ScratchEnd(temp);
        }
    }

    // Init frame allocators
    {
        GPUBufferDesc desc;
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mSize          = megabytes(32);
        desc.mResourceUsage = ResourceUsage::NotBindless; // | ResourceUsage::VertexBuffer | ResourceUsage::IndexBuffer | ResourceUsage::UniformBuffer |
        for (u32 i = 0; i < cNumBuffers; i++)
        {
            CreateBuffer(&mFrameAllocator[i].mBuffer, desc, 0);
            mFrameAllocator[i].mAlignment = 8;
        }
    }

    // Initialize ring buffer
    {
        u32 ringBufferSize = megabytes(256);
        GPUBufferDesc desc;
        desc.mUsage         = MemoryUsage::CPU_TO_GPU;
        desc.mSize          = ringBufferSize;
        desc.mResourceUsage = ResourceUsage::TransferSrc;

        for (u32 i = 0; i < ArrayLength(stagingRingAllocators); i++)
        {
            RingAllocator &stagingRingAllocator = stagingRingAllocators[i];
            CreateBuffer(&stagingRingAllocator.transferRingBuffer, desc, 0);
            SetName(&stagingRingAllocator.transferRingBuffer, "Transfer Staging Buffer");

            stagingRingAllocator.ringBufferSize = ringBufferSize;
            stagingRingAllocator.writePos = stagingRingAllocator.readPos = 0;
            stagingRingAllocator.allocationReadPos                       = 0;
            stagingRingAllocator.allocationWritePos                      = 0;
            stagingRingAllocator.alignment                               = 16;
        }
    }

    // Default samplers
    {
        // Null sampler
        VkSamplerCreateInfo samplerCreate = {};
        samplerCreate.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &mNullSampler));

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
        samplerCreate.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreate.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreate.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        // sampler linear wrap
        immutableSamplers.emplace_back();
        VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &immutableSamplers.back()));

        // samler nearest wrap
        samplerCreate.minFilter  = VK_FILTER_NEAREST;
        samplerCreate.magFilter  = VK_FILTER_NEAREST;
        samplerCreate.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        immutableSamplers.emplace_back();
        VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &immutableSamplers.back()));

        // sampler linear clamp
        samplerCreate.minFilter  = VK_FILTER_LINEAR;
        samplerCreate.magFilter  = VK_FILTER_LINEAR;
        samplerCreate.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        immutableSamplers.emplace_back();
        VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &immutableSamplers.back()));

        // sampler nearest compare
        samplerCreate.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreate.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreate.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreate.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreate.compareEnable = VK_TRUE;
        samplerCreate.compareOp     = VK_COMPARE_OP_GREATER_OR_EQUAL;
        immutableSamplers.emplace_back();
        VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &immutableSamplers.back()));
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
        VK_CHECK(vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &mNullImage2D, &mNullImage2DAllocation, 0));

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

        VK_CHECK(vkCreateImageView(mDevice, &createInfo, 0, &mNullImageView2D));

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        VK_CHECK(vkCreateImageView(mDevice, &createInfo, 0, &mNullImageView2DArray));

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

        VK_CHECK(vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo, &mNullBuffer, &mNullBufferAllocation, 0));
    }
} // namespace graphics

b32 mkGraphicsVulkan::CreateSwapchain(Window window, SwapchainDesc *desc, Swapchain *inSwapchain)
{
    SwapchainVulkan *swapchain = 0;
    if (inSwapchain->IsValid())
    {
        swapchain = ToInternal(inSwapchain);
    }
    else
    {
        MutexScope(&mArenaMutex)
        {
            swapchain = freeSwapchain;
            if (swapchain)
            {
                StackPop(freeSwapchain);
            }
            else
            {
                swapchain = PushStruct(mArena, SwapchainVulkan);
            }
        }
    }
    inSwapchain->mDesc         = *desc;
    inSwapchain->internalState = swapchain;
// Create surface
#if WINDOWS
    VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
    win32SurfaceCreateInfo.sType                       = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    win32SurfaceCreateInfo.hwnd                        = window;
    win32SurfaceCreateInfo.hinstance                   = GetModuleHandleW(0);

    VK_CHECK(vkCreateWin32SurfaceKHR(mInstance, &win32SurfaceCreateInfo, 0, &swapchain->mSurface));
#else
#error not supported
#endif

    // Check whether physical device has a queue family that supports presenting to the surface
    u32 presentFamily = VK_QUEUE_FAMILY_IGNORED;
    for (u32 familyIndex = 0; familyIndex < mQueueFamilyProperties.size(); familyIndex++)
    {
        VkBool32 supported = false;
        // TODO: why is this function pointer null?
        // if (vkGetPhysicalDeviceSurfaceSupportKHR == 0)
        // {
        //     volkLoadInstanceOnly(mInstance);
        // }
        Assert(vkGetPhysicalDeviceSurfaceSupportKHR);
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, swapchain->mSurface, &supported));

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

    u32 formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, swapchain->mSurface, &formatCount, 0));
    list<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, swapchain->mSurface, &formatCount, surfaceFormats.data()));

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, swapchain->mSurface, &surfaceCapabilities));

    u32 presentCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, swapchain->mSurface, &presentCount, 0));
    list<VkPresentModeKHR> surfacePresentModes;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, swapchain->mSurface, &presentCount, surfacePresentModes.data()));

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

        VK_CHECK(vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, 0, &swapchain->mSwapchain));

        // Clean up the old swap chain, if it exists
        if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
        {
            u32 currentBuffer = GetCurrentBuffer();
            MutexScope(&mCleanupMutex)
            {
                mCleanupSwapchains[currentBuffer].push_back(swapchainCreateInfo.oldSwapchain);
                for (u32 i = 0; i < (u32)swapchain->mImageViews.size(); i++)
                {
                    mCleanupImageViews[currentBuffer].push_back(swapchain->mImageViews[i]);
                }
                for (u32 i = 0; i < (u32)swapchain->mAcquireSemaphores.size(); i++)
                {
                    mCleanupSemaphores[currentBuffer].push_back(swapchain->mAcquireSemaphores[i]);
                }
                swapchain->mAcquireSemaphores.clear();
            }
        }

        // Get swapchain images
        VK_CHECK(vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapchain, &imageCount, 0));
        swapchain->mImages.resize(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapchain, &imageCount, swapchain->mImages.data()));

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
            VK_CHECK(vkCreateImageView(mDevice, &createInfo, 0, &swapchain->mImageViews[i]));
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
                    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &swapchain->mAcquireSemaphores[i]));
                }
            }
            if (swapchain->mReleaseSemaphore == VK_NULL_HANDLE)
            {
                VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &swapchain->mReleaseSemaphore));
            }
        }
    }
    return true;
}

void mkGraphicsVulkan::CreateShader(Shader *shader, string shaderData)
{
    ShaderVulkan *shaderVulkan = 0;
    MutexScope(&mArenaMutex)
    {
        shaderVulkan = freeShader;
        if (shaderVulkan)
        {
            StackPop(freeShader);
        }
        else
        {
            shaderVulkan = PushStruct(mArena, ShaderVulkan);
        }
    }

    shader->internalState = shaderVulkan;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode                    = (u32 *)shaderData.str;
    createInfo.codeSize                 = shaderData.size;
    VK_CHECK(vkCreateShaderModule(mDevice, &createInfo, 0, &shaderVulkan->module));

    VkPipelineShaderStageCreateInfo &pipelineStageInfo = shaderVulkan->pipelineStageInfo;
    pipelineStageInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    switch (shader->stage)
    {
        case ShaderStage::Vertex: pipelineStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; break;
        case ShaderStage::Fragment: pipelineStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
        case ShaderStage::Compute: pipelineStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
        default: Assert(0); break;
    }
    pipelineStageInfo.module = shaderVulkan->module;
    pipelineStageInfo.pName  = "main";

    {
        spirv_reflect::SpvReflectShaderModule module = {};
        spirv_reflect::SpvReflectResult result       = spvReflectCreateShaderModule(createInfo.codeSize, createInfo.pCode, &module);
        Assert(result == spirv_reflect::SPV_REFLECT_RESULT_SUCCESS);

        u32 bindingCount = 0;
        result           = spirv_reflect::spvReflectEnumerateDescriptorBindings(&module, &bindingCount, 0);
        Assert(result == spirv_reflect::SPV_REFLECT_RESULT_SUCCESS);

        list<spirv_reflect::SpvReflectDescriptorBinding *> descriptorBindings;
        descriptorBindings.resize(bindingCount);
        result = spirv_reflect::spvReflectEnumerateDescriptorBindings(&module, &bindingCount, descriptorBindings.data());
        Assert(result == spirv_reflect::SPV_REFLECT_RESULT_SUCCESS);

        u32 pushConstantCount = 0;
        result                = spirv_reflect::spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, 0);
        Assert(result == spirv_reflect::SPV_REFLECT_RESULT_SUCCESS);

        list<spirv_reflect::SpvReflectBlockVariable *> pushConstants;
        pushConstants.resize(pushConstantCount);
        result = spirv_reflect::spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
        Assert(result == spirv_reflect::SPV_REFLECT_RESULT_SUCCESS);

        for (auto &binding : descriptorBindings)
        {
            b32 bindless = binding->set > 0;

            if (bindless) continue;

            VkDescriptorSetLayoutBinding b = {};
            b.binding                      = binding->binding;
            b.stageFlags                   = pipelineStageInfo.stage;
            b.descriptorType               = (VkDescriptorType)binding->descriptor_type;
            b.descriptorCount              = binding->count;

            // Immutable samplers start at register s50
            if (binding->descriptor_type == spirv_reflect::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER &&
                binding->binding >= VK_BINDING_SHIFT_S + IMMUTABLE_SAMPLER_START)
            {
                b.pImmutableSamplers = &immutableSamplers[binding->binding - VK_BINDING_SHIFT_S - IMMUTABLE_SAMPLER_START];
            }

            shaderVulkan->layoutBindings.push_back(b);
        }

        for (auto &pc : pushConstants)
        {
            VkPushConstantRange *range = &shaderVulkan->pushConstantRange;
            range->offset              = pc->offset;
            range->size                = pc->size;
            range->stageFlags          = VK_SHADER_STAGE_ALL;
        }

        spvReflectDestroyShaderModule(&module);
    }
}

void mkGraphicsVulkan::CreatePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)
{
    PipelineStateVulkan *ps = 0;
    MutexScope(&mArenaMutex)
    {
        ps = freePipeline;
        if (ps)
        {
            StackPop(freePipeline);
        }
        else
        {
            ps = PushStruct(mArena, PipelineStateVulkan);
        }
    }
    outPS->internalState = ps;
    outPS->mDesc         = *inDesc;

    TempArena temp = ScratchStart(0, 0);

    // Pipeline create shader stage info
    list<VkPipelineShaderStageCreateInfo> pipelineShaderStageInfo;

    // Add all the shader info
    for (u32 stage = 0; stage < (u32)ShaderStage::Count; stage++)
    {
        if (inDesc->shaders[stage] != 0)
        {
            // Add the pipeline stage creation info
            Shader *shader             = inDesc->shaders[stage];
            ShaderVulkan *shaderVulkan = ToInternal(shader);
            pipelineShaderStageInfo.push_back(shaderVulkan->pipelineStageInfo);

            string stageName;
            switch (shaderVulkan->pipelineStageInfo.stage)
            {
                case VK_SHADER_STAGE_VERTEX_BIT: stageName = "VS "; break;
                case VK_SHADER_STAGE_FRAGMENT_BIT: stageName = "FS "; break;
                case VK_SHADER_STAGE_COMPUTE_BIT: stageName = "Compute "; break;
                default: Assert(0); break;
            }
            SetName(shaderVulkan->module, (const char *)StrConcat(temp.arena, stageName, name).str);

            // Add the descriptor bindings
            for (auto &shaderBinding : shaderVulkan->layoutBindings)
            {
                b8 found = false;
                for (auto &layoutBinding : ps->mLayoutBindings)
                {
                    if (shaderBinding.binding == layoutBinding.binding)
                    {
                        // No overlapping bindings allowed (e.g t0 and b0 isn't allowed)
                        Assert(shaderBinding.descriptorCount == layoutBinding.descriptorCount);
                        Assert(shaderBinding.descriptorType == layoutBinding.descriptorType);
                        found = true;
                        layoutBinding.stageFlags |= shaderBinding.stageFlags;
                    }
                }
                if (!found)
                {
                    ps->mLayoutBindings.push_back(shaderBinding);
                }
            }
            // Push constant range
            VkPushConstantRange &pc       = ps->mPushConstantRange;
            VkPushConstantRange &shaderPc = shaderVulkan->pushConstantRange;
            pc.size                       = Max(pc.size, shaderPc.size);
            pc.offset                     = Min(pc.size, shaderPc.offset);
            pc.stageFlags |= shaderPc.stageFlags;
        }
    }

    ScratchEnd(temp);
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
        VkDescriptorSetLayoutCreateInfo descriptorCreateInfo = {};

        descriptorCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorCreateInfo.bindingCount = (u32)ps->mLayoutBindings.size();
        descriptorCreateInfo.pBindings    = ps->mLayoutBindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descriptorCreateInfo, 0, &descriptorLayout));

        ps->mDescriptorSetLayouts.push_back(descriptorLayout);
    }

    // Push bindless descriptor set layouts
    for (auto &layout : bindlessDescriptorSetLayouts)
    {
        ps->mDescriptorSetLayouts.push_back(layout);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount             = (u32)ps->mDescriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts                = ps->mDescriptorSetLayouts.data();
    // Push constants are kind of like compile time constants for shaders? except they don't have to be
    // specified at shader creation, instead pipeline creation
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = 0;

    VkPushConstantRange &range = ps->mPushConstantRange;
    if (range.size > 0)
    {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &range;
    }

    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, 0, &ps->mPipelineLayout));

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
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &ps->mPipeline));
    SetName(ps->mPipeline, (const char *)name.str);
}

void mkGraphicsVulkan::CreateComputePipeline(PipelineStateDesc *inDesc, PipelineState *outPS, string name)
{
    PipelineStateVulkan *ps = 0;
    MutexScope(&mArenaMutex)
    {
        ps = freePipeline;
        if (ps)
        {
            StackPop(freePipeline);
        }
        else
        {
            ps = PushStruct(mArena, PipelineStateVulkan);
        }
    }
    outPS->internalState = ps;
    outPS->mDesc         = *inDesc;

    Shader *computeShader      = inDesc->compute;
    ShaderVulkan *shaderVulkan = ToInternal(computeShader);

    for (auto &shaderBinding : shaderVulkan->layoutBindings)
    {
        ps->mLayoutBindings.push_back(shaderBinding);
    }

    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorSetLayoutCreateInfo descriptorCreateInfo = {};
    descriptorCreateInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorCreateInfo.bindingCount                    = (u32)ps->mLayoutBindings.size();
    descriptorCreateInfo.pBindings                       = ps->mLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descriptorCreateInfo, 0, &descriptorLayout));

    ps->mDescriptorSetLayouts.push_back(descriptorLayout);

    // Push bindless descriptor set layouts
    for (auto &layout : bindlessDescriptorSetLayouts)
    {
        ps->mDescriptorSetLayouts.push_back(layout);
    }

    TempArena temp = ScratchStart(0, 0);
    SetName(shaderVulkan->module, (const char *)StrConcat(temp.arena, "CS ", name).str);
    ScratchEnd(temp);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount             = (u32)ps->mDescriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts                = ps->mDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount     = 0;
    pipelineLayoutInfo.pPushConstantRanges        = 0;
    if (shaderVulkan->pushConstantRange.size > 0)
    {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &shaderVulkan->pushConstantRange;
    }

    ps->mPushConstantRange = shaderVulkan->pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, 0, &ps->mPipelineLayout));

    VkComputePipelineCreateInfo computePipelineInfo = {};
    computePipelineInfo.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.stage                       = shaderVulkan->pipelineStageInfo;
    computePipelineInfo.layout                      = ps->mPipelineLayout;
    computePipelineInfo.basePipelineHandle          = VK_NULL_HANDLE;
    computePipelineInfo.basePipelineIndex           = 0;

    VK_CHECK(vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &computePipelineInfo, 0, &ps->mPipeline));
    SetName(ps->mPipeline, (const char *)name.str);
}

void mkGraphicsVulkan::Barrier(CommandList cmd, GPUBarrier *barriers, u32 count)
{
    CommandListVulkan *command = ToInternal(cmd);

    list<VkBufferMemoryBarrier2> bufferBarriers;
    list<VkMemoryBarrier2> memoryBarriers;
    list<VkImageMemoryBarrier2> imageBarriers;

    for (u32 i = 0; i < count; i++)
    {
        GPUBarrier *barrier               = &barriers[i];
        VkPipelineStageFlags2 stageBefore = ConvertPipelineFlags(barrier->stageBefore);
        VkPipelineStageFlags2 stageAfter  = ConvertPipelineFlags(barrier->stageAfter);
        VkAccessFlags2 accessBefore       = ConvertAccessFlags(barrier->accessBefore);
        VkAccessFlags2 accessAfter        = ConvertAccessFlags(barrier->accessAfter);

        switch (barrier->type)
        {
            case GPUBarrier::Type::Buffer:
            {
                GPUBuffer *buffer             = (GPUBuffer *)barrier->resource;
                GPUBufferVulkan *bufferVulkan = ToInternal(buffer);

                bufferBarriers.emplace_back();
                VkBufferMemoryBarrier2 &bufferBarrier = bufferBarriers.back();
                bufferBarrier.sType                   = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                bufferBarrier.buffer                  = bufferVulkan->mBuffer;
                bufferBarrier.offset                  = 0;
                bufferBarrier.size                    = buffer->mDesc.mSize;
                bufferBarrier.srcStageMask            = stageBefore;
                bufferBarrier.srcAccessMask           = accessBefore;
                bufferBarrier.dstStageMask            = stageAfter;
                bufferBarrier.dstAccessMask           = accessAfter;
                bufferBarrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
            };
            break;
            case GPUBarrier::Type::Memory:
            {
                memoryBarriers.emplace_back();
                VkMemoryBarrier2 &memoryBarrier = memoryBarriers.back();
                memoryBarrier.sType             = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                memoryBarrier.srcStageMask      = stageBefore;
                memoryBarrier.srcAccessMask     = accessBefore;
                memoryBarrier.dstStageMask      = stageAfter;
                memoryBarrier.dstAccessMask     = accessAfter;
            }
            break;
            case GPUBarrier::Type::Image:
            {
                Texture *texture             = (Texture *)barrier->resource;
                TextureVulkan *textureVulkan = ToInternal(texture);
                Assert(textureVulkan->mImage != VK_NULL_HANDLE);
                imageBarriers.emplace_back();
                VkImageMemoryBarrier2 &imageBarrier          = imageBarriers.back();
                imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                imageBarrier.image                           = textureVulkan->mImage;
                imageBarrier.oldLayout                       = ConvertImageLayout(barrier->layoutBefore);
                imageBarrier.newLayout                       = ConvertImageLayout(barrier->layoutAfter);
                imageBarrier.srcStageMask                    = stageBefore;
                imageBarrier.srcAccessMask                   = accessBefore;
                imageBarrier.dstStageMask                    = stageAfter;
                imageBarrier.dstAccessMask                   = accessAfter;
                imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                imageBarrier.subresourceRange.baseMipLevel   = 0;
                imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
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
    dependencyInfo.imageMemoryBarrierCount  = (u32)imageBarriers.size();
    dependencyInfo.pImageMemoryBarriers     = imageBarriers.data();

    vkCmdPipelineBarrier2(command->GetCommandBuffer(), &dependencyInfo);
}

u64 mkGraphicsVulkan::GetMinAlignment(GPUBufferDesc *inDesc)
{
    u64 alignment = 1;
    if (HasFlags(inDesc->mResourceUsage, ResourceUsage::UniformBuffer))
    {
        alignment = Max(alignment, mDeviceProperties.properties.limits.minUniformBufferOffsetAlignment);
    }
    if (HasFlags(inDesc->mResourceUsage, ResourceUsage::UniformTexelBuffer) || HasFlags(inDesc->mResourceUsage, ResourceUsage::StorageTexelBuffer))
    {
        alignment = Max(alignment, mDeviceProperties.properties.limits.minTexelBufferOffsetAlignment);
    }
    if (HasFlags(inDesc->mResourceUsage, ResourceUsage::StorageBuffer) || HasFlags(inDesc->mResourceUsage, ResourceUsage::StorageTexelBuffer))
    {
        alignment = Max(alignment, mDeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
    }
    return alignment;
}

void mkGraphicsVulkan::CreateBufferCopy(GPUBuffer *inBuffer, GPUBufferDesc inDesc, CopyFunction initCallback)
{
    GPUBufferVulkan *buffer = 0;
    MutexScope(&mArenaMutex)
    {
        buffer = freeBuffer;
        if (buffer)
        {
            StackPop(freeBuffer);
        }
        else
        {
            buffer = PushStruct(mArena, GPUBufferVulkan);
        }
    }

    buffer->subresourceSrv  = -1;
    buffer->subresourceUav  = -1;
    inBuffer->internalState = buffer;
    inBuffer->mDesc         = inDesc;
    inBuffer->mMappedData   = 0;
    inBuffer->mResourceType = GPUResource::ResourceType::Buffer;
    inBuffer->ticket.ticket = 0;

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
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::UniformTexelBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::StorageBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::StorageTexelBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::IndirectBuffer))
    {
        createInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
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
    else if (inDesc.mUsage == MemoryUsage::GPU_TO_CPU)
    {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; // TODO: not necessary?
    }

    // Buffers only on GPU must be copied to using a staging buffer
    else if (inDesc.mUsage == MemoryUsage::GPU_ONLY)
    {
        createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VK_CHECK(vmaCreateBuffer(mAllocator, &createInfo, &allocCreateInfo, &buffer->mBuffer, &buffer->mAllocation, 0));

    // Map the buffer if it's a staging buffer
    if (inDesc.mUsage == MemoryUsage::CPU_TO_GPU || inDesc.mUsage == MemoryUsage::GPU_TO_CPU)
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
            mappedData = cmd.ringAllocation->mappedData;
        }

        initCallback(mappedData);

        if (cmd.IsValid())
        {
            if (inBuffer->mDesc.mSize != 0)
            {
                // Memory copy data to the staging buffer
                VkBufferCopy bufferCopy = {};
                bufferCopy.srcOffset    = cmd.ringAllocation->offset;
                bufferCopy.dstOffset    = 0;
                bufferCopy.size         = inBuffer->mDesc.mSize;

                RingAllocator *allocator = &stagingRingAllocators[cmd.ringAllocation->ringId];

                // Copy from the staging buffer to the allocated buffer
                vkCmdCopyBuffer(cmd.mCmdBuffer, ToInternal(&allocator->transferRingBuffer)->mBuffer, buffer->mBuffer, 1, &bufferCopy);
            }
            FenceVulkan *fenceVulkan = ToInternal(&cmd.fence);
            inBuffer->ticket.fence   = cmd.fence;
            inBuffer->ticket.ticket  = fenceVulkan->count;
            Submit(cmd);
        }
    }

    // resource bound using traditional binding method
    if (HasFlags(inDesc.mResourceUsage, ResourceUsage::NotBindless))
    {
        GPUBufferVulkan::Subresource subresource;
        subresource.info.buffer = buffer->mBuffer;
        subresource.info.offset = 0;
        subresource.info.range  = VK_WHOLE_SIZE;
        buffer->subresources.push_back(subresource);

        Assert(!HasFlags(inDesc.mResourceUsage, ResourceUsage::UniformTexelBuffer));
        Assert(!HasFlags(inDesc.mResourceUsage, ResourceUsage::StorageBuffer));

        buffer->subresourceSrv = 0;
    }
    else if (!HasFlags(inDesc.mResourceUsage, ResourceUsage::MegaBuffer))
    {
        i32 subresourceIndex = -1;
        if (HasFlags(inDesc.mResourceUsage, ResourceUsage::StorageTexelBuffer) || HasFlags(inDesc.mResourceUsage, ResourceUsage::StorageBuffer))
        {
            subresourceIndex       = CreateSubresource(inBuffer, ResourceType::UAV);
            buffer->subresourceUav = subresourceIndex;
        }
        if (HasFlags(inDesc.mResourceUsage, ResourceUsage::UniformTexelBuffer) || HasFlags(inDesc.mResourceUsage, ResourceUsage::UniformBuffer))
        {
            subresourceIndex       = CreateSubresource(inBuffer, ResourceType::SRV);
            buffer->subresourceSrv = subresourceIndex;
        }
    }
}

void mkGraphicsVulkan::CreateTexture(Texture *outTexture, TextureDesc desc, void *inData)
{
    TextureVulkan *texVulk = 0;
    MutexScope(&mArenaMutex)
    {
        texVulk = freeTexture;
        if (texVulk)
        {
            StackPop(freeTexture);
        }
        else
        {
            texVulk = PushStruct(mArena, TextureVulkan);
        }
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
    if (HasFlags(&desc, ResourceUsage::SampledImage))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (HasFlags(&desc, ResourceUsage::StorageImage))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (HasFlags(&desc, ResourceUsage::DepthStencil))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (HasFlags(&desc, ResourceUsage::TransferSrc))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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

    if (desc.mUsage == MemoryUsage::GPU_TO_CPU)
    {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        u32 size                        = GetTextureSize(desc);
        VkBufferCreateInfo bufferCreate = {};
        bufferCreate.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreate.size               = size;
        bufferCreate.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK(vmaCreateBuffer(mAllocator, &bufferCreate, &allocInfo, &texVulk->stagingBuffer, &texVulk->mAllocation, &info));

        // TODO: support readback of multiple mips/layers/depth
        Assert(desc.mDepth == 1 && desc.mNumMips == 1 && desc.mNumLayers == 1);

        TextureMappedData data;
        data.mappedData = texVulk->mAllocation->GetMappedData();
        data.size       = (u32)texVulk->mAllocation->GetSize();

        outTexture->mappedData = data; //.push_back(data);
    }
    else if (desc.mUsage == MemoryUsage::GPU_ONLY)
    {
        VK_CHECK(vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &texVulk->mImage, &texVulk->mAllocation, &info));
    }

    // TODO: handle 3d texture creation
    if (inData)
    {
        TransferCommand cmd;
        void *mappedData = 0;
        u64 texSize      = texVulk->mAllocation->GetSize();
        cmd              = Stage(texSize);
        mappedData       = cmd.ringAllocation->mappedData;

#if 0
        u32 numBlocks       = GetBlockSize(desc.mFormat);
        u32 numBlocksWidth  = Max(1, imageInfo.extent.width / numBlocks);
        u32 numBlocksHeight = Max(1, imageInfo.extent.height / numBlocks);

        u32 dstRowPitch = GetFormatSize(desc.mFormat) * numBlocksWidth;
        u32 srcRowPitch = GetFormatSize(desc.mFormat) * numBlocksWidth;

        u8 *dest = (u8 *)mappedData;
        u8 *src  = (u8 *)inData;

        for (u32 h = 0; h < numBlocksHeight; h++)
        {
            MemoryCopy(dest + h * dstRowPitch, src + h * srcRowPitch, dstRowPitch);
        }
#endif
        MemoryCopy(mappedData, inData, texSize);

        if (cmd.IsValid())
        {
            // Copy the contents of the staging buffer to the image
            VkBufferImageCopy imageCopy               = {};
            imageCopy.bufferOffset                    = cmd.ringAllocation->offset;
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

            RingAllocator *allocator = &stagingRingAllocators[cmd.ringAllocation->ringId];
            vkCmdCopyBufferToImage(cmd.mCmdBuffer, ToInternal(&allocator->transferRingBuffer)->mBuffer, texVulk->mImage,
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

            FenceVulkan *fenceVulkan  = ToInternal(&cmd.fence);
            outTexture->ticket.fence  = cmd.fence;
            outTexture->ticket.ticket = fenceVulkan->count;
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

        FenceVulkan *fenceVulkan  = ToInternal(&cmd.fence);
        outTexture->ticket.fence  = cmd.fence;
        outTexture->ticket.ticket = fenceVulkan->count;
        Submit(cmd);
    }

    if (desc.mUsage != MemoryUsage::GPU_TO_CPU)
    {
        CreateSubresource(outTexture);
    }
    Assert(outTexture->ticket.fence.internalState);
}

void mkGraphicsVulkan::DeleteTexture(Texture *texture)
{
    TextureVulkan *textureVulkan = ToInternal(texture);

    texture->internalState = 0;
    u32 currentBuffer      = GetCurrentBuffer();

    MutexScope(&mCleanupMutex)
    {
        if (textureVulkan->mImage != VK_NULL_HANDLE)
        {
            cleanupTextures[currentBuffer].emplace_back();
            CleanupTexture &cleanup = cleanupTextures[currentBuffer].back();
            cleanup.image           = textureVulkan->mImage;
            cleanup.allocation      = textureVulkan->mAllocation;

            BindlessDescriptorPool &pool = bindlessDescriptorPools[DescriptorType_SampledImage];

            if (textureVulkan->mSubresource.IsValid())
            {
                mCleanupImageViews[currentBuffer].push_back(textureVulkan->mSubresource.mImageView);
                pool.Free(textureVulkan->mSubresource.descriptorIndex);
            }

            for (auto &subresource : textureVulkan->mSubresources)
            {
                mCleanupImageViews[currentBuffer].push_back(subresource.mImageView);
                pool.Free(subresource.descriptorIndex);
            }
        }
        else if (textureVulkan->stagingBuffer != VK_NULL_HANDLE)
        {
            mCleanupBuffers[currentBuffer].emplace_back();
            CleanupBuffer &cleanup = mCleanupBuffers[currentBuffer].back();
            cleanup.mBuffer        = textureVulkan->stagingBuffer;
            cleanup.mAllocation    = textureVulkan->mAllocation;
        }
    }

    MutexScope(&mArenaMutex)
    {
        StackPush(freeTexture, textureVulkan);
    }
}

void mkGraphicsVulkan::CreateSampler(Sampler *sampler, SamplerDesc desc)
{
    SamplerVulkan *samplerVulk = 0;
    MutexScope(&mArenaMutex)
    {
        samplerVulk = freeSampler;
        if (samplerVulk)
        {
            StackPop(freeSampler);
        }
        else
        {
            samplerVulk = PushStruct(mArena, SamplerVulkan);
        }
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

    VK_CHECK(vkCreateSampler(mDevice, &samplerCreate, 0, &samplerVulk->mSampler));
}

void mkGraphicsVulkan::BindResource(GPUResource *resource, ResourceType type, u32 slot, CommandList cmd, i32 subresource)
{
    CommandListVulkan *command = ToInternal(cmd);
    Assert(command);
    Assert(slot < cMaxBindings);

    if (resource)
    {
        BindedResource *bindedResource = 0;
        switch (type)
        {
            case ResourceType::SRV: bindedResource = &command->srvTable[slot]; break;
            case ResourceType::UAV: bindedResource = &command->uavTable[slot]; break;
            default: Assert(0);
        }

        if (bindedResource->resource == 0 || bindedResource->resource != resource || bindedResource->subresourceIndex != subresource)
        {
            bindedResource->resource         = resource;
            bindedResource->subresourceIndex = subresource;
        }
    }
}

i32 mkGraphicsVulkan::GetDescriptorIndex(GPUResource *resource, ResourceType type, i32 subresourceIndex)
{
    i32 descriptorIndex = -1;
    if (resource)
    {
        switch (resource->mResourceType)
        {
            case GPUResource::ResourceType::Buffer:
            {
                GPUBufferVulkan *buffer = ToInternal((GPUBuffer *)resource);
                if (subresourceIndex != -1)
                {
                    descriptorIndex = buffer->subresources[subresourceIndex].descriptorIndex;
                }
                else
                {
                    switch (type)
                    {
                        case ResourceType::SRV:
                        {
                            Assert(buffer->subresourceSrv != -1);
                            descriptorIndex = buffer->subresources[buffer->subresourceSrv].descriptorIndex;
                        }
                        break;
                        case ResourceType::UAV:
                        {
                            Assert(buffer->subresourceUav != -1);
                            descriptorIndex = buffer->subresources[buffer->subresourceUav].descriptorIndex;
                        }
                        break;
                    }
                }
            }
            break;
            case GPUResource::ResourceType::Image:
            {
                TextureVulkan *texture = ToInternal((Texture *)resource);
                // TODO
                Assert(type == ResourceType::SRV);
                if (subresourceIndex != -1)
                {
                    descriptorIndex = texture->mSubresources[subresourceIndex].descriptorIndex;
                }
                else
                {
                    descriptorIndex = texture->mSubresource.descriptorIndex;
                }
            }
            break;
            default: Assert(0);
        }
    }
    return descriptorIndex;
}

// Only used to create bindless subresources
i32 mkGraphicsVulkan::CreateSubresource(GPUBuffer *buffer, ResourceType type, u64 offset, u64 size, Format format, const char *name)
{
    i32 subresourceIndex     = -1;
    GPUBufferVulkan *bufVulk = ToInternal(buffer);

    DescriptorType descriptorType;
    VkDescriptorType vkDescriptorType;
    if (format == Format::Null && (type == ResourceType::SRV || type == ResourceType::UAV))
    {
        descriptorType   = DescriptorType_StorageBuffer;
        vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if (format != Format::Null && type == ResourceType::SRV)
    {
        descriptorType   = DescriptorType_UniformTexel;
        vkDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    }
    else if (format != Format::Null && type == ResourceType::UAV)
    {
        descriptorType   = DescriptorType_StorageTexelBuffer;
        vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    }
    else
    {
        Assert(0);
    }

    GPUBufferVulkan::Subresource subresource;
    if (format != Format::Null)
    {
        VkBufferViewCreateInfo createView = {};
        createView.sType                  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        createView.buffer                 = bufVulk->mBuffer;
        createView.format                 = ConvertFormat(format);
        createView.offset                 = offset;
        createView.range                  = size;

        VK_CHECK(vkCreateBufferView(mDevice, &createView, 0, &subresource.view));
        if (name)
        {
            SetName((u64)subresource.view, VK_OBJECT_TYPE_BUFFER_VIEW, name);
        }
    }

    BindlessDescriptorPool &pool   = bindlessDescriptorPools[descriptorType];
    i32 subresourceDescriptorIndex = pool.Allocate();

    VkWriteDescriptorSet write = {};
    write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet               = pool.set;
    write.dstBinding           = 0;
    write.descriptorCount      = 1;
    write.dstArrayElement      = subresourceDescriptorIndex;
    write.descriptorType       = vkDescriptorType;

    subresource.info.buffer = bufVulk->mBuffer;
    subresource.info.offset = offset;
    subresource.info.range  = size;
    write.pBufferInfo       = &subresource.info;

    if (format != Format::Null)
    {
        write.pTexelBufferView = &subresource.view;
    }

    vkUpdateDescriptorSets(mDevice, 1, &write, 0, 0);

    subresource.descriptorIndex = subresourceDescriptorIndex;
    subresource.type            = descriptorType;
    bufVulk->subresources.push_back(subresource);
    i32 numSubresources = (i32)bufVulk->subresources.size();
    subresourceIndex    = numSubresources - 1;
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
    TextureVulkan::Subresource *subresource;
    if (baseLayer == 0 && numLayers == VK_REMAINING_ARRAY_LAYERS)
    {
        subresource             = &textureVulk->mSubresource;
        subresource->mBaseLayer = 0;
        subresource->mNumLayers = VK_REMAINING_ARRAY_LAYERS;
        VK_CHECK(vkCreateImageView(mDevice, &createInfo, 0, &subresource->mImageView));
    }
    else
    {
        textureVulk->mSubresources.emplace_back();
        subresource             = &textureVulk->mSubresources.back();
        subresource->mBaseLayer = baseLayer;
        subresource->mNumLayers = numLayers;

        VK_CHECK(vkCreateImageView(mDevice, &createInfo, 0, &subresource->mImageView));
        result = (i32)(textureVulk->mSubresources.size() - 1);
    }

    if (HasFlags(texture->mDesc.mInitialUsage, ResourceUsage::SampledImage) || HasFlags(texture->mDesc.mFutureUsages, ResourceUsage::SampledImage))
    {
        // Adds to the bindless combined image samplers array
        BindlessDescriptorPool &pool   = bindlessDescriptorPools[DescriptorType_SampledImage];
        i32 subresourceDescriptorIndex = pool.Allocate();
        subresource->descriptorIndex   = subresourceDescriptorIndex;
        VkDescriptorImageInfo info;

        info.imageView   = subresource->mImageView;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet               = pool.set;
        writeSet.dstBinding           = 0;
        writeSet.descriptorCount      = 1;
        writeSet.dstArrayElement      = subresourceDescriptorIndex;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writeSet.pImageInfo           = &info;

        vkUpdateDescriptorSets(mDevice, 1, &writeSet, 0, 0);
    }
    return result;
}

void mkGraphicsVulkan::UpdateDescriptorSet(CommandList cmd)
{
    CommandListVulkan *command = ToInternal(cmd);
    Assert(command);

    PipelineState *pipeline = command->currentPipeline;
    Assert(pipeline);
    PipelineStateVulkan *pipelineVulkan = ToInternal(pipeline);
    Assert(pipelineVulkan);

    list<VkWriteDescriptorSet> descriptorWrites;
    list<VkDescriptorBufferInfo> bufferInfos;
    list<VkDescriptorImageInfo> imageInfos;

    descriptorWrites.reserve(32);
    bufferInfos.reserve(32);
    imageInfos.reserve(32);

    // u32 currentSet = pipelineVulkan->currentSet++;
    u32 currentSet = command->currentSet++;
    VkDescriptorSet *descriptorSet;
    if (currentSet >= command->descriptorSets[GetCurrentBuffer()].size())
    {
        command->descriptorSets[GetCurrentBuffer()].emplace_back();
        descriptorSet = &command->descriptorSets[GetCurrentBuffer()].back();

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = mPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = pipelineVulkan->mDescriptorSetLayouts.data();
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo, descriptorSet));
    }
    else
    {
        descriptorSet = &command->descriptorSets[GetCurrentBuffer()][currentSet];
    }

    for (auto &layoutBinding : pipelineVulkan->mLayoutBindings)
    {
        if (layoutBinding.pImmutableSamplers != 0)
        {
            continue;
        }
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
                    u32 mappedBinding = layoutBinding.binding;
                    Assert(mappedBinding < cMaxBindings);
                    BindedResource *bindedResource = &command->srvTable[mappedBinding];
                    GPUResource *resource          = bindedResource->resource;

                    bufferInfos.emplace_back();
                    if (!bindedResource->IsValid() || !resource->IsBuffer())
                    {
                        VkDescriptorBufferInfo &info = bufferInfos.back();
                        info.buffer                  = mNullBuffer;
                        info.offset                  = 0;
                        info.range                   = VK_WHOLE_SIZE;
                    }
                    else
                    {
                        GPUBufferVulkan *bufferVulkan = ToInternal((GPUBuffer *)resource);
                        i32 subresourceIndex          = bindedResource->subresourceIndex == -1 ? bufferVulkan->subresourceSrv : bindedResource->subresourceIndex;
                        Assert(subresourceIndex != -1);
                        bufferInfos.back() = bufferVulkan->subresources[subresourceIndex].info;
                    }

                    descriptorWrite.pBufferInfo = &bufferInfos.back();
                }
                break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                {
                    u32 mappedBinding = layoutBinding.binding - VK_BINDING_SHIFT_T;
                    Assert(mappedBinding < cMaxBindings);

                    BindedResource *bindedResource = &command->srvTable[layoutBinding.binding - VK_BINDING_SHIFT_T];
                    GPUResource *resource          = bindedResource->resource;

                    VkImageView view;
                    if (!bindedResource->IsValid() || !resource->IsTexture())
                    {
                        view = mNullImageView2D;
                    }
                    else
                    {
                        Texture *tex           = (Texture *)(resource);
                        TextureVulkan *texture = ToInternal(tex);
                        view                   = texture->mSubresource.mImageView;
                    }
                    imageInfos.emplace_back();
                    VkDescriptorImageInfo &imageInfo = imageInfos.back();
                    imageInfo.imageView              = view;
                    imageInfo.imageLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    descriptorWrite.pImageInfo = &imageInfo;
                }
                break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                {
                    u32 mappedBinding = layoutBinding.binding - VK_BINDING_SHIFT_U;
                    Assert(mappedBinding < cMaxBindings);

                    BindedResource *bindedResource = &command->uavTable[layoutBinding.binding - VK_BINDING_SHIFT_U];
                    GPUResource *resource          = bindedResource->resource;
                    VkImageView view;
                    if (!bindedResource->IsValid() || !resource->IsTexture())
                    {
                        view = mNullImageView2D;
                    }
                    else
                    {
                        Texture *tex           = (Texture *)(resource);
                        TextureVulkan *texture = ToInternal(tex);
                        view                   = texture->mSubresource.mImageView;
                    }
                    imageInfos.emplace_back();
                    VkDescriptorImageInfo &imageInfo = imageInfos.back();
                    imageInfo.imageView              = view;
                    imageInfo.imageLayout            = VK_IMAGE_LAYOUT_GENERAL;

                    descriptorWrite.pImageInfo = &imageInfo;
                }
                break;
                default:
                    Assert(!"Not implemented");
            }
        }
    }
    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (command->type == QueueType_Compute)
    {
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    }
    vkUpdateDescriptorSets(mDevice, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, 0);
    vkCmdBindDescriptorSets(command->GetCommandBuffer(), bindPoint, pipelineVulkan->mPipelineLayout, 0, 1, descriptorSet, 0, 0);
}

void mkGraphicsVulkan::FrameAllocate(GPUBuffer *inBuf, void *inData, CommandList cmd, u64 inSize, u64 inOffset)
{
    CommandListVulkan *command  = ToInternal(cmd);
    FrameData *currentFrameData = &mFrameAllocator[GetCurrentBuffer()];

    GPUBufferVulkan *bufVulk = ToInternal(inBuf);
    // Is power of 2
    Assert(IsPow2(currentFrameData->mAlignment));

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

// NOTE: loops through 4 rings until one with space for the allocation is found.
mkGraphicsVulkan::RingAllocation *mkGraphicsVulkan::RingAlloc(u64 size)
{
    RingAllocation *result = 0;
    const u32 arrayLength  = ArrayLength(stagingRingAllocators);
    u32 id                 = 0;
    while (result == 0)
    {
        id     = id & (arrayLength - 1);
        result = RingAllocInternal(id, size);
        id++;
    }
    return result;
}

// TODO: there is a memory bug!!!! YAY!
mkGraphicsVulkan::RingAllocation *mkGraphicsVulkan::RingAllocInternal(u32 id, u64 size)
{
    RingAllocator *allocator = &stagingRingAllocators[id];
    u64 ringBufferSize       = allocator->ringBufferSize;

    size = AlignPow2(size, (u64)allocator->alignment);
    Assert(size <= ringBufferSize);
    Assert(allocator->writePos <= ringBufferSize);
    Assert(allocator->readPos <= ringBufferSize);

    RingAllocation *result = 0;
    i32 offset             = -1;

    TicketMutexScope(&allocator->lock);
    {
        u32 writePos = allocator->writePos;
        u32 readPos  = allocator->readPos;
        if (writePos >= readPos)
        {
            // Normal default case: enough space for allocation b/t writePos and end of buffer
            if (ringBufferSize - writePos >= size)
            {
                offset = writePos;
                allocator->writePos += (u32)size;
            }
            // Not enough space, need to go back to the beginning of the buffer
            else if (ringBufferSize - writePos < size)
            {
                if (readPos >= size)
                {
                    offset              = 0;
                    allocator->writePos = (u32)size;
                }
            }
        }
        else
        {
            // Normal default case: enough space for allocation b/t readPos
            if (readPos - writePos >= size)
            {
                offset = writePos;
                allocator->writePos += (u32)size;
            }
        }

        if (offset != -1)
        {
            RingAllocation allocation;
            allocation.size       = size;
            allocation.offset     = (u32)offset;
            allocation.mappedData = (u8 *)allocator->transferRingBuffer.mMappedData + offset;
            allocation.ringId     = id;
            allocation.freed      = 0;

            u32 length = ArrayLength(allocator->allocations);
            Assert(length - (allocator->allocationWritePos - allocator->allocationReadPos) >= 1);
            u32 allocationWritePos                     = allocator->allocationWritePos++ & (length - 1);
            allocator->allocations[allocationWritePos] = allocation;
            result                                     = &allocator->allocations[allocationWritePos];
        }
    }

    return result;
}

void mkGraphicsVulkan::RingFree(RingAllocation *allocation)
{
    RingAllocator *allocator = &stagingRingAllocators[allocation->ringId];
    TicketMutexScope(&allocator->lock)
    {
        allocation->freed = 1;

        while (allocator->allocationReadPos != allocator->allocationWritePos)
        {
            u32 allocationReadPos                     = allocator->allocationReadPos & (ArrayLength(allocator->allocations) - 1);
            RingAllocation *potentiallyFreeAllocation = &allocator->allocations[allocationReadPos];

            if (potentiallyFreeAllocation == 0 || !potentiallyFreeAllocation->freed) break;
            allocator->readPos = potentiallyFreeAllocation->offset + (u32)potentiallyFreeAllocation->size;
            allocator->allocationReadPos++;
        }
    }
}

mkGraphicsVulkan::TransferCommand mkGraphicsVulkan::Stage(u64 size)
{
    BeginMutex(&mTransferMutex);

    TransferCommand cmd;
    for (u32 i = 0; i < (u32)mTransferFreeList.size(); i++)
    {
        // Submission is done, can reuse cmd pool
        TransferCommand &testCmd = mTransferFreeList[i];
        if (vkGetFenceStatus(mDevice, ToInternal(&mTransferFreeList[i].fence)->fence) == VK_SUCCESS)
        {
            FenceVulkan *fenceVulkan = ToInternal(&mTransferFreeList[i].fence);
            fenceVulkan->count++;
            std::atomic_thread_fence(std::memory_order_release);
            // Only some cmds will have ring allocations
            if (testCmd.ringAllocation)
            {
                RingFree(testCmd.ringAllocation);
            }
            cmd                = testCmd;
            cmd.ringAllocation = 0;

            mTransferFreeList[i] = mTransferFreeList[mTransferFreeList.size() - 1];
            mTransferFreeList.pop_back();
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
        VK_CHECK(vkCreateCommandPool(mDevice, &poolInfo, 0, &cmd.mCmdPool));
        poolInfo.queueFamilyIndex = mGraphicsFamily;
        VK_CHECK(vkCreateCommandPool(mDevice, &poolInfo, 0, &cmd.mTransitionPool));

        VkCommandBufferAllocateInfo bufferInfo = {};
        bufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandPool                 = cmd.mCmdPool;
        bufferInfo.commandBufferCount          = 1;
        bufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VK_CHECK(vkAllocateCommandBuffers(mDevice, &bufferInfo, &cmd.mCmdBuffer));
        bufferInfo.commandPool = cmd.mTransitionPool;
        VK_CHECK(vkAllocateCommandBuffers(mDevice, &bufferInfo, &cmd.mTransitionBuffer));

        FenceVulkan *fenceVulkan = 0;
        MutexScope(&mArenaMutex)
        {
            fenceVulkan = freeFence;
            if (fenceVulkan)
            {
                StackPop(freeFence);
            }
            else
            {
                fenceVulkan = PushStruct(mArena, FenceVulkan);
            }
        }
        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(mDevice, &fenceInfo, 0, &fenceVulkan->fence));
        cmd.fence.internalState = fenceVulkan;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (u32 i = 0; i < ArrayLength(cmd.mSemaphores); i++)
        {
            VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &cmd.mSemaphores[i]));
        }

        cmd.ringAllocation = 0;
    }

    VK_CHECK(vkResetCommandPool(mDevice, cmd.mCmdPool, 0));
    VK_CHECK(vkResetCommandPool(mDevice, cmd.mTransitionPool, 0));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = 0;
    VK_CHECK(vkBeginCommandBuffer(cmd.mCmdBuffer, &beginInfo));
    VK_CHECK(vkBeginCommandBuffer(cmd.mTransitionBuffer, &beginInfo));

    FenceVulkan *fenceVulkan = ToInternal(&cmd.fence);
    VK_CHECK(vkResetFences(mDevice, 1, &fenceVulkan->fence));

    if (size != 0)
    {
        cmd.ringAllocation = RingAlloc(size);
    }

    return cmd;
}

void mkGraphicsVulkan::Submit(TransferCommand cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd.mCmdBuffer));
    VK_CHECK(vkEndCommandBuffer(cmd.mTransitionBuffer));

    VkCommandBufferSubmitInfo bufSubmitInfo = {};
    bufSubmitInfo.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    VkSemaphoreSubmitInfo waitSemInfo = {};
    waitSemInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

    VkSemaphoreSubmitInfo submitSemInfo = {};
    submitSemInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    // Submit the copy command to the transfer queue.
    {
        bufSubmitInfo.commandBuffer = cmd.mCmdBuffer;

        submitSemInfo.semaphore = cmd.mSemaphores[0];
        submitSemInfo.value     = 0;
        submitSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos    = &bufSubmitInfo;

        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos    = &submitSemInfo;

        MutexScope(&mQueues[QueueType_Copy].mLock)
        {
            VK_CHECK(vkQueueSubmit2(mQueues[QueueType_Copy].mQueue, 1, &submitInfo, VK_NULL_HANDLE));
        }
    }
    // Insert the execution dependency (semaphores) and memory dependency (barrier) on the graphics queue
    {
        bufSubmitInfo.commandBuffer = cmd.mTransitionBuffer;

        waitSemInfo.semaphore = cmd.mSemaphores[0];
        waitSemInfo.value     = 0;
        waitSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitSemInfo.semaphore = cmd.mSemaphores[1];
        submitSemInfo.value     = 0;
        submitSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitInfo.commandBufferInfoCount   = 1;
        submitInfo.pCommandBufferInfos      = &bufSubmitInfo;
        submitInfo.waitSemaphoreInfoCount   = 1;
        submitInfo.pWaitSemaphoreInfos      = &waitSemInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos    = &submitSemInfo;

        MutexScope(&mQueues[QueueType_Graphics].mLock)
        {
            VK_CHECK(vkQueueSubmit2(mQueues[QueueType_Graphics].mQueue, 1, &submitInfo, VK_NULL_HANDLE));
        }
    }
    // Execution dependency on compute queue
    {
        waitSemInfo.semaphore = cmd.mSemaphores[1];
        waitSemInfo.value     = 0;
        waitSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        submitInfo.commandBufferInfoCount   = 0;
        submitInfo.pCommandBufferInfos      = 0;
        submitInfo.waitSemaphoreInfoCount   = 1;
        submitInfo.pWaitSemaphoreInfos      = &waitSemInfo;
        submitInfo.signalSemaphoreInfoCount = 0;
        submitInfo.pSignalSemaphoreInfos    = 0;

        MutexScope(&mQueues[QueueType_Compute].mLock)
        {
            VK_CHECK(vkQueueSubmit2(mQueues[QueueType_Compute].mQueue, 1, &submitInfo, ToInternal(&cmd.fence)->fence));
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
    u32 currentBuffer     = GetCurrentBuffer();

    MutexScope(&mCleanupMutex)
    {
        mCleanupBuffers[currentBuffer].emplace_back();
        CleanupBuffer &cleanup = mCleanupBuffers[currentBuffer].back();
        cleanup.mBuffer        = bufferVulkan->mBuffer;
        cleanup.mAllocation    = bufferVulkan->mAllocation;

        for (auto &subresource : bufferVulkan->subresources)
        {
            BindlessDescriptorPool &pool = bindlessDescriptorPools[subresource.type];
            if (subresource.view != VK_NULL_HANDLE)
            {
                cleanupBufferViews[currentBuffer].push_back(subresource.view);
            }
            if (subresource.IsBindless())
            {
                pool.Free(subresource.descriptorIndex);
            }
        }
    }

    MutexScope(&mArenaMutex)
    {
        StackPush(freeBuffer, bufferVulkan);
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

void mkGraphicsVulkan::CopyTexture(CommandList cmd, Texture *dst, Texture *src, Rect3U32 *rect)
{
    CommandListVulkan *command = ToInternal(cmd);

    TextureVulkan *dstVulkan = ToInternal(dst);
    TextureVulkan *srcVulkan = ToInternal(src);

    u32 width, height, depth, offsetX, offsetY, offsetZ;
    if (rect)
    {
        offsetX = rect->minX;
        offsetY = rect->minY;
        offsetZ = rect->minZ;

        width  = rect->maxX - rect->minX;
        height = rect->maxY - rect->minY;
        depth  = rect->maxZ - rect->minZ;
    }
    else
    {
        offsetX = offsetY = offsetZ = 0;
        width                       = Min(src->mDesc.mWidth, dst->mDesc.mWidth);
        height                      = Min(src->mDesc.mHeight, dst->mDesc.mHeight);
        depth                       = Min(src->mDesc.mDepth, dst->mDesc.mDepth);
    }

    if (dst->mDesc.mUsage == MemoryUsage::GPU_TO_CPU)
    {
        VkBufferImageCopy copy               = {};
        copy.bufferOffset                    = 0;
        copy.imageOffset.x                   = offsetX;
        copy.imageOffset.y                   = offsetY;
        copy.imageOffset.z                   = offsetZ;
        copy.imageExtent.width               = width;
        copy.imageExtent.height              = height;
        copy.imageExtent.depth               = depth;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount     = 1;
        copy.imageSubresource.mipLevel       = 0;
        copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

        Assert(dst->mDesc.mNumMips == 1 && dst->mDesc.mNumLayers == 1 && dst->mDesc.mDepth == 1);

        vkCmdCopyImageToBuffer(command->GetCommandBuffer(), srcVulkan->mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstVulkan->stagingBuffer, 1, &copy);
    }
    else
    {

        VkImageSubresourceLayers srcLayer = {};
        srcLayer.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
        srcLayer.mipLevel                 = 0;
        srcLayer.baseArrayLayer           = 0;
        srcLayer.layerCount               = 1;

        VkImageSubresourceLayers dstLayer = {};
        dstLayer.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
        dstLayer.mipLevel                 = 0;
        dstLayer.baseArrayLayer           = 0;
        dstLayer.layerCount               = 1;

        VkImageCopy2 imageCopyInfo   = {};
        imageCopyInfo.sType          = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
        imageCopyInfo.srcSubresource = srcLayer;
        imageCopyInfo.dstSubresource = dstLayer;
        imageCopyInfo.srcOffset.x    = 0;
        imageCopyInfo.srcOffset.y    = 0;
        imageCopyInfo.srcOffset.z    = 0;
        imageCopyInfo.dstOffset.x    = 0;
        imageCopyInfo.dstOffset.y    = 0;
        imageCopyInfo.dstOffset.z    = 0;
        imageCopyInfo.extent.width   = Min(dst->mDesc.mWidth, src->mDesc.mWidth);
        imageCopyInfo.extent.height  = Min(dst->mDesc.mHeight, src->mDesc.mHeight);
        imageCopyInfo.extent.depth   = Min(dst->mDesc.mDepth, src->mDesc.mDepth);

        VkCopyImageInfo2 copyInfo = {};
        copyInfo.sType            = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
        copyInfo.srcImage         = srcVulkan->mImage;
        copyInfo.dstImage         = dstVulkan->mImage;
        copyInfo.srcImageLayout   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyInfo.dstImageLayout   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyInfo.regionCount      = 1;
        copyInfo.pRegions         = &imageCopyInfo;

        vkCmdCopyImage2(command->GetCommandBuffer(), &copyInfo);
    }
}

// So from my understanding, the command list contains buffer * queue_type command pools, each with 1
// command buffer. If the command buffer isn't initialize, the pool/command buffer/semaphore is created for it.
CommandList mkGraphicsVulkan::BeginCommandList(QueueType queue)
{
    BeginTicketMutex(&mCommandMutex);
    u32 currentCmd;
    CommandList cmd;
    currentCmd = numCommandLists++;
    if (currentCmd >= commandLists.size())
    {
        MutexScope(&mArenaMutex)
        {
            CommandListVulkan *cmdVulkan = PushStruct(mArena, CommandListVulkan);
            commandLists.push_back(cmdVulkan);
        }
    }
    cmd.internalState = commandLists[currentCmd];
    EndTicketMutex(&mCommandMutex);

    CommandListVulkan &command = GetCommandList(cmd);
    command.currentBuffer      = GetCurrentBuffer();
    command.type               = queue;
    command.waitedOn.store(0);
    command.waitForCmds.clear();

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

            VK_CHECK(vkCreateCommandPool(mDevice, &poolInfo, 0, &command.commandPools[buffer]));

            VkCommandBufferAllocateInfo bufferInfo = {};
            bufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            bufferInfo.commandPool                 = command.commandPools[buffer];
            bufferInfo.commandBufferCount          = 1;
            bufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            VK_CHECK(vkAllocateCommandBuffers(mDevice, &bufferInfo, &command.commandBuffers[buffer]));

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreInfo, 0, &command.semaphore));
        }
    } // namespace graphics

    // Reset command pool
    VK_CHECK(vkResetCommandPool(mDevice, command.GetCommandPool(), 0));

    // Start command buffer recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = 0; // for secondary command buffers
    VK_CHECK(vkBeginCommandBuffer(command.GetCommandBuffer(), &beginInfo));

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

    commandList->endPassImageMemoryBarriers.push_back(barrier);
    commandList->updateSwapchains.push_back(*inSwapchain);
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
            command->endPassImageMemoryBarriers.emplace_back();

            VkImageMemoryBarrier2 &barrier      = command->endPassImageMemoryBarriers.back();
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

void mkGraphicsVulkan::Dispatch(CommandList cmd, u32 groupCountX, u32 groupCountY, u32 groupCountZ)
{
    CommandListVulkan *commandList = ToInternal(cmd);
    Assert(commandList);
    vkCmdDispatch(commandList->GetCommandBuffer(), groupCountX, groupCountY, groupCountZ);
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
    if (!commandList->endPassImageMemoryBarriers.empty())
    {
        VkDependencyInfo dependencyInfo        = {};
        dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = (u32)commandList->endPassImageMemoryBarriers.size();
        dependencyInfo.pImageMemoryBarriers    = commandList->endPassImageMemoryBarriers.data();
        vkCmdPipelineBarrier2(commandList->GetCommandBuffer(), &dependencyInfo);

        commandList->endPassImageMemoryBarriers.clear();
    }
}

void mkGraphicsVulkan::SubmitCommandLists()
{
    VkResult res;
    list<VkCommandBufferSubmitInfo> bufferSubmitInfo[QueueType_Count - 1];
    list<VkSemaphoreSubmitInfo> waitSemaphores[QueueType_Count - 1];
    list<VkSemaphoreSubmitInfo> signalSemaphores[QueueType_Count - 1];

    // Passed to vkQueuePresent
    list<VkSemaphore> submitSemaphores; // signaled when cmd list is submitted to queue
    list<Swapchain *> previousSwapchains;
    list<VkSwapchainKHR> presentSwapchains; // swapchains to present
    list<u32> swapchainImageIndices;        // swapchain image to present

    {
        auto submitQueue = [&](QueueType type, VkFence fence) {
            VkSubmitInfo2 submitInfo            = {};
            submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submitInfo.waitSemaphoreInfoCount   = (u32)waitSemaphores[type].size();
            submitInfo.pWaitSemaphoreInfos      = waitSemaphores[type].data();
            submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphores[type].size();
            submitInfo.pSignalSemaphoreInfos    = signalSemaphores[type].data();
            submitInfo.commandBufferInfoCount   = (u32)bufferSubmitInfo[type].size();
            submitInfo.pCommandBufferInfos      = bufferSubmitInfo[type].data();

            MutexScope(&mQueues[type].mLock)
            {
                vkQueueSubmit2(mQueues[type].mQueue, 1, &submitInfo, fence);
                if (!presentSwapchains.empty())
                {
                    VkPresentInfoKHR presentInfo   = {};
                    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                    presentInfo.waitSemaphoreCount = (u32)submitSemaphores.size();
                    presentInfo.pWaitSemaphores    = submitSemaphores.data();
                    presentInfo.swapchainCount     = (u32)presentSwapchains.size();
                    presentInfo.pSwapchains        = presentSwapchains.data();
                    presentInfo.pImageIndices      = swapchainImageIndices.data();
                    res                            = vkQueuePresentKHR(mQueues[QueueType_Graphics].mQueue, &presentInfo);

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
            }

            bufferSubmitInfo[type].clear();
            waitSemaphores[type].clear();
            signalSemaphores[type].clear();

            submitSemaphores.clear();
            previousSwapchains.clear();
            swapchainImageIndices.clear();
            presentSwapchains.clear();
        };

        for (u32 i = 0; i < numCommandLists; i++)
        {
            CommandListVulkan *commandList = commandLists[i];
            vkEndCommandBuffer(commandList->GetCommandBuffer());
            QueueType type = commandList->type;

            bufferSubmitInfo[type].emplace_back();
            VkCommandBufferSubmitInfo &bufferInfo = bufferSubmitInfo[type].back();
            bufferInfo.sType                      = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            bufferInfo.commandBuffer              = commandList->GetCommandBuffer();

            // TODO: I'm not sure if this is ever more than one. also, when this code is extended to multiple queues,
            // compute/transfer don't have these
            for (auto &sc : commandList->updateSwapchains)
            {
                SwapchainVulkan *swapchain = ToInternal(&sc);
                previousSwapchains.push_back(&sc);

                VkSemaphoreSubmitInfo waitSemaphore = {};
                waitSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                waitSemaphore.semaphore             = swapchain->mAcquireSemaphores[swapchain->mAcquireSemaphoreIndex];
                waitSemaphore.value                 = 0;
                waitSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                waitSemaphores[type].push_back(waitSemaphore);

                VkSemaphoreSubmitInfo signalSemaphore = {};
                signalSemaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                signalSemaphore.semaphore             = swapchain->mReleaseSemaphore;
                signalSemaphore.value                 = 0;
                signalSemaphore.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                signalSemaphores[type].push_back(signalSemaphore);

                submitSemaphores.push_back(swapchain->mReleaseSemaphore);

                presentSwapchains.push_back(swapchain->mSwapchain);
                swapchainImageIndices.push_back(swapchain->mImageIndex);
            }
            // Command lists to wait for before execution
            if (!commandList->waitForCmds.empty() || commandList->waitedOn.load())
            {
                for (auto &cmd : commandList->waitForCmds)
                {
                    CommandListVulkan *commandListVulkan = ToInternal(cmd);
                    waitSemaphores[type].emplace_back();
                    VkSemaphoreSubmitInfo &waitSemaphore = waitSemaphores[type].back();
                    waitSemaphore.sType                  = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    waitSemaphore.semaphore              = commandListVulkan->semaphore;
                    waitSemaphore.value                  = 0;
                    waitSemaphore.stageMask              = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                }
                if (commandList->waitedOn.load())
                {
                    signalSemaphores[type].emplace_back();
                    VkSemaphoreSubmitInfo &signalSemaphore = signalSemaphores[type].back();
                    signalSemaphore.sType                  = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    signalSemaphore.semaphore              = commandList->semaphore;
                    signalSemaphore.value                  = 0;
                    signalSemaphore.stageMask              = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                }
                submitQueue(type, VK_NULL_HANDLE);
            }
            commandList->updateSwapchains.clear();
            commandList->currentSet = 0;
        }
        submitQueue(QueueType_Graphics, mFrameFences[GetCurrentBuffer()][QueueType_Graphics]);
        submitQueue(QueueType_Compute, mFrameFences[GetCurrentBuffer()][QueueType_Compute]);
    }

    // Wait for the queue submission of the previous frame to resolve before continuing.
    {
        // Changes GetCurrentBuffer()
        numCommandLists = 0;
        mFrameCount++;
        // Waits for previous previous frame
        if (mFrameCount >= cNumBuffers)
        {
            u32 currentBuffer = GetCurrentBuffer();
            for (u32 type = 0; type < QueueType_Count; type++)
            {
                if (type == QueueType_Copy) continue;
                if (mFrameFences[currentBuffer][type] == VK_NULL_HANDLE) continue;
                VK_CHECK(vkWaitForFences(mDevice, 1, &mFrameFences[currentBuffer][type], VK_TRUE, UINT64_MAX));
                VK_CHECK(vkResetFences(mDevice, 1, &mFrameFences[currentBuffer][type]));
            }
        }
        mFrameAllocator[GetCurrentBuffer()].mOffset.store(0);
    }
    Cleanup();
}

void mkGraphicsVulkan::PushConstants(CommandList cmd, u32 size, void *data, u32 offset)
{
    CommandListVulkan *command = ToInternal(cmd);
    if (command->currentPipeline)
    {
        PipelineStateVulkan *pipeline = ToInternal(command->currentPipeline);
        Assert(pipeline->mPushConstantRange.size > 0);

        vkCmdPushConstants(command->GetCommandBuffer(), pipeline->mPipelineLayout,
                           pipeline->mPushConstantRange.stageFlags, offset, size, data);
    }
}

void mkGraphicsVulkan::BindPipeline(PipelineState *ps, CommandList cmd)
{
    CommandListVulkan *command    = ToInternal(cmd);
    command->currentPipeline      = ps;
    PipelineStateVulkan *psVulkan = ToInternal(ps);
    vkCmdBindPipeline(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, psVulkan->mPipeline);
    vkCmdBindDescriptorSets(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, psVulkan->mPipelineLayout,
                            1, (u32)bindlessDescriptorSets.size(), bindlessDescriptorSets.data(), 0, 0);
}

void mkGraphicsVulkan::BindCompute(PipelineState *ps, CommandList cmd)
{
    Assert(ps->mDesc.compute);
    CommandListVulkan *command          = ToInternal(cmd);
    command->currentPipeline            = ps;
    PipelineStateVulkan *pipelineVulkan = ToInternal(ps);
    vkCmdBindPipeline(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineVulkan->mPipeline);
    vkCmdBindDescriptorSets(command->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineVulkan->mPipelineLayout,
                            1, (u32)bindlessDescriptorSets.size(), bindlessDescriptorSets.data(), 0, 0);
}

b32 mkGraphicsVulkan::IsSignaled(FenceTicket ticket)
{
    FenceVulkan *fenceVulkan = ToInternal(&ticket.fence);
    Assert(fenceVulkan->count < 100000);
    b32 result = ticket.ticket < fenceVulkan->count || vkGetFenceStatus(mDevice, fenceVulkan->fence) == VK_SUCCESS;
    return result;
}

b32 mkGraphicsVulkan::IsLoaded(GPUResource *resource)
{
    FenceTicket ticket = resource->ticket;
    return IsSignaled(ticket);
}

void mkGraphicsVulkan::WaitForGPU()
{
    VK_CHECK(vkDeviceWaitIdle(mDevice));
}

// TODO: maybe explore render graphs in the future
void mkGraphicsVulkan::Wait(CommandList waitFor, CommandList cmd)
{
    CommandListVulkan *waitForCmd = ToInternal(waitFor);
    CommandListVulkan *command    = ToInternal(cmd);

    waitForCmd->waitedOn.store(1);
    command->waitForCmds.push_back(waitFor);
}

void mkGraphicsVulkan::CreateQueryPool(QueryPool *pool, QueryType type, u32 queryCount)
{
    pool->type                       = type;
    pool->queryCount                 = queryCount;
    VkQueryPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    createInfo.queryCount            = queryCount;
    switch (type)
    {
        case QueryType_PipelineStatistics:
        {
            createInfo.queryType          = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            createInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
        }
        break;
        case QueryType_Timestamp: createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP; break;
        case QueryType_Occlusion: createInfo.queryType = VK_QUERY_TYPE_OCCLUSION; break;
        default: Assert(0);
    }

    VkQueryPool vkQueryPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateQueryPool(mDevice, &createInfo, 0, &vkQueryPool));

    // TODO: free list?
    pool->internalState = (void *)vkQueryPool;
}

void mkGraphicsVulkan::BeginQuery(QueryPool *pool, CommandList cmd, u32 queryIndex)
{
    CommandListVulkan *commandVulkan = ToInternal(cmd);
    VkQueryPool queryPoolVulkan      = ToInternal(pool);
    switch (pool->type)
    {
        case QueryType_PipelineStatistics: vkCmdBeginQuery(commandVulkan->GetCommandBuffer(), queryPoolVulkan, queryIndex, 0); break;
        case QueryType_Timestamp: break;
        case QueryType_Occlusion: vkCmdBeginQuery(commandVulkan->GetCommandBuffer(), queryPoolVulkan, queryIndex, VK_QUERY_CONTROL_PRECISE_BIT); break;
    }
}

void mkGraphicsVulkan::EndQuery(QueryPool *pool, CommandList cmd, u32 queryIndex)
{
    CommandListVulkan *commandVulkan = ToInternal(cmd);
    VkQueryPool queryPoolVulkan      = ToInternal(pool);
    switch (pool->type)
    {
        case QueryType_Occlusion:
        case QueryType_PipelineStatistics: vkCmdEndQuery(commandVulkan->GetCommandBuffer(), queryPoolVulkan, queryIndex); break;
        case QueryType_Timestamp: vkCmdWriteTimestamp2(commandVulkan->GetCommandBuffer(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queryPoolVulkan, queryIndex); break;
    }
}

void mkGraphicsVulkan::ResolveQuery(QueryPool *pool, CommandList cmd, GPUBuffer *buffer, u32 queryIndex, u32 count, u32 destOffset)
{
    CommandListVulkan *commandVulkan = ToInternal(cmd);
    VkQueryPool queryPoolVulkan      = ToInternal(pool);
    GPUBufferVulkan *bufferVulkan    = ToInternal(buffer);

    VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
    vkCmdCopyQueryPoolResults(commandVulkan->GetCommandBuffer(), queryPoolVulkan, queryIndex, count, bufferVulkan->mBuffer,
                              destOffset, sizeof(u64), flags);
    // switch (pool->type)
    // {
    //     case QueryType_Timestamp:
    //     {
    //         vkGetQueryPoolResults(mDevice, queryPoolVulkan, 0, );
    //     };
    //     break;
    // }
}

void mkGraphicsVulkan::ResetQuery(QueryPool *pool, CommandList cmd, u32 index, u32 count)
{
    CommandListVulkan *commandVulkan = ToInternal(cmd);
    VkQueryPool queryPoolVulkan      = ToInternal(pool);
    vkCmdResetQueryPool(commandVulkan->GetCommandBuffer(), queryPoolVulkan, index, count);
}

// void mkGraphicsVulkan::ResolveQuery(QueryPool *pool, )
u32 mkGraphicsVulkan::GetCount(Fence f)
{
    FenceVulkan *fenceVulkan = ToInternal(&f);
    u32 count                = fenceVulkan->count;
    return count;
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
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(mDevice, &info));
}

void mkGraphicsVulkan::SetName(GPUResource *resource, string name)
{
    SetName(resource, (char *)name.str);
}

void mkGraphicsVulkan::SetName(u64 handle, VkObjectType type, const char *name)
{
    if (!mDebugUtils || handle == 0)
    {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.pObjectName                   = name;
    info.objectType                    = type;
    info.objectHandle                  = handle;
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(mDevice, &info));
}

void mkGraphicsVulkan::SetName(VkDescriptorSetLayout handle, const char *name)
{
    SetName((u64)handle, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, name);
}

void mkGraphicsVulkan::SetName(VkDescriptorSet handle, const char *name)
{
    SetName((u64)handle, VK_OBJECT_TYPE_DESCRIPTOR_SET, name);
}

void mkGraphicsVulkan::SetName(VkShaderModule handle, const char *name)
{
    SetName((u64)handle, VK_OBJECT_TYPE_SHADER_MODULE, name);
}

void mkGraphicsVulkan::SetName(VkPipeline handle, const char *name)
{
    SetName((u64)handle, VK_OBJECT_TYPE_PIPELINE, name);
}

void mkGraphicsVulkan::SetName(VkQueue handle, const char *name)
{
    SetName((u64)handle, VK_OBJECT_TYPE_QUEUE, name);
}

// void mkGraphicsVulkan::BeginEvent()
// {
//     VkDebugUtilsLabelEXT debugLabel;
//     debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
// }

} // namespace graphics
