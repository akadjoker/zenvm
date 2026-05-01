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

    /* Optional: disassemble in debug mode */
#ifdef ZEN_DEBUG_TRACE_EXEC
    disassemble(fn, filename);
    printf("--- execution ---\n");
#endif

    vm.run(fn);
    return 0;
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

    if (argc == 3 && strcmp(argv[1], "-e") == 0)
    {
        /* Inline code */
        return run_source(argv[2], "<cmdline>");
    }

    /* File argument */
    const char *path = argv[1];
    char *source = read_file(path);
    if (!source)
        return 1;

    int result = run_source(source, path);
    free(source);
    return result;
}
