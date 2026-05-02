cmake_minimum_required(VERSION 3.10)
project(zen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common warnings
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# --- Build types ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Sanitizer flags for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fno-omit-frame-pointer)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# --- libzen (static + shared libraries) ---
add_subdirectory(libzen)

# --- CLI (zen executable) ---
add_subdirectory(cli)

# --- Test functions (ScriptComponent O(1) pattern) ---
#add_subdirectory(test_functions)

# --- Class API tests (C++ ↔ Script vtable) ---
add_subdirectory(tests/test_class_api)

# --- Visual benchmark (raylib, process system) ---
option(ZEN_BUILD_VISUAL "Build visual benchmark (requires raylib)" ON)
if(ZEN_BUILD_VISUAL)
    add_subdirectory(test_visual)
endif()

option(ZEN_BUILD_GFX "Build graphics engine prototype (requires raylib)" ON)
if(ZEN_BUILD_GFX)
    add_subdirectory(test_gfx)
endif()