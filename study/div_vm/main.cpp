// main.cpp — escreve código DIV como string, compila e corre
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include "vm.hpp"
#include "compiler.hpp"
#include "process.hpp"

static const char* PROGRAM = R"(

global
    string greeting = "ola mundo";
    float pi = 3.5;
end

process filho(label, ratio)
begin
    write(label);
    write(ratio);
    return;
end

function soma(a, b)
begin
    return a + b + 0.5;
end

process main()
local
    string title = "main";
end
begin
    write(greeting);
    write(pi);
    write(title);
    write(soma(2.25, 3.5));
    filho("filho", 1.5);
    frame;
    return;
end

)";

static int parse_int_arg(const char* text, int fallback)
{
    if (!text || !*text)
        return fallback;

    char* end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (!end || *end != '\0')
        return fallback;
    return static_cast<int>(value);
}

static std::string make_fib_bench_program(int n, int repeats)
{
    return "function fib(n)\n"
           "begin\n"
           "    if (n <= 1)\n"
           "        return n;\n"
           "    end\n"
           "    return fib(n - 1) + fib(n - 2);\n"
           "end\n\n"
           "process main()\n"
           "local\n"
           "    int i = 0;\n"
           "    int acc = 0;\n"
           "    int start_ms = 0;\n"
           "    int elapsed_ms = 0;\n"
           "end\n"
           "begin\n"
           "    start_ms = clock();\n"
           "    while (i < " + std::to_string(repeats) + ")\n"
           "        acc += fib(" + std::to_string(n) + ");\n"
           "        i++;\n"
           "    end\n"
           "    elapsed_ms = clock() - start_ms;\n"
           "    write(acc);\n"
           "    write(elapsed_ms);\n"
           "    return;\n"
           "end\n";
}

// ---------------------------------------------------------------------------
// Builtins
// ---------------------------------------------------------------------------

static void builtin_write(VM& vm) {
    Value v = vm.pop();
    if (vm.verbose) {
        Process& me = vm.current_process();
        printf("  [%s]  ", me.name.c_str());
    }
    vm.print_value(v);
    printf("\n");
}

static int clock_ms_now()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
    return static_cast<int>(elapsed.count());
}

static void builtin_clock(VM& vm)
{
    vm.push(Value::make_int(clock_ms_now()));
}

static std::string read_text_file(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error(std::string("não consegui abrir source file: ") + path);

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[])
{
    bool do_dis         = false;
    bool do_trace       = false;
    bool do_parse_trace = false;
    bool quiet          = false;
    bool fib_bench      = false;
    int fib_n           = 30;
    int fib_repeats     = 1;
    const char* source_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dis"))         do_dis = true;
        else if (!strcmp(argv[i], "--trace"))       do_trace = true;
        else if (!strcmp(argv[i], "--parse-trace")) do_parse_trace = true;
        else if (!strcmp(argv[i], "--quiet"))       quiet = true;
        else if (!strcmp(argv[i], "--fib-bench")) {
            fib_bench = true;
            fib_n = parse_int_arg(i + 1 < argc ? argv[i + 1] : nullptr, 30);
            fib_repeats = parse_int_arg(i + 2 < argc ? argv[i + 2] : nullptr, 1);
            if (i + 1 < argc && argv[i + 1][0] != '-')
                i++;
            if (i + 1 < argc && argv[i + 1][0] != '-')
                i++;
        }
        else if (argv[i][0] == '-') {
            std::fprintf(stderr, "opção desconhecida: %s\n", argv[i]);
            return 1;
        } else if (!source_path) {
            source_path = argv[i];
        } else {
            std::fprintf(stderr, "source file extra ignorado: %s\n", argv[i]);
            return 1;
        }
    }

    VM vm(16, 2048);
    Compiler compiler(vm);
    compiler.trace_parse = do_parse_trace;

    compiler.register_builtin("write", vm.add_builtin(builtin_write));
    compiler.register_builtin("clock", vm.add_builtin(builtin_clock));

    std::string program_source = PROGRAM;
    if (fib_bench) {
        program_source = make_fib_bench_program(fib_n, fib_repeats);
        quiet = true;
    } else if (source_path) {
        program_source = read_text_file(source_path);
    }

    if (!quiet) {
        printf("=== compilar ===\n");
        if (fib_bench)
            printf("mode: fib-bench n=%d repeats=%d\n", fib_n, fib_repeats);
        else if (source_path)
            printf("source: %s\n", source_path);
    }
    try {
        compiler.compile(program_source);
    } catch (const std::exception& e) {
        printf("ERRO: %s\n", e.what());
        return 1;
    }
    if (!quiet)
        printf("código: %zu words\n\n", vm.code.size());

    if (do_dis) {
        vm.disassemble();
        if (!do_trace) return 0;   // só disassembla, não corre
    }

    vm.trace_ops = do_trace;
    vm.verbose   = !quiet;

    int main_entry = compiler.entry_of("main");
    if (main_entry < 0) { printf("'main' não encontrado\n"); return 1; }

    vm.run(main_entry, "main", 100);
    if (!quiet)
        printf("\n=== fim ===\n");
    return 0;
}

