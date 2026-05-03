cmake_minimum_required(VERSION 3.10)
project(zen LANGUAGES CXX)
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(SystemRaylib)

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

# --- Optional modules ---
option(ZEN_ENABLE_STB_IMAGE "Enable stb_image image module" ON)
if(ZEN_ENABLE_STB_IMAGE)
    add_subdirectory(modules/stb_image)
endif()

option(ZEN_ENABLE_GLFW "Enable GLFW window module" ON)
if(ZEN_ENABLE_GLFW)
    add_subdirectory(modules/glfw)
endif()

option(ZEN_ENABLE_SDL2 "Enable SDL2 window module" OFF)
if(ZEN_ENABLE_SDL2)
    add_subdirectory(modules/sdl2)
endif()

# --- CLI (zen executable) ---
add_subdirectory(cli)

# --- Test functions (ScriptComponent O(1) pattern) ---
#add_subdirectory(test_functions)

# --- Class API tests (C++ ↔ Script vtable) ---
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_class_api/CMakeLists.txt")
    add_subdirectory(tests/test_class_api)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/test_class_api/CMakeLists.txt")
    add_subdirectory(test_class_api)
endif()

# --- Visual benchmark (raylib, process system) ---
option(ZEN_BUILD_VISUAL "Build visual benchmark (requires raylib)" ON)
option(ZEN_BUILD_GFX "Build graphics engine prototype (requires raylib)" ON)
if(ZEN_BUILD_VISUAL OR ZEN_BUILD_GFX)
    zen_find_system_raylib(ZEN_SYSTEM_RAYLIB_FOUND)
    if(NOT ZEN_SYSTEM_RAYLIB_FOUND)
        message(WARNING "System raylib not found; disabling raylib demos. Install raylib or set ZEN_BUILD_VISUAL=OFF/ZEN_BUILD_GFX=OFF.")
        set(ZEN_BUILD_VISUAL OFF)
        set(ZEN_BUILD_GFX OFF)
    endif()
endif()

if(ZEN_BUILD_VISUAL)
    add_subdirectory(test_visual)
    add_subdirectory(test_visual_loop)
    add_subdirectory(test_visual_intern)
    add_subdirectory(test_visual_engine)
    
endif()
if(ZEN_BUILD_GFX)
    add_subdirectory(test_gfx)
endif()

# --- Snapshot tests for tests/*.zen (ctest integration) ---
# Run with:
#   cd build && ctest --output-on-failure
# Refresh snapshots with:
#   tests/run_zen_tests.sh ./bin/zen --update
enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/run_zen_tests.sh")
    add_test(
        NAME zen_snapshot_suite
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/run_zen_tests.sh"
                "$<TARGET_FILE:zen>"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(zen_snapshot_suite PROPERTIES
        TIMEOUT 600
        LABELS "snapshot"
    )
endif()
