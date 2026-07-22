#include "catsystem2/CS2_Hg3Codec.h"

#include "catsystem2/CS2_Error.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <string>
#include <vector>

namespace catsystem2::hg3
{
namespace
{

std::size_t checked_image_size(const std::uint32_t width, const std::uint32_t height)
{
    if(width == 0 || height == 0 || static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / 4U / height)
    {
        throw Error("invalid HG3 image dimensions");
    }
    return static_cast<std::size_t>(width) * height * 4U;
}

std::uint8_t reverse_pairs(const std::uint8_t value)
{
    return static_cast<std::uint8_t>(((value & 0x03U) << 6U) | ((value & 0x0cU) << 2U) | ((value & 0x30U) >> 2U) | ((value & 0xc0U) >> 6U));
}

std::uint8_t reverse_bits(std::uint8_t value)
{
    value = static_cast<std::uint8_t>(((value & 0x55U) << 1U) | ((value & 0xaaU) >> 1U));
    value = static_cast<std::uint8_t>(((value & 0x33U) << 2U) | ((value & 0xccU) >> 2U));
    return static_cast<std::uint8_t>((value << 4U) | (value >> 4U));
}

std::vector<std::uint8_t> bytes_to_bits(const std::span<const std::uint8_t> bytes)
{
    std::vector<std::uint8_t> bits;
    bits.reserve(bytes.size() * 8U);
    for(const auto byte : bytes)
    {
        for(int shift = 7; shift >= 0; --shift)
        {
            bits.push_back(static_cast<std::uint8_t>((byte >> shift) & 1U));
        }
    }
    return bits;
}

std::uint32_t read_gamma(const std::span<const std::uint8_t> bits, std::size_t& position)
{
    std::size_t zero_count = 0;
    while(position < bits.size() && bits[position] == 0)
    {
        ++zero_count;
        ++position;
    }
    if(position == bits.size() || zero_count >= 32U || position + zero_count >= bits.size())
    {
        throw Error("invalid Elias gamma code in HG3 map");
    }
    std::uint32_t value = 1;
    ++position;
    for(std::size_t index = 0; index < zero_count; ++index)
    {
        value = static_cast<std::uint32_t>((value << 1U) | bits[position++]);
    }
    return value;
}

void append_gamma(std::vector<std::uint8_t>& bits, const std::uint32_t value)
{
    if(value == 0)
    {
        throw Error("Elias gamma coding cannot encode zero");
    }
    const unsigned significant_bits = std::bit_width(value);
    bits.insert(bits.end(), significant_bits - 1U, 0);
    for(int shift = static_cast<int>(significant_bits) - 1; shift >= 0; --shift)
    {
        bits.push_back(static_cast<std::uint8_t>((value >> shift) & 1U));
    }
}

Bytes pack_map_bits(std::vector<std::uint8_t> bits)
{
    bits.resize((bits.size() + 7U) / 8U * 8U, 0);
    Bytes result(bits.size() / 8U, 0);
    for(std::size_t index = 0; index < bits.size(); ++index)
    {
        result[index / 8U] |= static_cast<std::uint8_t>(bits[index] << (7U - index % 8U));
    }
    for(auto& byte : result)
    {
        byte = reverse_bits(byte);
    }
    return result;
}

}

Bytes decode_pixels(const std::span<const std::uint8_t> data, const std::span<const std::uint8_t> map, const std::uint32_t width,
                    const std::uint32_t height)
{
    const std::size_t pixel_bytes = checked_image_size(width, height);
    if(map.empty())
    {
        throw Error("HG3 image map is empty");
    }

    Bytes normalized_map(map.begin(), map.end());
    for(auto& byte : normalized_map)
    {
        byte = reverse_bits(byte);
    }
    const auto bits = bytes_to_bits(normalized_map);
    std::size_t bit_position = 0;
    const bool starts_with_data = bits[bit_position++] != 0;
    const std::uint32_t mixed_size_u32 = read_gamma(bits, bit_position);
    const std::size_t mixed_size = mixed_size_u32;
    if(mixed_size < pixel_bytes || mixed_size % 4U != 0)
    {
        throw Error("invalid HG3 mixed-table size");
    }

    Bytes mixed(mixed_size, 0);
    std::size_t mixed_position = 0;
    std::size_t data_position = 0;
    bool is_data_run = starts_with_data;
    while(mixed_position < mixed.size())
    {
        const std::size_t run = read_gamma(bits, bit_position);
        if(run > mixed.size() - mixed_position)
        {
            throw Error("HG3 map run exceeds the mixed table");
        }
        if(is_data_run)
        {
            if(run > data.size() - data_position)
            {
                throw Error("HG3 image data is shorter than its map describes");
            }
            std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(data_position), static_cast<std::ptrdiff_t>(run),
                        mixed.begin() + static_cast<std::ptrdiff_t>(mixed_position));
            data_position += run;
        }
        mixed_position += run;
        is_data_run = !is_data_run;
    }
    if(data_position != data.size())
    {
        throw Error("HG3 image data contains bytes not referenced by its map");
    }

    for(auto& byte : mixed)
    {
        byte = reverse_pairs(byte);
    }
    const std::size_t quarter = mixed.size() / 4U;
    if(quarter < pixel_bytes / 4U)
    {
        throw Error("HG3 mixed table has incomplete color planes");
    }

    Bytes pixels(pixel_bytes, 0);
    const std::size_t pixel_count = pixel_bytes / 4U;
    for(std::size_t pixel = 0; pixel < pixel_count; ++pixel)
    {
        for(std::size_t channel = 0; channel < 4U; ++channel)
        {
            const unsigned shift = static_cast<unsigned>(6U - channel * 2U);
            const std::uint8_t zigzag = static_cast<std::uint8_t>(
                (((mixed[pixel] >> shift) & 3U) << 6U) | (((mixed[quarter + pixel] >> shift) & 3U) << 4U) |
                (((mixed[quarter * 2U + pixel] >> shift) & 3U) << 2U) | ((mixed[quarter * 3U + pixel] >> shift) & 3U));
            const int delta = (zigzag & 1U) == 0U ? static_cast<int>(zigzag / 2U) : -static_cast<int>((zigzag + 1U) / 2U);
            pixels[pixel * 4U + channel] = static_cast<std::uint8_t>(delta);
        }
    }

    for(std::uint32_t column = 1; column < width; ++column)
    {
        for(std::size_t channel = 0; channel < 4U; ++channel)
        {
            const std::size_t current = static_cast<std::size_t>(column) * 4U + channel;
            pixels[current] = static_cast<std::uint8_t>(pixels[current] + pixels[current - 4U]);
        }
    }
    const std::size_t row_size = static_cast<std::size_t>(width) * 4U;
    for(std::uint32_t row = 1; row < height; ++row)
    {
        for(std::size_t offset = 0; offset < row_size; ++offset)
        {
            const std::size_t current = static_cast<std::size_t>(row) * row_size + offset;
            pixels[current] = static_cast<std::uint8_t>(pixels[current] + pixels[current - row_size]);
        }
    }
    return pixels;
}

EncodedPixels encode_pixels(const std::span<const std::uint8_t> bgra, const std::uint32_t width, const std::uint32_t height)
{
    const std::size_t pixel_bytes = checked_image_size(width, height);
    if(bgra.size() != pixel_bytes)
    {
        throw Error("BGRA buffer size does not match HG3 image dimensions");
    }
    const std::size_t mixed_size = std::max<std::size_t>(pixel_bytes, 0x400U);
    if(mixed_size > std::numeric_limits<std::uint32_t>::max())
    {
        throw Error("HG3 image is too large");
    }

    Bytes differences(bgra.begin(), bgra.end());
    const std::size_t row_size = static_cast<std::size_t>(width) * 4U;
    for(std::uint32_t row = height - 1U; row > 0; --row)
    {
        for(std::size_t offset = 0; offset < row_size; ++offset)
        {
            const std::size_t current = static_cast<std::size_t>(row) * row_size + offset;
            differences[current] = static_cast<std::uint8_t>(differences[current] - differences[current - row_size]);
        }
    }
    for(std::uint32_t column = width - 1U; column > 0; --column)
    {
        for(std::size_t channel = 0; channel < 4U; ++channel)
        {
            const std::size_t current = static_cast<std::size_t>(column) * 4U + channel;
            differences[current] = static_cast<std::uint8_t>(differences[current] - differences[current - 4U]);
        }
    }

    Bytes mixed(mixed_size, 0);
    const std::size_t quarter = mixed_size / 4U;
    const std::size_t pixel_count = pixel_bytes / 4U;
    for(std::size_t pixel = 0; pixel < pixel_count; ++pixel)
    {
        for(std::size_t channel = 0; channel < 4U; ++channel)
        {
            const std::uint8_t byte = differences[pixel * 4U + channel];
            const int signed_value = byte < 128U ? static_cast<int>(byte) : static_cast<int>(byte) - 256;
            const std::uint8_t zigzag = static_cast<std::uint8_t>(signed_value >= 0 ? signed_value * 2 : -signed_value * 2 - 1);
            const unsigned shift = static_cast<unsigned>(6U - channel * 2U);
            mixed[pixel] |= static_cast<std::uint8_t>(((zigzag >> 6U) & 3U) << shift);
            mixed[quarter + pixel] |= static_cast<std::uint8_t>(((zigzag >> 4U) & 3U) << shift);
            mixed[quarter * 2U + pixel] |= static_cast<std::uint8_t>(((zigzag >> 2U) & 3U) << shift);
            mixed[quarter * 3U + pixel] |= static_cast<std::uint8_t>((zigzag & 3U) << shift);
        }
    }
    for(auto& byte : mixed)
    {
        byte = reverse_pairs(byte);
    }

    EncodedPixels result;
    std::vector<std::uint32_t> runs;
    bool current_is_data = mixed.front() != 0;
    const bool starts_with_data = current_is_data;
    std::size_t position = 0;
    while(position < mixed.size())
    {
        const std::size_t start = position;
        while(position < mixed.size() && (mixed[position] != 0) == current_is_data)
        {
            if(current_is_data)
            {
                result.data.push_back(mixed[position]);
            }
            ++position;
        }
        runs.push_back(static_cast<std::uint32_t>(position - start));
        current_is_data = !current_is_data;
    }

    std::vector<std::uint8_t> map_bits;
    map_bits.push_back(starts_with_data ? 1U : 0U);
    append_gamma(map_bits, static_cast<std::uint32_t>(mixed.size()));
    for(const auto run : runs)
    {
        append_gamma(map_bits, run);
    }
    result.map = pack_map_bits(std::move(map_bits));
    return result;
}

}
