cmake_minimum_required(VERSION 3.10)
project(zen_module_canvas LANGUAGES CXX)

# ── Base library — no GL backend (used by CLI and any non-GL runner) ──────────
add_library(zen_module_canvas STATIC
    src/builtin_canvas.cpp
)

target_include_directories(zen_module_canvas
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(zen_module_canvas PUBLIC zen_static)
target_compile_definitions(zen_module_canvas
    PUBLIC  ZEN_ENABLE_CANVAS
    PRIVATE ZEN_CANVAS_NO_BACKEND)
target_compile_options(zen_module_canvas PRIVATE $<$<CONFIG:Release>:-O3>)

# ── GL variant — desktop OpenGL 3.3 (used by zen_gl runner) ──────────────────
add_library(zen_module_canvas_gl STATIC
    src/builtin_canvas.cpp
)

target_include_directories(zen_module_canvas_gl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/opengl/vendor   # glad.h
)

target_link_libraries(zen_module_canvas_gl PUBLIC zen_static)
target_compile_definitions(zen_module_canvas_gl
    PUBLIC  ZEN_ENABLE_CANVAS
    PRIVATE ZEN_CANVAS_GL)
target_compile_options(zen_module_canvas_gl PRIVATE $<$<CONFIG:Release>:-O3>)

if(UNIX AND NOT APPLE)
    target_link_libraries(zen_module_canvas_gl PRIVATE GL)
elseif(APPLE)
    target_link_libraries(zen_module_canvas_gl PRIVATE "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(zen_module_canvas_gl PRIVATE opengl32)
endif()

# ── GLES variant — OpenGL ES 3 / WebGL 2 (Emscripten / Android / iOS) ────────
if(EMSCRIPTEN OR ANDROID OR IOS OR TVOS)
    add_library(zen_module_canvas_gles STATIC
        src/builtin_canvas.cpp
    )
    target_include_directories(zen_module_canvas_gles
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(zen_module_canvas_gles PUBLIC zen_static)
    target_compile_definitions(zen_module_canvas_gles
        PUBLIC  ZEN_ENABLE_CANVAS
        PRIVATE ZEN_CANVAS_GLES)
    target_compile_options(zen_module_canvas_gles PRIVATE $<$<CONFIG:Release>:-O3>)
    if(ANDROID)
        target_link_libraries(zen_module_canvas_gles PRIVATE GLESv3 EGL)
    elseif(IOS OR TVOS)
        target_link_libraries(zen_module_canvas_gles PRIVATE "-framework OpenGLES")
    endif()
endif()
