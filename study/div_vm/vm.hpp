#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "opcodes.hpp"
#include "process.hpp"
#include "value.hpp"

class VM {
public:
    enum class GcColor : unsigned char {
        WHITE = 0,
        GRAY = 1,
        BLACK = 2,
    };

    enum class HeapKind {
        STRING = 1,
    };

    struct ObjString {
        HeapKind kind = HeapKind::STRING;
        GcColor color = GcColor::WHITE;
        bool immortal = false;
        bool alive = false;
        char* data = nullptr;
        int length = 0;
        int capacity = 0;
        std::uint32_t hash = 0;
    };

    struct DebugInfo {
        int line = 0;
        int column = 0;
        std::string scope;
        std::string line_text;
    };

    // -----------------------------------------------------------------------
    // Memória
    // -----------------------------------------------------------------------
    std::vector<int> code;          // bytecode (só leitura)
    std::vector<Value> globals;     // variáveis globais partilhadas

    // loc_init — equivale ao loc[] do DIV:
    // valores iniciais das variáveis LOCAL de cada tipo de processo.
    // Ao spawnar, copia-se a fatia relevante para o novo processo.
    // (= memcpy(&mem[id],&mem[iloc],iloc_pub_len<<2) no lcal do DIV)
    std::vector<Value> loc_init;

    // Heap de strings. Literais ficam como objetos imortais; strings dinâmicas
    // poderão entrar aqui mais tarde e ser varridas por mark-and-sweep.
    std::vector<ObjString> string_heap;
    std::unordered_map<std::string, int> interned_strings;
    std::vector<int> free_string_slots;

    // Por entry point: (offset_em_loc_init, tamanho) — = iloc_pub_len por tipo
    std::unordered_map<int, std::pair<int,int>> proc_loc_map;
    // Por entry point de função: nº de slots locais/parâmetros usados por essa função
    std::unordered_map<int, int> function_locals_map;

    // nome de cada entry point (process ou function) — para disassembly
    std::unordered_map<int, std::string> entry_name_map;
    // nome de cada builtin pelo índice — para disassembly
    std::vector<std::string> builtin_names;

    // Mapa de debug: opcode offset -> origem no source compilado.
    std::unordered_map<int, DebugInfo> debug_info;

    // -----------------------------------------------------------------------
    // Estado de execução corrente
    // sp e stack pertencem ao PROCESSO corrente; são copiados para cá
    // no início de run_until_yield e guardados de volta ao fazer FRAME.
    // -----------------------------------------------------------------------
    int current_id = -1;
    int current_index = -1;
    int ip         = 0;
    int sp         = -1;   // sp vivo durante a execução (= pila[] offset do DIV)

    // -----------------------------------------------------------------------
    // Processos — lista de todos os processos vivos/mortos
    // No DIV eram blocos contíguos em mem[]; aqui é um vector de structs
    // -----------------------------------------------------------------------
    std::vector<Process> processes;

    // -----------------------------------------------------------------------
    // Funções built-in (write, signal, get_key, ...)
    // -----------------------------------------------------------------------
    using BuiltinFn = void(*)(VM&);
    std::vector<BuiltinFn> builtins;
    int add_builtin(BuiltinFn fn);

    // -----------------------------------------------------------------------
    // Helpers de stack — operam na stack do processo corrente
    // -----------------------------------------------------------------------
    void push(const Value& v) { processes[current_index].stack[++sp] = v; }
    Value pop()              { return processes[current_index].stack[sp--]; }
    Value& top()             { return processes[current_index].stack[sp]; }
    const Value& top() const { return processes[current_index].stack[sp]; }

    // -----------------------------------------------------------------------
    // API pública
    // -----------------------------------------------------------------------
    VM(int globals_size = 256, int stack_size = 2048);
    ~VM();

    // Flags de debug
    bool trace_ops    = false;   // imprime cada opcode durante a execução
    bool verbose      = true;    // imprime spawns/returns/frames (já existia)
    int  trace_last_pid = -2;    // pid visto no trace anterior (separador por processo)

    // Disassembla o bytecode para stdout
    void disassemble() const;

    // Spawna o processo principal e arranca o loop de frames
    void run(int entry_point, const std::string& name = "main", int priority = 100);

    // Spawna um processo manualmente (usado também pelo opcode SPAWN)
    int  spawn(int entry_point, const std::string& name, int priority, int father_id = -1,
               const std::vector<Value>& args = {});

    // Acesso ao processo corrente
    Process& current_process();
    const Process& current_process() const;
    Process* find(int id);

    int add_string_literal(const std::string& text);
    int add_runtime_string(const std::string& text);
    Value make_string_value(const std::string& text);
    const char* string_at(int id) const;
    int string_length(int id) const;
    int compare_strings(int lhs_id, int rhs_id) const;
    void clear_string_marks();
    void mark_value(const Value& value);
    void mark_string(int id);
    void sweep_strings();
    void print_value(const Value& value) const;
    void set_debug_info(int offset, const DebugInfo& info);
    const DebugInfo* debug_at(int offset) const;
    std::string format_debug_at(int offset) const;
    int read_int(const Value& value, const char* what) const;
    double read_number(const Value& value, const char* what) const;
    bool truthy(const Value& value) const;
    Value numeric_result(double value, bool want_float) const;

private:
    int next_id = 0;

    int spawn_from_values(int entry_point, const std::string& name, int priority, int father_id,
                          const Value* args, int arg_count);

    int add_string_object(const std::string& text, bool immortal);
    int add_string_buffer(const char* data, int length, bool immortal);
    void init_string_object(ObjString& object, const char* data, int length, bool immortal);
    void destroy_string_object(ObjString& object);
    static std::uint32_t hash_bytes(const char* data, int length);

    // Corre o processo corrente até FRAME/FRAME_N ou RETURN
    // Devolve true se fez yield, false se morreu
    bool run_until_yield();

    // Uma frame do scheduler — espelho do exec_process() do DIV (i.cpp)
    // 1. Reset executed_this_frame em todos
    // 2. Loop: escolhe o de maior prioridade ainda por executar
    // 3. Verifica frame_accum — se >=100 desconta e salta (não corre)
    // 4. Restaura ip, corre, guarda ip
    void tick();
};
