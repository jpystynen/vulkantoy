#ifndef CORE_SHADER_H
#define CORE_SHADER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <string>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class GfxDevice;

struct ShaderFiles
{
    std::string vertShader;
    std::string fragShader;

    enum class ShaderFileTypes : uint32_t
    {
        spirv = 0,
        glsl = 1,
    };
    ShaderFileTypes shaderFileTypes = ShaderFileTypes::spirv;
};

class Shader
{
public:
    Shader(GfxDevice* const p_gfxDevice, const ShaderFiles& shaderFiles);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    VkShaderModule vert = nullptr;
    VkShaderModule frag = nullptr;
private:
    GfxDevice* const mp_gfxDevice = nullptr;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_SHADER_H
