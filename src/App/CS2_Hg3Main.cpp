#include "catsystem2/CS2_Hg3Archive.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

void PrintUsage()
{
    std::cout << "CatSystem2 HG3 tool\n\n"
                 "Usage:\n"
                 "  catsystem2-hg3 extract [input-directory] [work-directory] [--metadata-only]\n"
                 "  catsystem2-hg3 pack    [work-directory]  [output-directory]\n\n"
                 "Defaults:\n"
                 "  extract: . -> hg3-work\n"
                 "  pack:    hg3-work -> .\n";
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

        if(strCommand == "extract")
        {
            catsystem2::hg3::ExtractOptions stOptions;

            if(nArgc >= 3)
            {
                stOptions.input_directory = std::filesystem::path(pArgv[2]);
            }

            if(nArgc >= 4)
            {
                stOptions.output_directory = std::filesystem::path(pArgv[3]);
            }

            if(nArgc >= 5)
            {
                if(std::string(pArgv[4]) != "--metadata-only")
                {
                    PrintUsage();
                    return 1;
                }

                stOptions.metadata_only = true;
            }

            const std::size_t uCount = catsystem2::hg3::extract(stOptions);
            std::cout << "Extracted " << uCount << " HG3 block(s).\n";
            return 0;
        }

        if(strCommand == "pack")
        {
            catsystem2::hg3::PackOptions stOptions;

            if(nArgc >= 3)
            {
                stOptions.input_directory = std::filesystem::path(pArgv[2]);
            }

            if(nArgc >= 4)
            {
                stOptions.output_directory = std::filesystem::path(pArgv[3]);
            }

            const std::size_t uCount = catsystem2::hg3::pack(stOptions);
            std::cout << "Packed " << uCount << " HG3 archive(s).\n";
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
