
project(test_visual)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(NOT TARGET raylib)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/raylib vendor/raylib EXCLUDE_FROM_ALL)
endif()

add_executable(test_visual main.cpp)
target_link_libraries(test_visual PRIVATE zen_static raylib)
target_include_directories(test_visual PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/raylib/src
)
target_compile_definitions(test_visual PRIVATE
    ZENVM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# No sanitizers for visual test (raylib doesn't like them)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_visual PRIVATE -O2 -g -fno-sanitize=all)
endif()
