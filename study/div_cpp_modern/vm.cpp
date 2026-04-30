#include "vm.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Construtor
// ---------------------------------------------------------------------------
VM::VM(int globals_size, int process_stack_size)
{
    globals.resize(globals_size, Value::make_int(0));
    process_stack_capacity = process_stack_size;
}

// ---------------------------------------------------------------------------
// add_builtin
// ---------------------------------------------------------------------------
int VM::add_builtin(BuiltinFn fn)
{
    builtins.push_back(fn);
    return (int)builtins.size() - 1;
}

// ---------------------------------------------------------------------------
// add_string_literal
// ---------------------------------------------------------------------------
int VM::add_string_literal(const char* text)
{
    int handle = alloc_string(text);
    strings[handle].ref_count = 1; // a tabela de literais segura uma referência.
    string_literals.push_back(handle);
    return (int)string_literals.size() - 1;
}

Value VM::make_string_value(const char* text)
{
    return Value::make_string(alloc_string(text));
}

// ---------------------------------------------------------------------------
// register_process_type
// ---------------------------------------------------------------------------
void VM::register_process_type(
    int entry_point,
    std::vector<Value> local_defaults,
    std::vector<Value> private_defaults)
{
    auto it = process_layouts.find(entry_point);
    if (it != process_layouts.end()) {
        release_values(it->second.local_defaults);
        release_values(it->second.private_defaults);
    }

    retain_values(local_defaults);
    retain_values(private_defaults);
    process_layouts[entry_point] = ProcessLayout{
        std::move(local_defaults),
        std::move(private_defaults),
    };
}

// ---------------------------------------------------------------------------
// current_process
// ---------------------------------------------------------------------------
Process& VM::current_process()
{
    Process* process = find_process(current_process_id);
    if (!process)
        throw std::runtime_error("processo corrente invalido");
    return *process;
}

const Process& VM::current_process() const
{
    for (const auto& process : processes) {
        if (process.id == current_process_id)
            return process;
    }
    throw std::runtime_error("processo corrente invalido");
}

// ---------------------------------------------------------------------------
// Stack helpers públicos
// ---------------------------------------------------------------------------
void VM::push_int(int value)
{
    push_value(Value::make_int(value));
}

void VM::push_float(float value)
{
    push_value(Value::make_float(value));
}

void VM::pop_discard()
{
    if (sp < 0)
        throw std::runtime_error("stack underflow");
    auto& stack = current_process().stack;
    reset_value(stack[sp--]);
}

const Value& VM::top() const
{
    if (sp < 0)
        throw std::runtime_error("stack empty");
    return current_process().stack[sp];
}

void VM::print_value(const Value& value) const
{
    switch (value.kind) {
    case ValueKind::INT:
        std::printf("%d", value.payload);
        return;
    case ValueKind::FLOAT:
        std::printf("%g", value.as_float());
        return;
    case ValueKind::STRING:
        std::printf("%s", string_at(value.payload).bytes.data());
        return;
    case ValueKind::NONE:
        std::printf("<none>");
        return;
    }
}

// ---------------------------------------------------------------------------
// spawn — cria um processo e regista-o na lista
// ---------------------------------------------------------------------------
int VM::spawn(int entry_point, int priority, int father_id, int caller_id)
{
    Process p;
    p.id                  = next_id++;
    p.status              = ProcessStatus::ALIVE;
    p.saved_ip            = entry_point;
    p.saved_sp            = -1;           // stack vazia ao começar
    p.priority            = priority;
    p.executed_this_frame = false;
    p.father_id           = father_id;
    p.caller_id           = caller_id;

    p.stack.assign(process_stack_capacity, Value::none());

    auto layout_it = process_layouts.find(entry_point);
    if (layout_it != process_layouts.end()) {
        const ProcessLayout& layout = layout_it->second;

        p.locals.assign(layout.local_defaults.size(), Value::make_int(0));
        for (size_t i = 0; i < layout.local_defaults.size(); ++i)
            store_value(p.locals[i], layout.local_defaults[i]);

        p.privates.assign(layout.private_defaults.size(), Value::make_int(0));
        for (size_t i = 0; i < layout.private_defaults.size(); ++i)
            store_value(p.privates[i], layout.private_defaults[i]);
    }

    processes.push_back(p);
    return p.id;
}

// ---------------------------------------------------------------------------
// find_process
// ---------------------------------------------------------------------------
Process* VM::find_process(int id)
{
    for (auto& p : processes)
        if (p.id == id) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------------
// String / value helpers
// ---------------------------------------------------------------------------
int VM::alloc_string(const char* text)
{
    int handle;
    if (!free_string_ids.empty()) {
        handle = free_string_ids.back();
        free_string_ids.pop_back();
    } else {
        handle = (int)strings.size();
        strings.push_back(DivString{});
    }

    DivString& slot = strings[handle];
    slot.ref_count = 0;
    size_t len = std::strlen(text);
    slot.bytes.assign(text, text + len);
    slot.bytes.push_back('\0');
    return handle;
}

void VM::retain_value(const Value& value)
{
    if (value.kind != ValueKind::STRING)
        return;
    if (value.payload < 0 || value.payload >= (int)strings.size())
        throw std::runtime_error("string handle invalido");
    ++strings[value.payload].ref_count;
}

void VM::release_value(const Value& value)
{
    if (value.kind != ValueKind::STRING)
        return;
    if (value.payload < 0 || value.payload >= (int)strings.size())
        throw std::runtime_error("string handle invalido");

    DivString& slot = strings[value.payload];
    if (slot.ref_count <= 0)
        throw std::runtime_error("decref invalido");

    --slot.ref_count;
    if (slot.ref_count == 0) {
        slot.bytes.clear();
        free_string_ids.push_back(value.payload);
    }
}

void VM::store_value(Value& slot, const Value& value)
{
    retain_value(value);
    release_value(slot);
    slot = value;
}

void VM::reset_value(Value& slot)
{
    release_value(slot);
    slot = Value::none();
}

void VM::move_value(Value& dst, Value& src)
{
    release_value(dst);
    dst = src;
    src = Value::none();
}

void VM::push_value(const Value& value)
{
    auto& stack = current_process().stack;
    if (sp + 1 >= (int)stack.size())
        throw std::runtime_error("stack overflow");
    ++sp;
    store_value(stack[sp], value);
}

Value VM::pop_value()
{
    if (sp < 0)
        throw std::runtime_error("stack underflow");

    auto& stack = current_process().stack;
    Value value = stack[sp];
    stack[sp] = Value::none();
    --sp;
    return value;
}

void VM::push_string_literal(int literal_index)
{
    if (literal_index < 0 || literal_index >= (int)string_literals.size())
        throw std::runtime_error("literal de string invalido");
    push_value(Value::make_string(string_literals[literal_index]));
}

void VM::retain_values(const std::vector<Value>& values)
{
    for (const Value& value : values)
        retain_value(value);
}

void VM::release_values(std::vector<Value>& values)
{
    for (Value& value : values)
        reset_value(value);
}

int VM::read_int(const Value& value, const char* op_name) const
{
    if (value.kind != ValueKind::INT) {
        std::printf("[VM] %s esperava INT mas recebeu kind=%d\n",
                    op_name, static_cast<int>(value.kind));
        throw std::runtime_error("type mismatch");
    }
    return value.payload;
}

double VM::read_number(const Value& value, const char* op_name) const
{
    switch (value.kind) {
    case ValueKind::INT:
        return static_cast<double>(value.payload);
    case ValueKind::FLOAT:
        return static_cast<double>(value.as_float());
    default:
        std::printf("[VM] %s esperava numero mas recebeu kind=%d\n",
                    op_name, static_cast<int>(value.kind));
        throw std::runtime_error("type mismatch");
    }
}

bool VM::read_truthy(const Value& value, const char* op_name) const
{
    switch (value.kind) {
    case ValueKind::INT:
        return value.payload != 0;
    case ValueKind::FLOAT:
        return value.as_float() != 0.0f;
    case ValueKind::STRING:
        return string_at(value.payload).bytes.size() > 1;
    case ValueKind::NONE:
        std::printf("[VM] %s recebeu NONE como condicao\n", op_name);
        throw std::runtime_error("type mismatch");
    }
    throw std::runtime_error("type mismatch");
}

Value VM::make_numeric_result(double value, bool use_float) const
{
    if (use_float)
        return Value::make_float(static_cast<float>(value));
    return Value::make_int(static_cast<int>(value));
}

int VM::pop_int()
{
    if (sp < 0)
        throw std::runtime_error("stack underflow");

    auto& stack = current_process().stack;
    int value = read_int(stack[sp], "pop_int");
    reset_value(stack[sp--]);
    return value;
}

Value& VM::global_at(int addr)
{
    if (addr < 0 || addr >= (int)globals.size())
        throw std::runtime_error("global access out of range");
    return globals[addr];
}

const Value& VM::global_at(int addr) const
{
    if (addr < 0 || addr >= (int)globals.size())
        throw std::runtime_error("global access out of range");
    return globals[addr];
}

Value& VM::local_at(Process& process, int addr)
{
    if (addr < 0 || addr >= (int)process.locals.size())
        throw std::runtime_error("local access out of range");
    return process.locals[addr];
}

const Value& VM::local_at(const Process& process, int addr) const
{
    if (addr < 0 || addr >= (int)process.locals.size())
        throw std::runtime_error("local access out of range");
    return process.locals[addr];
}

Value& VM::private_at(Process& process, int addr)
{
    if (addr < 0 || addr >= (int)process.privates.size())
        throw std::runtime_error("private access out of range");
    return process.privates[addr];
}

const Value& VM::private_at(const Process& process, int addr) const
{
    if (addr < 0 || addr >= (int)process.privates.size())
        throw std::runtime_error("private access out of range");
    return process.privates[addr];
}

const DivString& VM::string_at(int handle) const
{
    if (handle < 0 || handle >= (int)strings.size())
        throw std::runtime_error("string handle invalido");
    return strings[handle];
}

void VM::cleanup_process(Process& process)
{
    release_values(process.stack);
    release_values(process.locals);
    release_values(process.privates);
    for (auto& frame : process.call_stack)
        release_values(frame.saved_locals);
}

// ---------------------------------------------------------------------------
// run_until_yield
// Interpreta opcodes do processo corrente até:
//   FRAME  → suspende (yield), devolve true
//   RETURN → morre,            devolve false
//   NOP    → fim de código,    devolve false
// ---------------------------------------------------------------------------
bool VM::run_until_yield()
{
    while (true) {
        Op op = static_cast<Op>(code[ip++]);
        Process& me = current_process();

        switch (op) {

        // --- fim de código ---
        case Op::NOP:
            me.status = ProcessStatus::DEAD;
            return false;

        // ---- stack / literais --------------------------------------------

        case Op::PUSH:
            push_int(code[ip++]);
            break;

        case Op::PUSH_STRING:
            push_string_literal(code[ip++]);
            break;

        case Op::PUSH_FLOAT: {
            float value = 0.0f;
            int bits = code[ip++];
            std::memcpy(&value, &bits, sizeof(value));
            push_float(value);
            break;
        }

        // ---- memória -----------------------------------------------------

        case Op::LOAD_GLOBAL:
            push_value(global_at(code[ip++]));
            break;

        case Op::STORE_GLOBAL:
            store_value(global_at(code[ip++]), top());
            pop_discard();
            break;

        case Op::LOAD_LOCAL:
            push_value(local_at(me, code[ip++]));
            break;

        case Op::STORE_LOCAL:
            store_value(local_at(me, code[ip++]), top());
            pop_discard();
            break;

        case Op::LOAD_PRIVATE:
            push_value(private_at(me, code[ip++]));
            break;

        case Op::STORE_PRIVATE:
            store_value(private_at(me, code[ip++]), top());
            pop_discard();
            break;

        // ---- aritmética --------------------------------------------------

        case Op::ADD: {
            auto& stack = me.stack;
            ValueKind rhs_kind = stack[sp].kind;
            double b = read_number(stack[sp], "ADD");
            double a = read_number(stack[sp - 1], "ADD");
            bool use_float = stack[sp - 1].kind == ValueKind::FLOAT || rhs_kind == ValueKind::FLOAT;
            reset_value(stack[sp--]);
            store_value(stack[sp], make_numeric_result(a + b, use_float));
            break;
        }
        case Op::SUB: {
            auto& stack = me.stack;
            ValueKind rhs_kind = stack[sp].kind;
            double b = read_number(stack[sp], "SUB");
            double a = read_number(stack[sp - 1], "SUB");
            bool use_float = stack[sp - 1].kind == ValueKind::FLOAT || rhs_kind == ValueKind::FLOAT;
            reset_value(stack[sp--]);
            store_value(stack[sp], make_numeric_result(a - b, use_float));
            break;
        }
        case Op::MUL: {
            auto& stack = me.stack;
            ValueKind rhs_kind = stack[sp].kind;
            double b = read_number(stack[sp], "MUL");
            double a = read_number(stack[sp - 1], "MUL");
            bool use_float = stack[sp - 1].kind == ValueKind::FLOAT || rhs_kind == ValueKind::FLOAT;
            reset_value(stack[sp--]);
            store_value(stack[sp], make_numeric_result(a * b, use_float));
            break;
        }
        case Op::DIV: {
            auto& stack = me.stack;
            ValueKind rhs_kind = stack[sp].kind;
            double b = read_number(stack[sp], "DIV");
            double a = read_number(stack[sp - 1], "DIV");
            bool use_float = stack[sp - 1].kind == ValueKind::FLOAT || rhs_kind == ValueKind::FLOAT;
            reset_value(stack[sp--]);
            store_value(stack[sp], make_numeric_result(a / b, use_float));
            break;
        }
        case Op::NEG: {
            auto& stack = me.stack;
            bool use_float = stack[sp].kind == ValueKind::FLOAT;
            double a = read_number(stack[sp], "NEG");
            store_value(stack[sp], make_numeric_result(-a, use_float));
            break;
        }

        // ---- comparação — deixam 1 (verdade) ou 0 (falso) na stack ------

        case Op::EQ:  {
            auto& stack = me.stack;
            bool result = false;
            if (stack[sp - 1].kind == ValueKind::STRING && stack[sp].kind == ValueKind::STRING) {
                result = std::strcmp(
                    string_at(stack[sp - 1].payload).bytes.data(),
                    string_at(stack[sp].payload).bytes.data()) == 0;
            } else {
                result = read_number(stack[sp - 1], "EQ") == read_number(stack[sp], "EQ");
            }
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::NEQ: {
            auto& stack = me.stack;
            bool result = false;
            if (stack[sp - 1].kind == ValueKind::STRING && stack[sp].kind == ValueKind::STRING) {
                result = std::strcmp(
                    string_at(stack[sp - 1].payload).bytes.data(),
                    string_at(stack[sp].payload).bytes.data()) != 0;
            } else {
                result = read_number(stack[sp - 1], "NEQ") != read_number(stack[sp], "NEQ");
            }
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::GT:  {
            auto& stack = me.stack;
            bool result = read_number(stack[sp - 1], "GT") > read_number(stack[sp], "GT");
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::LT:  {
            auto& stack = me.stack;
            bool result = read_number(stack[sp - 1], "LT") < read_number(stack[sp], "LT");
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::GTE: {
            auto& stack = me.stack;
            bool result = read_number(stack[sp - 1], "GTE") >= read_number(stack[sp], "GTE");
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::LTE: {
            auto& stack = me.stack;
            bool result = read_number(stack[sp - 1], "LTE") <= read_number(stack[sp], "LTE");
            reset_value(stack[sp--]);
            store_value(stack[sp], Value::make_int(result ? 1 : 0));
            break;
        }

        case Op::JUMP:
            // salta incondicionalmente para o endereço no código
            ip = code[ip];
            break;

        case Op::JUMP_FALSE: {
            // se o topo for 0 (falso), salta; senão avança
            bool condition = read_truthy(me.stack[sp], "JUMP_FALSE");
            reset_value(me.stack[sp--]);
            if (condition)
                ip++;           // verdade: pula o operando de destino
            else
                ip = code[ip]; // falso: salta para o destino
            break;
        }

        // ---- funções built-in --------------------------------------------

        case Op::CALL_BUILTIN: {
            int fn_index = code[ip++];
            builtins[fn_index](*this);
            break;
        }

        case Op::FUNC_CALL: {
            int addr = code[ip++];
            int nargs = code[ip++];

            std::vector<Value> args(nargs, Value::none());
            for (int i = nargs - 1; i >= 0; --i)
                args[i] = pop_value();

            CallFrame frame;
            frame.ret_ip = ip;
            frame.saved_locals = std::move(me.locals);
            me.call_stack.push_back(std::move(frame));

            std::vector<Value> fresh_locals;
            auto layout_it = process_layouts.find(addr);
            if (layout_it != process_layouts.end()) {
                const auto& defaults = layout_it->second.local_defaults;
                fresh_locals.assign(defaults.size(), Value::make_int(0));
                for (size_t i = 0; i < defaults.size(); ++i)
                    store_value(fresh_locals[i], defaults[i]);
            }

            if ((int)fresh_locals.size() < nargs)
                fresh_locals.resize(nargs, Value::make_int(0));

            for (int i = 0; i < nargs; ++i)
                move_value(fresh_locals[i], args[i]);

            me.locals = std::move(fresh_locals);
            ip = addr;
            break;
        }

        case Op::FUNC_RET: {
            if (me.call_stack.empty())
                throw std::runtime_error("FUNC_RET sem frame activo");

            Value retval = pop_value();
            release_values(me.locals);

            CallFrame frame = std::move(me.call_stack.back());
            me.call_stack.pop_back();
            me.locals = std::move(frame.saved_locals);
            ip = frame.ret_ip;

            push_value(retval);
            reset_value(retval);
            break;
        }

        // ---- spawn (= lcal do DIV) ---------------------------------------
        // Cria um novo processo filho.
        // Operandos no código: entry_point, n_params
        // Deixa o id do filho no topo da stack para o pai poder guardar.
        case Op::SPAWN: {
            int entry   = code[ip++];
            int nparams = code[ip++];   // ainda não usado — para futuro
            (void)nparams;

            // Guarda os valores ANTES de chamar spawn() — push_back pode
            // realocar o vector e invalidar qualquer ponteiro guardado.
            int me_id       = current_process_id;
              int me_priority = me.priority;

            int child_id = spawn(entry, me_priority, me_id, me_id);
              push_int(child_id);

            printf("[SPAWN] processo pai=%d criou filho id=%d entry=0x%x\n",
                   me_id, child_id, entry);
            break;
        }

        // ---- return (= lret do DIV) --------------------------------------
        case Op::RETURN: {
            me.status  = ProcessStatus::DEAD;
            printf("[RETURN] processo id=%d morreu\n", me.id);
            return false; // morreu
        }

        // ---- pop ---------------------------------------------------------
        case Op::POP:
            pop_discard();
            break;

        // ---- frame (= lfrm do DIV) — o coração do scheduler -------------
        // Suspende o processo actual.  O scheduler vai retomá-lo na frame seguinte.
        case Op::FRAME: {
            // ip já avançou para a instrução DEPOIS do FRAME,
            // portanto quando retomar, continua exactamente a partir daí.
            me.saved_ip = ip;
            me.saved_sp = sp;
            printf("[FRAME] processo id=%d suspendeu (volta ao ip=0x%x)\n", me.id, ip);
            return true; // yield
        }

        default:
            printf("[VM] opcode desconhecido %d no ip=%d\n",
                   static_cast<int>(op), ip - 1);
            me.status = ProcessStatus::DEAD;
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
// scheduler_tick
// Uma frame: encontra os processos vivos por ordem de prioridade e corre cada
// um uma vez (igual ao exec_process() do DIV original em i.cpp).
// ---------------------------------------------------------------------------
void VM::scheduler_tick()
{
    // reset da flag "já executou nesta frame"
    for (auto& p : processes)
        p.executed_this_frame = false;

    while (true) {
        // --- encontra o processo vivo com maior prioridade ainda por executar ---
        Process* chosen = nullptr;
        for (auto& p : processes) {
            if (p.status != ProcessStatus::ALIVE) continue;
            if (p.executed_this_frame) continue;
            if (!chosen || p.priority > chosen->priority)
                chosen = &p;
        }
        if (!chosen) break; // todos executaram

        // --- restaura estado e executa ---
        current_process_id = chosen->id;
        ip = chosen->saved_ip;
        sp = chosen->saved_sp;

        bool yielded = run_until_yield();

        // se morreu, run_until_yield já marcou DEAD
        // se fez yield, saved_ip/sp já foram salvos dentro de FRAME
        // (mas sp pode ter mudado se havia mais stack depois do FRAME — aqui
        //  está correcto porque FRAME salva antes de retornar)
        (void)yielded;

        // marca como executado para o scheduler não o voltar a escolher
        Process* p = find_process(current_process_id);
        if (p) p->executed_this_frame = true;
    }

    // remove processos mortos da lista e liberta valores retidos.
    std::vector<Process> alive;
    alive.reserve(processes.size());
    for (auto& process : processes) {
        if (process.status == ProcessStatus::DEAD) {
            cleanup_process(process);
            continue;
        }
        alive.push_back(std::move(process));
    }
    processes = std::move(alive);
}

// ---------------------------------------------------------------------------
// run — ponto de entrada público
// ---------------------------------------------------------------------------
void VM::run(int entry_point, int priority)
{
    spawn(entry_point, priority, -1, -1);

    int frame_number = 0;
    while (!processes.empty()) {
        printf("=== frame %d  (%zu processo(s) vivo(s)) ===\n",
               frame_number++, processes.size());
        scheduler_tick();

        if (frame_number > 1000) {
            printf("[VM] limite de frames atingido\n");
            break;
        }
    }
}
