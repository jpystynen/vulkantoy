#ifndef CORE_RENDERER_H
#define CORE_RENDERER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "GfxResources.h"
#include "Window.h"

#include <memory>
#include <vector>
#include <cstdint>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class DescriptorSet;
class GpuBufferUniform;
class GpuBufferStaging;
class GpuImage;
class Shader;

class RendererInput
{
public:
    float date[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
    MousePos mousePos{};

    float globalTime    = 0.0f;
    float deltaTime     = 0.0f;

    uint32_t frameIndex = 0;
};

class Renderer
{
public:
    Renderer(GfxResources* const p_gfxResources, Window* const p_window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void render(const RendererInput& rendererInput);

    void resizeFramebuffer();
    void updateShaders(const std::vector<std::string>& shaderNames);
    void updateImages(const std::vector<std::string>& imageNames);

private:
    const uint32_t c_bufferingCount = 3;

    void renderCopyImages(GfxCmdBuffer::CmdBuffer& p_cmdBuffer);

    void createImages();
    void createImage(const uint32_t index, const std::string& filename);
    bool createShaders(const bool fromGlsl);

    void createDescriptorsUniform();
    void createDescriptorsImage();
    void createRenderPasses();
    void createFramebuffers();
    void createGraphicsPipeline();

    GfxResources* const mp_gfxResources = nullptr;
    GfxDevice* const mp_gfxDevice       = nullptr;
    Window* const mp_window             = nullptr;

    VkRenderPass m_renderPass           = nullptr;
    VkPipeline m_graphicsPipeline       = nullptr;
    VkPipelineLayout m_pipelineLayout   = nullptr;

    struct ShaderInputUniform
    {
        float iChannelResolution[4][4];
        float iMouse[4];
        float iDate[4];
        float iResolution[4];

        float iChannelTime[4];

        float iGlobalDelta;
        float iGlobalFrame;
        float iGlobalTime;
        float iSampleRate;
    };

    std::unique_ptr<DescriptorSet> m_descriptorSetUniform;
    std::unique_ptr<GpuBufferUniform> m_gpuBufferUniform;

    std::unique_ptr<DescriptorSet> m_descriptorSetImage;

    std::vector<VkFramebuffer> m_framebuffers;

    struct ImageSet
    {
        std::vector<std::unique_ptr<GpuBufferStaging> > stagingBuffers;
        std::vector<std::unique_ptr<GpuImage> > images;
        std::vector<bool> dirtyFlags;

        bool dirty = false; // some images need copying
    };
    ImageSet m_imageSet;

    std::unique_ptr<Shader> m_shader;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_RENDERER_H
