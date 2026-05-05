cmake_minimum_required(VERSION 3.10)
project(zen_module_stb_font LANGUAGES CXX)

add_library(zen_module_stb_font STATIC
    src/builtin_font.cpp
)

target_include_directories(zen_module_stb_font
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(zen_module_stb_font PUBLIC zen_static)
target_compile_definitions(zen_module_stb_font PUBLIC ZEN_ENABLE_STB_FONT)
target_compile_options(zen_module_stb_font PRIVATE -w $<$<CONFIG:Release>:-O3>)
