#ifndef CORE_IMAGE_LOADER_H
#define CORE_IMAGE_LOADER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class ImageLoader
{
public:
    ImageLoader(const std::string& imagepath);
    ~ImageLoader() = default;

    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    uint32_t getBytesize() const;
    uint8_t* getData();

    std::tuple<uint32_t, uint32_t> getSize() const;
    uint32_t getChannelCount() const;

private:
    void loadStbImage(const std::string& imagePath);

    std::tuple<uint32_t, uint32_t> m_size;
    uint32_t m_channelCount = 0;
    uint32_t m_channelDepth = 8;
    uint32_t m_bytesize     = 0;

    std::vector<uint8_t> m_byteData;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_IMAGE_LOADER_H
