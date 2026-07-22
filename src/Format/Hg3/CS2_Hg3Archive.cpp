#include "catsystem2/CS2_Hg3Archive.h"

#include "catsystem2/CS2_BinaryIO.h"
#include "catsystem2/CS2_Compression.h"
#include "catsystem2/CS2_Error.h"
#include "catsystem2/CS2_Hg3Codec.h"
#include "catsystem2/CS2_Image.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace catsystem2::hg3
{
namespace
{

constexpr std::array<std::uint8_t, 4> hg3_magic{'H', 'G', '-', '3'};
constexpr std::string_view metadata_magic{"HG3META\0", 8};
constexpr std::uint32_t metadata_version = 1;

struct Segment final
{
    std::array<std::uint8_t, 8> label{};
    Bytes payload;
};

struct Metadata final
{
    std::uint32_t block_flag = 0;
    std::vector<Segment> segments;
};

std::uint32_t checked_u32(const std::size_t value, const char* description)
{
    if(value > std::numeric_limits<std::uint32_t>::max())
    {
        throw Error(std::string(description) + " exceeds the HG3 32-bit size limit");
    }
    return static_cast<std::uint32_t>(value);
}

std::string label_text(const std::array<std::uint8_t, 8>& label)
{
    const auto end = std::find(label.begin(), label.end(), 0);
    return std::string(label.begin(), end);
}

std::array<std::uint8_t, 8> make_label(const std::string_view text)
{
    if(text.size() > 8U)
    {
        throw Error("HG3 segment label is too long");
    }
    std::array<std::uint8_t, 8> result{};
    std::copy(text.begin(), text.end(), result.begin());
    return result;
}

std::uint32_t read_u32_at(const std::span<const std::uint8_t> data, const std::size_t offset)
{
    ByteReader reader(data);
    reader.seek(offset);
    return reader.read_le<std::uint32_t>();
}

void patch_u32(Bytes& data, const std::size_t offset, const std::uint32_t value)
{
    if(offset > data.size() || 4U > data.size() - offset)
    {
        throw Error("HG3 metadata field is missing");
    }
    for(std::size_t index = 0; index < 4U; ++index)
    {
        data[offset + index] = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xffU);
    }
}

bool has_extension(const std::filesystem::path& path, std::string extension)
{
    std::string actual = path.extension().string();
    std::ranges::transform(actual, actual.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return actual == extension;
}

std::vector<std::filesystem::path> files_with_extension(const std::filesystem::path& directory, const std::string& extension)
{
    if(!std::filesystem::is_directory(directory))
    {
        throw Error("directory does not exist: " + directory.string());
    }
    std::vector<std::filesystem::path> result;
    for(const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if(entry.is_regular_file() && has_extension(entry.path(), extension))
        {
            result.push_back(entry.path());
        }
    }
    std::ranges::sort(result);
    return result;
}

std::string block_stem(const std::string& archive_stem, const std::size_t index)
{
    std::ostringstream name;
    name << archive_stem << '#' << std::setw(4) << std::setfill('0') << index;
    return name.str();
}

Bytes serialize_metadata(const Metadata& metadata)
{
    ByteWriter writer;
    writer.write_text(metadata_magic);
    writer.write_le(metadata_version);
    writer.write_le(metadata.block_flag);
    writer.write_le(checked_u32(metadata.segments.size(), "HG3 metadata segment count"));
    for(const auto& segment : metadata.segments)
    {
        writer.write_bytes(segment.label);
        writer.write_le(checked_u32(segment.payload.size(), "HG3 metadata segment"));
        writer.write_bytes(segment.payload);
    }
    return std::move(writer).take();
}

Metadata parse_metadata(const std::span<const std::uint8_t> bytes)
{
    ByteReader reader(bytes);
    const auto magic = reader.read_bytes(metadata_magic.size());
    if(!std::equal(magic.begin(), magic.end(), metadata_magic.begin()))
    {
        throw Error("invalid HG3 metadata signature");
    }
    if(reader.read_le<std::uint32_t>() != metadata_version)
    {
        throw Error("unsupported HG3 metadata version");
    }
    Metadata result;
    result.block_flag = reader.read_le<std::uint32_t>();
    const std::uint32_t count = reader.read_le<std::uint32_t>();
    result.segments.reserve(count);
    for(std::uint32_t index = 0; index < count; ++index)
    {
        Segment segment;
        const auto label = reader.read_bytes(segment.label.size());
        std::copy(label.begin(), label.end(), segment.label.begin());
        const std::uint32_t size = reader.read_le<std::uint32_t>();
        const auto payload = reader.read_bytes(size);
        segment.payload.assign(payload.begin(), payload.end());
        result.segments.push_back(std::move(segment));
    }
    if(reader.remaining() != 0)
    {
        throw Error("HG3 metadata contains trailing data");
    }
    return result;
}

Image decode_img0000(const std::span<const std::uint8_t> payload, const std::uint32_t width, const std::uint32_t height)
{
    ByteReader reader(payload);
    (void)reader.read_le<std::uint32_t>();
    (void)reader.read_le<std::uint32_t>();
    const std::uint32_t compressed_data_size = reader.read_le<std::uint32_t>();
    const std::uint32_t data_size = reader.read_le<std::uint32_t>();
    const std::uint32_t compressed_map_size = reader.read_le<std::uint32_t>();
    const std::uint32_t map_size = reader.read_le<std::uint32_t>();
    const auto compressed_data = reader.read_bytes(compressed_data_size);
    const auto compressed_map = reader.read_bytes(compressed_map_size);
    if(reader.remaining() != 0)
    {
        throw Error("img0000 segment contains trailing data");
    }
    const Bytes data = zlib_decompress(compressed_data, data_size);
    const Bytes map = zlib_decompress(compressed_map, map_size);
    return Image{width, height, decode_pixels(data, map, width, height)};
}

Segment make_img0000(const Image& image)
{
    const EncodedPixels encoded = encode_pixels(image.bgra, image.width, image.height);
    const Bytes compressed_data = zlib_compress(encoded.data);
    const Bytes compressed_map = zlib_compress(encoded.map);
    ByteWriter payload;
    payload.write_le<std::uint32_t>(0);
    payload.write_le(image.height);
    payload.write_le(checked_u32(compressed_data.size(), "compressed HG3 image data"));
    payload.write_le(checked_u32(encoded.data.size(), "HG3 image data"));
    payload.write_le(checked_u32(compressed_map.size(), "compressed HG3 image map"));
    payload.write_le(checked_u32(encoded.map.size(), "HG3 image map"));
    payload.write_bytes(compressed_data);
    payload.write_bytes(compressed_map);
    return Segment{make_label("img0000"), std::move(payload).take()};
}

Bytes serialize_block(const Metadata& metadata, const Image& image, const bool has_next)
{
    std::vector<Segment> segments = metadata.segments;
    const auto stdinfo = std::ranges::find_if(segments, [](const Segment& segment) { return label_text(segment.label) == "stdinfo"; });
    if(stdinfo == segments.end() || stdinfo->payload.size() < 40U)
    {
        throw Error("HG3 metadata is missing a valid stdinfo segment");
    }
    patch_u32(stdinfo->payload, 0, image.width);
    patch_u32(stdinfo->payload, 4, image.height);
    segments.insert(stdinfo + 1, make_img0000(image));

    ByteWriter block;
    block.write_le<std::uint32_t>(0);
    block.write_le(metadata.block_flag);
    for(std::size_t index = 0; index < segments.size(); ++index)
    {
        const auto& segment = segments[index];
        block.write_bytes(segment.label);
        const std::uint32_t next_offset =
            index + 1U == segments.size() ? 0U : checked_u32(16U + segment.payload.size(), "HG3 segment offset");
        block.write_le(next_offset);
        block.write_le(checked_u32(segment.payload.size(), "HG3 segment payload"));
        block.write_bytes(segment.payload);
    }
    Bytes result = std::move(block).take();
    if(has_next)
    {
        patch_u32(result, 0, checked_u32(result.size(), "HG3 block"));
    }
    return result;
}

std::size_t extract_archive(const std::filesystem::path& archive_path, const std::filesystem::path& output_root, const bool metadata_only)
{
    const Bytes archive = read_file(archive_path);
    ByteReader reader(archive);
    const auto magic = reader.read_bytes(hg3_magic.size());
    if(!std::equal(magic.begin(), magic.end(), hg3_magic.begin()))
    {
        throw Error("not an HG-3 archive: " + archive_path.string());
    }
    const std::uint32_t header_size = reader.read_le<std::uint32_t>();
    (void)reader.read_le<std::uint32_t>();
    if(header_size < 12U || header_size > archive.size())
    {
        throw Error("invalid HG3 header size");
    }
    reader.seek(header_size);

    const std::string archive_stem = archive_path.stem().string();
    const auto output_directory = output_root / archive_path.stem();
    std::filesystem::create_directories(output_directory);
    std::size_t block_index = 0;
    while(reader.remaining() > 0)
    {
        const std::size_t block_start = reader.position();
        const std::uint32_t next_block_offset = reader.read_le<std::uint32_t>();
        Metadata metadata;
        metadata.block_flag = reader.read_le<std::uint32_t>();
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::optional<Image> image;
        Bytes jpeg;
        Bytes alpha;

        while(true)
        {
            const std::size_t segment_start = reader.position();
            Segment segment;
            const auto label = reader.read_bytes(segment.label.size());
            std::copy(label.begin(), label.end(), segment.label.begin());
            const std::uint32_t next_segment_offset = reader.read_le<std::uint32_t>();
            const std::uint32_t payload_size = reader.read_le<std::uint32_t>();
            const auto payload = reader.read_bytes(payload_size);
            const std::string name = label_text(segment.label);
            if(name == "stdinfo")
            {
                if(payload.size() < 40U)
                {
                    throw Error("truncated HG3 stdinfo segment");
                }
                width = read_u32_at(payload, 0);
                height = read_u32_at(payload, 4);
                segment.payload.assign(payload.begin(), payload.end());
                metadata.segments.push_back(std::move(segment));
            }
            else if(name == "img0000")
            {
                if(!metadata_only)
                {
                    if(width == 0 || height == 0)
                    {
                        throw Error("HG3 image segment appears before stdinfo");
                    }
                    image = decode_img0000(payload, width, height);
                }
            }
            else if(name == "img_jpg")
            {
                if(!metadata_only)
                {
                    jpeg.assign(payload.begin(), payload.end());
                }
            }
            else if(name == "img_al")
            {
                if(!metadata_only)
                {
                    ByteReader alpha_reader(payload);
                    const std::uint32_t compressed_size = alpha_reader.read_le<std::uint32_t>();
                    const std::uint32_t uncompressed_size = alpha_reader.read_le<std::uint32_t>();
                    const auto compressed = alpha_reader.read_bytes(compressed_size);
                    if(alpha_reader.remaining() != 0)
                    {
                        throw Error("img_al segment size does not match its header");
                    }
                    alpha = zlib_decompress(compressed, uncompressed_size);
                }
            }
            else
            {
                segment.payload.assign(payload.begin(), payload.end());
                metadata.segments.push_back(std::move(segment));
            }
            if(next_segment_offset == 0)
            {
                break;
            }
            const std::size_t next_segment = segment_start + next_segment_offset;
            const std::size_t block_end = next_block_offset == 0 ? archive.size() : block_start + next_block_offset;
            if(next_segment < reader.position() || next_segment > block_end)
            {
                throw Error("HG3 segment offset is outside the current block");
            }
            reader.seek(next_segment);
        }

        if(!metadata_only && !jpeg.empty())
        {
            image = decode_jpeg(jpeg, alpha);
            if(image->width != width || image->height != height)
            {
                throw Error("JPEG dimensions do not match the HG3 stdinfo segment");
            }
        }
        const std::string stem = block_stem(archive_stem, block_index);
        write_file(output_directory / (stem + ".hg3meta"), serialize_metadata(metadata));
        if(!metadata_only)
        {
            if(!image.has_value())
            {
                throw Error("HG3 block has no supported image segment");
            }
            encode_png(output_directory / (stem + ".png"), *image);
        }
        ++block_index;

        if(next_block_offset == 0)
        {
            if(reader.remaining() != 0)
            {
                throw Error("HG3 archive contains data after its final block");
            }
            break;
        }
        const std::size_t next_position = block_start + next_block_offset;
        if(next_position <= block_start || next_position > archive.size())
        {
            throw Error("HG3 block offset is outside the archive");
        }
        reader.seek(next_position);
    }
    return block_index;
}

}

std::size_t extract(const ExtractOptions& options)
{
    const auto archives = files_with_extension(options.input_directory, ".hg3");
    if(archives.empty())
    {
        throw Error("no .hg3 files found in: " + options.input_directory.string());
    }
    std::size_t blocks = 0;
    for(const auto& archive : archives)
    {
        blocks += extract_archive(archive, options.output_directory, options.metadata_only);
    }
    return blocks;
}

std::size_t pack(const PackOptions& options)
{
    if(!std::filesystem::is_directory(options.input_directory))
    {
        throw Error("directory does not exist: " + options.input_directory.string());
    }
    std::vector<std::filesystem::path> directories;
    for(const auto& entry : std::filesystem::directory_iterator(options.input_directory))
    {
        if(entry.is_directory())
        {
            directories.push_back(entry.path());
        }
    }
    std::ranges::sort(directories);
    std::filesystem::create_directories(options.output_directory);
    std::size_t archive_count = 0;
    for(const auto& directory : directories)
    {
        const auto metadata_files = files_with_extension(directory, ".hg3meta");
        if(metadata_files.empty())
        {
            continue;
        }
        ByteWriter archive;
        archive.write_bytes(hg3_magic);
        archive.write_le<std::uint32_t>(12);
        archive.write_le<std::uint32_t>(0x300);
        for(std::size_t index = 0; index < metadata_files.size(); ++index)
        {
            const auto image_path = metadata_files[index].parent_path() / (metadata_files[index].stem().string() + ".png");
            if(!std::filesystem::is_regular_file(image_path))
            {
                throw Error("missing HG3 PNG image: " + image_path.string());
            }
            const Metadata metadata = parse_metadata(read_file(metadata_files[index]));
            const Image image = decode_image_file(image_path);
            const Bytes block = serialize_block(metadata, image, index + 1U < metadata_files.size());
            archive.write_bytes(block);
        }
        write_file(options.output_directory / (directory.filename().string() + ".hg3"), std::move(archive).take());
        ++archive_count;
    }
    if(archive_count == 0)
    {
        throw Error("no directories containing .hg3meta files found in: " + options.input_directory.string());
    }
    return archive_count;
}

}
