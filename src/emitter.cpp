#include "emitter.h"
#include "memory.h"

namespace zen {

Emitter::Emitter(GC* gc) : gc_(gc), func_(nullptr), last_line_(0) {}

void Emitter::begin(const char* name, int arity, const char* source) {
    func_ = new_func(gc_);
    func_->arity = arity;
    func_->name = name ? intern_string(gc_, name, (int)strlen(name)) : nullptr;
    func_->source = source ? intern_string(gc_, source, (int)strlen(source)) : nullptr;
}

/* --- Grow buffers --- */

void Emitter::grow_code() {
    int old_cap = func_->code_capacity;
    int new_cap = old_cap < 8 ? 8 : old_cap * 2;
    func_->code = (Instruction*)zen_realloc(gc_,
        func_->code, old_cap * sizeof(Instruction), new_cap * sizeof(Instruction));
    func_->lines = (int32_t*)zen_realloc(gc_,
        func_->lines, old_cap * sizeof(int32_t), new_cap * sizeof(int32_t));
    func_->code_capacity = new_cap;
}

void Emitter::grow_constants() {
    int old_cap = func_->const_capacity;
    int new_cap = old_cap < 8 ? 8 : old_cap * 2;
    func_->constants = (Value*)zen_realloc(gc_,
        func_->constants, old_cap * sizeof(Value), new_cap * sizeof(Value));
    func_->const_capacity = new_cap;
}

/* --- Emit --- */

int Emitter::emit(Instruction instr, int line) {
    if (func_->code_count >= func_->code_capacity) {
        grow_code();
    }
    int offset = func_->code_count;
    func_->code[offset] = instr;
    func_->lines[offset] = line;
    func_->code_count++;
    last_line_ = line;
    return offset;
}

int Emitter::emit_abc(OpCode op, int a, int b, int c, int line) {
    return emit(ZEN_ENCODE(op, a, b, c), line);
}

int Emitter::emit_abx(OpCode op, int a, int bx, int line) {
    return emit(ZEN_ENCODE_BX(op, a, bx), line);
}

int Emitter::emit_asbx(OpCode op, int a, int sbx, int line) {
    return emit(ZEN_ENCODE_SBX(op, a, sbx), line);
}

/* --- Constants --- */

int Emitter::add_constant(Value val) {
    /* Deduplica ints/floats simples (strings já são internadas) */
    for (int i = 0; i < func_->const_count; i++) {
        if (values_equal(func_->constants[i], val)) return i;
    }
    if (func_->const_count >= func_->const_capacity) {
        grow_constants();
    }
    int idx = func_->const_count;
    func_->constants[idx] = val;
    func_->const_count++;
    return idx;
}

int Emitter::add_string_constant(const char* str, int len) {
    if (len < 0) len = (int)strlen(str);
    ObjString* s = intern_string(gc_, str, len);
    return add_constant(val_obj((Obj*)s));
}

/* --- Jumps --- */

int Emitter::emit_jump(OpCode op, int a, int line) {
    /* Emite com placeholder sBx=0, retorna offset para patch */
    return emit(ZEN_ENCODE_SBX(op, a, 0), line);
}

void Emitter::patch_jump(int offset) {
    /* Calcula distância: de (offset+1) até current */
    int jump = func_->code_count - (offset + 1);
    uint32_t instr = func_->code[offset];
    uint8_t op = ZEN_OP(instr);
    uint8_t a = ZEN_A(instr);
    func_->code[offset] = ZEN_ENCODE_SBX((OpCode)op, a, jump);
}

void Emitter::patch_jump_to(int offset, int target) {
    int jump = target - (offset + 1);
    uint32_t instr = func_->code[offset];
    uint8_t op = ZEN_OP(instr);
    uint8_t a = ZEN_A(instr);
    func_->code[offset] = ZEN_ENCODE_SBX((OpCode)op, a, jump);
}

int Emitter::emit_loop(int loop_start, int a, int line) {
    /* Jump negativo para loop_start */
    int jump = loop_start - (func_->code_count + 1);
    return emit(ZEN_ENCODE_SBX(OP_JMP, a, jump), line);
}

/* --- Fused compare+jump (2-word superinstructions) --- */

int Emitter::emit_lt_jmpifnot(int b, int c, int line) {
    emit(ZEN_ENCODE(OP_LTJMPIFNOT, 0, b, c), line);
    /* Second word: sBx placeholder (to be patched) */
    return emit(ZEN_ENCODE_SBX(OP_JMP, 0, 0), line); /* offset of the sBx word */
}

int Emitter::emit_le_jmpifnot(int b, int c, int line) {
    emit(ZEN_ENCODE(OP_LEJMPIFNOT, 0, b, c), line);
    return emit(ZEN_ENCODE_SBX(OP_JMP, 0, 0), line);
}

void Emitter::patch_fused_jump(int sbx_offset) {
    /* The sBx word is at sbx_offset. Jump distance = from (sbx_offset+1) to current. */
    int jump = func_->code_count - (sbx_offset + 1);
    func_->code[sbx_offset] = ZEN_ENCODE_SBX(OP_JMP, 0, jump);
}

/* --- Fused global call (2-word) --- */

void Emitter::emit_callglobal(int a, int nargs, int nresults, int global_idx, int line) {
    emit(ZEN_ENCODE(OP_CALLGLOBAL, a, nargs, nresults), line);
    emit(ZEN_ENCODE_BX(OP_HALT, 0, global_idx), line); /* word 2: Bx = global index */
}

/* --- Finalizar --- */

ObjFunc* Emitter::end(int num_regs) {
    /* Garante HALT no fim */
    emit_abc(OP_HALT, 0, 0, 0, last_line_);

    func_->num_regs = num_regs;

    /* Shrink buffers ao tamanho exacto (opcional, poupa memória) */
    if (func_->code_count < func_->code_capacity) {
        func_->code = (Instruction*)zen_realloc(gc_,
            func_->code,
            func_->code_capacity * sizeof(Instruction),
            func_->code_count * sizeof(Instruction));
        func_->lines = (int32_t*)zen_realloc(gc_,
            func_->lines,
            func_->code_capacity * sizeof(int32_t),
            func_->code_count * sizeof(int32_t));
        func_->code_capacity = func_->code_count;
    }
    if (func_->const_count < func_->const_capacity) {
        func_->constants = (Value*)zen_realloc(gc_,
            func_->constants,
            func_->const_capacity * sizeof(Value),
            func_->const_count * sizeof(Value));
        func_->const_capacity = func_->const_count;
    }

    ObjFunc* result = func_;
    func_ = nullptr;
    return result;
}

} /* namespace zen */
