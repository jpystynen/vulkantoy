#ifndef CORE_SHADER_COMPILER_H
#define CORE_SHADER_COMPILER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class GfxDevice;

class ShaderCompiler
{
public:
    struct ShaderCompileData
    {
        std::vector<uint32_t> data;
        bool valid = false;
    };

    ShaderCompiler(GfxDevice* const p_gfxDevice);
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    ShaderCompileData compileShader(
        const std::string& shaderFile,
        const VkShaderStageFlagBits shaderStage);

private:
    GfxDevice* const mp_gfxDevice = nullptr;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_SHADER_COMPILER_H
