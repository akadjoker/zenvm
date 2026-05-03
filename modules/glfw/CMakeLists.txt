cmake_minimum_required(VERSION 3.10)
project(zen_module_glfw LANGUAGES C CXX)

add_library(zen_glfw_backend STATIC
    src/rglfw.c
)

target_include_directories(zen_glfw_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/src
)

target_compile_options(zen_glfw_backend PRIVATE -w)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    target_include_directories(zen_glfw_backend PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(zen_glfw_backend PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m ${X11_LIBRARIES})
    if(X11_Xrandr_LIB)
        target_link_libraries(zen_glfw_backend PUBLIC ${X11_Xrandr_LIB})
    endif()
    if(X11_Xinerama_LIB)
        target_link_libraries(zen_glfw_backend PUBLIC ${X11_Xinerama_LIB})
    endif()
    if(X11_Xcursor_LIB)
        target_link_libraries(zen_glfw_backend PUBLIC ${X11_Xcursor_LIB})
    endif()
    if(X11_Xi_LIB)
        target_link_libraries(zen_glfw_backend PUBLIC ${X11_Xi_LIB})
    endif()
endif()

if(WIN32)
    target_link_libraries(zen_glfw_backend PUBLIC gdi32 user32 shell32)
endif()

if(APPLE)
    target_link_libraries(zen_glfw_backend PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
endif()

add_library(zen_module_glfw STATIC
    src/builtin_glfw.cpp
)

target_include_directories(zen_module_glfw
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
)

target_link_libraries(zen_module_glfw PUBLIC zen_static zen_glfw_backend)
target_compile_definitions(zen_module_glfw PUBLIC ZEN_ENABLE_GLFW)
target_compile_options(zen_module_glfw PRIVATE $<$<CONFIG:Release>:-O3>)
