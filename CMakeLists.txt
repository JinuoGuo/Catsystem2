cmake_minimum_required(VERSION 3.24)

project(CatSystem2Tools VERSION 2.0.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "CatSystem2Tools currently targets Windows because image and text-codec support uses Windows system APIs.")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Only MinGW (GNU C/C++ compiler on Windows) is supported.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)

add_library(catsystem2_core STATIC
    src/Base/CS2_BinaryIO.cpp
    src/Base/CS2_Compression.cpp
    src/Platform/CS2_WindowsImage.cpp
    src/Format/Cst/CS2_CstArchive.cpp
    src/Format/Hg3/CS2_Hg3Archive.cpp
    src/Format/Hg3/CS2_Hg3Codec.cpp
    src/Format/Simple/CS2_SimpleArchive.cpp
)
target_include_directories(catsystem2_core PUBLIC include)
target_link_libraries(catsystem2_core
    PUBLIC ZLIB::ZLIBSTATIC
    PRIVATE ole32 windowscodecs
)
target_compile_definitions(catsystem2_core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(catsystem2_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(catsystem2-cst src/App/CS2_CstMain.cpp)
target_link_libraries(catsystem2-cst PRIVATE catsystem2_core)
target_link_options(catsystem2-cst PRIVATE -static-libgcc -static-libstdc++)

add_executable(catsystem2-hg3 src/App/CS2_Hg3Main.cpp)
target_link_libraries(catsystem2-hg3 PRIVATE catsystem2_core)
target_link_options(catsystem2-hg3 PRIVATE -static-libgcc -static-libstdc++)

add_executable(catsystem2-fes src/App/CS2_SimpleMain.cpp)
target_compile_definitions(catsystem2-fes PRIVATE
    CS2_ARCHIVE_NAME="FES"
    CS2_ARCHIVE_EXTENSION=".fes"
)
target_link_libraries(catsystem2-fes PRIVATE catsystem2_core)
target_link_options(catsystem2-fes PRIVATE -static-libgcc -static-libstdc++)

add_executable(catsystem2-kcs src/App/CS2_SimpleMain.cpp)
target_compile_definitions(catsystem2-kcs PRIVATE
    CS2_ARCHIVE_NAME="KCS"
    CS2_ARCHIVE_EXTENSION=".kcs"
)
target_link_libraries(catsystem2-kcs PRIVATE catsystem2_core)
target_link_options(catsystem2-kcs PRIVATE -static-libgcc -static-libstdc++)

add_executable(catsystem2-ksc src/App/CS2_SimpleMain.cpp)
target_compile_definitions(catsystem2-ksc PRIVATE
    CS2_ARCHIVE_NAME="KSC"
    CS2_ARCHIVE_EXTENSION=".ksc"
)
target_link_libraries(catsystem2-ksc PRIVATE catsystem2_core)
target_link_options(catsystem2-ksc PRIVATE -static-libgcc -static-libstdc++)

install(TARGETS
    catsystem2-cst
    catsystem2-hg3
    catsystem2-fes
    catsystem2-kcs
    catsystem2-ksc
    RUNTIME DESTINATION bin
)
