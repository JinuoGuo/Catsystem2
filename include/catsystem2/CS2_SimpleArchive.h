#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace catsystem2::simple
{

struct ArchiveFormat final
{
    std::array<std::uint8_t, 4> magic;
    std::string_view extension;
};

std::size_t unpack(const std::filesystem::path& input_directory, const std::filesystem::path& output_directory,
                   const ArchiveFormat& format);

std::size_t pack(const std::filesystem::path& input_directory, const std::filesystem::path& output_directory, const ArchiveFormat& format);

}
