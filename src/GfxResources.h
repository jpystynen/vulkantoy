#ifndef CORE_GFX_RESOURCES_H
#define CORE_GFX_RESOURCES_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <assert.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

static const uint32_t s_defaultTimeout = 1000000000; // 1 s

namespace core
{

// Helper for checking the returned result from Vulkan functions
#define CHECK_VK_RESULT_SUCCESS(check_vk_func) \
{ \
const VkResult result = (check_vk_func); \
assert(result == VK_SUCCESS); \
}

// Helper function for device memory type index.
uint32_t getPhysicalDeviceMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
    const VkMemoryRequirements& memoryRequirements,
    const VkMemoryPropertyFlags memoryPropertyFlags);

// Helper function for memory alignment.
uint32_t getAlignedByteSize(
    const uint32_t byteSize,
    const uint32_t alignment);

///////////////////////////////////////////////////////////////////////////////

class Window;

class GfxPreferredSetup
{
public:
    std::vector<VkPresentModeKHR> presentation
    { VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR };

    std::vector<VkFormat> surfaceFormats
    { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB };
};

class GfxDevice
{
public:
    VkDevice logicalDevice          = nullptr;
    VkPhysicalDevice physicalDevice = nullptr;

    VkPhysicalDeviceProperties physicalDeviceProperties             {};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties {};
    VkPhysicalDeviceFeatures physicalDeviceFeatures                 {};
};

class GfxSwapchain
{
public:
    // current index for buffered handles
    uint32_t imageIndex = 0;

    // Tries to create c_bufferingCount of images
    // and recycles them frame by frame
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    // signaled when image is acquired
    VkSemaphore swapchainImageSemaphore = nullptr;

    VkSurfaceKHR surface        = nullptr;
    VkSwapchainKHR swapchain    = nullptr;

    VkFormat imageFormat        = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
};

class GfxQueue
{
public:
    VkQueue queue               = nullptr;
    uint32_t queueFamilyIndex   = ~0u;
};

class GfxCmdBuffer
{
public:
    struct CmdBuffer
    {
        VkCommandBuffer commandBuffer   = nullptr;
        VkFence fence                   = nullptr;
        VkSemaphore submitSemaphore     = nullptr;
    };

    CmdBuffer getNextCmdBuffer()
    {
        m_bufferIndex = (m_bufferIndex + 1) % commandBuffers.size();

        CmdBuffer cmdbuf;
        cmdbuf.commandBuffer = commandBuffers[m_bufferIndex];
        cmdbuf.fence = commandBufferFences[m_bufferIndex];
        cmdbuf.submitSemaphore = cmdBufferSubmitSemaphore;
        return cmdbuf;
    }

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> commandBufferFences;

    // signaled when cmd buffer submit is done
    VkSemaphore cmdBufferSubmitSemaphore;

private:
    uint32_t m_bufferIndex = 0;
};

class GfxDescriptorPool
{
public:
    const uint32_t c_maxSetsUniform         = 1;
    const uint32_t c_bindingCountUniform    = 1;
    VkDescriptorPool uniforms               = nullptr;

    const uint32_t c_maxSetsImage       = 1;
    const uint32_t c_bindingCountImage  = 4;
    VkDescriptorPool images             = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class GfxResources
{
public:
    explicit GfxResources(Window* const p_window);
    ~GfxResources();

    GfxResources(const GfxResources&) = delete;
    GfxResources& operator=(const GfxResources&) = delete;

    GfxDevice* getDevice();
    GfxSwapchain* getSwapchain();
    GfxCmdBuffer* getCmdBuffer();
    GfxDescriptorPool* getDescriptorPool();
    GfxQueue* getQueue();

    // Call when window has been resized.
    void resizeWindow();

    // Only call when cleaning up or when closing.
    void waitForIdle();

private:

    const uint32_t c_bufferingCount = 3;

    void create();
    void destroy();

    void createInstance();
    void createPhysicalDevice();
    void createSwapchain();
    void createDescriptorPools();
    void createQueuesAndPools();

    void destroySwapchain();

    const Window* mp_window = nullptr;

    VkInstance m_instance   = nullptr;

    GfxDevice m_device;
    GfxSwapchain m_swapchain;
    GfxCmdBuffer m_cmdBuffer;
    GfxDescriptorPool m_descriptorPool;
    GfxQueue m_queue;

#ifdef _DEBUG
    VkDebugReportCallbackEXT m_debugReportCallback = nullptr;
#endif

    VkCommandPool m_commandPool = nullptr;
    uint32_t m_queueFamilyIndex = ~0u;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_GFX_RESOURCES_H
