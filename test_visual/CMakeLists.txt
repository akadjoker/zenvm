
project(test_visual)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(NOT TARGET raylib)
    include(${CMAKE_SOURCE_DIR}/cmake/SystemRaylib.cmake)
    zen_find_system_raylib(ZEN_SYSTEM_RAYLIB_FOUND)
    if(NOT ZEN_SYSTEM_RAYLIB_FOUND)
        message(FATAL_ERROR "System raylib not found. Install raylib or disable ZEN_BUILD_VISUAL.")
    endif()
endif()

add_executable(test_visual main.cpp)
target_link_libraries(test_visual PRIVATE zen_static raylib)
target_compile_definitions(test_visual PRIVATE
    ZENVM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# No sanitizers for visual test (raylib doesn't like them)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_visual PRIVATE -O2 -g -fno-sanitize=all)
endif()
