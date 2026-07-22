#include "catsystem2/CS2_CstArchive.h"

#include "catsystem2/CS2_BinaryIO.h"
#include "catsystem2/CS2_Compression.h"
#include "catsystem2/CS2_Error.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace catsystem2::cst
{
namespace
{

constexpr std::string_view archive_magic = "CatScene";
constexpr std::string_view metadata_extension = ".cstmeta";

std::uint32_t checked_u32(const std::size_t value, const char* description)
{
    if(value > std::numeric_limits<std::uint32_t>::max())
    {
        throw Error(std::string(description) + " exceeds the CST 32-bit size limit");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t read_u32_at(const std::span<const std::uint8_t> data, const std::size_t offset)
{
    ByteReader reader(data);
    reader.seek(offset);
    return reader.read_le<std::uint32_t>();
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

Bytes convert_code_page(const std::span<const std::uint8_t> input, const UINT source_code_page, const UINT destination_code_page)
{
    if(input.empty())
    {
        return {};
    }
    if(input.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw Error("text command is too large for Windows code-page conversion");
    }
    const int input_size = static_cast<int>(input.size());
    const int wide_size = MultiByteToWideChar(source_code_page, 0, reinterpret_cast<const char*>(input.data()), input_size, nullptr, 0);
    if(wide_size <= 0)
    {
        throw Error("failed to decode text from Windows code page " + std::to_string(source_code_page));
    }
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if(MultiByteToWideChar(source_code_page, 0, reinterpret_cast<const char*>(input.data()), input_size, wide.data(), wide_size) !=
       wide_size)
    {
        throw Error("failed to decode CST text");
    }
    const int output_size = WideCharToMultiByte(destination_code_page, 0, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if(output_size <= 0)
    {
        throw Error("failed to encode text in Windows code page " + std::to_string(destination_code_page));
    }
    Bytes output(static_cast<std::size_t>(output_size));
    if(WideCharToMultiByte(destination_code_page, 0, wide.data(), wide_size, reinterpret_cast<char*>(output.data()), output_size, nullptr,
                           nullptr) != output_size)
    {
        throw Error("failed to encode CST text");
    }
    return output;
}

std::span<const std::uint8_t> read_c_string(ByteReader& reader)
{
    const std::size_t start = reader.position();
    while(reader.remaining() > 0)
    {
        if(reader.read_le<std::uint8_t>() == 0)
        {
            const std::size_t length = reader.position() - start - 1U;
            reader.seek(start);
            const auto result = reader.read_bytes(length);
            reader.skip(1);
            return result;
        }
    }
    throw Error("unterminated command string in CST data");
}

bool is_translatable(const std::uint8_t code, const std::span<const std::uint8_t> value)
{
    if(code == 0x20U || code == 0x21U)
    {
        return true;
    }
    if(code != 0x30U || value.empty())
    {
        return false;
    }
    const bool numbered_choice = value.front() >= '0' && value.front() <= '9';
    constexpr std::array<std::uint8_t, 5> scene_prefix{'s', 'c', 'e', 'n', 'e'};
    const bool scene_title = value.size() >= scene_prefix.size() && std::equal(scene_prefix.begin(), scene_prefix.end(), value.begin());
    return numbered_choice || scene_title;
}

Bytes unpack_payload(const std::span<const std::uint8_t> archive)
{
    ByteReader reader(archive);
    const auto magic = reader.read_bytes(archive_magic.size());
    if(!std::equal(magic.begin(), magic.end(), archive_magic.begin()))
    {
        throw Error("not a CatScene CST archive");
    }
    const std::uint32_t compressed_size = reader.read_le<std::uint32_t>();
    const std::uint32_t uncompressed_size = reader.read_le<std::uint32_t>();
    if(reader.remaining() != compressed_size)
    {
        throw Error("CST compressed size does not match its header");
    }
    return zlib_decompress(reader.read_bytes(compressed_size), uncompressed_size);
}

struct Extracted final
{
    Bytes metadata;
    Bytes text;
};

Extracted extract_payload(const std::span<const std::uint8_t> payload)
{
    if(payload.size() < 16U)
    {
        throw Error("CST payload is shorter than its header");
    }
    const std::size_t body_start = 16U + read_u32_at(payload, 12U);
    if(body_start > payload.size())
    {
        throw Error("CST body offset is outside the payload");
    }

    Extracted result;
    result.metadata.insert(result.metadata.end(), payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(body_start));
    ByteReader body(payload.subspan(body_start));
    while(body.remaining() > 0)
    {
        if(body.remaining() < 3U)
        {
            throw Error("truncated CST command");
        }
        const std::uint8_t marker = body.read_le<std::uint8_t>();
        std::uint8_t code = body.read_le<std::uint8_t>();
        const auto value = read_c_string(body);
        const bool translated = is_translatable(code, value);
        if(translated && code == 0x30U)
        {
            code = 0xffU;
        }
        result.metadata.push_back(marker);
        result.metadata.push_back(code);
        if(translated)
        {
            const Bytes converted = convert_code_page(value, 932U, 936U);
            result.text.insert(result.text.end(), converted.begin(), converted.end());
            result.text.push_back('\n');
        }
        else
        {
            result.metadata.insert(result.metadata.end(), value.begin(), value.end());
            result.metadata.push_back(0);
        }
    }
    return result;
}

std::vector<Bytes> split_lines(const std::span<const std::uint8_t> text)
{
    std::vector<Bytes> lines;
    std::size_t start = 0;
    while(start < text.size())
    {
        const auto newline = std::find(text.begin() + static_cast<std::ptrdiff_t>(start), text.end(), static_cast<std::uint8_t>('\n'));
        if(newline == text.end())
        {
            throw Error("CST text file must end every translated command with a newline");
        }
        std::size_t end = static_cast<std::size_t>(newline - text.begin());
        if(end > start && text[end - 1U] == '\r')
        {
            --end;
        }
        lines.emplace_back(text.begin() + static_cast<std::ptrdiff_t>(start), text.begin() + static_cast<std::ptrdiff_t>(end));
        start = static_cast<std::size_t>(newline - text.begin()) + 1U;
    }
    return lines;
}

void restore_special_gbk_characters(Bytes& line)
{
    for(std::size_t index = 0; index + 1U < line.size(); ++index)
    {
        if((line[index] & 0x80U) == 0U)
        {
            continue;
        }
        if(line[index] == 0xf9U && line[index + 1U] == 0xa1U)
        {
            line[index] = 0xa6U;
            line[index + 1U] = 0x81U;
        }
        else if(line[index] == 0xc0U && line[index + 1U] == 0xa3U)
        {
            line[index] = 0x97U;
            line[index + 1U] = 0x81U;
        }
        ++index;
    }
}

Bytes rebuild_payload(const std::span<const std::uint8_t> metadata, const std::span<const std::uint8_t> text)
{
    if(metadata.size() < 16U)
    {
        throw Error("CST metadata file is shorter than its header");
    }
    const std::uint32_t table_offset = read_u32_at(metadata, 8U);
    const std::size_t body_start = 16U + read_u32_at(metadata, 12U);
    if(body_start > metadata.size())
    {
        throw Error("CST metadata body offset is outside the file");
    }
    const std::vector<Bytes> lines = split_lines(text);
    std::size_t line_index = 0;
    std::size_t command_index = 0;
    std::uint32_t command_offset = 0;

    ByteWriter output;
    output.write_bytes(metadata.first(body_start));
    ByteReader commands(metadata.subspan(body_start));
    while(commands.remaining() > 0)
    {
        if(commands.remaining() < 2U)
        {
            throw Error("truncated CST metadata command");
        }
        const std::uint8_t marker = commands.read_le<std::uint8_t>();
        const std::uint8_t stored_code = commands.read_le<std::uint8_t>();
        const bool translated = stored_code == 0xffU || stored_code == 0x20U || stored_code == 0x21U;
        output.write_le(marker);
        output.write_le(static_cast<std::uint8_t>(stored_code == 0xffU ? 0x30U : stored_code));

        std::size_t value_size = 0;
        if(translated)
        {
            if(line_index >= lines.size())
            {
                throw Error("CST text file has fewer lines than the metadata requires");
            }
            Bytes line = lines[line_index++];
            restore_special_gbk_characters(line);
            value_size = line.size() + 1U;
            output.write_bytes(line);
            output.write_le<std::uint8_t>(0);
        }
        else
        {
            const auto value = read_c_string(commands);
            value_size = value.size() + 1U;
            output.write_bytes(value);
            output.write_le<std::uint8_t>(0);
        }

        const std::size_t table_entry = 16U + table_offset + command_index * sizeof(std::uint32_t);
        output.patch_le(table_entry, command_offset);
        const std::size_t command_size = value_size + 2U;
        if(command_size > std::numeric_limits<std::uint32_t>::max() - command_offset)
        {
            throw Error("CST command data exceeds its 32-bit offset range");
        }
        command_offset += static_cast<std::uint32_t>(command_size);
        ++command_index;
    }
    if(line_index != lines.size())
    {
        throw Error("CST text file has more lines than the metadata requires");
    }
    if(output.size() < 16U)
    {
        throw Error("rebuilt CST payload is invalid");
    }
    output.patch_le<std::uint32_t>(0, checked_u32(output.size() - 16U, "CST payload"));
    return std::move(output).take();
}

Bytes make_archive(const std::span<const std::uint8_t> payload)
{
    const Bytes compressed = zlib_compress(payload);
    ByteWriter archive;
    archive.write_text(archive_magic);
    archive.write_le(checked_u32(compressed.size(), "compressed CST payload"));
    archive.write_le(checked_u32(payload.size(), "CST payload"));
    archive.write_bytes(compressed);
    return std::move(archive).take();
}

}

std::size_t unpack(const UnpackOptions& options)
{
    const auto archives = files_with_extension(options.input_directory, ".cst");
    if(archives.empty())
    {
        throw Error("no .cst files found in: " + options.input_directory.string());
    }
    const auto text_directory = options.output_directory / "text";
    const auto metadata_directory = options.output_directory / "metadata";
    std::filesystem::create_directories(text_directory);
    std::filesystem::create_directories(metadata_directory);
    for(const auto& archive_path : archives)
    {
        const Bytes archive = read_file(archive_path);
        const Bytes payload = unpack_payload(archive);
        const Extracted extracted = extract_payload(payload);
        const std::string stem = archive_path.stem().string();
        write_file(text_directory / (stem + ".txt"), extracted.text);
        write_file(metadata_directory / (stem + std::string(metadata_extension)), extracted.metadata);
    }
    return archives.size();
}

std::size_t pack(const PackOptions& options)
{
    const auto text_directory = options.input_directory / "text";
    const auto metadata_directory = options.input_directory / "metadata";
    const auto text_files = files_with_extension(text_directory, ".txt");
    if(text_files.empty())
    {
        throw Error("no .txt files found in: " + text_directory.string());
    }
    std::filesystem::create_directories(options.output_directory);
    for(const auto& text_path : text_files)
    {
        const auto metadata_path = metadata_directory / (text_path.stem().string() + std::string(metadata_extension));
        if(!std::filesystem::is_regular_file(metadata_path))
        {
            throw Error("missing CST metadata file: " + metadata_path.string());
        }
        const Bytes payload = rebuild_payload(read_file(metadata_path), read_file(text_path));
        write_file(options.output_directory / (text_path.stem().string() + ".cst"), make_archive(payload));
    }
    return text_files.size();
}

}
