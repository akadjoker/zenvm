#ifndef ZEN_EMITTER_H
#define ZEN_EMITTER_H

#include "memory.h"
#include "opcodes.h"

namespace zen
{

    /*
    ** Emitter — Gera bytecode para um ObjFunc.
    **
    ** Responsável por:
    **   1. Crescer code[] e lines[] em paralelo (realloc seguro)
    **   2. Crescer constants[] (pool de constantes)
    **   3. Backpatching de jumps
    **   4. Tracking de linhas para debug
    **
    ** SEGURANÇA DE MEMÓRIA:
    **   - code[] pode ser realocado durante emit (muda de endereço)
    **   - Nunca guardes ponteiro directo para code[] durante compilação
    **   - Usa sempre índices (int offset), não Instruction*
    **   - Após compilação: ObjFunc.code[] NUNCA mais muda
    **   - O IP da VM só aponta para code[] DEPOIS de compilar → seguro
    **
    ** Uso:
    **   Emitter e(&gc);
    **   e.begin("my_func", 2);          // nome, arity
    **   e.emit(ZEN_ENCODE(...), line);   // emite instrução
    **   int hole = e.emit_jump(OP_JMPIFNOT, reg, line);
    **   // ... código do then ...
    **   e.patch_jump(hole);              // backpatch
    **   ObjFunc* fn = e.end(num_regs);   // finaliza, devolve func
    */
    class Emitter
    {
    public:
        Emitter() : gc_(nullptr) {}
        explicit Emitter(GC *gc);

        /* Inicia nova função */
        void begin(const char *name, int arity, const char *source = nullptr);

        /* --- Emitir instruções --- */
        int emit(Instruction instr, int line);
        int emit_abc(OpCode op, int a, int b, int c, int line);
        int emit_abx(OpCode op, int a, int bx, int line);
        int emit_asbx(OpCode op, int a, int sbx, int line);

        /* --- Constantes --- */
        int add_constant(Value val); /* retorna índice na pool */
        int add_string_constant(const char *str, int len = -1);
        int add_escaped_string_constant(const char *str, int len);
        int add_verbatim_string_constant(const char *str, int len);

        /* --- Jumps (backpatching) --- */
        int emit_jump(OpCode op, int a, int line);      /* retorna offset do hole */
        void patch_jump(int offset);                    /* preenche com distância actual */
        void patch_jump_to(int offset, int target);     /* preenche com destino explícito */
        int emit_loop(int loop_start, int a, int line); /* jump para trás */

        /* --- Fused compare+jump (2-word superinstructions) --- */
        int emit_lt_jmpifnot(int b, int c, int line); /* retorna offset do sBx word */
        int emit_le_jmpifnot(int b, int c, int line);
        void patch_fused_jump(int sbx_offset); /* patch the sBx word */

        /* --- Fused global call (2-word: CALLGLOBAL + global_idx) --- */
        void emit_callglobal(int a, int nargs, int nresults, int global_idx, int line);

        /* --- Finalizar --- */
        ObjFunc *end(int num_regs);

        /* --- Accessors --- */
        int current_offset() const { return func_->code_count; }
        int last_line() const { return last_line_; }

    private:
        void grow_code();
        void grow_constants();

        GC *gc_;
        ObjFunc *func_; /* func em construção */
        int last_line_;
    };

} /* namespace zen */

#endif /* ZEN_EMITTER_H */
