// test_struct.cpp — Testa struct O(1) field access
#include "zen/vm.h"
#include "zen/compiler.h"
#include "zen/module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>

using namespace zen;

struct Color { uint8_t r, g, b, a; };

static void color_ctor(VM *vm, void *buffer, int argc, Value *args)
{
    (void)vm;
    Color *c = (Color *)buffer;
    c->r = argc > 0 ? (uint8_t)to_integer(args[0]) : 0;
    c->g = argc > 1 ? (uint8_t)to_integer(args[1]) : 0;
    c->b = argc > 2 ? (uint8_t)to_integer(args[2]) : 0;
    c->a = argc > 3 ? (uint8_t)to_integer(args[3]) : 255;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open '%s'\n", path); return nullptr; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main()
{
    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    // Native struct binding — zero-copy, como BuLang
    vm.register_native_struct("Color", sizeof(Color), color_ctor, nullptr)
        .field("r", offsetof(Color, r), FIELD_BYTE)
        .field("g", offsetof(Color, g), FIELD_BYTE)
        .field("b", offsetof(Color, b), FIELD_BYTE)
        .field("a", offsetof(Color, a), FIELD_BYTE)
        .end();

    const char *filename = "test_functions/struct_native_binding.zen";
    char *source = read_file(filename);
    if (!source) return 1;

    Compiler compiler;
    ObjFunc *func = compiler.compile(&vm.get_gc(), &vm, source, filename);
    free(source);

    if (!func)
    {
        fprintf(stderr, "Compile error!\n");
        return 1;
    }

    printf("=== Native Struct Binding Test ===\n");
    vm.run(func);
    printf("=== PASS ===\n");
    return 0;
}
