cmake_minimum_required(VERSION 3.16)

project(div_cpp_modern LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(div_modern
    builtins.cpp
    main.cpp
    vm.cpp
)

target_include_directories(div_modern PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(div_modern PRIVATE
    -Wall
    -Wextra
    -g
)