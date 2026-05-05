cmake_minimum_required(VERSION 3.10)
project(zen_module_dnn LANGUAGES CXX)

add_library(zen_module_dnn STATIC
    src/builtin_nn.cpp
)

target_include_directories(zen_module_dnn
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(zen_module_dnn PUBLIC zen_static)
target_compile_definitions(zen_module_dnn PUBLIC ZEN_ENABLE_NN)
target_compile_options(zen_module_dnn PRIVATE $<$<CONFIG:Release>:-O3>)

# --- MiniDNN (optional: -DZEN_ENABLE_MINIDNN=ON) ---
option(ZEN_ENABLE_MINIDNN "Enable Network class (MiniDNN + Eigen header-only)" OFF)
if(ZEN_ENABLE_MINIDNN)
    target_compile_definitions(zen_module_dnn PUBLIC ZEN_ENABLE_MINIDNN)
    target_include_directories(zen_module_dnn PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/eigen
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/MiniDNN/include
    )
    # Eigen is header-only, MiniDNN is header-only — no extra libs needed
    # Suppress warnings from header-only vendored libraries
    target_compile_options(zen_module_dnn PRIVATE -w)
endif()
