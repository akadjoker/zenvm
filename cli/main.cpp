/*
** zen_main.cpp — CLI entry point for the Zen language.
** Usage:
**   ./zen              → REPL (interactive)
**   ./zen file.zen     → compile and run file
**   ./zen -e "code"    → compile and run inline code
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "debug.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace zen;

/* =========================================================
** Read file into malloc'd buffer
** ========================================================= */

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "zen: cannot open '%s'\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        fprintf(stderr, "zen: out of memory\n");
        return nullptr;
    }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

/* =========================================================
** Run source code (compile + execute)
** ========================================================= */

static bool g_disassemble = false;
static bool g_dis_only = false;
static const char *g_search_paths[16];
static int g_num_search_paths = 0;

static int run_source(const char *source, const char *filename,
                      int script_argc = 0, char **script_argv = nullptr)
{
    VM vm;
    vm.open_lib_globals(&zen_lib_base); /* always available */
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_time);
    vm.register_lib(&zen_lib_fs);
    vm.register_lib(&zen_lib_path);
    vm.register_lib(&zen_lib_file);
#ifdef ZEN_ENABLE_REGEX
    vm.register_lib(&zen_lib_re);
#endif
#ifdef ZEN_ENABLE_ZIP
    vm.register_lib(&zen_lib_zip);
#endif
#ifdef ZEN_ENABLE_NET
    vm.register_lib(&zen_lib_net);
#endif
#ifdef ZEN_ENABLE_HTTP
    vm.register_lib(&zen_lib_http);
#endif
#ifdef ZEN_ENABLE_CRYPTO
    vm.register_lib(&zen_lib_crypto);
#endif
#ifdef ZEN_ENABLE_NN
    vm.register_lib(&zen_lib_nn);
#endif
#ifdef ZEN_ENABLE_JSON
    vm.register_lib(&zen_lib_json);
#endif
#ifdef ZEN_ENABLE_UTF8
    vm.register_lib(&zen_lib_utf8);
#endif
    vm.register_lib(&zen_lib_easing);
    vm.register_lib(&zen_lib_base64);
    for (int i = 0; i < g_num_search_paths; i++)
        vm.add_search_path(g_search_paths[i]);

    /* Create global "args" array from script arguments */
    ObjArray *args_arr = new_array(&vm.get_gc());
    for (int i = 0; i < script_argc; i++)
    {
        ObjString *s = vm.make_string(script_argv[i]);
        array_push(&vm.get_gc(), args_arr, val_obj((Obj *)s));
    }
    vm.def_global("args", val_obj((Obj *)args_arr));

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, filename);

    if (!fn)
    {
        fprintf(stderr, "zen: compilation failed.\n");
        return 1;
    }

    if (g_disassemble)
    {
        disassemble_func(fn, filename);
        /* Also disassemble nested functions found in constants */
        for (int i = 0; i < fn->const_count; i++)
        {
            Value v = fn->constants[i];
            if (v.type == VAL_OBJ && v.as.obj && v.as.obj->type == OBJ_FUNC)
            {
                ObjFunc *nested = (ObjFunc *)v.as.obj;
                const char *n = nested->name ? nested->name->chars : "<anon>";
                disassemble_func(nested, n);
            }
        }
        printf("--- execution ---\n");
    }

    if (g_dis_only) return 0;

    vm.run(fn);
    return vm.had_error() ? 1 : 0;
}

/* =========================================================
** REPL
** ========================================================= */

static void repl()
{
    printf("zen %s  [type 'exit' to quit]\n", "0.1.0");

    VM vm;
    vm.open_lib_globals(&zen_lib_base); /* always available */
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_time);
    vm.register_lib(&zen_lib_fs);
    vm.register_lib(&zen_lib_path);
    vm.register_lib(&zen_lib_file);
#ifdef ZEN_ENABLE_REGEX
    vm.register_lib(&zen_lib_re);
#endif
#ifdef ZEN_ENABLE_ZIP
    vm.register_lib(&zen_lib_zip);
#endif
#ifdef ZEN_ENABLE_NET
    vm.register_lib(&zen_lib_net);
#endif
#ifdef ZEN_ENABLE_HTTP
    vm.register_lib(&zen_lib_http);
#endif
#ifdef ZEN_ENABLE_CRYPTO
    vm.register_lib(&zen_lib_crypto);
#endif
#ifdef ZEN_ENABLE_NN
    vm.register_lib(&zen_lib_nn);
#endif
#ifdef ZEN_ENABLE_JSON
    vm.register_lib(&zen_lib_json);
#endif
#ifdef ZEN_ENABLE_UTF8
    vm.register_lib(&zen_lib_utf8);
#endif
    vm.register_lib(&zen_lib_easing);
    vm.register_lib(&zen_lib_base64);
    Compiler compiler;

    char line[4096];
    for (;;)
    {
        printf(">> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
        {
            printf("\n");
            break;
        }

        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        /* Exit command */
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;

        /* Skip empty lines */
        if (line[0] == '\0')
            continue;

        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, line, "<repl>");
        if (fn)
        {
            vm.run(fn);
        }
    }
}

/* =========================================================
** Main
** ========================================================= */

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        /* No arguments: REPL */
        repl();
        return 0;
    }

    /* Parse flags */
    const char *source_code = nullptr;
    const char *file_path = nullptr;
    int script_arg_start = 0; /* index in argv where script args begin */

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--dis") == 0 || strcmp(argv[i], "-d") == 0)
        {
            g_disassemble = true;
        }
        else if (strcmp(argv[i], "--dis-only") == 0)
        {
            g_disassemble = true;
            g_dis_only = true;
        }
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc)
        {
            source_code = argv[++i];
        }
        else if ((strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--include") == 0) && i + 1 < argc)
        {
            if (g_num_search_paths < 16)
                g_search_paths[g_num_search_paths++] = argv[++i];
            else
                i++; /* skip silently */
        }
        else if (argv[i][0] != '-')
        {
            file_path = argv[i];
            script_arg_start = i + 1; /* everything after is script args */
            break;
        }
        else
        {
            fprintf(stderr, "zen: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (source_code)
    {
        return run_source(source_code, "<cmdline>");
    }

    if (file_path)
    {
        char *source = read_file(file_path);
        if (!source)
            return 1;
        int sa = script_arg_start;
        int result = run_source(source, file_path, argc - sa, argv + sa);
        free(source);
        return result;
    }

    fprintf(stderr, "zen: no input specified\n");
    return 1;
}
