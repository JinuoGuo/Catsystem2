#pragma once

#include <cstddef>
#include <filesystem>

namespace catsystem2::cst
{

struct UnpackOptions final
{
    std::filesystem::path input_directory = "scene";
    std::filesystem::path output_directory = "cst-work";
};

struct PackOptions final
{
    std::filesystem::path input_directory = "cst-work";
    std::filesystem::path output_directory = "scene";
};

std::size_t unpack(const UnpackOptions& options);
std::size_t pack(const PackOptions& options);

}
