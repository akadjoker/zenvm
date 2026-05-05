cmake_minimum_required(VERSION 3.10)
project(zen_module_sqlite LANGUAGES C CXX)

# SQLite amalgamation — compiled as C, wrapped by C++
add_library(zen_module_sqlite STATIC
    vendor/sqlite3.c
    src/builtin_sqlite.cpp
)

target_include_directories(zen_module_sqlite
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(zen_module_sqlite PUBLIC zen_static)
target_compile_definitions(zen_module_sqlite PUBLIC ZEN_ENABLE_SQLITE)

# sqlite3.c is pure C — suppress C++ warnings on it
set_source_files_properties(vendor/sqlite3.c PROPERTIES COMPILE_FLAGS "-w")

# Link dl (dlopen used by SQLite on Linux for extensions)
if(UNIX)
    target_link_libraries(zen_module_sqlite PUBLIC dl)
endif()
