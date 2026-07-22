#pragma once

#include "catsystem2/CS2_BinaryIO.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace catsystem2
{

struct Image final
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    Bytes bgra;
};

Image decode_image_file(const std::filesystem::path& path);
Image decode_jpeg(std::span<const std::uint8_t> encoded, std::span<const std::uint8_t> alpha = {});
void encode_png(const std::filesystem::path& path, const Image& image);

}
