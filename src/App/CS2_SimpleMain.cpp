#include "catsystem2/CS2_SimpleArchive.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef CS2_ARCHIVE_NAME
#error CS2_ARCHIVE_NAME is required
#endif

#ifndef CS2_ARCHIVE_EXTENSION
#error CS2_ARCHIVE_EXTENSION is required
#endif

namespace
{

std::string LowerName()
{
    std::string strName = CS2_ARCHIVE_NAME;
    std::ranges::transform(strName, strName.begin(), [](unsigned char uValue) { return static_cast<char>(std::tolower(uValue)); });
    return strName;
}

catsystem2::simple::ArchiveFormat GetFormat()
{
    const std::string strName = CS2_ARCHIVE_NAME;

    if(strName.size() != 3)
    {
        throw std::runtime_error("archive name must contain three characters");
    }

    return {{static_cast<std::uint8_t>(strName[0]), static_cast<std::uint8_t>(strName[1]), static_cast<std::uint8_t>(strName[2]), 0},
            CS2_ARCHIVE_EXTENSION};
}

void PrintUsage()
{
    const std::string strName = LowerName();
    std::cout << "CatSystem2 " << CS2_ARCHIVE_NAME << " tool\n\n"
              << "Usage:\n"
              << "  catsystem2-" << strName << " unpack [input-directory] [output-directory]\n"
              << "  catsystem2-" << strName << " pack   [input-directory] [output-directory]\n";
}

}

int main(int nArgc, char* pArgv[])
{
    try
    {
        if(nArgc < 2)
        {
            PrintUsage();
            return 1;
        }

        const std::string strCommand = pArgv[1];
        const std::string strName = LowerName();
        const auto stFormat = GetFormat();

        if(strCommand == "unpack")
        {
            const std::filesystem::path strInput = nArgc >= 3 ? pArgv[2] : ".";
            const std::filesystem::path strOutput = nArgc >= 4 ? pArgv[3] : strName + "-output";
            const std::size_t uCount = catsystem2::simple::unpack(strInput, strOutput, stFormat);
            std::cout << "Unpacked " << uCount << " " << CS2_ARCHIVE_NAME << " file(s).\n";
            return 0;
        }

        if(strCommand == "pack")
        {
            const std::filesystem::path strInput = nArgc >= 3 ? pArgv[2] : strName + "-input";
            const std::filesystem::path strOutput = nArgc >= 4 ? pArgv[3] : ".";
            const std::size_t uCount = catsystem2::simple::pack(strInput, strOutput, stFormat);
            std::cout << "Packed " << uCount << " " << CS2_ARCHIVE_NAME << " file(s).\n";
            return 0;
        }

        PrintUsage();
        return 1;
    }
    catch(const std::exception& stException)
    {
        std::cerr << "Error: " << stException.what() << '\n';
        return 2;
    }
}
