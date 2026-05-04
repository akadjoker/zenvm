cmake_minimum_required(VERSION 3.10)
project(zen_module_raylib LANGUAGES CXX)

include(${CMAKE_SOURCE_DIR}/cmake/SystemRaylib.cmake)
zen_find_system_raylib(ZEN_SYSTEM_RAYLIB_FOUND)
if(NOT ZEN_SYSTEM_RAYLIB_FOUND)
    message(WARNING "raylib not found — zen_module_raylib will not be built.")
    return()
endif()

add_library(zen_module_raylib STATIC
    src/builtin_raylib.cpp
)

target_include_directories(zen_module_raylib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(zen_module_raylib PUBLIC zen_static raylib)
target_compile_definitions(zen_module_raylib PUBLIC ZEN_ENABLE_RAYLIB)
target_compile_options(zen_module_raylib PRIVATE $<$<CONFIG:Release>:-O3>)
