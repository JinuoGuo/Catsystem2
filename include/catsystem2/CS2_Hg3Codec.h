#pragma once

#include "catsystem2/CS2_BinaryIO.h"

#include <cstdint>
#include <span>

namespace catsystem2::hg3
{

struct EncodedPixels final
{
    Bytes data;
    Bytes map;
};

Bytes decode_pixels(std::span<const std::uint8_t> data, std::span<const std::uint8_t> map, std::uint32_t width, std::uint32_t height);
EncodedPixels encode_pixels(std::span<const std::uint8_t> bgra, std::uint32_t width, std::uint32_t height);

}
