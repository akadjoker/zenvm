cmake_minimum_required(VERSION 3.10)
project(zen_module_gif LANGUAGES CXX)

add_library(zen_module_gif STATIC
    src/builtin_gif.cpp
)

target_include_directories(zen_module_gif
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(zen_module_gif PUBLIC zen_static)
target_compile_definitions(zen_module_gif PUBLIC ZEN_ENABLE_GIF)
target_compile_options(zen_module_gif PRIVATE $<$<CONFIG:Release>:-O3> -Wno-missing-field-initializers)
