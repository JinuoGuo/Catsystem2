#include "catsystem2/CS2_Compression.h"

#include "catsystem2/CS2_Error.h"

#include <limits>
#include <string>

#include <zlib.h>

namespace catsystem2
{

Bytes zlib_compress(const std::span<const std::uint8_t> input, const int level)
{
    if(input.size() > std::numeric_limits<uLong>::max())
    {
        throw Error("input is too large for zlib");
    }
    uLongf output_size = compressBound(static_cast<uLong>(input.size()));
    Bytes output(static_cast<std::size_t>(output_size));
    const int status = compress2(output.data(), &output_size, input.data(), static_cast<uLong>(input.size()), level);
    if(status != Z_OK)
    {
        throw Error("zlib compression failed with status " + std::to_string(status));
    }
    output.resize(static_cast<std::size_t>(output_size));
    return output;
}

Bytes zlib_decompress(const std::span<const std::uint8_t> input, const std::size_t expected_size)
{
    if(input.size() > std::numeric_limits<uLong>::max() || expected_size > std::numeric_limits<uLongf>::max())
    {
        throw Error("input or output is too large for zlib");
    }
    Bytes output(expected_size);
    uLongf output_size = static_cast<uLongf>(output.size());
    const int status = uncompress(output.data(), &output_size, input.data(), static_cast<uLong>(input.size()));
    if(status != Z_OK)
    {
        throw Error("zlib decompression failed with status " + std::to_string(status));
    }
    if(output_size != expected_size)
    {
        throw Error("zlib output size does not match the archive header");
    }
    return output;
}

}
