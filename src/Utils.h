#ifndef CORE_UTILS_H
#define CORE_UTILS_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class GlobalVariables
{
public:
    GlobalVariables(const GlobalVariables&) = delete;
    GlobalVariables& operator=(const GlobalVariables&) = delete;

    static GlobalVariables& getInstance()
    {
        static GlobalVariables instance;
        return instance;
    }

    std::string applicationName     = "ApplicationName";
    std::string engineName          = "Core";
    uint32_t applicationVersion     = VK_MAKE_VERSION(1, 0, 0);
    uint32_t engineVersion          = VK_MAKE_VERSION(1, 0, 0);
    uint32_t apiVersion             = VK_API_VERSION_1_0;

    uint32_t windowWidth            = 1280;
    uint32_t windowHeight           = 720;

private:
    GlobalVariables() = default;
    ~GlobalVariables() = default;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_UTILS_H
