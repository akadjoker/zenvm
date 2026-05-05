cmake_minimum_required(VERSION 3.10)
project(zen_module_gl LANGUAGES CXX)

# Base sources — ES 3.0 core, available everywhere
set(ZEN_GL_SOURCES src/zen_gl.cpp)

# Desktop-only sources (GL 4.x + legacy immediate mode)
# Excluded on GLES targets (Emscripten, Android)
if(NOT EMSCRIPTEN AND NOT ANDROID)
    list(APPEND ZEN_GL_SOURCES
        src/zen_gl4.cpp
        src/zen_gl_legacy.cpp
    )
endif()

add_library(zen_module_gl STATIC ${ZEN_GL_SOURCES})

target_include_directories(zen_module_gl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(zen_module_gl PUBLIC zen_static)
target_compile_definitions(zen_module_gl PUBLIC ZEN_ENABLE_GL)
target_compile_options(zen_module_gl PRIVATE $<$<CONFIG:Release>:-O3>)

# Link platform GL library
if(EMSCRIPTEN)
    # WebGL — no extra link needed, emscripten provides it
elseif(ANDROID)
    target_link_libraries(zen_module_gl PUBLIC GLESv3 EGL)
elseif(APPLE)
    find_library(OPENGL_LIB OpenGL)
    target_link_libraries(zen_module_gl PUBLIC ${OPENGL_LIB})
else()
    # Linux/Windows desktop
    find_package(OpenGL REQUIRED)
    target_link_libraries(zen_module_gl PUBLIC OpenGL::GL)
    if(UNIX AND NOT APPLE)
        target_link_libraries(zen_module_gl PUBLIC ${CMAKE_DL_LIBS})
    endif()
endif()

# Define ZEN_ENABLE_GL4 for desktop builds
if(NOT EMSCRIPTEN AND NOT ANDROID)
    target_compile_definitions(zen_module_gl PUBLIC ZEN_ENABLE_GL4)
endif()
