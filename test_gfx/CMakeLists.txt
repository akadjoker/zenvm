# test_gfx — standalone Raylib graphics engine prototype
# No VM, no scripting — pure C++ test of regions/cameras/ctype

set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)

# Reuse raylib already added by test_visual (or add it if building standalone)
if(NOT TARGET raylib)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/raylib vendor/raylib_gfx EXCLUDE_FROM_ALL)
endif()

add_executable(test_gfx main.cpp graph.cpp quadtree.cpp)
target_link_libraries(test_gfx PRIVATE raylib)
target_include_directories(test_gfx PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/raylib/src
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_gfx PRIVATE -O2 -g -fno-sanitize=all)
endif()
