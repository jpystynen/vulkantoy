// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "ImageLoader.h"

#pragma warning(push)
#pragma warning(disable : 4244) // conversion from int to stbi_uc
#pragma warning(disable : 4456) // declaration hides previous local declaration
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb/stb_image.h"
#pragma warning(pop)

#include <cstdint>
#include <assert.h>
#include <iostream>
#include <tuple>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

ImageLoader::ImageLoader(const std::string& imagepath)
{
    loadStbImage(imagepath);
}

void ImageLoader::loadStbImage(const std::string& imagepath)
{
    int x = 0;  // width
    int y = 0;  // height
    int n = 0;  // channels per pixel
    uint8_t* p_data = stbi_load(imagepath.c_str(), &x, &y, &n, STBI_rgb_alpha);

    if (p_data != nullptr)
    {
        m_size = std::make_tuple(x, y);
        m_channelCount = 4; // forced

        m_bytesize = x * y * m_channelCount; // one channel one byte
        m_byteData.resize(m_bytesize);

        std::memcpy(m_byteData.data(), p_data, m_bytesize);
    }
    else
    {
        std::cerr << "image file not found: " << imagepath;
    }

    stbi_image_free(p_data);
}

uint8_t* ImageLoader::getData()
{
    return m_byteData.data();
}

uint32_t ImageLoader::getBytesize() const
{
    return m_bytesize;
}

std::tuple<uint32_t, uint32_t> ImageLoader::getSize() const
{
    return m_size;
}

uint32_t ImageLoader::getChannelCount() const
{
    return m_channelCount;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
