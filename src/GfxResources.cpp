// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "GfxResources.h"

#include "Utils.h"
#include "Window.h"

#include <assert.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

#define DEF_PRINT_DEVICE_PROPERTIES 1
#ifdef _DEBUG
#define DEF_USE_DEBUG_VALIDATION 1
#endif

namespace core
{

#if (DEF_USE_DEBUG_VALIDATION == 1)

static VkBool32 VKAPI_PTR debugCallback(
    VkDebugReportFlagsEXT       /*flags*/,
    VkDebugReportObjectTypeEXT  /*objectType*/,
    uint64_t                    /*object*/,
    size_t                      /*location*/,
    int32_t                     /*messageCode*/,
    const char*                 /*pLayerPrefix*/,
    const char*                 pMessage,
    void*                       /*pUserData*/)
{
    std::cerr << "debug validation: " << pMessage << std::endl;
    assert(false);
    return VK_FALSE;
}

PFN_vkCreateDebugReportCallbackEXT	fvkCreateDebugReportCallbackEXT = nullptr;
PFN_vkDestroyDebugReportCallbackEXT	fvkDestroyDebugReportCallbackEXT = nullptr;

#endif

uint32_t getPhysicalDeviceMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
    const VkMemoryRequirements& memoryRequirements,
    const VkMemoryPropertyFlags memoryPropertyFlags)
{
    uint32_t memTypeIndex = ~0u;
    for (uint32_t idx = 0; idx < physicalDeviceMemoryProperties.memoryTypeCount; ++idx)
    {
        if ((memoryRequirements.memoryTypeBits & (1 << idx))
            && ((physicalDeviceMemoryProperties.memoryTypes[idx].propertyFlags & memoryPropertyFlags)
                == memoryPropertyFlags))
        {
            memTypeIndex = idx;
        }
    }
    assert(memTypeIndex != ~0u); // requested memory not found
    return memTypeIndex;
}

uint32_t getAlignedByteSize(
    const uint32_t byteSize,
    const uint32_t alignment)
{
    return (byteSize + alignment - 1) & (~(alignment - 1));
}

static VkPresentModeKHR getPresentMode(
    const std::vector<VkPresentModeKHR>& preferred,
    const std::vector<VkPresentModeKHR>& available,
    const VkPresentModeKHR defaultVal)
{
    for (const auto& preferredRef : preferred)
    {
        for (const auto& availableRef : available)
        {
            if (preferredRef == availableRef)
            {
                return availableRef;
            }
        }
    }
    return defaultVal;
}

static VkFormat getSurfaceFormat(
    const std::vector<VkFormat>& preferred,
    const std::vector<VkSurfaceFormatKHR>& available,
    const VkFormat defaultVal)
{
    for (const auto& preferredRef : preferred)
    {
        for (const auto& availableRef : available)
        {
            if (preferredRef == availableRef.format)
            {
                return availableRef.format;
            }
        }
    }
    return defaultVal;
}

///////////////////////////////////////////////////////////////////////////////

GfxResources::GfxResources(Window* const p_window)
    : mp_window(p_window)
{
    assert(mp_window);

    create();
}

GfxResources::~GfxResources()
{
    destroy();
}

void GfxResources::destroy()
{
    vkDeviceWaitIdle(m_device.logicalDevice);

    vkDestroyDescriptorPool(m_device.logicalDevice, m_descriptorPool.uniforms, nullptr);
    vkDestroyDescriptorPool(m_device.logicalDevice, m_descriptorPool.images, nullptr);

    for (uint32_t idx = 0; idx < m_cmdBuffer.commandBufferFences.size(); ++idx)
    {
        vkDestroyFence(m_device.logicalDevice, m_cmdBuffer.commandBufferFences[idx], nullptr);
    }
    // vkFreeCommandBuffers() not needed due to vkDestroyCommandPool.
    vkDestroyCommandPool(m_device.logicalDevice, m_commandPool, nullptr);
    vkDestroySemaphore(m_device.logicalDevice, m_cmdBuffer.cmdBufferSubmitSemaphore, nullptr);

    destroySwapchain();

    vkDestroyDevice(m_device.logicalDevice, nullptr);

#if (DEF_USE_DEBUG_VALIDATION == 1)
    fvkDestroyDebugReportCallbackEXT(m_instance, m_debugReportCallback, nullptr);
#endif

    vkDestroyInstance(m_instance, nullptr);
}

void GfxResources::destroySwapchain()
{
    vkDestroySemaphore(m_device.logicalDevice, m_swapchain.swapchainImageSemaphore, nullptr);
    for (uint32_t idx = 0; idx < m_swapchain.imageViews.size(); ++idx)
    {
        vkDestroyImageView(m_device.logicalDevice, m_swapchain.imageViews[idx], nullptr);
    }
    vkDestroySwapchainKHR(m_device.logicalDevice, m_swapchain.swapchain, nullptr);
    vkDestroySurfaceKHR(m_instance, m_swapchain.surface, nullptr);
}

void GfxResources::create()
{
    createInstance();
    createPhysicalDevice();
    createSwapchain();
    createDescriptorPools();
    createQueuesAndPools();
}

void GfxResources::createInstance()
{
    const GlobalVariables& gv = GlobalVariables::getInstance();

    const VkApplicationInfo applicationInfo =
    {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // sType
        nullptr,                            // pNext
        gv.applicationName.c_str(),         // pApplicationName
        gv.applicationVersion,              // applicationVersion
        gv.engineName.c_str(),              // pEngineName
        gv.engineVersion,                   // engineVersion
        gv.apiVersion,                      // apiVersion
    };

    std::vector<const char*> extensions;
    std::vector<const char*> layers;

    extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#if (DEF_USE_DEBUG_VALIDATION == 1)
    extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    // this is the most important thing
    layers.emplace_back("VK_LAYER_LUNARG_standard_validation");
#endif

    const VkInstanceCreateInfo instanceCreateInfo =
    {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // sType
        nullptr,                                // pNext
        0,                                      // flags
        &applicationInfo,                       // pApplicationInfo
        (uint32_t)layers.size(),                // enabledLayerCount
        layers.data(),                          // ppEnabledLayerNames
        (uint32_t)extensions.size(),            // enabledExtensionCount
        extensions.data()                       // ppEnabledExtensionNames
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateInstance(
        &instanceCreateInfo,    // pCreateInfo
        nullptr,                // pAllocator
        &m_instance));          // pInstance

#if (DEF_USE_DEBUG_VALIDATION == 1)
    fvkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
    assert(fvkCreateDebugReportCallbackEXT);
    fvkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
        vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
    assert(fvkDestroyDebugReportCallbackEXT);

    const VkDebugReportCallbackCreateInfoEXT reportCallbackCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,            // sType
        nullptr,                                                            // pNext
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,                        // flags
        debugCallback,                                                      // pfnCallback
        nullptr                                                             // pUserData
    };

    CHECK_VK_RESULT_SUCCESS(fvkCreateDebugReportCallbackEXT(
        m_instance,                 // instance
        &reportCallbackCreateInfo,  // pCreateInfo
        nullptr,                    // pAllocator
        &m_debugReportCallback));   // pCallback
#endif
}

void GfxResources::createPhysicalDevice()
{
    uint32_t physicalDeviceCount = 1; // let's try to get one
    CHECK_VK_RESULT_SUCCESS(vkEnumeratePhysicalDevices(
        m_instance,                 // instance
        &physicalDeviceCount,       // pPhysicalDeviceCount
        &m_device.physicalDevice)); // pPhysicalDevices

    vkGetPhysicalDeviceProperties(
        m_device.physicalDevice,               // physicalDevice
        &m_device.physicalDeviceProperties);   // pProperties

#if (DEF_PRINT_DEVICE_PROPERTIES == 1)
    {
        const uint32_t version = m_device.physicalDeviceProperties.apiVersion;
        const uint32_t drVersion = m_device.physicalDeviceProperties.driverVersion;
        std::cout << "apiVersion:        " << VK_VERSION_MAJOR(version) << "."
            << VK_VERSION_MINOR(version) << "." << VK_VERSION_PATCH(version) << std::endl;
        std::cout << "driverVersion:     " << VK_VERSION_MAJOR(drVersion) << "."
            << VK_VERSION_MINOR(drVersion) << "." << VK_VERSION_PATCH(drVersion) << std::endl;
        std::cout << "vendorID:          " << m_device.physicalDeviceProperties.vendorID << std::endl;
        std::cout << "deviceID:          " << m_device.physicalDeviceProperties.deviceID << std::endl;
        std::cout << "deviceType:        " << m_device.physicalDeviceProperties.deviceType << std::endl;
        std::cout << "deviceName:        " << m_device.physicalDeviceProperties.deviceName << std::endl;
    }
#endif

    vkGetPhysicalDeviceFeatures(
        m_device.physicalDevice,            // physicalDevice
        &m_device.physicalDeviceFeatures);  // pFeatures

    vkGetPhysicalDeviceMemoryProperties(
        m_device.physicalDevice,                   // physicalDevice
        &m_device.physicalDeviceMemoryProperties); // pMemoryProperties

    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_device.physicalDevice,        // physicalDevice
        &queueFamilyPropertyCount,      // pQueueFamilyPropertyCount
        nullptr);                       // pQueueFamilyProperties

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_device.physicalDevice,        // physicalDevice
        &queueFamilyPropertyCount,      // pQueueFamilyPropertyCount
        queueFamilyProperties.data());  // pQueueFamilyProperties

    for (uint32_t idx = 0; idx < queueFamilyProperties.size(); ++idx)
    {
        // just get the first with graphics queue
        if (queueFamilyProperties[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            m_queue.queueFamilyIndex = idx;
            break;
        }
    }
    assert(m_queue.queueFamilyIndex != ~0u);

    // we don't need anything fancy
    VkPhysicalDeviceFeatures requiredDeviceFeatures = {};

    constexpr float queuePriorities[] = { 0.0f };
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                    // pNext
        0,                                          // flags
        m_queue.queueFamilyIndex,                   // queueFamilyIndex
        1,                                          // queueCount
        queuePriorities                             // pQueuePriorities
    };

    std::vector<const char*> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    const VkDeviceCreateInfo deviceCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // sType
        nullptr,                                // pNext
        0,                                      // flags
        1,                                      // queueCreateInfoCount
        &deviceQueueCreateInfo,                 // pQueueCreateInfos
        0,                                      // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        (uint32_t)extensions.size(),            // enabledExtensionCount
        extensions.data(),                      // ppEnabledExtensionNames
        &requiredDeviceFeatures                 // pEnabledFeatures
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateDevice(
        m_device.physicalDevice,    // physicalDevice
        &deviceCreateInfo,          // pCreateInfo
        nullptr,                    // pAllocator
        &m_device.logicalDevice));  // pDevice
}

void GfxResources::createSwapchain()
{
    // surface
    {
        const VkBool32 hasPresentationSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(
            m_device.physicalDevice,    // physicalDevice
            m_queue.queueFamilyIndex);  // queueFamilyIndex
        assert(hasPresentationSupport);

        const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo =
        {
            //VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
            VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,    // sType
            nullptr,                                            // pNext
            0,                                                  // flags
            mp_window->getHinstance(),                          // hinstance
            mp_window->getHwnd()                                // hwnd
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateWin32SurfaceKHR(
            m_instance,             // instance
            &surfaceCreateInfo,     // pCreateInfo
            nullptr,                // pAllocator
            &m_swapchain.surface)); // pSurface
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_device.physicalDevice,    // physicalDevice
        m_swapchain.surface,        // surface
        &surfaceCapabilities));     // pSurfaceCapabilities

    uint32_t surfaceFormatCount = 0;
    CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device.physicalDevice,    // physicalDevice
        m_swapchain.surface,        // surface
        &surfaceFormatCount,        // pSurfaceFormatCount
        nullptr));                  // pSurfaceFormats

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device.physicalDevice,    // physicalDevice
        m_swapchain.surface,        // surface
        &surfaceFormatCount,        // pSurfaceFormatCount
        surfaceFormats.data()));    // pSurfaceFormats
    assert(surfaceFormats.size() > 0);

    const GfxPreferredSetup gfxPreferredSetup;

    m_swapchain.imageFormat = getSurfaceFormat(
        gfxPreferredSetup.surfaceFormats,
        surfaceFormats,
        VK_FORMAT_UNDEFINED);

    // if preferred (srgb) formats not found, take the first
    if (m_swapchain.imageFormat == VK_FORMAT_UNDEFINED)
    {
        m_swapchain.imageFormat = surfaceFormats[0].format;
        m_swapchain.colorSpace = surfaceFormats[0].colorSpace;
    }

    uint32_t presentModeCount = 0;
    CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device.physicalDevice,    // physicalDevice
        m_swapchain.surface,        // surface
        &presentModeCount,          // pPresentModeCount
        nullptr));                  // pPresentModes

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device.physicalDevice,    // physicalDevice
        m_swapchain.surface,        // surface
        &presentModeCount,          // pPresentModeCount
        presentModes.data()));      // pPresentModes
    assert(presentModes.size() > 0);

    const VkPresentModeKHR presentMode = getPresentMode(
        gfxPreferredSetup.presentation,
        presentModes,
        VK_PRESENT_MODE_FIFO_KHR);

    {
        VkBool32 surfaceSupported = VK_FALSE;
        CHECK_VK_RESULT_SUCCESS(vkGetPhysicalDeviceSurfaceSupportKHR(
            m_device.physicalDevice,    // physicalDevice
            m_queue.queueFamilyIndex,   // queueFamilyIndex
            m_swapchain.surface,        // surface
            &surfaceSupported));        // pSupported
        assert(surfaceSupported == VK_TRUE);
    }

    const VkSwapchainCreateInfoKHR swapchainCreateInfo =
    {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,            // sType
        nullptr,                                                // pNext
        0,                                                      // flags
        m_swapchain.surface,                                    // surface
        c_bufferingCount,                                       // minImageCount
        m_swapchain.imageFormat,                                // imageFormat
        m_swapchain.colorSpace,                                 // imageColorSpace
        VkExtent2D{ mp_window->getWidth(), mp_window->getHeight() },  // imageExtent
        1,                                                      // imageArrayLayers
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,                    // imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                              // imageSharingMode
        1,                                                      // queueFamilyIndexCount
        &m_queue.queueFamilyIndex,                              // pQueueFamilyIndices
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,                  // preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,                      // compositeAlpha
        presentMode,                                            // presentMode
        VK_TRUE,                                                // clipped
        nullptr,                                                // oldSwapchain
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateSwapchainKHR(
        m_device.logicalDevice,     // device
        &swapchainCreateInfo,       // pCreateInfo
        nullptr,                    // pAllocator
        &m_swapchain.swapchain));   // pSwapchain

    uint32_t imageCount = 0;

    CHECK_VK_RESULT_SUCCESS(vkGetSwapchainImagesKHR(
        m_device.logicalDevice,     // device
        m_swapchain.swapchain,      // swapchain
        &imageCount,                // pSwapchainImageCount
        nullptr));                  // pSwapchainImages

    m_swapchain.images.resize(imageCount);
    CHECK_VK_RESULT_SUCCESS(vkGetSwapchainImagesKHR(
        m_device.logicalDevice,         // device
        m_swapchain.swapchain,          // swapchain
        &imageCount,                    // pSwapchainImageCount
        m_swapchain.images.data()));    // pSwapchainImages

    constexpr VkComponentMapping componentMapping =
    {
        VK_COMPONENT_SWIZZLE_IDENTITY,  // r
        VK_COMPONENT_SWIZZLE_IDENTITY,  // g
        VK_COMPONENT_SWIZZLE_IDENTITY,  // b
        VK_COMPONENT_SWIZZLE_IDENTITY,  // a
    };
    constexpr VkImageSubresourceRange imageSubresourceRange =
    {
        VK_IMAGE_ASPECT_COLOR_BIT,  // aspectMask
        0,                          // baseMipLevel
        1,                          // levelCount
        0,                          // baseArrayLayer
        1,                          // layerCount
    };

    m_swapchain.imageViews.resize(imageCount);
    for (uint32_t idx = 0; idx < m_swapchain.imageViews.size(); ++idx)
    {
        const VkImageViewCreateInfo imageViewCreateInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // sType
            nullptr,                                    // pNext
            0,                                          // flags
            m_swapchain.images[idx],                    // image
            VK_IMAGE_VIEW_TYPE_2D,                      // viewType
            m_swapchain.imageFormat,                    // format
            componentMapping,                           // components
            imageSubresourceRange                       // subresourceRange;
        };
        CHECK_VK_RESULT_SUCCESS(vkCreateImageView(
            m_device.logicalDevice,         // device
            &imageViewCreateInfo,           // pCreateInfo
            nullptr,                        // pAllocator
            &m_swapchain.imageViews[idx])); // pView
    }

    constexpr VkSemaphoreCreateInfo semaphoreCreateInfo =
    {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,    // sType
        nullptr,                                    // pNext
        0,                                          // flags
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateSemaphore(
        m_device.logicalDevice,                 // device
        &semaphoreCreateInfo,                   // pCreateInfo
        nullptr,                                // pAllocator
        &m_swapchain.swapchainImageSemaphore)); // pSemaphore
}

void GfxResources::createDescriptorPools()
{
    // descriptor pool for uniforms
    {
        const VkDescriptorPoolSize descriptorPoolSize =
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,  // type
            m_descriptorPool.c_bindingCountUniform      // descriptorCount
        };

        // create with free-flag for convinience with image descriptor updates

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo =
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,      // sType
            nullptr,                                            // pNext
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // flags
            m_descriptorPool.c_maxSetsUniform,                  // maxSets
            1,                                                  // poolSizeCount
            &descriptorPoolSize                                 // pPoolSizes
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateDescriptorPool(
            m_device.logicalDevice,         // device,
            &descriptorPoolCreateInfo,      // pCreateInfo,
            nullptr,                        // pAllocator,
            &m_descriptorPool.uniforms))    // pDescriptorPool
    }

    // descriptor pool for images
    {
        const VkDescriptorPoolSize descriptorPoolSize =
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // type
            m_descriptorPool.c_bindingCountImage        // descriptorCount
        };

        // create with free-flag for convenience with image descriptor updates

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo =
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,      // sType
            nullptr,                                            // pNext
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // flags
            m_descriptorPool.c_maxSetsImage,                    // maxSets
            1,                                                  // poolSizeCount
            &descriptorPoolSize                                 // pPoolSizes
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateDescriptorPool(
            m_device.logicalDevice,     // device,
            &descriptorPoolCreateInfo,  // pCreateInfo,
            nullptr,                    // pAllocator,
            &m_descriptorPool.images)); // pDescriptorPool
    }
}

void GfxResources::createQueuesAndPools()
{
    vkGetDeviceQueue(
        m_device.logicalDevice,     // device
        m_queue.queueFamilyIndex,   // queueFamilyIndex
        0,                          // queueIndex
        &m_queue.queue);            // pQueue
    assert(m_queue.queue);

    const VkCommandPoolCreateInfo commandPoolCreateInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,         // sType
        nullptr,                                            // pNext
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,    // flags
        m_queue.queueFamilyIndex                            // queueFamilyIndex
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateCommandPool(
        m_device.logicalDevice, // device
        &commandPoolCreateInfo, // pCreateInfo
        nullptr,                // pAllocator
        &m_commandPool));       // pCommandPool
    assert(m_commandPool);

    // create command buffers and their fences
    m_cmdBuffer.commandBuffers.resize(c_bufferingCount);
    m_cmdBuffer.commandBufferFences.resize(c_bufferingCount);

    const uint32_t cmdBufferCount = (uint32_t)m_cmdBuffer.commandBuffers.size();
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType
        nullptr,                                        // pNext
        m_commandPool,                                  // commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // level
        cmdBufferCount                                  // commandBufferCount
    };

    CHECK_VK_RESULT_SUCCESS(vkAllocateCommandBuffers(
        m_device.logicalDevice,                 // device
        &commandBufferAllocateInfo,             // pAllocateInfo
        m_cmdBuffer.commandBuffers.data()));    // pCommandBuffers

    // set as signaled, so we pass the first check of vkWaitForFences() as well
    constexpr VkFenceCreateInfo fenceCreateInfo =
    {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,    // sType
        nullptr,                                // pNext
        VK_FENCE_CREATE_SIGNALED_BIT            // flags
    };

    for (uint32_t idx = 0; idx < m_cmdBuffer.commandBufferFences.size(); ++idx)
    {
        CHECK_VK_RESULT_SUCCESS(vkCreateFence(
            m_device.logicalDevice,                     // device
            &fenceCreateInfo,                           // pCreateInfo
            nullptr,                                    // pAllocator
            &m_cmdBuffer.commandBufferFences[idx]));    // pFence
    }

    constexpr VkSemaphoreCreateInfo semaphoreCreateInfo =
    {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,    // sType
        nullptr,                                    // pNext
        0,                                          // flags
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateSemaphore(
        m_device.logicalDevice,                     // device
        &semaphoreCreateInfo,                       // pCreateInfo
        nullptr,                                    // pAllocator
        &m_cmdBuffer.cmdBufferSubmitSemaphore));    // pSemaphore
}

void GfxResources::waitForIdle()
{
    vkDeviceWaitIdle(m_device.logicalDevice);
}

void GfxResources::resizeWindow()
{
    waitForIdle();
    destroySwapchain();
    createSwapchain();
}

GfxDevice* GfxResources::getDevice()
{
    return &m_device;
}

GfxSwapchain* GfxResources::getSwapchain()
{
    return &m_swapchain;
}

GfxCmdBuffer* GfxResources::getCmdBuffer()
{
    return &m_cmdBuffer;
}

GfxQueue* GfxResources::getQueue()
{
    return &m_queue;
}

GfxDescriptorPool* GfxResources::getDescriptorPool()
{
    return &m_descriptorPool;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
