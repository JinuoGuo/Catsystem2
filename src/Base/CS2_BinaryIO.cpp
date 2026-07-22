#include "catsystem2/CS2_BinaryIO.h"

#include "catsystem2/CS2_Error.h"

#include <fstream>
#include <limits>
#include <string>

namespace catsystem2
{

Bytes read_file(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if(!stream)
    {
        throw Error("cannot open input file: " + path.string());
    }
    const auto end = stream.tellg();
    if(end < 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max())
    {
        throw Error("input file is too large: " + path.string());
    }
    Bytes result(static_cast<std::size_t>(end));
    stream.seekg(0);
    if(!result.empty())
    {
        stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    }
    if(!stream)
    {
        throw Error("failed to read input file: " + path.string());
    }
    return result;
}

void write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data)
{
    if(!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if(!stream)
    {
        throw Error("cannot open output file: " + path.string());
    }
    if(!data.empty())
    {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    if(!stream)
    {
        throw Error("failed to write output file: " + path.string());
    }
}

void ByteReader::seek(const std::size_t position)
{
    if(position > data_.size())
    {
        throw Error("binary seek is outside the input buffer");
    }
    position_ = position;
}

void ByteReader::skip(const std::size_t count)
{
    (void)read_bytes(count);
}

std::span<const std::uint8_t> ByteReader::read_bytes(const std::size_t count)
{
    if(count > remaining())
    {
        throw Error("unexpected end of binary data");
    }
    const auto result = data_.subspan(position_, count);
    position_ += count;
    return result;
}

void ByteWriter::write_bytes(const std::span<const std::uint8_t> bytes)
{
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void ByteWriter::write_text(const std::string_view text)
{
    data_.insert(data_.end(), text.begin(), text.end());
}

}
