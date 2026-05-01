# Generate libbu/include/stdlib_embedded.h from libbu/stdlib.bu
# Usage:
#   cmake -DINPUT=<path/to/stdlib.bu> -DOUTPUT=<path/to/stdlib_embedded.h> -P embed_stdlib.cmake

if(NOT DEFINED INPUT)
    message(FATAL_ERROR "embed_stdlib.cmake: INPUT is required")
endif()

if(NOT DEFINED OUTPUT)
    message(FATAL_ERROR "embed_stdlib.cmake: OUTPUT is required")
endif()

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "embed_stdlib.cmake: INPUT does not exist: ${INPUT}")
endif()

file(READ "${INPUT}" _stdlib_hex HEX)
string(LENGTH "${_stdlib_hex}" _stdlib_hex_len)

if(_stdlib_hex_len EQUAL 0)
    set(_stdlib_len 0)
    set(_stdlib_body "0x00")
else()
    math(EXPR _stdlib_len "${_stdlib_hex_len} / 2")
    math(EXPR _last_index "${_stdlib_len} - 1")

    set(_stdlib_body "")
    set(_bytes_in_line 0)

    foreach(_i RANGE 0 ${_last_index})
        math(EXPR _offset "${_i} * 2")
        string(SUBSTRING "${_stdlib_hex}" ${_offset} 2 _byte)
        string(APPEND _stdlib_body "0x${_byte}, ")

        math(EXPR _bytes_in_line "${_bytes_in_line} + 1")
        if(_bytes_in_line EQUAL 16)
            string(APPEND _stdlib_body "\n    ")
            set(_bytes_in_line 0)
        endif()
    endforeach()

    # Keep a C-string-compatible trailing NUL while preserving logical source length.
    string(APPEND _stdlib_body "0x00")
endif()

file(WRITE "${OUTPUT}" "// Auto-generated from stdlib.bu — DO NOT EDIT\n")
file(APPEND "${OUTPUT}" "#pragma once\n\n")
file(APPEND "${OUTPUT}" "#include <cstddef>\n\n")
file(APPEND "${OUTPUT}" "static const unsigned char STDLIB_SOURCE_BYTES[] = {\n")
file(APPEND "${OUTPUT}" "    ${_stdlib_body}\n")
file(APPEND "${OUTPUT}" "};\n\n")
file(APPEND "${OUTPUT}" "static const char* STDLIB_SOURCE = reinterpret_cast<const char*>(STDLIB_SOURCE_BYTES);\n")
file(APPEND "${OUTPUT}" "\n")
file(APPEND "${OUTPUT}" "static const size_t STDLIB_SOURCE_LEN = ${_stdlib_len};\n")
