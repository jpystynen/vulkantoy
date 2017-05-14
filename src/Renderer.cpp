// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Renderer.h"

#include "DescriptorSet.h"
#include "GfxResources.h"
#include "GpuBuffer.h"
#include "GpuImage.h"
#include "ImageLoader.h"
#include "ResourceList.h"
#include "ShaderCompiler.h"
#include "Shader.h"
#include "Window.h"

#include <assert.h>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

Renderer::Renderer(GfxResources* const p_gfxResources, Window* const p_window)
    : mp_gfxResources(p_gfxResources),
    mp_gfxDevice(p_gfxResources->getDevice()),
    mp_window(p_window)
{
    assert(mp_gfxResources);
    assert(mp_gfxDevice);
    assert(mp_window);

    createImages();
    createShaders(false);

    createDescriptorsImage();
    createDescriptorsUniform();
    createRenderPasses();
    createFramebuffers();
    createGraphicsPipeline();
}

Renderer::~Renderer()
{
    if (mp_gfxDevice->logicalDevice)
    {
        mp_gfxResources->waitForIdle();

        for (uint32_t idx = 0; idx < m_framebuffers.size(); ++idx)
        {
            vkDestroyFramebuffer(mp_gfxDevice->logicalDevice,
                m_framebuffers[idx], nullptr);
        }

        vkDestroyRenderPass(mp_gfxDevice->logicalDevice, m_renderPass, nullptr);

        vkDestroyPipelineLayout(mp_gfxDevice->logicalDevice, m_pipelineLayout, nullptr);
        vkDestroyPipeline(mp_gfxDevice->logicalDevice, m_graphicsPipeline, nullptr);
    }
}

void Renderer::render(const RendererInput& rendererInput)
{
    VkDevice logicalDevice = mp_gfxDevice->logicalDevice;
    GfxSwapchain* const p_gfxSwapchain = mp_gfxResources->getSwapchain();
    VkSwapchainKHR swapchain = p_gfxSwapchain->swapchain;
    VkRenderPass renderPass = m_renderPass;
    VkPipeline graphicsPipeline = m_graphicsPipeline;
    VkQueue queue = mp_gfxResources->getQueue()->queue;

    GfxCmdBuffer::CmdBuffer cmdBuffer = mp_gfxResources->getCmdBuffer()->getNextCmdBuffer();

    VkSemaphore swapchainImageSemaphore = p_gfxSwapchain->swapchainImageSemaphore;

    // get index for buffered resources
    {
        CHECK_VK_RESULT_SUCCESS(vkAcquireNextImageKHR(
            logicalDevice,                  // device
            swapchain,                      // swapchin
            0,                              // timeout
            swapchainImageSemaphore,        // semaphore
            nullptr,                        // fence
            &p_gfxSwapchain->imageIndex));  //  pImageIndex
        assert(p_gfxSwapchain->imageIndex < (uint32_t)p_gfxSwapchain->images.size());
    }

    const uint32_t currImageIndex = p_gfxSwapchain->imageIndex;
    VkFramebuffer framebuffer = m_framebuffers[currImageIndex];

    // setup command buffer
    {
        const uint32_t timeout = s_defaultTimeout;
        CHECK_VK_RESULT_SUCCESS(vkWaitForFences(
            logicalDevice,      // device
            1,                  // fenceCount
            &cmdBuffer.fence,   // pFences
            VK_TRUE,            // waitAll
            timeout));          // timeout

        CHECK_VK_RESULT_SUCCESS(vkResetCommandBuffer(
            cmdBuffer.commandBuffer,    // commandBuffer
            0));                        // flags

        constexpr VkCommandBufferBeginInfo commandBufferBeginInfo =
        {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,    // sType
            nullptr,                                        // pNext
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,    // flags
            nullptr                                         // pInheritanceInfo
        };

        CHECK_VK_RESULT_SUCCESS(vkBeginCommandBuffer(
            cmdBuffer.commandBuffer,    // commandBuffer
            &commandBufferBeginInfo));  // pBeginInfo
    }

    // copy image data
    if (m_imageSet.dirty)
    {
        renderCopyImages(cmdBuffer); // using the same command buffer
    }

    // setup descriptors
    {
        std::vector<VkDescriptorSet> descriptorSets
        { m_descriptorSetUniform->descriptorSet,
        m_descriptorSetImage->descriptorSet };

        VkBuffer buffer = m_gpuBufferUniform->buffer;
        const VkDeviceSize bufByteSize = m_gpuBufferUniform->byteSize;
        const VkDeviceSize bufByteOffset = m_gpuBufferUniform->getByteOffset();
        VkPipelineLayout pipelineLayout = m_pipelineLayout;

        ShaderInputUniform shaderInputUniform;
        for (uint32_t idx = 0; idx < 4; ++idx)
        {
            shaderInputUniform.iChannelResolution[idx][0] = (float)m_imageSet.images[idx]->size.width;
            shaderInputUniform.iChannelResolution[idx][1] = (float)m_imageSet.images[idx]->size.height;
            shaderInputUniform.iChannelResolution[idx][2] = (float)m_imageSet.images[idx]->size.depth;
            shaderInputUniform.iChannelResolution[idx][3] = 0.0f;
        }
        for (uint32_t idx = 0; idx < 4; ++idx)
        {
            shaderInputUniform.iDate[idx] = rendererInput.date[idx];
            shaderInputUniform.iChannelTime[idx] = rendererInput.globalTime;
        }
        shaderInputUniform.iMouse[0] = (float)rendererInput.mousePos.leftPosX;
        shaderInputUniform.iMouse[1] = (float)rendererInput.mousePos.leftPosY;
        shaderInputUniform.iMouse[2] = (float)rendererInput.mousePos.clickLeft;
        shaderInputUniform.iMouse[3] = (float)rendererInput.mousePos.clickLeft;
        shaderInputUniform.iResolution[0] = (float)mp_window->getWidth();
        shaderInputUniform.iResolution[1] = (float)mp_window->getHeight();
        shaderInputUniform.iResolution[2] = shaderInputUniform.iResolution[0] / shaderInputUniform.iResolution[1];
        shaderInputUniform.iResolution[3] = 0.0f;
        shaderInputUniform.iGlobalDelta = rendererInput.deltaTime;
        shaderInputUniform.iGlobalFrame = (float)rendererInput.frameIndex;
        shaderInputUniform.iSampleRate = 44100.0f; // don't know about this
        shaderInputUniform.iGlobalTime = rendererInput.globalTime;

        m_gpuBufferUniform->copyData((uint32_t)bufByteSize, (uint8_t*)&shaderInputUniform);
        
        const VkBufferMemoryBarrier bufferMemoryBarrier =
        {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,    // sType
            nullptr,                                    // pNext
            VK_ACCESS_HOST_WRITE_BIT,                   // srcAccessMask
            VK_ACCESS_UNIFORM_READ_BIT,                 // dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,                    // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // dstQueueFamilyIndex
            buffer,                                     // buffer
            bufByteOffset,                              // offset
            bufByteSize                                 // size
        };

        vkCmdPipelineBarrier(
            cmdBuffer.commandBuffer,            // commandBuffer
            VK_PIPELINE_STAGE_HOST_BIT,         // srcStageMask
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,// dstStageMask
            0,                                  // dependencyFlags
            0,                                  // memoryBarrierCount
            nullptr,                            // pMemoryBarriers
            1,                                  // bufferMemoryBarrierCount
            &bufferMemoryBarrier,               // pBufferMemoryBarriers,
            0,                                  // imageMemoryBarrierCount
            nullptr);                           // pImageMemoryBarriers

        const uint32_t dynamicOffset = (uint32_t)bufByteOffset;
        vkCmdBindDescriptorSets(
            cmdBuffer.commandBuffer,            // commandBuffer
            VK_PIPELINE_BIND_POINT_GRAPHICS,    // pipelineBindPoint
            pipelineLayout,                     // layout
            0,                                  // firstSet
            (uint32_t)descriptorSets.size(),    // descriptorSetCount
            descriptorSets.data(),              // pDescriptorSets
            1,                                  // dynamicOffsetCount
            &dynamicOffset);                    // pDynamicOffsets
    }

    const uint32_t width = mp_window->getWidth();
    const uint32_t height = mp_window->getHeight();

    const VkRect2D renderArea =
    {
        { 0, 0 },           // offset
        { width, height}    // extent
    };
    constexpr VkClearValue clearValue =
    {
        { 0.0f, 0.0f, 0.0f, 0.0f }, // color
    };

    const VkRenderPassBeginInfo renderPassBeginInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,   // sType
        nullptr,                                    // pNext
        renderPass,                                 // renderPass
        framebuffer,                                // framebuffer
        renderArea,                                 // renderArea
        1,                                          // clearValueCount
        &clearValue                                 // pClearValues;
    };

    vkCmdBeginRenderPass(
        cmdBuffer.commandBuffer,        // commandBuffer
        &renderPassBeginInfo,           // pRenderPassBegin
        VK_SUBPASS_CONTENTS_INLINE);    // contents

    vkCmdBindPipeline(
        cmdBuffer.commandBuffer,            // commandBuffer
        VK_PIPELINE_BIND_POINT_GRAPHICS,    // pipelineBindPoint
        graphicsPipeline);                  // pipeline

    vkCmdDraw(
        cmdBuffer.commandBuffer,    // commandBuffer
        3,                          // vertexCount
        1,                          // instanceCount
        0,                          // firstVertex
        0);                         // firstInstance

    vkCmdEndRenderPass(cmdBuffer.commandBuffer);

    CHECK_VK_RESULT_SUCCESS(vkEndCommandBuffer(cmdBuffer.commandBuffer));

    // submit
    {
        vkResetFences(
            logicalDevice,      // device
            1,                  // fenceCount
            &cmdBuffer.fence);  // pFences

        constexpr VkPipelineStageFlags waitStageFlags =
        {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        const VkSubmitInfo submitInfo =
        {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,  // sType
            nullptr,                        // pNext
            1,                              // waitSemaphoreCount
            &swapchainImageSemaphore,       // pWaitSemaphores
            &waitStageFlags,                // pWaitDstStageMask
            1,                              // commandBufferCount
            &cmdBuffer.commandBuffer,       // pCommandBuffers
            1,                              // signalSemaphoreCount
            &cmdBuffer.submitSemaphore      // pSignalSemaphores
        };

        CHECK_VK_RESULT_SUCCESS(vkQueueSubmit(
            queue,              // queue
            1,                  // submitCount
            &submitInfo,        // pSubmits
            cmdBuffer.fence));  // fence
    }

    // present
    {
        const VkPresentInfoKHR presentInfo =
        {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // sType
            nullptr,                            // pNext
            1,                                  // waitSemaphoreCount
            &cmdBuffer.submitSemaphore,         // pWaitSemaphores
            1,                                  // swapchainCount
            &swapchain,                         // pSwapchains
            &currImageIndex,                    // pImageIndices
            nullptr                             // pResults
        };

        CHECK_VK_RESULT_SUCCESS(vkQueuePresentKHR(
            queue,          // queue
            &presentInfo)); // pPresentInfo
    }
}

void Renderer::renderCopyImages(GfxCmdBuffer::CmdBuffer& cmdBuffer)
{
    // create barriers
    std::vector<VkImageMemoryBarrier> preImageMemoryBarriers;
    std::vector<VkImageMemoryBarrier> postImageMemoryBarriers;
    for (uint32_t idx = 0; idx < m_imageSet.dirtyFlags.size(); ++idx)
    {
        if (m_imageSet.dirtyFlags[idx])
        {
            const VkImageSubresourceRange imageSubresourceRange =
            {
                VK_IMAGE_ASPECT_COLOR_BIT,  // aspectMask
                0,                          // baseMipLevel
                1,                          // levelCount
                0,                          // baseArrayLayer
                1,                          // layerCount
            };
            const VkImageMemoryBarrier imageMemoryBarrierPre =
            {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // sType
                nullptr,                                // pNext
                0,                                      // srcAccessMask
                VK_ACCESS_TRANSFER_WRITE_BIT,           // dstAccessMask
                VK_IMAGE_LAYOUT_UNDEFINED,              // oldLayout
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // newLayout
                VK_QUEUE_FAMILY_IGNORED,                // srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                // dstQueueFamilyIndex
                m_imageSet.images[idx]->image,          // image
                imageSubresourceRange                   // subresourceRange
            };
            preImageMemoryBarriers.emplace_back(std::move(imageMemoryBarrierPre));

            const VkImageMemoryBarrier imageMemoryBarrierPost =
            {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // sType
                nullptr,                                    // pNext
                VK_ACCESS_TRANSFER_WRITE_BIT,               // srcAccessMask
                VK_ACCESS_SHADER_READ_BIT,                  // dstAccessMask
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // oldLayout
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   // newLayout
                VK_QUEUE_FAMILY_IGNORED,                    // srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                    // dstQueueFamilyIndex
                m_imageSet.images[idx]->image,              // image
                imageSubresourceRange                       // subresourceRange
            };
            postImageMemoryBarriers.emplace_back(std::move(imageMemoryBarrierPost));
        }
    }

    vkCmdPipelineBarrier(
        cmdBuffer.commandBuffer,                    // commandBuffer
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,          // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // dstStageMask
        VK_DEPENDENCY_BY_REGION_BIT,                // dependencyFlags
        0,                                          // memoryBarrierCount
        nullptr,                                    // pMemoryBarriers
        0,                                          // bufferMemoryBarrierCount
        nullptr,                                    // pBufferMemoryBarriers
        (uint32_t)preImageMemoryBarriers.size(),    // imageMemoryBarrierCount
        preImageMemoryBarriers.data()               // pImageMemoryBarriers
    );

    // copy from staging buffers to gpu images
    for (uint32_t idx = 0; idx < m_imageSet.dirtyFlags.size(); ++idx)
    {
        if (m_imageSet.dirtyFlags[idx])
        {
            const VkExtent3D imageExtend = m_imageSet.images[idx]->size;
            const uint32_t bufferRowLength = imageExtend.width;
            const uint32_t bufferRowHeight = imageExtend.height;
            const VkImageSubresourceLayers imageSubresourceLayers =
            {
                VK_IMAGE_ASPECT_COLOR_BIT,  // aspectMask
                0,                          // mipLevel
                0,                          // baseArrayLayer
                1,                          // layerCount
            };
            const VkBufferImageCopy bufferImageCopy =
            {
                0,                      // bufferOffset
                bufferRowLength,        // bufferRowLength
                bufferRowHeight,        // bufferImageHeight
                imageSubresourceLayers, // imageSubresource
                {0,0,0},                // imageOffset
                imageExtend             // imageExtent
            };
            vkCmdCopyBufferToImage(
                cmdBuffer.commandBuffer,                    // commandBuffer
                m_imageSet.stagingBuffers[idx]->buffer,     // srcBuffer
                m_imageSet.images[idx]->image,              // dstImage
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // dstImageLayout
                1,                                          // regionCount
                &bufferImageCopy);                          // pRegions
        }
    }

    vkCmdPipelineBarrier(
        cmdBuffer.commandBuffer,                    // commandBuffer
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,      // dstStageMask
        VK_DEPENDENCY_BY_REGION_BIT,                // dependencyFlags
        0,                                          // memoryBarrierCount
        nullptr,                                    // pMemoryBarriers
        0,                                          // bufferMemoryBarrierCount
        nullptr,                                    // pBufferMemoryBarriers
        (uint32_t)postImageMemoryBarriers.size(),   // imageMemoryBarrierCount
        postImageMemoryBarriers.data()              // pImageMemoryBarriers
    );

    m_imageSet.dirty = false;
    for (auto&& flagsRef : m_imageSet.dirtyFlags)
    {
        flagsRef = false;
    }
}

void Renderer::createDescriptorsUniform()
{
    const uint32_t bufferByteSize = sizeof(ShaderInputUniform);

    m_descriptorSetUniform.reset(new DescriptorSet(
        mp_gfxResources->getDevice(),
        mp_gfxResources->getDescriptorPool()->uniforms,
        1,
        VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_SHADER_STAGE_FRAGMENT_BIT));

    m_gpuBufferUniform.reset(new GpuBufferUniform(mp_gfxDevice, bufferByteSize));

    // update the descriptor set

    const VkDescriptorBufferInfo descriptorBufferInfo =
    {
        m_gpuBufferUniform->buffer,     // buffer
        0,                              // offset
        m_gpuBufferUniform->byteSize    // range
    };

    const VkWriteDescriptorSet writeDescriptorSet =
    {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     // sType
        nullptr,                                    // pNext
        m_descriptorSetUniform->descriptorSet,      // dstSet
        0,                                          // dstBinding
        0,                                          // dstArrayElement
        1,                                          // descriptorCount
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,  // descriptorType
        nullptr,                                    // pImageInfo
        &descriptorBufferInfo,                      // pBufferInfo
        nullptr,                                    // pTexelBufferView
    };

    vkUpdateDescriptorSets(
        mp_gfxDevice->logicalDevice,   // device
        1,                          // descriptorWriteCount
        &writeDescriptorSet,        // pDescriptorWrites
        0,                          // descriptorCopyCount
        nullptr);                   // pDescriptorCopies
}

void Renderer::createDescriptorsImage()
{
    const uint32_t imageCount = (uint32_t)m_imageSet.images.size();

    m_descriptorSetImage.reset();
    m_descriptorSetImage.reset(new DescriptorSet(
        mp_gfxResources->getDevice(),
        mp_gfxResources->getDescriptorPool()->images,
        imageCount,
        VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT));

    std::vector<VkDescriptorImageInfo> descriptorImageInfoArray(imageCount);
    for (uint32_t idx = 0; idx < descriptorImageInfoArray.size(); ++idx)
    {
        const VkDescriptorImageInfo descriptorImageInfo =
        {
            m_imageSet.images[idx]->sampler,       // sampler
            m_imageSet.images[idx]->imageView,     // imageView
            m_imageSet.images[idx]->imageLayout    // imageLayout
        };
        descriptorImageInfoArray[idx] = descriptorImageInfo;
    }

    const VkWriteDescriptorSet writeDescriptorSet =
    {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     // sType
        nullptr,                                    // pNext
        m_descriptorSetImage->descriptorSet,        // dstSet
        0,                                          // dstBinding
        0,                                          // dstArrayElement
        imageCount,                                 // descriptorCount
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // descriptorType
        descriptorImageInfoArray.data(),            // pImageInfo
        nullptr,                                    // pBufferInfo
        nullptr,                                    // pTexelBufferView
    };

    vkUpdateDescriptorSets(
        mp_gfxDevice->logicalDevice,// device
        1,                          // descriptorWriteCount
        &writeDescriptorSet,        // pDescriptorWrites
        0,                          // descriptorCopyCount
        nullptr);                   // pDescriptorCopies
}

void Renderer::createRenderPasses()
{
    GfxSwapchain* const gfxSwapchain = mp_gfxResources->getSwapchain();
    const VkAttachmentDescription attachments[] =
    {
        {
            0,                                  // flags
            gfxSwapchain->imageFormat,          // format
            VK_SAMPLE_COUNT_1_BIT,              // samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,        // loadOp
            VK_ATTACHMENT_STORE_OP_STORE,       // storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,    // stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,   // stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,          // initialLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR     // finalLayout
        }
    };

    constexpr VkAttachmentReference attachmentReferences[] =
    {
        {
            0,                                          // attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // layout
        }
    };

    constexpr VkSubpassDescription subpassDescription =
    {
        0,                              // flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,// pipelineBindPoint
        0,                              // inputAttachmentCount
        nullptr,                        // pInputAttachments
        1,                              // colorAttachmentCount
        &attachmentReferences[0],       // pColorAttachments
        nullptr,                        // pResolveAttachments
        nullptr,                        // pDepthStencilAttachment
        0,                              // preserveAttachmentCount
        nullptr                         // pPreserveAttachments
    };

    const VkRenderPassCreateInfo createInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,  // sType
        nullptr,                                    // pNext
        0,                                          // flags
        1,                                          // attachmentCount
        &attachments[0],                            // pAttachments
        1,                                          // subpassCount
        &subpassDescription,                        // pSubpasses
        0,                                          // dependencyCount
        nullptr,                                    // pDependencies
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateRenderPass(
        mp_gfxDevice->logicalDevice,   // device
        &createInfo,                // pCreateInfo
        nullptr,                    // pAllocator
        &m_renderPass));            // pRenderPass
}

void Renderer::createFramebuffers()
{
    // create framebuffers for swapchain image views
    GfxSwapchain* const gfxSwapchain = mp_gfxResources->getSwapchain();
    m_framebuffers.resize(gfxSwapchain->imageViews.size());
    for (uint32_t idx = 0; idx < m_framebuffers.size(); ++idx)
    {
        const VkFramebufferCreateInfo framebufferCreateInfo =
        {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  // sType
            nullptr,                                    // pNext
            0,                                          // flags
            m_renderPass,                               // renderPass
            1,                                          // attachmentCount
            &gfxSwapchain->imageViews[idx],             // pAttachments
            mp_window->getWidth(),                      // width
            mp_window->getHeight(),                     // height
            1,                                          // layers
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateFramebuffer(
            mp_gfxDevice->logicalDevice,   // device
            &framebufferCreateInfo,     // pCreateInfo
            nullptr,                    // pAllocator
            &m_framebuffers[idx]));     // pFramebuffer
    }
}

void Renderer::createGraphicsPipeline()
{
    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo[] =
    {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,    // sType
            nullptr,                                                // pNext
            0,                                                      // flags
            VK_SHADER_STAGE_VERTEX_BIT,                             // stage
            m_shader->vert,                                         // module
            "main",                                                 // pName
            nullptr                                                 // pSpecializationInfo
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,    // sType
            nullptr,                                                // pNext
            0,                                                      // flags
            VK_SHADER_STAGE_FRAGMENT_BIT,                           // stage
            m_shader->frag,                                         // module
            "main",                                                 // pName
            nullptr                                                 // pSpecializationInfo
        }
    };

    constexpr VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        0,                                                          // vertexBindingDescriptionCount
        nullptr,                                                    // pVertexBindingDescriptions
        0,                                                          // vertexAttributeDescriptionCount
        nullptr                                                     // pVertexAttributeDescriptions
    };

    constexpr VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,    // sType
        nullptr,                                                        // pNext
        0,                                                              // flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                            // topology
        VK_FALSE                                                        // primitiveRestartEnable
    };

    const uint32_t width = mp_window->getWidth();
    const uint32_t height = mp_window->getHeight();

    const VkViewport viewport =
    {
        0.0f,           // x
        0.0f,           // y
        (float)width,   // width
        (float)height,  // height
        0.0f,           // minDepth
        1.0f,           // maxDepth
    };

    const VkRect2D scissor =
    {
        { 0, 0 },           // offset
        { width, height }   // extent
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  // sType
        nullptr,                                                // pNext
        0,                                                      // flags
        1,                                                      // viewportCount
        &viewport,                                              // pViewports
        1,                                                      // scissorCount
        &scissor                                                // pScissors
    };

    constexpr VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FALSE,                                                   // depthClampEnable
        VK_FALSE,                                                   // rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,                                       // polygonMode
        VK_CULL_MODE_NONE,                                          // cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // frontFace
        VK_FALSE,                                                   // depthBiasEnable
        0.0f,                                                       // depthBiasConstantFactor
        0.0f,                                                       // depthBiasClamp
        0.0f,                                                       // depthBiasSlopeFactor
        1.0f                                                        // lineWidth
    };

    constexpr VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
    {
        VK_FALSE,                                                   // blendEnable
        VK_BLEND_FACTOR_ZERO,                                       // srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,                                       // dstColorBlendFactor
        VK_BLEND_OP_ADD,                                            // colorBlendOp
        VK_BLEND_FACTOR_ZERO,                                       // srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,                                       // dstAlphaBlendFactor
        VK_BLEND_OP_ADD,                                            // alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,  // colorWriteMask
    };

    constexpr VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FALSE,                                                   // logicOpEnable
        VK_LOGIC_OP_COPY,                                           // logicOp
        1,                                                          // attachmentCount
        &colorBlendAttachmentState,                                 // pAttachments
        { 0.0f, 0.0f, 0.0f, 0.0f }                                  // blendConstants[4]
    };

    constexpr VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SAMPLE_COUNT_1_BIT,                                      // rasterizationSamples
        VK_FALSE,                                                   // sampleShadingEnable
        1.0f,                                                       // minSampleShading
        nullptr,                                                    // pSampleMask
        VK_FALSE,                                                   // alphaToCoverageEnable
        VK_FALSE,                                                   // alphaToOneEnable
    };

    std::vector<VkDescriptorSetLayout> setLayouts
    { m_descriptorSetUniform->descriptorSetLayout, m_descriptorSetImage->descriptorSetLayout };
    
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,  // sType
        nullptr,                                        // pNext
        0,                                              // flags
        (uint32_t)setLayouts.size(),                    // setLayoutCount
        setLayouts.data(),                              // pSetLayouts
        0,                                              // pushConstantRangeCount
        nullptr                                         // pPushConstantRanges
    };

    CHECK_VK_RESULT_SUCCESS(vkCreatePipelineLayout(
        mp_gfxDevice->logicalDevice,   // device
        &pipelineLayoutCreateInfo,  // pCreateInfo
        nullptr,                    // pAllocator,
        &m_pipelineLayout));        // pPipelineLayout

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo =
    {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,    // sType
        nullptr,                        // pNext
        0,                              // flags
        2,                              // stageCount
        &shaderStageCreateInfo[0],      // pStages
        &vertexInputStateCreateInfo,    // pVertexInputState
        &inputAssemblyStateCreateInfo,  // pInputAssemblyState
        nullptr,                        // pTessellationState
        &viewportStateCreateInfo,       // pViewportState
        &rasterizationStateCreateInfo,  // pRasterizationState
        &multisampleStateCreateInfo,    // pMultisampleState
        nullptr,                        // pDepthStencilState
        &colorBlendStateCreateInfo,     // pColorBlendState
        nullptr,                        // pDynamicState
        m_pipelineLayout,               // layout
        m_renderPass,                   // renderPass
        0,                              // subpass
        nullptr,                        // basePipelineHandle
        0                               // basePipelineIndex
    };

    CHECK_VK_RESULT_SUCCESS(vkCreateGraphicsPipelines(
        mp_gfxDevice->logicalDevice,   // device
        nullptr,                    // pipelineCache
        1,                          // createInfoCount
        &pipelineCreateInfo,        // pCreateInfos
        nullptr,                    // pAllocator
        &m_graphicsPipeline));      // pPipelines
}

void Renderer::resizeFramebuffer()
{
    mp_gfxResources->waitForIdle();

    for (uint32_t idx = 0; idx < m_framebuffers.size(); ++idx)
    {
        vkDestroyFramebuffer(mp_gfxDevice->logicalDevice,
            m_framebuffers[idx], nullptr);
    }
    vkDestroyPipelineLayout(mp_gfxDevice->logicalDevice, m_pipelineLayout, nullptr);
    vkDestroyPipeline(mp_gfxDevice->logicalDevice, m_graphicsPipeline, nullptr);

    createGraphicsPipeline();
    createFramebuffers();
}

void Renderer::createImages()
{
    ResourceList& rl = ResourceList::getInstance();
    const uint32_t imageCount = (uint32_t)rl.imageFiles.size();
    m_imageSet.stagingBuffers.resize(imageCount);
    m_imageSet.images.resize(imageCount);
    m_imageSet.dirtyFlags.resize(imageCount);
    for (uint32_t idx = 0; idx < imageCount; ++idx)
    {
        createImage(idx, rl.imagePath + "/" + rl.imageFiles[idx]);
    }
    m_imageSet.dirty = true;
}

void Renderer::createImage(const uint32_t index, const std::string& filename)
{
    ImageLoader imgLoader(filename);

    if (imgLoader.getBytesize() > 0)
    {
        m_imageSet.stagingBuffers[index].reset(new GpuBufferStaging(
            mp_gfxDevice,
            imgLoader.getBytesize(),
            imgLoader.getData()));

        m_imageSet.images[index].reset(new GpuImage(
            mp_gfxDevice,
            std::get<0>(imgLoader.getSize()),
            std::get<1>(imgLoader.getSize()),
            VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT));

        m_imageSet.dirtyFlags[index] = true;
    }
}

bool Renderer::createShaders(const bool fromGlsl)
{
    ResourceList& rl = ResourceList::getInstance();
    assert(rl.shaderFiles.size() >= 2);

    bool valid = false;
    ShaderFiles shaderFiles;
    if (fromGlsl)
    {
        shaderFiles.vertShader = rl.shaderPath + "/" + rl.shaderFiles[0];
        shaderFiles.fragShader = rl.shaderPath + "/" + rl.shaderFiles[1];
        shaderFiles.shaderFileTypes = ShaderFiles::ShaderFileTypes::glsl;
    }
    else
    {
        shaderFiles.vertShader = rl.shaderPath + "/" + rl.spirvFiles[0];
        shaderFiles.fragShader = rl.shaderPath + "/" + rl.spirvFiles[1];
        shaderFiles.shaderFileTypes = ShaderFiles::ShaderFileTypes::spirv;
    }
    std::unique_ptr<Shader> newShader(new Shader(mp_gfxDevice, shaderFiles));
    if (newShader->vert != nullptr && newShader->frag != nullptr)
    {
        m_shader.reset(newShader.release());
        valid = true;
    }
    return valid;
}

void Renderer::updateImages(const std::vector<std::string>& imageNames)
{
    mp_gfxResources->waitForIdle(); // no buffering for resources, need to wait

    ResourceList& rl = ResourceList::getInstance();
    std::cout << "Texture file(s) changed ( ";
    for (const auto& nameRef : imageNames)
    {
        std::cout << nameRef << " ";
        std::string compareName = nameRef;
        const size_t lastIndex = compareName.find_last_of(".");
        if (lastIndex != std::string::npos)
        {
            compareName = compareName.substr(0, lastIndex);
        }
        for (uint32_t idx = 0; idx < rl.imageFilesForSearch.size(); ++idx)
        {
            if (compareName == rl.imageFilesForSearch[idx])
            {
                createImage(idx, rl.imagePath + "/" + nameRef);
                m_imageSet.dirty = true;
            }
        }
    }

    std::cout << ")." << std::endl;
    std::cout << "New image data created." << std::endl;

    createDescriptorsImage();
}

void Renderer::updateShaders(const std::vector<std::string>& shaderNames)
{
    mp_gfxResources->waitForIdle(); // no buffering for resources, need to wait

    std::cout << "Shader file(s) changed ( ";
    for (const auto& shaderNameRef : shaderNames)
    {
        std::cout << shaderNameRef << " ";
    }
    std::cout << "). Compiling..." << std::endl;

    const bool valid = createShaders(true);

    if (valid)
    {
        // destroy old before creating new
        vkDestroyPipelineLayout(mp_gfxDevice->logicalDevice, m_pipelineLayout, nullptr);
        vkDestroyPipeline(mp_gfxDevice->logicalDevice, m_graphicsPipeline, nullptr);

        createGraphicsPipeline();

        std::cout << "Done." << std::endl;
    }
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
