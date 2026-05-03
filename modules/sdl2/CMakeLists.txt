cmake_minimum_required(VERSION 3.10)
project(zen_module_sdl2 LANGUAGES CXX)

find_package(SDL2 REQUIRED)

add_library(zen_module_sdl2 STATIC
    src/builtin_sdl2.cpp
)

target_include_directories(zen_module_sdl2
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(zen_module_sdl2 PUBLIC zen_static SDL2::SDL2)
else()
    target_include_directories(zen_module_sdl2 PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(zen_module_sdl2 PUBLIC zen_static ${SDL2_LIBRARIES})
endif()

target_compile_definitions(zen_module_sdl2 PUBLIC ZEN_ENABLE_SDL2)
target_compile_options(zen_module_sdl2 PRIVATE $<$<CONFIG:Release>:-O3>)
