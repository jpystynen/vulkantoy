#ifndef CORE_GPU_IMAGE_H
#define CORE_GPU_IMAGE_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "GfxResources.h"

#include <assert.h>
#include <cstdint>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class GpuImage
{
public:
    GpuImage(GfxDevice* const p_device,
        const uint32_t width,
        const uint32_t height,
        const VkFormat imgFormat,
        const VkImageUsageFlags imgUsageFlags,
        const VkImageLayout imgLayout,
        const VkFilter filter,
        const VkSamplerAddressMode samplerAddressMode)
        : mp_device(p_device),
        imageFormat(imgFormat),
        imageUsage(imgUsageFlags),
        imageLayout(imgLayout),
        size{ width, height, 1 }
    {
        assert(mp_device);
        assert(size.width > 0 && size.height > 0 && size.depth > 0);

        const VkImageCreateInfo imageCreateInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,    // sType
            nullptr,                                // pNext
            0,                                      // flags
            VK_IMAGE_TYPE_2D,                       // imageType
            imageFormat,                            // format
            size,                                   // extent
            1,                                      // mipLevels
            1,                                      // arrayLayers
            VK_SAMPLE_COUNT_1_BIT,                  // samples
            VK_IMAGE_TILING_OPTIMAL,                // tiling
            imageUsage,                             // usage
            VK_SHARING_MODE_EXCLUSIVE,              // sharingMode
            0,                                      // queueFamilyIndexCount
            nullptr,                                // pQueueFamilyIndices
            imageLayout                             // initialLayout
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateImage(
            mp_device->logicalDevice,   // device
            &imageCreateInfo,           // pCreateInfo
            nullptr,                    // pAllocator
            &image));                   // pImage

        // allocate memory

        vkGetImageMemoryRequirements(
            mp_device->logicalDevice,   // device
            image,                      // image
            &memoryRequirements);       // pMemoryRequirements

        const VkMemoryPropertyFlags memPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t memTypeIndex = getPhysicalDeviceMemoryTypeIndex(
            mp_device->physicalDeviceMemoryProperties,
            memoryRequirements,
            memPropertyFlags);

        const VkMemoryAllocateInfo memoryAllocateInfo =
        {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // sType
            nullptr,                                // pNext
            memoryRequirements.size,                // allocationSize
            memTypeIndex,                           // memoryTypeIndex
        };

        CHECK_VK_RESULT_SUCCESS(vkAllocateMemory(
            mp_device->logicalDevice,   // device
            &memoryAllocateInfo,        // pAllocateInfo
            nullptr,                    // pAllocator
            &m_deviceMemory));          // pMemory

        CHECK_VK_RESULT_SUCCESS(vkBindImageMemory(
            mp_device->logicalDevice,   // device
            image,                      // image
            m_deviceMemory,             // memory
            0));                        // memoryOffset

        // image view

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

        const VkImageViewCreateInfo imageViewCreateInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // sType
            nullptr,                                    // pNext
            0,                                          // flags
            image,                                      // image
            VK_IMAGE_VIEW_TYPE_2D,                      // viewType
            imageFormat,                                // format
            componentMapping,                           // components
            imageSubresourceRange                       // subresourceRange
        };
        CHECK_VK_RESULT_SUCCESS(vkCreateImageView(
            mp_device->logicalDevice,   // device
            &imageViewCreateInfo,       // pCreateInfo
            nullptr,                    // pAllocator
            &imageView));               // pView

        // sampler

        const VkSamplerCreateInfo samplerCreateInfo =
        {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,  // sType
            nullptr,                                // pNext
            0,                                      // flags
            filter,                                 // magFilter
            filter,                                 // minFilter
            VK_SAMPLER_MIPMAP_MODE_NEAREST,         // mipmapMode
            samplerAddressMode,                     // addressModeU
            samplerAddressMode,                     // addressModeV
            samplerAddressMode,                     // addressModeW
            0.0f,                                   // mipLodBias
            false,                                  // anisotropyEnable
            1.0f,                                   // maxAnisotropy
            false,                                  // compareEnable
            VK_COMPARE_OP_NEVER,                    // compareOp
            0.0f,                                   // minLod
            0.0f,                                   // maxLod
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,// borderColor
            false                                   // unnormalizedCoordinates
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateSampler(
            mp_device->logicalDevice,   // device
            &samplerCreateInfo,         // pCreateInfo
            nullptr,                    // pAllocator
            &sampler));                 // pSampler
    }

    ~GpuImage()
    {
        if (mp_device->logicalDevice)
        {
            vkDestroyImage(mp_device->logicalDevice, image, nullptr);
            vkDestroyImageView(mp_device->logicalDevice, imageView, nullptr);
            vkDestroySampler(mp_device->logicalDevice, sampler, nullptr);
            vkFreeMemory(mp_device->logicalDevice, m_deviceMemory, nullptr);
        }
    }

    GpuImage(const GpuImage&) = delete;
    GpuImage& operator=(const GpuImage&) = delete;

    // variables (public for easier access)

    VkImage image                           = nullptr;
    VkImageView imageView                   = nullptr;
    VkSampler sampler                       = nullptr;

    VkFormat imageFormat                    = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags imageUsage            = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
    VkImageLayout imageLayout               = VK_IMAGE_LAYOUT_UNDEFINED;
    VkMemoryRequirements memoryRequirements { 0, 0, 0 };

    VkExtent3D size { 0, 0, 0 };

private:
    GfxDevice* const mp_device      = nullptr;
    VkDeviceMemory m_deviceMemory   = nullptr;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_GPU_IMAGE_H
