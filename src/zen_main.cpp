/*
** zen_main.cpp — CLI entry point for the Zen language.
** Usage:
**   ./zen              → REPL (interactive)
**   ./zen file.zen     → compile and run file
**   ./zen -e "code"    → compile and run inline code
*/

#include "vm.h"
#include "compiler.h"
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

static int run_source(const char *source, const char *filename)
{
    VM vm;

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
        else if (argv[i][0] != '-')
        {
            file_path = argv[i];
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
        int result = run_source(source, file_path);
        free(source);
        return result;
    }

    fprintf(stderr, "zen: no input specified\n");
    return 1;
}
