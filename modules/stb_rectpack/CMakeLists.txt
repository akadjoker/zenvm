cmake_minimum_required(VERSION 3.10)
project(zen_module_stb_rectpack LANGUAGES CXX)

add_library(zen_module_stb_rectpack STATIC
    src/builtin_rectpack.cpp
)

target_include_directories(zen_module_stb_rectpack
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(zen_module_stb_rectpack PUBLIC zen_static)
target_compile_definitions(zen_module_stb_rectpack PUBLIC ZEN_ENABLE_STB_RECTPACK)
target_compile_options(zen_module_stb_rectpack PRIVATE -w $<$<CONFIG:Release>:-O3>)
