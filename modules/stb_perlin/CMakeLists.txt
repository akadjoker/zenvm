cmake_minimum_required(VERSION 3.10)
project(zen_module_stb_perlin LANGUAGES CXX)

add_library(zen_module_stb_perlin STATIC
    src/builtin_noise.cpp
)

target_include_directories(zen_module_stb_perlin
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(zen_module_stb_perlin PUBLIC zen_static)
target_compile_definitions(zen_module_stb_perlin PUBLIC ZEN_ENABLE_STB_PERLIN)
target_compile_options(zen_module_stb_perlin PRIVATE -w $<$<CONFIG:Release>:-O3>)
