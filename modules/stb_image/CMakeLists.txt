cmake_minimum_required(VERSION 3.10)
project(zen_module_stb_image LANGUAGES CXX)

add_library(zen_module_stb_image STATIC
    src/builtin_image.cpp
)

target_include_directories(zen_module_stb_image
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(zen_module_stb_image PUBLIC zen_static)
target_compile_definitions(zen_module_stb_image PUBLIC ZEN_ENABLE_STB_IMAGE)
target_compile_options(zen_module_stb_image PRIVATE -w $<$<CONFIG:Release>:-O3>)
