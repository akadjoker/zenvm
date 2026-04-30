// main.cpp — monta bytecode directamente e corre a VM
//
// Simula o que o compilador DIV geraria para:
//
//   process main()
//   begin
//     x = 3 + 4;           // global x = 7
//     write(x);            // imprime 7
//
//     local loop_counter = 0;
//     while (loop_counter < 3)  // imprime 0,1,2
//       write(loop_counter);
//       loop_counter = loop_counter + 1;
//     end
//
//     write(main_label);       // string declarativa em local
//     write(add_bonus(2.25, 3.5)); // float + chamada inline
//
//     filho();             // spawna processo filho
//     frame;               // yield — cede ao scheduler
//     write("main retomou");
//     frame;
//     write(99);
//   end
//
//   process filho()
//   begin
//     private counter = 40;
//     private status_text = "estado privado";
//     write("filho arrancou");
//     write(status_text);
//     write(counter);
//     counter = counter + 1;
//     frame;
//     write(counter);
//   end

#include <cstdio>

#include "builtins.hpp"
#include "vm.hpp"

// Slots nas várias áreas de memória
static const int GLOBAL_X            = 0;
static const int MAIN_LOCAL_LOOP     = 0;
static const int MAIN_LOCAL_LABEL    = 1;
static const int CHILD_PRIVATE_COUNT = 0;
static const int CHILD_PRIVATE_TEXT  = 1;
static const int FUNC_ARG_A          = 0;
static const int FUNC_ARG_B          = 1;
static const int FUNC_LOCAL_BONUS    = 2;

// Helper para acrescentar words ao code[]
static int cursor = 0;
static void E(VM& vm, int v) { vm.code.push_back(v); cursor++; }
static void E(VM& vm, Op op) { E(vm, static_cast<int>(op)); }

static int float_bits(float value)
{
    int bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float size inesperado");
    std::memcpy(&bits, &value, sizeof(value));
    return bits;
}

int main()
{
    VM vm(/*globals_size=*/256, /*process_stack_size=*/256);

    int LIT_MAIN_RESUMED = vm.add_string_literal("main retomou");
    int LIT_CHILD_START  = vm.add_string_literal("filho arrancou");

    Builtins::CoreIds builtins = Builtins::register_core(vm);

    // -----------------------------------------------------------------------
    // PROCESSO MAIN  (começa no address 0)
    // -----------------------------------------------------------------------
    int MAIN_ENTRY = (int)vm.code.size();
    vm.register_process_type(
        MAIN_ENTRY,
        std::vector<Value>{
            Value::make_int(0),
            vm.make_string_value("main local declarativo")
        },
        {}
    );

    // x = 3 + 4
    E(vm, Op::PUSH); E(vm, 3);
    E(vm, Op::PUSH); E(vm, 4);
    E(vm, Op::ADD);
    E(vm, Op::STORE_GLOBAL); E(vm, GLOBAL_X);

    // write(x)
    E(vm, Op::LOAD_GLOBAL); E(vm, GLOBAL_X);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // loop_counter = 0
    E(vm, Op::PUSH); E(vm, 0);
    E(vm, Op::STORE_LOCAL); E(vm, MAIN_LOCAL_LOOP);

    // while (loop_counter < 3)
    int WHILE_START = (int)vm.code.size();

    E(vm, Op::LOAD_LOCAL); E(vm, MAIN_LOCAL_LOOP);
    E(vm, Op::PUSH); E(vm, 3);
    E(vm, Op::LT);

    E(vm, Op::JUMP_FALSE); int hole_while_end = (int)vm.code.size(); E(vm, 0);

    // body: write(loop_counter)
    E(vm, Op::LOAD_LOCAL); E(vm, MAIN_LOCAL_LOOP);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // loop_counter = loop_counter + 1
    E(vm, Op::LOAD_LOCAL); E(vm, MAIN_LOCAL_LOOP);
    E(vm, Op::PUSH); E(vm, 1);
    E(vm, Op::ADD);
    E(vm, Op::STORE_LOCAL); E(vm, MAIN_LOCAL_LOOP);

    E(vm, Op::JUMP); E(vm, WHILE_START);

    int WHILE_END = (int)vm.code.size();
    vm.code[hole_while_end] = WHILE_END;

    // write(main_label)
    E(vm, Op::LOAD_LOCAL); E(vm, MAIN_LOCAL_LABEL);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // write(add_bonus(2.25, 3.5))
    E(vm, Op::PUSH_FLOAT); E(vm, float_bits(2.25f));
    E(vm, Op::PUSH_FLOAT); E(vm, float_bits(3.5f));
    E(vm, Op::FUNC_CALL); int hole_func_entry = (int)vm.code.size(); E(vm, 0); E(vm, 2);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // filho()  — spawna processo filho (entry será definido abaixo)
    E(vm, Op::SPAWN); int hole_filho_entry = (int)vm.code.size(); E(vm, 0); E(vm, 0);
    E(vm, Op::POP);

    // frame  — yield, cede ao scheduler
    E(vm, Op::FRAME);

    // write("main retomou")
    E(vm, Op::PUSH_STRING); E(vm, LIT_MAIN_RESUMED);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // dá mais uma frame ao filho para ele retomar o private já incrementado
    E(vm, Op::FRAME);

    // write(99)  — corre na frame seguinte
    E(vm, Op::PUSH); E(vm, 99);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    E(vm, Op::RETURN);

    // -----------------------------------------------------------------------
    // PROCESSO FILHO
    // -----------------------------------------------------------------------
    int FILHO_ENTRY = (int)vm.code.size();
    vm.code[hole_filho_entry] = FILHO_ENTRY;
    vm.register_process_type(
        FILHO_ENTRY,
        {},
        std::vector<Value>{
            Value::make_int(40),
            vm.make_string_value("estado privado")
        }
    );

    // write("filho arrancou")
    E(vm, Op::PUSH_STRING); E(vm, LIT_CHILD_START);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    E(vm, Op::LOAD_PRIVATE); E(vm, CHILD_PRIVATE_TEXT);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // write(counter private)
    E(vm, Op::LOAD_PRIVATE); E(vm, CHILD_PRIVATE_COUNT);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    // counter = counter + 1
    E(vm, Op::LOAD_PRIVATE); E(vm, CHILD_PRIVATE_COUNT);
    E(vm, Op::PUSH); E(vm, 1);
    E(vm, Op::ADD);
    E(vm, Op::STORE_PRIVATE); E(vm, CHILD_PRIVATE_COUNT);

    // devolve o controlo para o main e prova persistência do private
    E(vm, Op::FRAME);

    E(vm, Op::LOAD_PRIVATE); E(vm, CHILD_PRIVATE_COUNT);
    E(vm, Op::CALL_BUILTIN); E(vm, builtins.write);

    E(vm, Op::RETURN);

    // -----------------------------------------------------------------------
    // FUNCAO INLINE add_bonus(a, b)
    // locals[0]=a, locals[1]=b, locals[2]=0.5f
    // return a + b + 0.5
    // -----------------------------------------------------------------------
    int FUNC_ENTRY = (int)vm.code.size();
    vm.code[hole_func_entry] = FUNC_ENTRY;
    vm.register_process_type(
        FUNC_ENTRY,
        std::vector<Value>{
            Value::make_int(0),
            Value::make_int(0),
            Value::make_float(0.5f)
        },
        {}
    );

    E(vm, Op::LOAD_LOCAL); E(vm, FUNC_ARG_A);
    E(vm, Op::LOAD_LOCAL); E(vm, FUNC_ARG_B);
    E(vm, Op::ADD);
    E(vm, Op::LOAD_LOCAL); E(vm, FUNC_LOCAL_BONUS);
    E(vm, Op::ADD);
    E(vm, Op::FUNC_RET);

    // -----------------------------------------------------------------------
    // Arranca!
    // -----------------------------------------------------------------------
    printf("=== DIV C++ modern VM ===\n");
    printf("MAIN_ENTRY=0x%x  FILHO_ENTRY=0x%x  código=%zu words\n\n",
           MAIN_ENTRY, FILHO_ENTRY, vm.code.size());

    vm.run(MAIN_ENTRY, /*priority=*/100);

    printf("\n=== fim ===\n");
    return 0;
}
