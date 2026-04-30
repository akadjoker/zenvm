#include "vm.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>

VM::VM(int globals_size, int /*stack_size*/)
{
    globals.resize(globals_size, Value::make_int(0));
}

VM::~VM()
{
    for (auto& object : string_heap)
        destroy_string_object(object);
}

std::uint32_t VM::hash_bytes(const char* data, int length)
{
    std::uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= 16777619u;
    }
    return hash;
}

void VM::destroy_string_object(ObjString& object)
{
    delete[] object.data;
    object = ObjString{};
}

void VM::init_string_object(ObjString& object, const char* data, int length, bool immortal)
{
    object.kind = HeapKind::STRING;
    object.color = GcColor::WHITE;
    object.immortal = immortal;
    object.alive = true;
    object.length = length;
    object.capacity = length + 1;
    object.hash = hash_bytes(data, length);
    object.data = new char[object.capacity];
    if (length > 0)
        std::memcpy(object.data, data, static_cast<std::size_t>(length));
    object.data[length] = '\0';
}

// ---------------------------------------------------------------------------
int VM::add_builtin(BuiltinFn fn)
{
    builtins.push_back(fn);
    return (int)builtins.size() - 1;
}

int VM::add_string_buffer(const char* data, int length, bool immortal)
{
    std::string key(data, static_cast<std::size_t>(length));
    auto it = interned_strings.find(key);
    if (it != interned_strings.end()) {
        ObjString& object = string_heap.at(it->second);
        if (object.alive) {
            if (immortal)
                object.immortal = true;
            return it->second;
        }
    }

    int id = 0;
    if (!free_string_slots.empty()) {
        id = free_string_slots.back();
        free_string_slots.pop_back();
        destroy_string_object(string_heap[id]);
    } else {
        id = static_cast<int>(string_heap.size());
        string_heap.push_back(ObjString{});
    }

    init_string_object(string_heap[id], data, length, immortal);
    interned_strings[std::move(key)] = id;
    return id;
}

int VM::add_string_object(const std::string& text, bool immortal)
{
    return add_string_buffer(text.data(), static_cast<int>(text.size()), immortal);
}

int VM::add_string_literal(const std::string& text)
{
    return add_string_object(text, true);
}

int VM::add_runtime_string(const std::string& text)
{
    return add_string_object(text, false);
}

Value VM::make_string_value(const std::string& text)
{
    return Value::make_string(add_string_literal(text));
}

const char* VM::string_at(int id) const
{
    const ObjString& object = string_heap.at(id);
    return object.data ? object.data : "";
}

int VM::string_length(int id) const
{
    return string_heap.at(id).length;
}

int VM::compare_strings(int lhs_id, int rhs_id) const
{
    const ObjString& lhs = string_heap.at(lhs_id);
    const ObjString& rhs = string_heap.at(rhs_id);
    const int min_len = lhs.length < rhs.length ? lhs.length : rhs.length;
    int cmp = 0;
    if (min_len > 0)
        cmp = std::memcmp(lhs.data, rhs.data, static_cast<std::size_t>(min_len));
    if (cmp != 0)
        return cmp;
    if (lhs.length == rhs.length)
        return 0;
    return lhs.length < rhs.length ? -1 : 1;
}

void VM::clear_string_marks()
{
    for (auto& object : string_heap) {
        if (object.alive && !object.immortal)
            object.color = GcColor::WHITE;
    }
}

void VM::mark_value(const Value& value)
{
    if (value.kind == ValueKind::STRING)
        mark_string(value.payload);
}

void VM::mark_string(int id)
{
    ObjString& object = string_heap.at(id);
    if (!object.alive || object.immortal || object.color != GcColor::WHITE)
        return;
    object.color = GcColor::BLACK;
}

void VM::sweep_strings()
{
    for (int id = 0; id < static_cast<int>(string_heap.size()); ++id) {
        ObjString& object = string_heap[id];
        if (!object.alive || object.immortal || object.color != GcColor::WHITE)
            continue;
        interned_strings.erase(std::string(string_at(id), static_cast<std::size_t>(object.length)));
        destroy_string_object(object);
        free_string_slots.push_back(id);
    }
}

void VM::set_debug_info(int offset, const DebugInfo& info)
{
    debug_info[offset] = info;
}

const VM::DebugInfo* VM::debug_at(int offset) const
{
    auto it = debug_info.find(offset);
    if (it == debug_info.end())
        return nullptr;
    return &it->second;
}

std::string VM::format_debug_at(int offset) const
{
    const DebugInfo* info = debug_at(offset);
    if (!info)
        return std::string();

    std::string text = info->scope + " @ " + std::to_string(info->line) + ":" + std::to_string(info->column);
    if (!info->line_text.empty())
        text += " -> " + info->line_text;
    return text;
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
        std::printf("%s", string_at(value.payload));
        return;
    case ValueKind::NONE:
        std::printf("<none>");
        return;
    }
}

int VM::read_int(const Value& value, const char* what) const
{
    if (value.kind == ValueKind::INT)
        return value.payload;
    throw std::runtime_error(std::string(what) + " esperava int");
}

double VM::read_number(const Value& value, const char* what) const
{
    switch (value.kind) {
    case ValueKind::INT:
        return static_cast<double>(value.payload);
    case ValueKind::FLOAT:
        return static_cast<double>(value.as_float());
    default:
        throw std::runtime_error(std::string(what) + " esperava numero");
    }
}

bool VM::truthy(const Value& value) const
{
    switch (value.kind) {
    case ValueKind::INT:
        return value.payload != 0;
    case ValueKind::FLOAT:
        return value.as_float() != 0.0f;
    case ValueKind::STRING:
        return string_length(value.payload) > 0;
    case ValueKind::NONE:
        return false;
    }
    return false;
}

Value VM::numeric_result(double value, bool want_float) const
{
    if (want_float)
        return Value::make_float(static_cast<float>(value));
    return Value::make_int(static_cast<int>(value));
}

// ---------------------------------------------------------------------------
// disassemble — imprime o bytecode desassemblado (= listado do DIV)
// ---------------------------------------------------------------------------
void VM::disassemble() const
{
    auto local_name = [](int off) -> const char* {
        switch (off) {
        case 0: return "x";      case 1: return "y";
        case 2: return "angle";  case 3: return "graph";
        case 4: return "size";   case 5: return "flags";
        case 6: return "file";   case 7: return "father";
        case 8: return "son";
        default: return nullptr;
        }
    };

    auto builtin_name = [&](int idx) -> std::string {
        if (idx < (int)builtin_names.size() && !builtin_names[idx].empty())
            return builtin_names[idx];
        return std::to_string(idx);
    };

    auto entry_label = [&](int addr) -> std::string {
        auto it = entry_name_map.find(addr);
        if (it != entry_name_map.end()) return it->second;
        return "";
    };

    std::vector<int> entries;
    for (auto& [addr, _] : entry_name_map) entries.push_back(addr);
    std::sort(entries.begin(), entries.end());
    entries.push_back((int)code.size());

    std::printf("\n");
    int entry_idx = 0;
    int i = 0;

    while (i < (int)code.size()) {
        while (entry_idx < (int)entries.size() - 1 && entries[entry_idx] < i)
            entry_idx++;
        if (entry_idx < (int)entries.size() - 1 && entries[entry_idx] == i) {
            std::string lbl = entry_label(i);
            int bar_len = (int)lbl.size() + 6;
            std::printf("\n== %s @ 0x%04x ", lbl.c_str(), i);
            for (int b = 0; b < 40 - bar_len; b++) std::putchar('=');
            std::printf("\n");
            entry_idx++;
        }

        int op_offset = i;
        Op op = static_cast<Op>(code[i++]);

#define OPNAME(s) std::printf("%04x  %-20s", op_offset, s)

    const DebugInfo* debug = debug_at(op_offset);

        switch (op) {
        case Op::NOP:   OPNAME("NOP"); std::printf("\n"); break;

        case Op::PUSH_CONST:
            OPNAME("PUSH_CONST");
            std::printf("%d\n", code[i++]); break;
        case Op::PUSH_FLOAT: {
            OPNAME("PUSH_FLOAT");
            Value value{ValueKind::FLOAT, code[i++]};
            std::printf("%g\n", value.as_float()); break;
        }
        case Op::PUSH_STRING:
            OPNAME("PUSH_STRING");
            std::printf("\"%s\"\n", string_at(code[i++])); break;

        case Op::LOAD_GLOBAL:
            OPNAME("LOAD_GLOBAL");
            std::printf("g[%d]\n", code[i++]); break;
        case Op::STORE_GLOBAL:
            OPNAME("STORE_GLOBAL");
            std::printf("g[%d]\n", code[i++]); break;

        case Op::LOAD_GLOBAL_IDX:
            OPNAME("LOAD_GLOBAL_IDX");
            std::printf("g[%d + idx]\n", code[i++]); break;
        case Op::STORE_GLOBAL_IDX:
            OPNAME("STORE_GLOBAL_IDX");
            std::printf("g[%d + idx]\n", code[i++]); break;

        case Op::LOAD_LOCAL: {
            OPNAME("LOAD_LOCAL");
            int off = code[i++];
            const char* n = local_name(off);
            if (n) std::printf("l[%d]  (%s)\n", off, n);
            else   std::printf("l[%d]\n", off);
            break;
        }
        case Op::STORE_LOCAL: {
            OPNAME("STORE_LOCAL");
            int off = code[i++];
            const char* n = local_name(off);
            if (n) std::printf("l[%d]  (%s)\n", off, n);
            else   std::printf("l[%d]\n", off);
            break;
        }
        case Op::LOAD_LOCAL_IDX:
            OPNAME("LOAD_LOCAL_IDX");
            std::printf("l[%d + idx]\n", code[i++]); break;
        case Op::STORE_LOCAL_IDX:
            OPNAME("STORE_LOCAL_IDX");
            std::printf("l[%d + idx]\n", code[i++]); break;

        case Op::LOAD_ID_LOCAL: {
            OPNAME("LOAD_ID_LOCAL");
            int off = code[i++];
            const char* n = local_name(off);
            if (n) std::printf("id->l[%d]  (%s)\n", off, n);
            else   std::printf("id->l[%d]\n", off);
            break;
        }
        case Op::STORE_ID_LOCAL: {
            OPNAME("STORE_ID_LOCAL");
            int off = code[i++];
            const char* n = local_name(off);
            if (n) std::printf("id->l[%d]  (%s)\n", off, n);
            else   std::printf("id->l[%d]\n", off);
            break;
        }

        case Op::ADD:  OPNAME("ADD");  std::printf("\n"); break;
        case Op::SUB:  OPNAME("SUB");  std::printf("\n"); break;
        case Op::MUL:  OPNAME("MUL");  std::printf("\n"); break;
        case Op::DIV:  OPNAME("DIV");  std::printf("\n"); break;
        case Op::NEG:  OPNAME("NEG");  std::printf("\n"); break;
        case Op::MOD:  OPNAME("MOD");  std::printf("\n"); break;
        case Op::BAND: OPNAME("BAND"); std::printf("\n"); break;
        case Op::BOR:  OPNAME("BOR");  std::printf("\n"); break;
        case Op::BXOR: OPNAME("BXOR"); std::printf("\n"); break;
        case Op::BNOT: OPNAME("BNOT"); std::printf("\n"); break;
        case Op::EQ:   OPNAME("EQ");   std::printf("\n"); break;
        case Op::NEQ:  OPNAME("NEQ");  std::printf("\n"); break;
        case Op::GT:   OPNAME("GT");   std::printf("\n"); break;
        case Op::LT:   OPNAME("LT");   std::printf("\n"); break;
        case Op::GTE:  OPNAME("GTE");  std::printf("\n"); break;
        case Op::LTE:  OPNAME("LTE");  std::printf("\n"); break;
        case Op::AND:  OPNAME("AND");  std::printf("\n"); break;
        case Op::OR:   OPNAME("OR");   std::printf("\n"); break;
        case Op::NOT:  OPNAME("NOT");  std::printf("\n"); break;

        case Op::JUMP: {
            OPNAME("JUMP");
            int dst = code[i++];
            std::printf("0x%04x\n", dst); break;
        }
        case Op::JUMP_FALSE: {
            OPNAME("JUMP_FALSE");
            int dst = code[i++];
            std::printf("0x%04x\n", dst); break;
        }

        case Op::SPAWN: {
            OPNAME("SPAWN");
            int entry = code[i++], pri = code[i++], nargs = code[i++];
            std::string lbl = entry_label(entry);
            if (!lbl.empty()) std::printf("%-18s  pri=%d nargs=%d\n", lbl.c_str(), pri, nargs);
            else              std::printf("0x%04x  pri=%d nargs=%d\n", entry, pri, nargs);
            break;
        }
        case Op::FUNC_CALL: {
            OPNAME("FUNC_CALL");
            int addr = code[i++], nargs = code[i++];
            std::string lbl = entry_label(addr);
            if (!lbl.empty()) std::printf("%-18s  nargs=%d\n", lbl.c_str(), nargs);
            else              std::printf("0x%04x  nargs=%d\n", addr, nargs);
            break;
        }
        case Op::FUNC_RET: OPNAME("FUNC_RET"); std::printf("\n"); break;
        case Op::RETURN:   OPNAME("RETURN");   std::printf("\n"); break;
        case Op::FRAME:    OPNAME("FRAME");    std::printf("\n"); break;
        case Op::FRAME_N:  OPNAME("FRAME_N");  std::printf("%d%%\n", code[i++]); break;
        case Op::LOAD_ID:  OPNAME("LOAD_ID");  std::printf("\n"); break;
        case Op::SIGNAL:   OPNAME("SIGNAL");   std::printf("%d\n", code[i++]); break;

        case Op::CALL_BUILTIN:
            OPNAME("CALL_BUILTIN");
            std::printf("%s\n", builtin_name(code[i++]).c_str()); break;

        case Op::POP:    OPNAME("POP");    std::printf("\n"); break;
        case Op::DUP:    OPNAME("DUP");    std::printf("\n"); break;
        case Op::CASE_EQ:  OPNAME("CASE_EQ");  std::printf("0x%04x\n", code[i++]); break;
        case Op::CASE_RNG: OPNAME("CASE_RNG"); std::printf("0x%04x\n", code[i++]); break;

        case Op::LPRI: {
            OPNAME("LPRI");
            int jump = code[i++];
            int n = (jump - i) / 2;
            std::printf("jump=0x%04x  privates=[", jump);
            for (int k = 0; k < n; k++) {
                Value value{static_cast<ValueKind>(code[i++]), code[i++]};
                print_value(value);
                if (k < n - 1) std::printf(", ");
            }
            std::printf("]\n");
            break;
        }
        default:
            OPNAME("???");
            std::printf("%d\n", (int)op); break;
        }
        if (debug) {
            std::printf("      ; %s @ %d:%d\n", debug->scope.c_str(), debug->line, debug->column);
        }
#undef OPNAME
    }
    std::printf("\n");
}

// ---------------------------------------------------------------------------
Process& VM::current_process()
{
    if (current_index < 0 || current_index >= static_cast<int>(processes.size()))
        throw std::out_of_range("processo corrente nao encontrado");
    return processes[current_index];
}

const Process& VM::current_process() const
{
    if (current_index < 0 || current_index >= static_cast<int>(processes.size()))
        throw std::out_of_range("processo corrente nao encontrado");
    return processes[current_index];
}

Process* VM::find(int id)
{
    for (auto& p : processes)
        if (p.id == id) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------------
int VM::spawn_from_values(int entry_point, const std::string& name, int priority, int father_id,
                          const Value* args, int arg_count)
{
    Process p;
    p.id          = next_id++;
    p.name        = name;
    p.status      = ProcessStatus::ALIVE;
    p.priority    = priority;
    p.saved_ip    = entry_point;
    p.frame_accum = 0;
    p.father_id   = father_id;
    p.caller_id   = current_id;

    auto it = proc_loc_map.find(entry_point);
    if (it != proc_loc_map.end()) {
        int off  = it->second.first;
        int size = it->second.second;
        for (int i = 0; i < size; i++)
            p.locals[LOC_USER_START + i] = loc_init[off + i];
        for (int i = 0; i < arg_count && i < size; i++)
            p.locals[LOC_USER_START + i] = args[i];
        p.inicio_privadas = LOC_USER_START + size;
    }

    p.locals[LOC_FATHER] = Value::make_int(father_id);
    p.locals[LOC_SON]    = Value::make_int(0);

    int new_id = p.id;

    if (father_id >= 0) {
        Process* father = find(father_id);
        if (father) father->locals[LOC_SON] = Value::make_int(new_id);
    }

    processes.push_back(std::move(p));
    return new_id;
}

int VM::spawn(int entry_point, const std::string& name, int priority, int father_id,
              const std::vector<Value>& args)
{
    return spawn_from_values(entry_point, name, priority, father_id,
                             args.empty() ? nullptr : args.data(), static_cast<int>(args.size()));
}

// ---------------------------------------------------------------------------
// run_until_yield — espelho do nucleo_exec() do DIV (kernel.cpp)
// Corre opcodes do processo corrente até FRAME, FRAME_N, RETURN ou NOP.
// ---------------------------------------------------------------------------
bool VM::run_until_yield()
{
    Process* current = &processes[current_index];
    Value* local_slots = current->call_stack.empty() ? current->locals.data() : current->call_stack.back().slots;

    while (true) {
        Process* me = &processes[current_index];
        auto& stk = me->stack;

        if (trace_ops) {
            if (me->id != trace_last_pid) {
                std::printf("  --- [%s id=%d] ---\n", me->name.c_str(), me->id);
                trace_last_pid = me->id;
            }
            int save_i = ip;
            Op top_op = static_cast<Op>(code[save_i]);
            std::printf("    TRACE [ip=0x%02x sp=%d] ", save_i, sp);
            switch (top_op) {
            case Op::PUSH_CONST:     std::printf("PUSH_CONST %d", code[save_i+1]); break;
            case Op::PUSH_FLOAT: { Value value{ValueKind::FLOAT, code[save_i+1]}; std::printf("PUSH_FLOAT %g", value.as_float()); break; }
            case Op::PUSH_STRING:    std::printf("PUSH_STRING \"%s\"", string_at(code[save_i+1])); break;
            case Op::LOAD_GLOBAL:    std::printf("LOAD_GLOBAL [%d]", code[save_i+1]); break;
            case Op::STORE_GLOBAL:   std::printf("STORE_GLOBAL [%d]", code[save_i+1]); break;
            case Op::LOAD_LOCAL:     std::printf("LOAD_LOCAL [%d]", code[save_i+1]); break;
            case Op::STORE_LOCAL:    std::printf("STORE_LOCAL [%d]", code[save_i+1]); break;
            case Op::JUMP:           std::printf("JUMP 0x%x", code[save_i+1]); break;
            case Op::JUMP_FALSE:     std::printf("JUMP_FALSE 0x%x", code[save_i+1]); break;
            case Op::SPAWN:          std::printf("SPAWN entry=0x%x pri=%d nargs=%d", code[save_i+1], code[save_i+2], code[save_i+3]); break;
            case Op::CALL_BUILTIN:   std::printf("CALL_BUILTIN %d", code[save_i+1]); break;
            case Op::FUNC_CALL:      std::printf("FUNC_CALL 0x%x nargs=%d", code[save_i+1], code[save_i+2]); break;
            case Op::FRAME_N:        std::printf("FRAME_N %d", code[save_i+1]); break;
            case Op::LPRI:           std::printf("LPRI jump=0x%x", code[save_i+1]); break;
            case Op::ADD:  std::printf("ADD");  break;
            case Op::SUB:  std::printf("SUB");  break;
            case Op::MUL:  std::printf("MUL");  break;
            case Op::DIV:  std::printf("DIV");  break;
            case Op::EQ:   std::printf("EQ");   break;
            case Op::NEQ:  std::printf("NEQ");  break;
            case Op::GT:   std::printf("GT");   break;
            case Op::LT:   std::printf("LT");   break;
            case Op::GTE:  std::printf("GTE");  break;
            case Op::LTE:  std::printf("LTE");  break;
            case Op::FRAME:    std::printf("FRAME");    break;
            case Op::RETURN:   std::printf("RETURN");   break;
            case Op::FUNC_RET: std::printf("FUNC_RET"); break;
            case Op::POP:      std::printf("POP");      break;
            case Op::DUP:      std::printf("DUP");      break;
            case Op::NEG:      std::printf("NEG");      break;
            case Op::NOT:      std::printf("NOT");      break;
            case Op::LOAD_ID_LOCAL:  std::printf("LOAD_ID_LOCAL [%d]", code[save_i+1]); break;
            case Op::STORE_ID_LOCAL: std::printf("STORE_ID_LOCAL [%d]", code[save_i+1]); break;
            default: std::printf("op=%d", (int)top_op); break;
            }
            if (sp >= 0) {
                std::printf("  (TOS=");
                print_value(stk[sp]);
                std::printf(")");
            }
            std::string debug_text = format_debug_at(save_i);
            if (!debug_text.empty())
                std::printf("  {%s}", debug_text.c_str());
            std::printf("\n");
        }

        Op op = static_cast<Op>(code[ip++]);

        switch (op) {
        case Op::NOP:
            me->status = ProcessStatus::DEAD;
            return false;

        case Op::LOAD_GLOBAL:
            push(globals[code[ip++]]);
            break;
        case Op::STORE_GLOBAL:
            globals[code[ip++]] = pop();
            break;

        case Op::LOAD_LOCAL:
            push(local_slots[code[ip++]]);
            break;
        case Op::STORE_LOCAL:
            local_slots[code[ip++]] = pop();
            break;

        case Op::PUSH_CONST:
            push(Value::make_int(code[ip++]));
            break;
        case Op::PUSH_FLOAT:
            push(Value{ValueKind::FLOAT, code[ip++]});
            break;
        case Op::PUSH_STRING:
            push(Value::make_string(code[ip++]));
            break;

        case Op::ADD: {
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT) {
                stk[sp-1] = Value::make_int(stk[sp-1].payload + stk[sp].payload);
                sp--;
                break;
            }
            bool want_float = stk[sp-1].kind == ValueKind::FLOAT || stk[sp].kind == ValueKind::FLOAT;
            double lhs = read_number(stk[sp-1], "ADD");
            double rhs = read_number(stk[sp], "ADD");
            stk[sp-1] = numeric_result(lhs + rhs, want_float);
            sp--;
            break;
        }
        case Op::SUB: {
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT) {
                stk[sp-1] = Value::make_int(stk[sp-1].payload - stk[sp].payload);
                sp--;
                break;
            }
            bool want_float = stk[sp-1].kind == ValueKind::FLOAT || stk[sp].kind == ValueKind::FLOAT;
            double lhs = read_number(stk[sp-1], "SUB");
            double rhs = read_number(stk[sp], "SUB");
            stk[sp-1] = numeric_result(lhs - rhs, want_float);
            sp--;
            break;
        }
        case Op::MUL: {
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT) {
                stk[sp-1] = Value::make_int(stk[sp-1].payload * stk[sp].payload);
                sp--;
                break;
            }
            bool want_float = stk[sp-1].kind == ValueKind::FLOAT || stk[sp].kind == ValueKind::FLOAT;
            double lhs = read_number(stk[sp-1], "MUL");
            double rhs = read_number(stk[sp], "MUL");
            stk[sp-1] = numeric_result(lhs * rhs, want_float);
            sp--;
            break;
        }
        case Op::DIV: {
            double lhs = read_number(stk[sp-1], "DIV");
            double rhs = read_number(stk[sp], "DIV");
            stk[sp-1] = numeric_result(lhs / rhs, true);
            sp--;
            break;
        }
        case Op::NEG: {
            if (stk[sp].kind == ValueKind::INT) {
                stk[sp] = Value::make_int(-stk[sp].payload);
                break;
            }
            bool want_float = stk[sp].kind == ValueKind::FLOAT;
            double value = read_number(stk[sp], "NEG");
            stk[sp] = numeric_result(-value, want_float);
            break;
        }
        case Op::MOD: {
            int lhs = read_int(stk[sp-1], "MOD");
            int rhs = read_int(stk[sp], "MOD");
            stk[sp-1] = Value::make_int(lhs % rhs);
            sp--;
            break;
        }

        case Op::BAND: {
            int lhs = read_int(stk[sp-1], "BAND");
            int rhs = read_int(stk[sp], "BAND");
            stk[sp-1] = Value::make_int(lhs & rhs);
            sp--;
            break;
        }
        case Op::BOR: {
            int lhs = read_int(stk[sp-1], "BOR");
            int rhs = read_int(stk[sp], "BOR");
            stk[sp-1] = Value::make_int(lhs | rhs);
            sp--;
            break;
        }
        case Op::BXOR: {
            int lhs = read_int(stk[sp-1], "BXOR");
            int rhs = read_int(stk[sp], "BXOR");
            stk[sp-1] = Value::make_int(lhs ^ rhs);
            sp--;
            break;
        }
        case Op::BNOT: {
            int value = read_int(stk[sp], "BNOT");
            stk[sp] = Value::make_int(~value);
            break;
        }

        case Op::AND:
            stk[sp-1] = Value::make_int((truthy(stk[sp-1]) && truthy(stk[sp])) ? 1 : 0);
            sp--; break;
        case Op::OR:
            stk[sp-1] = Value::make_int((truthy(stk[sp-1]) || truthy(stk[sp])) ? 1 : 0);
            sp--; break;
        case Op::NOT:
            stk[sp] = Value::make_int(truthy(stk[sp]) ? 0 : 1);
            break;

        case Op::EQ: {
            bool result;
            if (stk[sp-1].kind == ValueKind::STRING && stk[sp].kind == ValueKind::STRING)
                result = compare_strings(stk[sp-1].payload, stk[sp].payload) == 0;
            else if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                result = stk[sp-1].payload == stk[sp].payload;
            else
                result = read_number(stk[sp-1], "EQ") == read_number(stk[sp], "EQ");
            stk[sp-1] = Value::make_int(result ? 1 : 0);
            sp--;
            break;
        }
        case Op::NEQ: {
            bool result;
            if (stk[sp-1].kind == ValueKind::STRING && stk[sp].kind == ValueKind::STRING)
                result = compare_strings(stk[sp-1].payload, stk[sp].payload) != 0;
            else if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                result = stk[sp-1].payload != stk[sp].payload;
            else
                result = read_number(stk[sp-1], "NEQ") != read_number(stk[sp], "NEQ");
            stk[sp-1] = Value::make_int(result ? 1 : 0);
            sp--;
            break;
        }
        case Op::GT:
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                stk[sp-1] = Value::make_int(stk[sp-1].payload > stk[sp].payload ? 1 : 0);
            else
                stk[sp-1] = Value::make_int(read_number(stk[sp-1], "GT") > read_number(stk[sp], "GT") ? 1 : 0);
            sp--; break;
        case Op::LT:
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                stk[sp-1] = Value::make_int(stk[sp-1].payload < stk[sp].payload ? 1 : 0);
            else
                stk[sp-1] = Value::make_int(read_number(stk[sp-1], "LT") < read_number(stk[sp], "LT") ? 1 : 0);
            sp--; break;
        case Op::GTE:
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                stk[sp-1] = Value::make_int(stk[sp-1].payload >= stk[sp].payload ? 1 : 0);
            else
                stk[sp-1] = Value::make_int(read_number(stk[sp-1], "GTE") >= read_number(stk[sp], "GTE") ? 1 : 0);
            sp--; break;
        case Op::LTE:
            if (stk[sp-1].kind == ValueKind::INT && stk[sp].kind == ValueKind::INT)
                stk[sp-1] = Value::make_int(stk[sp-1].payload <= stk[sp].payload ? 1 : 0);
            else
                stk[sp-1] = Value::make_int(read_number(stk[sp-1], "LTE") <= read_number(stk[sp], "LTE") ? 1 : 0);
            sp--; break;

        case Op::JUMP:
            ip = code[ip];
            break;

        case Op::JUMP_FALSE:
            if (truthy(stk[sp--])) ip++;
            else                   ip = code[ip];
            break;

        case Op::LOAD_ID:
            push(Value::make_int(current_id));
            break;

        case Op::CALL_BUILTIN:
            builtins[code[ip++]](*this);
            break;

        case Op::POP:
            sp--;
            break;

        case Op::DUP:
            push(stk[sp]);
            break;

        case Op::LOAD_GLOBAL_IDX: {
            int base = code[ip++];
            int idx  = read_int(stk[sp--], "LOAD_GLOBAL_IDX");
            push(globals[base + idx]);
            break;
        }
        case Op::STORE_GLOBAL_IDX: {
            int base = code[ip++];
            Value val = stk[sp--];
            int idx   = read_int(stk[sp--], "STORE_GLOBAL_IDX");
            globals[base + idx] = val;
            break;
        }
        case Op::LOAD_LOCAL_IDX: {
            int base = code[ip++];
            int idx  = read_int(stk[sp--], "LOAD_LOCAL_IDX");
            push(local_slots[base + idx]);
            break;
        }
        case Op::STORE_LOCAL_IDX: {
            int base = code[ip++];
            Value val = stk[sp--];
            int idx   = read_int(stk[sp--], "STORE_LOCAL_IDX");
            local_slots[base + idx] = val;
            break;
        }

        case Op::CASE_EQ: {
            int addr = code[ip++];
            Value case_val = stk[sp--];
            bool equal;
            if (stk[sp].kind == ValueKind::STRING && case_val.kind == ValueKind::STRING)
                equal = compare_strings(stk[sp].payload, case_val.payload) == 0;
            else if (stk[sp].kind == ValueKind::INT && case_val.kind == ValueKind::INT)
                equal = stk[sp].payload == case_val.payload;
            else
                equal = read_number(stk[sp], "CASE_EQ") == read_number(case_val, "CASE_EQ");
            if (!equal) ip = addr;
            break;
        }
        case Op::CASE_RNG: {
            int addr = code[ip++];
            int hi = read_int(stk[sp--], "CASE_RNG");
            int lo = read_int(stk[sp--], "CASE_RNG");
            int val = read_int(stk[sp], "CASE_RNG");
            if (val < lo || val > hi) ip = addr;
            break;
        }

        case Op::FUNC_CALL: {
            int addr  = code[ip++];
            int nargs = code[ip++];
            int callee_local_count = LOCALS_SIZE;
            auto fit = function_locals_map.find(addr);
            if (fit != function_locals_map.end())
                callee_local_count = fit->second;
            if (callee_local_count < nargs)
                callee_local_count = nargs;
            if (static_cast<int>(me->call_stack.size()) >= MAX_CALL_DEPTH) [[unlikely]]
                throw std::runtime_error("call stack overflow");

            const int slot_index = sp - nargs + 1;
            if (slot_index < 0) [[unlikely]]
                throw std::runtime_error("stack underflow em FUNC_CALL");
            if (slot_index + callee_local_count > PROC_STACK_SIZE) [[unlikely]]
                throw std::runtime_error("value stack overflow");

            CallFrame cf;
            cf.ret_ip = ip;
            cf.slots = &me->stack[slot_index];
            me->call_stack.push_back(cf);
            for (int i = nargs; i < callee_local_count; ++i)
                me->stack[slot_index + i] = Value::none();
            sp = slot_index + callee_local_count - 1;
            local_slots = cf.slots;
            ip = addr;
            break;
        }

        case Op::FUNC_RET: {
            Value retval = stk[sp--];
            if (me->call_stack.empty())
                throw std::runtime_error("call stack underflow em FUNC_RET");
            CallFrame cf = me->call_stack.back();
            me->call_stack.pop_back();
            sp = static_cast<int>(cf.slots - me->stack.data()) - 1;
            local_slots = me->call_stack.empty() ? me->locals.data() : me->call_stack.back().slots;
            ip = cf.ret_ip;
            stk[++sp] = retval;
            break;
        }

        case Op::LPRI: {
            int jump = code[ip];
            int n = (jump - ip - 1) / 2;
            for (int i = 0; i < n; i++) {
                int kind = code[ip + 1 + i * 2];
                int payload = code[ip + 2 + i * 2];
                me->locals[me->inicio_privadas + i] = Value{static_cast<ValueKind>(kind), payload};
            }
            me->inicio_privadas += n;
            ip = jump;
            break;
        }

        case Op::LOAD_ID_LOCAL: {
            int idx = code[ip++];
            int pid = read_int(pop(), "LOAD_ID_LOCAL");
            Process* target = find(pid);
            if (!target) { std::printf("LOAD_ID_LOCAL: id=%d inválido\n", pid); push(Value::make_int(0)); break; }
            push(target->locals[idx]);
            break;
        }
        case Op::STORE_ID_LOCAL: {
            int idx = code[ip++];
            Value value = pop();
            int pid = read_int(pop(), "STORE_ID_LOCAL");
            Process* target = find(pid);
            if (!target) { std::printf("STORE_ID_LOCAL: id=%d inválido\n", pid); break; }
            target->locals[idx] = value;
            break;
        }

        case Op::SPAWN: {
            int entry    = code[ip++];
            int pri      = code[ip++];
            int nargs    = code[ip++];
            int me_id    = current_id;
            std::string me_name = me->name;
            const int arg_base = sp - nargs + 1;
            if (arg_base < 0)
                throw std::runtime_error("stack underflow em SPAWN");
            int child_id = spawn_from_values(entry, "proc_" + std::to_string(next_id-1), pri, me_id,
                                             nargs > 0 ? &stk[arg_base] : nullptr, nargs);
            sp -= nargs;
            push(Value::make_int(child_id));
            local_slots = me->call_stack.empty() ? me->locals.data() : me->call_stack.back().slots;
            if (verbose)
                std::printf("  [SPAWN] '%s' (id=%d) criou filho id=%d entry=0x%x pri=%d nargs=%d\n",
                       me_name.c_str(), me_id, child_id, entry, pri, nargs);
            break;
        }

        case Op::RETURN:
            me->status = ProcessStatus::DEAD;
            if (verbose)
                std::printf("  [RETURN] '%s' (id=%d) morreu\n", me->name.c_str(), me->id);
            return false;

        case Op::FRAME:
            me->saved_ip = ip;
            me->saved_sp = sp;
            return true;

        case Op::FRAME_N: {
            int n = code[ip++];
            me->frame_accum += n;
            if (me->frame_accum >= 100) {
                me->frame_accum -= 100;
                me->saved_ip = ip;
                me->saved_sp = sp;
                return true;
            }
            break;
        }

        default:
            std::printf("  [VM] opcode desconhecido %d no ip=%d", static_cast<int>(op), ip-1);
            {
                std::string debug_text = format_debug_at(ip - 1);
                if (!debug_text.empty())
                    std::printf(" [%s]", debug_text.c_str());
            }
            std::printf("\n");
            me->status = ProcessStatus::DEAD;
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
void VM::tick()
{
    for (auto& p : processes)
        p.executed_this_frame = false;

    if (processes.size() == 1) {
        Process& only = processes[0];
        if (only.status == ProcessStatus::ALIVE && !only.executed_this_frame) {
            if (only.frame_accum >= 100) {
                only.frame_accum -= 100;
                only.executed_this_frame = true;
            } else {
                current_index = 0;
                current_id = only.id;
                ip = only.saved_ip;
                sp = only.saved_sp;
                run_until_yield();
                processes[0].executed_this_frame = true;
            }
        }

        if (processes[0].status == ProcessStatus::DEAD)
            processes.clear();
        return;
    }

    while (true) {
        int chosen_index = -1;
        for (int index = 0; index < static_cast<int>(processes.size()); ++index) {
            auto& p = processes[index];
            if (p.status != ProcessStatus::ALIVE) continue;
            if (p.executed_this_frame)            continue;
            if (chosen_index < 0 || p.priority > processes[chosen_index].priority)
                chosen_index = index;
        }
        if (chosen_index < 0) break;

        Process& chosen = processes[chosen_index];

        if (chosen.frame_accum >= 100) {
            chosen.frame_accum -= 100;
            chosen.executed_this_frame = true;
            continue;
        }

        current_index = chosen_index;
        current_id    = chosen.id;
        ip            = chosen.saved_ip;
        sp            = chosen.saved_sp;

        run_until_yield();

        if (current_index >= 0 && current_index < static_cast<int>(processes.size()))
            processes[current_index].executed_this_frame = true;
    }

    processes.erase(
        std::remove_if(processes.begin(), processes.end(),
            [](const Process& p){ return p.status == ProcessStatus::DEAD; }),
        processes.end());

    current_index = -1;
}

// ---------------------------------------------------------------------------
void VM::run(int entry_point, const std::string& name, int priority)
{
    spawn(entry_point, name, priority);

    int frame_n = 0;
    while (!processes.empty()) {
        if (verbose) {
            std::printf("\n=== FRAME %d  (%zu processo(s) vivo(s)) ===\n",
                   frame_n, processes.size());

            for (auto& p : processes)
                std::printf("  proc '%s' id=%d pri=%d frame_accum=%d ip=0x%x\n",
                       p.name.c_str(), p.id, p.priority, p.frame_accum, p.saved_ip);
            std::printf("---\n");
        }

        frame_n++;

        tick();

        if (frame_n > 200) { std::printf("[VM] limite de frames\n"); break; }
    }
}