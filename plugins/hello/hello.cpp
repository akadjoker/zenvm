#include "zen_plugin.h"
using namespace zen;

static int nat_hello_greet(VM *vm, Value *args, int nargs)
{
    const char *name = "world";
    if (nargs >= 1 && is_string(args[0]))
        name = as_cstring(args[0]);

    char buf[128];
    int len = snprintf(buf, sizeof(buf), "Hello, %s!", name);
    args[0] = val_obj((Obj *)vm->make_string(buf, len));
    return 1;
}

static int nat_hello_add(VM *vm, Value *args, int nargs)
{
    (void)vm;
    if (nargs < 2) { args[0] = val_int(0); return 1; }
    int64_t a = is_int(args[0]) ? args[0].as.integer : 0;
    int64_t b = is_int(args[1]) ? args[1].as.integer : 0;
    args[0] = val_int(a + b);
    return 1;
}

static const NativeReg hello_funcs[] = {
    {"greet", nat_hello_greet, -1},
    {"add", nat_hello_add, 2},
};

static const NativeLib hello_lib = {
    "hello", hello_funcs, 2, nullptr, 0
};

ZEN_EXPORT const NativeLib* zen_open_hello(void) {
    return &hello_lib;
}
