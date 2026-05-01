// main.cpp — Testa padrão ScriptComponent com call O(1)
// Compila script zen, cacheia índices, chama callbacks sem lookup

#include "zen/vm.h"
#include "zen/compiler.h"
#include "zen/module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace zen;

// ============================================
// ScriptComponent — índices cacheados O(1)
// ============================================

struct ScriptComponent
{
    int on_ready;
    int on_update;
    int on_draw;
    int on_hit;
    int var_hp;
    int var_x;
    int var_y;
};

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

static bool load_and_run(VM &vm, const char *filename)
{
    char *source = read_file(filename);
    if (!source) return false;

    Compiler compiler;
    ObjFunc *func = compiler.compile(&vm.get_gc(), &vm, source, filename);
    free(source);
    if (!func) { fprintf(stderr, "Compile error in '%s'\n", filename); return false; }

    vm.run(func);
    return true;
}

static ScriptComponent cache_indices(VM &vm)
{
    ScriptComponent sc;
    sc.on_ready  = vm.find_global("on_ready");
    sc.on_update = vm.find_global("on_update");
    sc.on_draw   = vm.find_global("on_draw");
    sc.on_hit    = vm.find_global("on_hit");
    sc.var_hp    = vm.find_global("hp");
    sc.var_x     = vm.find_global("x");
    sc.var_y     = vm.find_global("y");
    return sc;
}

// ============================================
// Teste: carrega script, chama callbacks O(1)
// ============================================

static int test_script(const char *filename)
{
    printf("=== Testing: %s ===\n", filename);

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);

    if (!load_and_run(vm, filename))
        return 1;

    ScriptComponent sc = cache_indices(vm);

    // on_ready — O(1)
    if (sc.on_ready >= 0)
        vm.call_global(sc.on_ready, nullptr, 0);

    // Simula 3 frames de on_update + on_draw — O(1) cada
    for (int frame = 0; frame < 3; frame++)
    {
        Value dt = val_float(0.016f);
        if (sc.on_update >= 0)
            vm.call_global(sc.on_update, &dt, 1);
        if (sc.on_draw >= 0)
            vm.call_global(sc.on_draw, nullptr, 0);
    }

    // on_hit(30) — O(1)
    if (sc.on_hit >= 0)
    {
        Value dmg = val_int(30);
        vm.call_global(sc.on_hit, &dmg, 1);
    }

    // Ler variável do script — O(1)
    if (sc.var_hp >= 0)
    {
        Value hp = vm.get_global(sc.var_hp);
        printf("  C++ reads hp = %lld\n", (long long)to_integer(hp));
    }

    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        // Testar ficheiro específico
        return test_script(argv[1]);
    }

    // Testar todos
    const char *scripts[] = { "player.zen", "enemy.zen", "callbacks.zen", "native_calls.zen" };
    int errors = 0;
    for (int i = 0; i < 4; i++)
        errors += test_script(scripts[i]);

    printf("Done. Errors: %d\n", errors);
    return errors;
}
