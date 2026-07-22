cmake_minimum_required(VERSION 3.24)

add_library(zlibstatic STATIC
    adler32.c
    compress.c
    crc32.c
    deflate.c
    infback.c
    inffast.c
    inflate.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
)

add_library(ZLIB::ZLIBSTATIC ALIAS zlibstatic)

target_include_directories(zlibstatic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME z)
