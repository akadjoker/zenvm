cmake_minimum_required(VERSION 3.10)
project(zen_cli LANGUAGES CXX)

# Find the zen library (built by parent)
# When built as subdirectory, zen_static target is already available

add_executable(zen main.cpp)
target_link_libraries(zen zen_static)
if(NOT WIN32)
    target_link_options(zen PRIVATE -rdynamic)
endif()

# Debug trace build
add_executable(zen_debug main.cpp)
target_link_libraries(zen_debug zen_static)
target_compile_definitions(zen_debug PRIVATE ZEN_DEBUG_TRACE_EXEC)
if(NOT WIN32)
    target_link_options(zen_debug PRIVATE -rdynamic)
endif()
