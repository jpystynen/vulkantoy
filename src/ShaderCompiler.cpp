// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "ShaderCompiler.h"

#include "GfxResources.h"

#include <cstdint>
#include <assert.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <vulkan/vulkan.h>
#include "glslang/glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/GlslangToSpv.h"

///////////////////////////////////////////////////////////////////////////////

namespace core
{

static TBuiltInResource initResources(
    GfxDevice* const /*p_gfxDevice*/)
{
    // to get device specific limits
    //VkPhysicalDeviceLimits& limits = p_gfxDevice->physicalDeviceProperties.limits;

    TBuiltInResource resources{};
    resources.maxLights                                 = 32;
    resources.maxClipPlanes                             = 6;
    resources.maxTextureUnits                           = 32;
    resources.maxTextureCoords                          = 32;
    resources.maxVertexAttribs                          = 64;
    resources.maxVertexUniformComponents                = 4096;
    resources.maxVaryingFloats                          = 64;
    resources.maxVertexTextureImageUnits                = 32;
    resources.maxCombinedTextureImageUnits              = 80;
    resources.maxTextureImageUnits                      = 32;
    resources.maxFragmentUniformComponents              = 4096;
    resources.maxDrawBuffers                            = 32;
    resources.maxVertexUniformVectors                   = 128;
    resources.maxVaryingVectors                         = 8;
    resources.maxFragmentUniformVectors                 = 16;
    resources.maxVertexOutputVectors                    = 16;
    resources.maxFragmentInputVectors                   = 15;
    resources.minProgramTexelOffset                     = -8;
    resources.maxProgramTexelOffset                     = 7;
    resources.maxClipDistances                          = 8;
    resources.maxComputeWorkGroupCountX                 = 65535;
    resources.maxComputeWorkGroupCountY                 = 65535;
    resources.maxComputeWorkGroupCountZ                 = 65535;
    resources.maxComputeWorkGroupSizeX                  = 1024;
    resources.maxComputeWorkGroupSizeY                  = 1024;
    resources.maxComputeWorkGroupSizeZ                  = 64;
    resources.maxComputeUniformComponents               = 1024;
    resources.maxComputeTextureImageUnits               = 16;
    resources.maxComputeImageUniforms                   = 8;
    resources.maxComputeAtomicCounters                  = 8;
    resources.maxComputeAtomicCounterBuffers            = 1;
    resources.maxVaryingComponents                      = 60;
    resources.maxVertexOutputComponents                 = 64;
    resources.maxGeometryInputComponents                = 64;
    resources.maxGeometryOutputComponents               = 128;
    resources.maxFragmentInputComponents                = 128;
    resources.maxImageUnits                             = 8;
    resources.maxCombinedImageUnitsAndFragmentOutputs   = 8;
    resources.maxCombinedShaderOutputResources          = 8;
    resources.maxImageSamples                           = 0;
    resources.maxVertexImageUniforms                    = 0;
    resources.maxTessControlImageUniforms               = 0;
    resources.maxTessEvaluationImageUniforms            = 0;
    resources.maxGeometryImageUniforms                  = 0;
    resources.maxFragmentImageUniforms                  = 8;
    resources.maxCombinedImageUniforms                  = 8;
    resources.maxGeometryTextureImageUnits              = 16;
    resources.maxGeometryOutputVertices                 = 256;
    resources.maxGeometryTotalOutputComponents          = 1024;
    resources.maxGeometryUniformComponents              = 1024;
    resources.maxGeometryVaryingComponents              = 64;
    resources.maxTessControlInputComponents             = 128;
    resources.maxTessControlOutputComponents            = 128;
    resources.maxTessControlTextureImageUnits           = 16;
    resources.maxTessControlUniformComponents           = 1024;
    resources.maxTessControlTotalOutputComponents       = 4096;
    resources.maxTessEvaluationInputComponents          = 128;
    resources.maxTessEvaluationOutputComponents         = 128;
    resources.maxTessEvaluationTextureImageUnits        = 16;
    resources.maxTessEvaluationUniformComponents        = 1024;
    resources.maxTessPatchComponents                    = 120;
    resources.maxPatchVertices                          = 32;
    resources.maxTessGenLevel                           = 64;
    resources.maxViewports                              = 16;
    resources.maxVertexAtomicCounters                   = 0;
    resources.maxTessControlAtomicCounters              = 0;
    resources.maxTessEvaluationAtomicCounters           = 0;
    resources.maxGeometryAtomicCounters                 = 0;
    resources.maxFragmentAtomicCounters                 = 8;
    resources.maxCombinedAtomicCounters                 = 8;
    resources.maxAtomicCounterBindings                  = 1;
    resources.maxVertexAtomicCounterBuffers             = 0;
    resources.maxTessControlAtomicCounterBuffers        = 0;
    resources.maxTessEvaluationAtomicCounterBuffers     = 0;
    resources.maxGeometryAtomicCounterBuffers           = 0;
    resources.maxFragmentAtomicCounterBuffers           = 1;
    resources.maxCombinedAtomicCounterBuffers           = 1;
    resources.maxAtomicCounterBufferSize                = 16384;
    resources.maxTransformFeedbackBuffers               = 4;
    resources.maxTransformFeedbackInterleavedComponents = 64;
    resources.maxCullDistances                          = 8;
    resources.maxCombinedClipAndCullDistances           = 8;
    resources.maxSamples                                = 4;
    resources.limits.nonInductiveForLoops               = 1;
    resources.limits.whileLoops                         = 1;
    resources.limits.doWhileLoops                       = 1;
    resources.limits.generalUniformIndexing             = 1;
    resources.limits.generalAttributeMatrixVectorIndexing   = 1;
    resources.limits.generalVaryingIndexing                 = 1;
    resources.limits.generalSamplerIndexing                 = 1;
    resources.limits.generalVariableIndexing                = 1;
    resources.limits.generalConstantMatrixVectorIndexing    = 1;

    return resources;
}

static EShLanguage vkStageToEsh(const VkShaderStageFlagBits shaderStage)
{
    switch (shaderStage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return EShLangVertex;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return EShLangFragment;
    default:
        std::cerr << "Shader stage not supported" << std::endl;
        assert(false);
        return EShLangVertex;
    }
}

static bool glslToSpv(
    GfxDevice* p_gfxDevice,
    const std::string& glslShaderStr,
    const VkShaderStageFlagBits shaderStage,
    std::vector<uint32_t>& spirv)
{
    TBuiltInResource resources = std::move(initResources(p_gfxDevice));

    const EShLanguage stage = vkStageToEsh(shaderStage);
    glslang::TShader shader(stage);
    std::vector<const char*> shaderStrings {glslShaderStr.c_str()};
    shader.setStrings(shaderStrings.data(), 1);

    // enable spirv and vulkan rules
    constexpr EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    constexpr uint32_t defaultVersion = 100;
    const bool compileResult = shader.parse(
        &resources,     // buildInResource
        defaultVersion, // defaultVersion
        false,          // forwardCompatible
        messages);      // eshMessageChoices
    if (!compileResult)
    {
        std::cerr << shader.getInfoLog() << std::endl;
        std::cerr << shader.getInfoDebugLog() << std::endl;
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    const bool linkResult = program.link(messages);
    if (!linkResult)
    {
        std::cerr << shader.getInfoLog() << std::endl;
        std::cerr << shader.getInfoDebugLog() << std::endl;
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

    return true;
}

ShaderCompiler::ShaderCompiler(GfxDevice* const p_gfxDevice)
    : mp_gfxDevice(p_gfxDevice)
{
    assert(p_gfxDevice);

    glslang::InitializeProcess();
}

ShaderCompiler::~ShaderCompiler()
{
    glslang::FinalizeProcess();
}

ShaderCompiler::ShaderCompileData ShaderCompiler::compileShader(
    const std::string& shaderFile,
    const VkShaderStageFlagBits shaderStage)
{
    std::ifstream file(shaderFile, std::ios::in);
    assert(file.is_open() && "Shader file not found. Correct working dir set?");

    ShaderCompileData scd{};

    if (file.is_open())
    {
        std::string strShader = std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>());
        file.close();

        scd.valid = glslToSpv(
            mp_gfxDevice,
            strShader,
            shaderStage,
            scd.data);
    }

    return scd;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
