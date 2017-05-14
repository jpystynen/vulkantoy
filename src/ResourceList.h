#ifndef CORE_RESOURCE_LIST_H
#define CORE_RESOURCE_LIST_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <vector>
#include <string>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class ResourceList
{
public:
    ResourceList(const ResourceList&) = delete;
    ResourceList& operator=(const ResourceList&) = delete;

    static ResourceList& getInstance()
    {
        static ResourceList instance;
        return instance;
    }

    const std::string imagePath { "textures" };
    const std::vector<std::string> imageFiles
    { "channel0.png", "channel1.png", "channel2.png", "channel3.png" };
    const std::vector<std::string> imageFilesForSearch
    { "channel0", "channel1", "channel2", "channel3" };

    const std::string shaderPath { "shaders" };
    const std::vector<std::string> shaderFiles { "toy.vert", "toy.frag" };
    const std::vector<std::string> spirvFiles { "toy.vert.spv", "toy.frag.spv" };

private:
    ResourceList() = default;
    ~ResourceList() = default;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_RESOURCE_LIST_H
