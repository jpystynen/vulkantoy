// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Shader.h"

#include "GfxResources.h"
#include "ShaderCompiler.h"

#include <cstdint>
#include <assert.h>
#include <iostream>
#include <fstream>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

static VkShaderModule createShaderModule(
    GfxDevice* p_gfxDevice,
    const std::string& shaderFile)
{
    assert(p_gfxDevice);
    VkShaderModule shaderModule = nullptr;

    std::ifstream file(shaderFile, std::ios::ate | std::ios::binary);
    assert(file.is_open() && "Spirv file not found. Correct working dir?");

    if (file.is_open())
    {
        const size_t fileSize = file.tellg();
        std::vector<char> binaryShader(fileSize);
        file.seekg(0);
        file.read(binaryShader.data(), fileSize);
        file.close();

        const VkShaderModuleCreateInfo shaderModuleCreateInfo =
        {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,// sType
            nullptr,                                    // pNext
            0,                                          // flags
            binaryShader.size(),                        // codeSize
            (uint32_t*)(binaryShader.data())            // pCode
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateShaderModule(
            p_gfxDevice->logicalDevice, // device
            &shaderModuleCreateInfo,    // pCreateInfo
            nullptr,                    // pAllocator
            &shaderModule));            // pShaderModule
    }

    return shaderModule;
}

static VkShaderModule createShaderModuleFromGlsl(
    GfxDevice* p_gfxDevice,
    const std::string& shaderFile,
    const VkShaderStageFlagBits shaderStage)
{
    assert(p_gfxDevice);
    VkShaderModule shaderModule = nullptr;

    ShaderCompiler shaderCompiler(p_gfxDevice);
    ShaderCompiler::ShaderCompileData scd =
        std::move(shaderCompiler.compileShader(shaderFile, shaderStage));
    if (scd.valid)
    {
        const VkShaderModuleCreateInfo shaderModuleCreateInfo =
        {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,// sType
            nullptr,                                    // pNext
            0,                                          // flags
            scd.data.size()*sizeof(uint32_t),           // codeSize
            (uint32_t*)(scd.data.data())                // pCode
        };

        CHECK_VK_RESULT_SUCCESS(vkCreateShaderModule(
            p_gfxDevice->logicalDevice, // device
            &shaderModuleCreateInfo,    // pCreateInfo
            nullptr,                    // pAllocator
            &shaderModule));            // pShaderModule
    }

    return shaderModule;
}

///////////////////////////////////////////////////////////////////////////////

Shader::Shader(GfxDevice* const p_gfxDevice,
    const ShaderFiles& shaderFiles)
    : mp_gfxDevice(p_gfxDevice)
{
    assert(mp_gfxDevice);

    if (shaderFiles.shaderFileTypes == ShaderFiles::ShaderFileTypes::glsl)
    {
        vert = createShaderModuleFromGlsl(mp_gfxDevice,
            shaderFiles.vertShader, VK_SHADER_STAGE_VERTEX_BIT);
        frag = createShaderModuleFromGlsl(mp_gfxDevice,
            shaderFiles.fragShader, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    else
    {
        vert = createShaderModule(mp_gfxDevice, shaderFiles.vertShader);
        frag = createShaderModule(mp_gfxDevice, shaderFiles.fragShader);
    }
}

Shader::~Shader()
{
    if (mp_gfxDevice)
    {
        vkDestroyShaderModule(mp_gfxDevice->logicalDevice, vert, nullptr);
        vkDestroyShaderModule(mp_gfxDevice->logicalDevice, frag, nullptr);
    }
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
