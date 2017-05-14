#ifndef CORE_GPU_BUFFER_H
#define CORE_GPU_BUFFER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "GfxResources.h"

#include <assert.h>
#include <algorithm>
#include <cstdint>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

// Triple buffered dynamic uniform buffer.
class GpuBufferUniform
{
public:
    GpuBufferUniform(GfxDevice* const p_gfxDevice,
        const uint32_t sizeInBytes) // bytesize of a single buffer (even when triple buffered)
        : mp_device(p_gfxDevice),
        byteSize(sizeInBytes)
    {
        assert(mp_device);
        assert(mp_device->logicalDevice);
        assert(byteSize > 0);

        const uint32_t minByteAlignment = (uint32_t)
            std::max(mp_device->physicalDeviceProperties.limits.minUniformBufferOffsetAlignment,
                mp_device->physicalDeviceProperties.limits.minMemoryMapAlignment);

        byteSize = getAlignedByteSize(byteSize, minByteAlignment);

        wholeByteSize = byteSize * c_bufferCount;

        const VkBufferCreateInfo bufferCreateInfo =
        {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,   // sType
            nullptr,                                // pNext
            0,                                      // flags
            wholeByteSize,                          // size
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // usage
            VK_SHARING_MODE_EXCLUSIVE,              // sharingMode
            0,                                      // queueFamilyIndexCount
            nullptr                                 // pQueueFamilyIndices
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateBuffer(
            mp_device->logicalDevice,   // device
            &bufferCreateInfo,          // pCreateInfo
            nullptr,                    // pAllocator
            &buffer));                  // pBuffer

        // allocate memory

        vkGetBufferMemoryRequirements(
            mp_device->logicalDevice,   // device
            buffer,                     // buffer
            &memoryRequirements);       // pMemoryRequirements

        const VkMemoryPropertyFlags memPropertyFlags =
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

        CHECK_VK_RESULT_SUCCESS(vkBindBufferMemory(
            mp_device->logicalDevice,   // device
            buffer,                     // buffer
            m_deviceMemory,             // memory
            0));                        // memoryOffset

        // map the whole coherent buffer
        CHECK_VK_RESULT_SUCCESS(vkMapMemory(
            mp_device->logicalDevice,   // device
            m_deviceMemory,             // memory
            0,                          // offset
            wholeByteSize,              // size
            0,                          // flags
            &mp_data));                 // ppData
    }

    ~GpuBufferUniform()
    {
        if (buffer)
        {
            vkUnmapMemory(mp_device->logicalDevice, m_deviceMemory);
            vkDestroyBuffer(mp_device->logicalDevice, buffer, nullptr);
        }
        if (m_deviceMemory)
        {
            vkFreeMemory(mp_device->logicalDevice, m_deviceMemory, nullptr);
        }
    }

    GpuBufferUniform(const GpuBufferUniform&) = delete;
    GpuBufferUniform& operator=(const GpuBufferUniform&) = delete;

    void copyData(const uint32_t sizeInBytes, const uint8_t* const p_data)
    {
        assert(sizeInBytes <= byteSize);

        // next buffer
        m_bufferIndex = (m_bufferIndex + 1) % c_bufferCount;

        const uint32_t minByteSize = std::min((uint32_t)byteSize, sizeInBytes);
        uint8_t* const offset = (uint8_t*)(mp_data) + m_bufferIndex * byteSize;
        std::memcpy(offset, p_data, minByteSize);
    }

    uint32_t getByteOffset()
    {
        return m_bufferIndex * byteSize;
    }

    // variables (public for easier access)

    VkBuffer buffer                         = nullptr;
    VkMemoryRequirements memoryRequirements { 0,0,0 };

    uint32_t byteSize           = 0;    // byte size of a single buffer
    uint32_t wholeByteSize      = 0;    // byte size of the whole buffer

private:
    const uint32_t c_bufferCount = 3;

    GfxDevice* const mp_device      = nullptr;
    VkDeviceMemory m_deviceMemory   = nullptr;

    void* mp_data           = nullptr;  // data pointer for copying data to buffer
    uint32_t m_bufferIndex  = 0;        // for triple buffer
};

///////////////////////////////////////////////////////////////////////////////

// Staging buffer for copying data to e.g. gpu images.
class GpuBufferStaging
{
public:
    GpuBufferStaging(GfxDevice* const p_device,
        const uint32_t sizeInBytes,
        const uint8_t* const p_inputData)
        : mp_gfxDevice(p_device),
        byteSize(sizeInBytes)
    {
        assert(mp_gfxDevice);
        assert(mp_gfxDevice->logicalDevice);
        assert(byteSize > 0);
        assert(p_inputData);

        const uint32_t minByteAlignment = (uint32_t)
            mp_gfxDevice->physicalDeviceProperties.limits.minMemoryMapAlignment;
        byteSize = getAlignedByteSize(byteSize, minByteAlignment);

        const VkBufferCreateInfo bufferCreateInfo =
        {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,   // sType
            nullptr,                                // pNext
            0,                                      // flags
            byteSize,                               // size
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // usage
            VK_SHARING_MODE_EXCLUSIVE,              // sharingMode
            0,                                      // queueFamilyIndexCount
            nullptr                                 // pQueueFamilyIndices
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateBuffer(
            mp_gfxDevice->logicalDevice,// device
            &bufferCreateInfo,          // pCreateInfo
            nullptr,                    // pAllocator
            &buffer));                  // pBuffer

        // allocate memory

        vkGetBufferMemoryRequirements(
            mp_gfxDevice->logicalDevice,   // device
            buffer,                     // buffer
            &memoryRequirements);       // pMemoryRequirements

        const VkMemoryPropertyFlags memPropertyFlags =
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        const uint32_t memTypeIndex = getPhysicalDeviceMemoryTypeIndex(
            mp_gfxDevice->physicalDeviceMemoryProperties,
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
            mp_gfxDevice->logicalDevice,// device
            &memoryAllocateInfo,        // pAllocateInfo
            nullptr,                    // pAllocator
            &m_deviceMemory));          // pMemory

        CHECK_VK_RESULT_SUCCESS(vkBindBufferMemory(
            mp_gfxDevice->logicalDevice,   // device
            buffer,                     // buffer
            m_deviceMemory,             // memory
            0));                        // memoryOffset

        // copy data to gpu

        void* p_memOffset = nullptr;
        CHECK_VK_RESULT_SUCCESS(vkMapMemory(
            mp_gfxDevice->logicalDevice,// device
            m_deviceMemory,             // memory
            0,                          // offset
            (VkDeviceSize)byteSize,     // size
            0,                          // flags
            &p_memOffset));             // ppData

        std::memcpy((uint8_t*)p_memOffset, p_inputData, byteSize);

        vkUnmapMemory(
            mp_gfxDevice->logicalDevice,    // device
            m_deviceMemory);                // memory
    }

    ~GpuBufferStaging()
    {
        if (mp_gfxDevice->logicalDevice)
        {
            vkDestroyBuffer(mp_gfxDevice->logicalDevice, buffer, nullptr);
            vkFreeMemory(mp_gfxDevice->logicalDevice, m_deviceMemory, nullptr);
        }
    }

    GpuBufferStaging(const GpuBufferStaging&) = delete;
    GpuBufferStaging& operator=(const GpuBufferStaging&) = delete;

    void flushMappedRange()
    {
        const VkMappedMemoryRange mappedMemoryRange =
        {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,  // sType
            nullptr,                                // pNext
            m_deviceMemory,                         // memory
            0,                                      // offset
            byteSize,                               // size
        };

        CHECK_VK_RESULT_SUCCESS(vkFlushMappedMemoryRanges(
            mp_gfxDevice->logicalDevice,// device
            1,                          // memoryRangeCount
            &mappedMemoryRange));       // pMemoryRanges
    }

    // variables (public for easier access)

    VkBuffer buffer = nullptr;
    VkMemoryRequirements memoryRequirements = { 0,0,0 };

    uint32_t byteSize = 0;  // byte size of a single buffer

private:
    GfxDevice* const mp_gfxDevice   = nullptr;
    VkDeviceMemory m_deviceMemory   = nullptr;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_GPU_BUFFER_H
