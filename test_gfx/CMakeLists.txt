# test_gfx — standalone Raylib graphics engine prototype
# No VM, no scripting — pure C++ test of regions/cameras/ctype

if(NOT TARGET raylib)
    include(${CMAKE_SOURCE_DIR}/cmake/SystemRaylib.cmake)
    zen_find_system_raylib(ZEN_SYSTEM_RAYLIB_FOUND)
    if(NOT ZEN_SYSTEM_RAYLIB_FOUND)
        message(FATAL_ERROR "System raylib not found. Install raylib or disable ZEN_BUILD_GFX.")
    endif()
endif()

add_executable(test_gfx main.cpp graph.cpp quadtree.cpp)
target_link_libraries(test_gfx PRIVATE raylib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_gfx PRIVATE -O2 -g -fno-sanitize=all)
endif()
