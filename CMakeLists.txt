cmake_minimum_required(VERSION 3.10)
project(zen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common warnings
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# --- Build types ---
# Debug (default): sanitizers + no optimization
# Release: -O2, no sanitizers
# RelWithDebInfo: -O2 -g

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Sanitizer flags for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fno-omit-frame-pointer)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Core library sources (everything except main entry points and tests)
file(GLOB CORE_SRCS src/*.cpp)
list(FILTER CORE_SRCS EXCLUDE REGEX "src/main\\.cpp$")
list(FILTER CORE_SRCS EXCLUDE REGEX "src/zen_main\\.cpp$")
list(FILTER CORE_SRCS EXCLUDE REGEX "src/test_.*\\.cpp$")

# --- zen CLI ---
add_executable(zen src/zen_main.cpp ${CORE_SRCS})
target_include_directories(zen PRIVATE src)

# --- VM test harness (old handcoded bytecode tests) ---
add_executable(zen_test_vm src/main.cpp ${CORE_SRCS})
target_include_directories(zen_test_vm PRIVATE src)

# --- Edge case tests ---
add_executable(zen_test_edge src/test_edge_cases.cpp src/memory.cpp)
target_include_directories(zen_test_edge PRIVATE src)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(zen_test_edge PRIVATE ASAN_OPTIONS=detect_leaks=0)
endif()

# --- Debug trace build ---
add_executable(zen_debug src/zen_main.cpp ${CORE_SRCS})
target_include_directories(zen_debug PRIVATE src)
target_compile_definitions(zen_debug PRIVATE ZEN_DEBUG_TRACE_EXEC)
