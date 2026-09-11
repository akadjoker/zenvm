cmake_minimum_required(VERSION 3.10)
project(zen_cli LANGUAGES CXX)

# CMAKE_SOURCE_DIR on purpose: when zenvm is a subproject the binaries land
# in the parent's bin/, next to whatever else that project builds.
set(ZEN_BIN_DIR ${CMAKE_SOURCE_DIR}/bin)

# The plain CLI links only the core. A batteries-included `zen` with native
# modules is built by zenvm-modules from this same main.cpp — see
# ZEN_CLI_EXTRA_LIBS_HEADER there.
add_executable(zen main.cpp)
set_target_properties(zen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})
target_compile_options(zen PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_definitions(zen PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_link_libraries(zen zen_static)
if(NOT WIN32)
    target_link_options(zen PRIVATE -rdynamic)
else()
    # Windows defaults to a 1 MB stack; the compiler's nested-function
    # CompilerState frames (~25 KB each, up to kMaxFuncNesting deep) need
    # the same 8 MB Linux gives by default.
    target_link_options(zen PRIVATE -Wl,--stack,8388608)
endif()

# Debug trace build
add_executable(zen_debug main.cpp)
set_target_properties(zen_debug PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZEN_BIN_DIR})
target_link_libraries(zen_debug zen_static)
target_compile_definitions(zen_debug PRIVATE ZEN_DEBUG_TRACE_EXEC)
if(WIN32)
    target_link_options(zen_debug PRIVATE -Wl,--stack,8388608)
endif()
if(NOT WIN32)
    target_link_options(zen_debug PRIVATE -rdynamic)
endif()
