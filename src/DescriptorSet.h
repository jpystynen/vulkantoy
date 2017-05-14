#ifndef CORE_DESCRIPTOR_SET_H
#define CORE_DESCRIPTOR_SET_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "GfxResources.h"

#include <cstdint>
#include <assert.h>
#include <map>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class DescriptorSet
{
public:
    DescriptorSet(GfxDevice* const p_gfxDevice,
        VkDescriptorPool descriptorPool,
        const uint32_t bindingCount,
        const VkDescriptorType descriptorType,
        const VkShaderStageFlags shaderStageFlags)
        : mp_gfxDevice(p_gfxDevice),
        m_descriptorPool(descriptorPool),
        m_bindingCount(bindingCount),
        m_descriptorType(descriptorType)
    {
        assert(mp_gfxDevice);
        assert(mp_gfxDevice->logicalDevice);

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindingArray(bindingCount);
        for (uint32_t idx = 0; idx < m_bindingCount; ++idx)
        {
            const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding =
            {
                idx,                // binding
                m_descriptorType,   // descriptorType
                1,                  // descriptorCount
                shaderStageFlags,   // stageFlags
                nullptr             // pImmutableSamplers
            };
            descriptorSetLayoutBindingArray[idx] = descriptorSetLayoutBinding;
        }

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,    // sType
            nullptr,                                                // pNext
            0,                                                      // flags
            m_bindingCount,                                         // bindingCount
            descriptorSetLayoutBindingArray.data(),                 // pBindings
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateDescriptorSetLayout(
            mp_gfxDevice->logicalDevice,       // device
            &descriptorSetLayoutCreateInfo, // pCreateInfo
            nullptr,                        // pAllocator
            &descriptorSetLayout));         // pSetLayout

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
            nullptr,                                        // pNext
            descriptorPool,                                 // descriptorPool
            1,                                              // descriptorSetCount
            &descriptorSetLayout                            // pSetLayouts
        };

        CHECK_VK_RESULT_SUCCESS(vkAllocateDescriptorSets(
            mp_gfxDevice->logicalDevice,   // device
            &descriptorSetAllocateInfo, // pAllocateInfo
            &descriptorSet));           // pDescriptorSets
    }

    ~DescriptorSet()
    {
        if (mp_gfxDevice->logicalDevice)
        {
            vkDestroyDescriptorSetLayout(mp_gfxDevice->logicalDevice, descriptorSetLayout, nullptr);
            if (descriptorSet)
            {
                vkFreeDescriptorSets(
                    mp_gfxDevice->logicalDevice,// device,
                    m_descriptorPool,           // descriptorPool,
                    1,                          // descriptorSetCount,
                    &descriptorSet);            // pDescriptorSets);
            }
        }
    }

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    // variables (public for easier access)

    VkDescriptorSet descriptorSet             = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = nullptr;

private:
    GfxDevice* const mp_gfxDevice           = nullptr;
    VkDescriptorPool const m_descriptorPool = nullptr;

    uint32_t m_bindingCount             = 0;
    VkDescriptorType m_descriptorType   = VK_DESCRIPTOR_TYPE_MAX_ENUM;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_DESCRIPTOR_SET_H
