/*
** zen_gl — Zen runner with GLFW + OpenGL + STB Image modules pre-loaded.
** Usage:
**   ./zen_gl game.zen
**   ./zen_gl -e "code"
**   ./zen_gl --dump out.znb game.zen
**   ./zen_gl out.znb
**
** The script is responsible for creating a GLFW window and calling
** LoadOpenGLExtensions() before issuing any GL commands.
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "debug.h"
#include "bytecode.h"
#ifdef ZEN_ENABLE_GLFW
#    include "zen/module_glfw.h"
#endif
#ifdef ZEN_ENABLE_GL
#    include "zen/module_gl.h"
#endif
#ifdef ZEN_ENABLE_STB_IMAGE
#    include "zen/module_image.h"
#endif
#ifdef ZEN_ENABLE_STB_PERLIN
#    include "zen/module_noise.h"
#endif
#ifdef ZEN_ENABLE_STB_RECTPACK
#    include "zen/module_rectpack.h"
#endif
#ifdef ZEN_ENABLE_STB_FONT
#    include "zen/module_font.h"
#endif
#ifdef ZEN_ENABLE_AUDIO
#    include "zen/module_audio.h"
#endif
#ifdef ZEN_ENABLE_SDL2
#    include "zen/module_sdl2.h"
#endif
#ifdef ZEN_ENABLE_CANVAS
#    include "module.h"  /* zen_lib_canvas declared in module.h */
#endif
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
    if (!f) { fprintf(stderr, "zen_gl: cannot open '%s'\n", path); return nullptr; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); fprintf(stderr, "zen_gl: out of memory\n"); return nullptr; }
    size_t r = fread(buf, 1, size, f);
    buf[r] = '\0';
    fclose(f);
    if (out_size) *out_size = (long)r;
    return buf;
}

/* =========================================================
** VM setup
** ========================================================= */

static const char *g_dump_path  = nullptr;
static bool        g_verbose    = false;
static bool        g_disassemble = false;

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
#ifdef ZEN_ENABLE_GLFW
    vm.register_lib(&zen_lib_glfw);
#endif
#ifdef ZEN_ENABLE_GL
    vm.register_lib(&zen_lib_gl);
#  ifdef ZEN_ENABLE_GL4
    vm.register_lib(&zen_lib_gl4);
    vm.register_lib(&zen_lib_gll);
#  endif
#endif
#ifdef ZEN_ENABLE_STB_IMAGE
    vm.register_lib(&zen_lib_image);
#endif
#ifdef ZEN_ENABLE_STB_PERLIN
    vm.register_lib(&zen_lib_noise);
#endif
#ifdef ZEN_ENABLE_STB_RECTPACK
    vm.register_lib(&zen_lib_rectpack);
#endif
#ifdef ZEN_ENABLE_STB_FONT
    vm.register_lib(&zen_lib_font);
#endif
#ifdef ZEN_ENABLE_AUDIO
    vm.register_lib(&zen_lib_audio);
#endif
#ifdef ZEN_ENABLE_SDL2
    vm.register_lib(&zen_lib_sdl2);
#endif
#ifdef ZEN_ENABLE_CANVAS
    vm.register_lib(&zen_lib_canvas);
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
    if (!fn) { fprintf(stderr, "zen_gl: compilation failed.\n"); return 1; }

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
            fprintf(stderr, "zen_gl: bytecode dump failed: %s\n",
                    err[0] ? err : "unknown error");
            return 1;
        }
        if (g_verbose)
        {
            printf("zen_gl: dumped bytecode '%s'\n", g_dump_path);
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
        fprintf(stderr, "zen_gl: bytecode load failed: %s\n",
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
    const char *inline_code    = nullptr;
    const char *file_path      = nullptr;
    int         script_arg_start = argc;

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
        long  size = 0;
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

    fprintf(stderr, "Usage: zen_gl <script.zen> [args...]\n"
                    "       zen_gl -e \"code\"\n"
                    "       zen_gl --dump out.znb script.zen\n"
                    "       zen_gl out.znb\n");
    return 1;
}
