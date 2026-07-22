#pragma once

#include <cstddef>
#include <filesystem>

namespace catsystem2::hg3
{

struct ExtractOptions final
{
    std::filesystem::path input_directory = ".";
    std::filesystem::path output_directory = "hg3-work";
    bool metadata_only = false;
};

struct PackOptions final
{
    std::filesystem::path input_directory = "hg3-work";
    std::filesystem::path output_directory = ".";
};

std::size_t extract(const ExtractOptions& options);
std::size_t pack(const PackOptions& options);

}
