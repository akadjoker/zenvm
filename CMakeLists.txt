cmake_minimum_required(VERSION 3.10)
project(zen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common warnings
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# --- Optional modules (disable with -DZEN_ENABLE_*=OFF) ---
option(ZEN_ENABLE_REGEX  "Enable regex module (libregexp, zero deps)" ON)
option(ZEN_ENABLE_ZIP    "Enable zip module (requires miniz)" ON)
option(ZEN_ENABLE_NET    "Enable net module (POSIX sockets)" ON)
option(ZEN_ENABLE_HTTP   "Enable http module (POSIX sockets)" ON)
option(ZEN_ENABLE_CRYPTO "Enable crypto module (pure C)" ON)
option(ZEN_ENABLE_NN     "Enable nn module (pure C math)" ON)
option(ZEN_ENABLE_JSON   "Enable json module (pure C)" ON)
option(ZEN_ENABLE_UTF8   "Enable utf8 module (pure C)" ON)

# --- Vendor libraries (conditional) ---
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ZEN_VENDOR_LIBS "")
set(ZEN_VENDOR_INCLUDES "")
set(ZEN_DEFINITIONS "")

if(ZEN_ENABLE_REGEX)
    # libregexp from QuickJS (Bellard) — zero external deps
    enable_language(C)
    add_library(libregexp STATIC
        vendor/libregexp/libregexp.c
        vendor/libregexp/libunicode.c
    )
    target_include_directories(libregexp PUBLIC vendor/libregexp)
    target_compile_options(libregexp PRIVATE -w)  # suppress warnings in vendored code
    list(APPEND ZEN_VENDOR_LIBS libregexp)
    list(APPEND ZEN_VENDOR_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/libregexp)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_REGEX)
else()
    list(APPEND ZEN_DEFINITIONS ZEN_NO_REGEX)
endif()

if(ZEN_ENABLE_ZIP)
    add_subdirectory(vendor/miniz vendor/miniz/build EXCLUDE_FROM_ALL)
    list(APPEND ZEN_VENDOR_LIBS miniz)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_ZIP)
else()
    list(APPEND ZEN_DEFINITIONS ZEN_NO_ZIP)
endif()

if(ZEN_ENABLE_NET)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_NET)
endif()

if(ZEN_ENABLE_HTTP)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_HTTP)
endif()

if(ZEN_ENABLE_CRYPTO)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_CRYPTO)
endif()

if(ZEN_ENABLE_NN)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_NN)
endif()

if(ZEN_ENABLE_JSON)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_JSON)
endif()

if(ZEN_ENABLE_UTF8)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_UTF8)
endif()

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
list(FILTER CORE_SRCS EXCLUDE REGEX "src/builtin_regex\\.cpp$")

# --- libzen (static library) ---
add_library(zen_static STATIC ${CORE_SRCS})
target_include_directories(zen_static PUBLIC include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_link_libraries(zen_static PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
target_compile_definitions(zen_static PUBLIC ${ZEN_DEFINITIONS})
set_target_properties(zen_static PROPERTIES OUTPUT_NAME zen POSITION_INDEPENDENT_CODE ON)

# --- libzen (shared library) ---
add_library(zen_shared SHARED ${CORE_SRCS})
target_include_directories(zen_shared PUBLIC include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_link_libraries(zen_shared PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
target_compile_definitions(zen_shared PUBLIC ${ZEN_DEFINITIONS})
set_target_properties(zen_shared PROPERTIES OUTPUT_NAME zen)

# --- CLI (zen executable) ---
add_subdirectory(cli)

# --- Test functions (ScriptComponent O(1) pattern) ---
add_subdirectory(test_functions)

# --- Class API tests (C++ ↔ Script vtable) ---
add_subdirectory(tests/test_class_api)

# --- VM test harness (old handcoded bytecode tests) ---
# add_executable(zen_test_vm src/main.cpp)
# target_link_libraries(zen_test_vm zen_static)

# --- Edge case tests ---
# add_executable(zen_test_edge src/test_edge_cases.cpp src/memory.cpp)
# target_include_directories(zen_test_edge PRIVATE include/zen)
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     target_compile_definitions(zen_test_edge PRIVATE ASAN_OPTIONS=detect_leaks=0)
# endif()
