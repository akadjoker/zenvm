# test_visual — raylib visual benchmark for process system
set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/raylib vendor/raylib EXCLUDE_FROM_ALL)

add_executable(test_visual main.cpp)
target_link_libraries(test_visual PRIVATE zen_static raylib)
target_include_directories(test_visual PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/raylib/src
)

# No sanitizers for visual test (raylib doesn't like them)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_visual PRIVATE -O2 -g -fno-sanitize=all)
endif()
