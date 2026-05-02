cmake_minimum_required(VERSION 3.10)
project(libzen LANGUAGES CXX)

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
        ../vendor/libregexp/libregexp.c
        ../vendor/libregexp/libunicode.c
    )
    target_include_directories(libregexp PUBLIC ../vendor/libregexp)
    target_compile_options(libregexp PRIVATE -w)  # suppress warnings in vendored code
    list(APPEND ZEN_VENDOR_LIBS libregexp)
    list(APPEND ZEN_VENDOR_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/libregexp)
    list(APPEND ZEN_DEFINITIONS ZEN_ENABLE_REGEX)
else()
    list(APPEND ZEN_DEFINITIONS ZEN_NO_REGEX)
endif()

if(ZEN_ENABLE_ZIP)
    add_subdirectory(../vendor/miniz vendor/miniz/build EXCLUDE_FROM_ALL)
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

# Core library sources
file(GLOB CORE_SRCS src/*.cpp)

# --- libzen (static library) ---
add_library(zen_static STATIC ${CORE_SRCS})
target_include_directories(zen_static PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_link_libraries(zen_static PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
target_compile_definitions(zen_static PUBLIC ${ZEN_DEFINITIONS})
set_target_properties(zen_static PROPERTIES OUTPUT_NAME zen POSITION_INDEPENDENT_CODE ON)

# --- libzen (shared library) ---
add_library(zen_shared SHARED ${CORE_SRCS})
target_include_directories(zen_shared PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_link_libraries(zen_shared PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
target_compile_definitions(zen_shared PUBLIC ${ZEN_DEFINITIONS})
set_target_properties(zen_shared PROPERTIES OUTPUT_NAME zen)

