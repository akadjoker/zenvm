/*
** zen_game — Zen runner with Raylib module pre-loaded.
** Usage:
**   ./zen_game game.zen
**   ./zen_game -e "rl_init_window(640,480,\"hi\"); ..."
**
** Identical to the CLI but always registers zen_lib_raylib.
** Keep the CLI lean (no Raylib dep); use this for games.
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "debug.h"
#include "bytecode.h"
#include "zen/module_raylib.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace zen;

/* =========================================================
** Read file into malloc'd buffer
** ========================================================= */

static char *read_file(const char *path, long *out_size = nullptr)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "zen_game: cannot open '%s'\n", path); return nullptr; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); fprintf(stderr, "zen_game: out of memory\n"); return nullptr; }
    size_t r = fread(buf, 1, size, f);
    buf[r] = '\0';
    fclose(f);
    if (out_size) *out_size = (long)r;
    return buf;
}

/* =========================================================
** VM setup
** ========================================================= */

static const char *g_dump_path = nullptr;
static bool g_verbose = false;
static bool g_disassemble = false;

static void register_libs(VM &vm)
{
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_time);
    vm.register_lib(&zen_lib_fs);
    vm.register_lib(&zen_lib_path);
    vm.register_lib(&zen_lib_file);
    vm.register_lib(&zen_lib_easing);
    vm.register_lib(&zen_lib_base64);
#ifdef ZEN_ENABLE_RAYLIB
    vm.open_lib(&zen_lib_raylib); /* open_lib: runs init_fn (registers types) + globals */
#endif
}

static void install_args(VM &vm, int argc, char **argv)
{
    ObjArray *arr = new_array(&vm.get_gc());
    for (int i = 0; i < argc; i++)
    {
        ObjString *s = vm.make_string(argv[i]);
        array_push(&vm.get_gc(), arr, val_obj((Obj *)s));
    }
    vm.def_global("args", val_obj((Obj *)arr));
}

static int run_source(const char *src, const char *filename,
                      int script_argc = 0, char **script_argv = nullptr)
{
    VM vm;
    register_libs(vm);
    install_args(vm, script_argc, script_argv);

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, src, filename);
    if (!fn) { fprintf(stderr, "zen_game: compilation failed.\n"); return 1; }

    if (g_disassemble)
    {
        disassemble_func(fn, filename);
        printf("--- execution ---\n");
    }

    if (g_dump_path)
    {
        char err[256] = {0};
        BytecodeStats stats;
        if (!dump_bytecode_file(&vm, fn, g_dump_path, false,
                                g_verbose ? &stats : nullptr, err, sizeof(err)))
        {
            fprintf(stderr, "zen_game: bytecode dump failed: %s\n",
                    err[0] ? err : "unknown error");
            return 1;
        }
        if (g_verbose)
        {
            printf("zen_game: dumped bytecode '%s'\n", g_dump_path);
            printf("  bytes:        %zu\n", stats.bytes);
            printf("  functions:    %u\n", stats.functions);
            printf("  instructions: %u\n", stats.instructions);
        }
        return 0;
    }

    vm.run(fn);
    return vm.had_error() ? 1 : 0;
}

static int run_bytecode(const uint8_t *data, size_t size, const char *filename,
                        int script_argc = 0, char **script_argv = nullptr)
{
    VM vm;
    register_libs(vm);
    install_args(vm, script_argc, script_argv);

    char err[256] = {0};
    ObjFunc *fn = load_bytecode_buffer(&vm, data, size, err, sizeof(err));
    if (!fn)
    {
        fprintf(stderr, "zen_game: bytecode load failed: %s\n",
                err[0] ? err : "unknown error");
        return 1;
    }

    if (g_disassemble)
    {
        disassemble_func(fn, filename);
        printf("--- execution ---\n");
    }

    vm.run(fn);
    return vm.had_error() ? 1 : 0;
}

/* =========================================================
** Main
** ========================================================= */

int main(int argc, char **argv)
{
    const char *inline_code = nullptr;
    const char *file_path   = nullptr;
    int script_arg_start    = argc;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-e") == 0 && i + 1 < argc)
        {
            inline_code = argv[++i];
            script_arg_start = i + 1;
            break;
        }
        else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
        {
            g_dump_path = argv[++i];
        }
        else if (strcmp(argv[i], "--dis") == 0 || strcmp(argv[i], "-d") == 0)
        {
            g_disassemble = true;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            g_verbose = true;
        }
        else if (argv[i][0] != '-')
        {
            file_path = argv[i];
            script_arg_start = i + 1;
            break;
        }
    }

    if (inline_code)
        return run_source(inline_code, "<cmdline>",
                          argc - script_arg_start, argv + script_arg_start);

    if (file_path)
    {
        long size = 0;
        char *data = read_file(file_path, &size);
        if (!data) return 1;
        int rc = is_bytecode_buffer((const uint8_t *)data, (size_t)size)
                     ? run_bytecode((const uint8_t *)data, (size_t)size, file_path,
                                    argc - script_arg_start, argv + script_arg_start)
                     : run_source(data, file_path,
                                  argc - script_arg_start, argv + script_arg_start);
        free(data);
        return rc;
    }

    fprintf(stderr, "Usage: zen_game <script.zen> [args...]\n"
                    "       zen_game -e \"code\"\n"
                    "       zen_game --dump out.znb script.zen\n"
                    "       zen_game out.znb\n");
    return 1;
}
