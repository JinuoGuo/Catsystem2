#pragma once

#include "catsystem2/CS2_BinaryIO.h"

#include <cstddef>
#include <span>

namespace catsystem2
{

Bytes zlib_compress(std::span<const std::uint8_t> input, int level = 9);
Bytes zlib_decompress(std::span<const std::uint8_t> input, std::size_t expected_size);

}
