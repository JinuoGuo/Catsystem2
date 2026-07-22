#include "catsystem2/CS2_CstArchive.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

void PrintUsage()
{
    std::cout << "CatSystem2 CST tool\n\n"
                 "Usage:\n"
                 "  catsystem2-cst unpack [scene-directory] [work-directory]\n"
                 "  catsystem2-cst pack   [work-directory]  [scene-directory]\n\n"
                 "Defaults:\n"
                 "  unpack: scene -> cst-work\n"
                 "  pack:   cst-work -> scene\n";
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

        if(strCommand == "unpack")
        {
            catsystem2::cst::UnpackOptions stOptions;

            if(nArgc >= 3)
            {
                stOptions.input_directory = std::filesystem::path(pArgv[2]);
            }

            if(nArgc >= 4)
            {
                stOptions.output_directory = std::filesystem::path(pArgv[3]);
            }

            const std::size_t uCount = catsystem2::cst::unpack(stOptions);
            std::cout << "Unpacked " << uCount << " CST file(s).\n";
            return 0;
        }

        if(strCommand == "pack")
        {
            catsystem2::cst::PackOptions stOptions;

            if(nArgc >= 3)
            {
                stOptions.input_directory = std::filesystem::path(pArgv[2]);
            }

            if(nArgc >= 4)
            {
                stOptions.output_directory = std::filesystem::path(pArgv[3]);
            }

            const std::size_t uCount = catsystem2::cst::pack(stOptions);
            std::cout << "Packed " << uCount << " CST file(s).\n";
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
