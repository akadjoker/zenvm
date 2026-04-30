#pragma once
#include <unordered_map>
#include <vector>

#include "opcodes.hpp"
#include "process.hpp"
#include "value.hpp"

struct ProcessLayout {
    std::vector<Value> local_defaults;
    std::vector<Value> private_defaults;
};

class VM {
public:
    // -----------------------------------------------------------------------
    // Memória — 3 zonas separadas (no DIV original era tudo num mem[] único)
    // -----------------------------------------------------------------------

    // CÓDIGO: bytecode compilado (só leitura durante execução)
    std::vector<int> code;

    // Tabela de literais de string. Cada entrada aponta para um handle no pool.
    std::vector<int> string_literals;

    // GLOBAIS: variáveis partilhadas entre todos os processos.
    std::vector<Value> globals;

    // Pool de strings custom. Values do tipo STRING guardam o handle deste pool.
    std::vector<DivString> strings;

    // -----------------------------------------------------------------------
    // Lista de processos
    // -----------------------------------------------------------------------
    std::vector<Process> processes;

    // -----------------------------------------------------------------------
    // Estado do processo actualmente em execução
    // -----------------------------------------------------------------------
    int current_process_id = -1;  // id do processo a correr agora
    int ip = 0;                   // instruction pointer (posição no code[])
    int sp = -1;                  // stack pointer do processo corrente

    // -----------------------------------------------------------------------
    // Funções built-in (ex: write, signal, key, ...)
    // Registadas com add_builtin(); chamadas pelo opcode CALL_BUILTIN.
    // -----------------------------------------------------------------------
    using BuiltinFn = void (*)(VM&);
    std::vector<BuiltinFn> builtins;

    // -----------------------------------------------------------------------
    // API pública
    // -----------------------------------------------------------------------
    VM(int globals_size = 1024, int process_stack_size = DEFAULT_PROCESS_STACK_SIZE);

    // Adiciona uma função built-in; devolve o seu índice (passa ao CALL_BUILTIN)
    int add_builtin(BuiltinFn fn);

    // Regista um literal de string e devolve o seu índice para PUSH_STRING.
    int add_string_literal(const char* text);

    // Cria um Value STRING ownership-safe para defaults declarativos.
    Value make_string_value(const char* text);

    // Define o layout de memória de um tipo de processo.
    void register_process_type(
        int entry_point,
        std::vector<Value> local_defaults = {},
        std::vector<Value> private_defaults = {}
    );

    // Cria o processo principal e arranca o loop
    void run(int entry_point, int priority = 100);

    Process& current_process();
    const Process& current_process() const;

    // Helpers de stack (úteis também em main.cpp para montar o bytecode)
    void push_int(int value);
    void push_float(float value);
    void pop_discard();
    const Value& top() const;
    void print_value(const Value& value) const;

private:
    int next_id = 0;
    int process_stack_capacity = DEFAULT_PROCESS_STACK_SIZE;
    std::vector<int> free_string_ids;
    std::unordered_map<int, ProcessLayout> process_layouts;

    // Cria um novo processo (filho do corrente, ou raiz se father=-1)
    int spawn(int entry_point, int priority, int father_id, int caller_id);

    // Procura o processo pelo id
    Process* find_process(int id);

    // Executa o processo corrente até FRAME (yield) ou RETURN (morte).
    // Devolve true se fez yield, false se morreu.
    bool run_until_yield();

    // Uma frame do scheduler: corre todos os processos vivos uma vez.
    void scheduler_tick();

    int alloc_string(const char* text);
    void retain_value(const Value& value);
    void release_value(const Value& value);
    void store_value(Value& slot, const Value& value);
    void reset_value(Value& slot);
    void move_value(Value& dst, Value& src);
    void push_value(const Value& value);
    Value pop_value();
    void push_string_literal(int literal_index);
    void retain_values(const std::vector<Value>& values);
    void release_values(std::vector<Value>& values);
    int pop_int();
    int read_int(const Value& value, const char* op_name) const;
    double read_number(const Value& value, const char* op_name) const;
    bool read_truthy(const Value& value, const char* op_name) const;
    Value make_numeric_result(double value, bool use_float) const;
    Value& global_at(int addr);
    const Value& global_at(int addr) const;
    Value& local_at(Process& process, int addr);
    const Value& local_at(const Process& process, int addr) const;
    Value& private_at(Process& process, int addr);
    const Value& private_at(const Process& process, int addr) const;
    const DivString& string_at(int handle) const;
    void cleanup_process(Process& process);
};
