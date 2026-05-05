cmake_minimum_required(VERSION 3.10)
project(zen_gl_runner LANGUAGES CXX)

set(ZEN_BIN_DIR ${CMAKE_SOURCE_DIR}/bin)

add_executable(zen_gl main.cpp)
set_target_properties(zen_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})
target_compile_options(zen_gl PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_definitions(zen_gl PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_link_libraries(zen_gl zen_static)

if(TARGET zen_module_glfw)
    target_link_libraries(zen_gl zen_module_glfw)
endif()

if(TARGET zen_module_gl)
    target_link_libraries(zen_gl zen_module_gl)
endif()

if(TARGET zen_module_stb_image)
    target_link_libraries(zen_gl zen_module_stb_image)
endif()

if(TARGET zen_module_stb_perlin)
    target_link_libraries(zen_gl zen_module_stb_perlin)
endif()

if(TARGET zen_module_stb_rectpack)
    target_link_libraries(zen_gl zen_module_stb_rectpack)
endif()

if(TARGET zen_module_stb_font)
    target_link_libraries(zen_gl zen_module_stb_font)
endif()

if(TARGET zen_module_audio)
    target_link_libraries(zen_gl zen_module_audio)
endif()

if(TARGET zen_module_sdl2)
    target_link_libraries(zen_gl zen_module_sdl2)
endif()

if(NOT WIN32)
    target_link_options(zen_gl PRIVATE -rdynamic)
endif()
