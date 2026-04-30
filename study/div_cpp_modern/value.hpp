#pragma once

#include <cstring>
#include <vector>

enum class ValueKind {
    NONE,
    INT,
    FLOAT,
    STRING,
};

struct Value {
    ValueKind kind = ValueKind::NONE;
    int payload = 0;

    static Value none() {
        return {};
    }

    static Value make_int(int value) {
        return Value{ValueKind::INT, value};
    }

    static Value make_float(float value) {
        int bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "float size inesperado");
        std::memcpy(&bits, &value, sizeof(value));
        return Value{ValueKind::FLOAT, bits};
    }

    static Value make_string(int handle) {
        return Value{ValueKind::STRING, handle};
    }

    float as_float() const {
        float value = 0.0f;
        std::memcpy(&value, &payload, sizeof(value));
        return value;
    }
};

struct DivString {
    int ref_count = 0;
    std::vector<char> bytes;
};