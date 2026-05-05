cmake_minimum_required(VERSION 3.10)
project(zen_module_audio LANGUAGES CXX)

add_library(zen_module_audio STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bugl_audio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin_audio.cpp
)

target_include_directories(zen_module_audio
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(zen_module_audio PUBLIC zen_static)
target_compile_definitions(zen_module_audio PUBLIC ZEN_ENABLE_AUDIO)

# miniaudio needs pthread + dl + m on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(zen_module_audio PUBLIC pthread dl m)
endif()

target_compile_options(zen_module_audio PRIVATE -w $<$<CONFIG:Release>:-O3>)
