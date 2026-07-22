#include "catsystem2/CS2_SimpleArchive.h"

#include "catsystem2/CS2_BinaryIO.h"
#include "catsystem2/CS2_Compression.h"
#include "catsystem2/CS2_Error.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <vector>

namespace catsystem2::simple
{
namespace
{

std::uint32_t CheckedU32(std::size_t uValue, const char* pDescription)
{
    if(uValue > std::numeric_limits<std::uint32_t>::max())
    {
        throw Error(std::string(pDescription) + " exceeds the 32-bit size limit");
    }

    return static_cast<std::uint32_t>(uValue);
}

bool HasExtension(const std::filesystem::path& strPath, std::string strExtension)
{
    std::string strActual = strPath.extension().string();
    std::ranges::transform(strActual, strActual.begin(), [](unsigned char uValue) { return static_cast<char>(std::tolower(uValue)); });
    std::ranges::transform(strExtension, strExtension.begin(),
                           [](unsigned char uValue) { return static_cast<char>(std::tolower(uValue)); });
    return strActual == strExtension;
}

std::vector<std::filesystem::path> GetFiles(const std::filesystem::path& strDirectory)
{
    if(!std::filesystem::is_directory(strDirectory))
    {
        throw Error("directory does not exist: " + strDirectory.string());
    }

    std::vector<std::filesystem::path> aFiles;

    for(const auto& stEntry : std::filesystem::directory_iterator(strDirectory))
    {
        if(stEntry.is_regular_file())
        {
            aFiles.push_back(stEntry.path());
        }
    }

    std::ranges::sort(aFiles);
    return aFiles;
}

Bytes MakeArchive(const Bytes& aPayload, const ArchiveFormat& stFormat)
{
    const Bytes aCompressed = zlib_compress(aPayload);
    ByteWriter oWriter;
    oWriter.write_bytes(stFormat.magic);
    oWriter.write_le(CheckedU32(aCompressed.size(), "compressed payload"));
    oWriter.write_le(CheckedU32(aPayload.size(), "payload"));
    oWriter.write_bytes(aCompressed);
    return std::move(oWriter).take();
}

Bytes ExtractArchive(const Bytes& aArchive, const ArchiveFormat& stFormat)
{
    ByteReader oReader(aArchive);
    const auto aMagic = oReader.read_bytes(stFormat.magic.size());

    if(!std::equal(aMagic.begin(), aMagic.end(), stFormat.magic.begin()))
    {
        throw Error("archive signature does not match its tool");
    }

    const std::uint32_t uCompressedSize = oReader.read_le<std::uint32_t>();
    const std::uint32_t uPayloadSize = oReader.read_le<std::uint32_t>();

    if(oReader.remaining() != uCompressedSize)
    {
        throw Error("compressed size does not match the archive header");
    }

    return zlib_decompress(oReader.read_bytes(uCompressedSize), uPayloadSize);
}

}

std::size_t unpack(const std::filesystem::path& strInputDirectory, const std::filesystem::path& strOutputDirectory,
                   const ArchiveFormat& stFormat)
{
    const std::vector<std::filesystem::path> aFiles = GetFiles(strInputDirectory);
    std::size_t uCount = 0;
    std::filesystem::create_directories(strOutputDirectory);

    for(const auto& strPath : aFiles)
    {
        if(!HasExtension(strPath, std::string(stFormat.extension)))
        {
            continue;
        }

        write_file(strOutputDirectory / strPath.stem(), ExtractArchive(read_file(strPath), stFormat));
        ++uCount;
    }

    if(uCount == 0)
    {
        throw Error("no " + std::string(stFormat.extension) + " files found in: " + strInputDirectory.string());
    }

    return uCount;
}

std::size_t pack(const std::filesystem::path& strInputDirectory, const std::filesystem::path& strOutputDirectory,
                 const ArchiveFormat& stFormat)
{
    const std::vector<std::filesystem::path> aFiles = GetFiles(strInputDirectory);

    if(aFiles.empty())
    {
        throw Error("no input files found in: " + strInputDirectory.string());
    }

    std::filesystem::create_directories(strOutputDirectory);

    for(const auto& strPath : aFiles)
    {
        const std::filesystem::path strOutputPath = strOutputDirectory / (strPath.filename().string() + std::string(stFormat.extension));
        write_file(strOutputPath, MakeArchive(read_file(strPath), stFormat));
    }

    return aFiles.size();
}

}
