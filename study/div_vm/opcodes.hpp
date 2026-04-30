#pragma once

// ============================================================================
// Opcodes da VM — fiéis ao DIV (divc.cpp / kernel.cpp)
//
// Equivalências com DIV:
//   PUSH_CONST     = lcar (load constant)
//   LOAD_GLOBAL    = lcar addr (load from global address)
//   STORE_GLOBAL   = lasi addr (store to global address)
//   LOAD_LOCAL     = lcar -offset; laid (local via id do processo)
//   LOAD_GLOBAL_IDX= lptr (dereferência com índice em runtime)
//   ADD/SUB/MUL/DIV= lsum/lres/lmul/ldiv
//   MOD            = lmod
//   EQ/NEQ/...     = ligu/ldis/lmay/lmen/lmai/lmei
//   AND/OR/NOT     = land/lor/lnot (lógico em condições)
//   BAND/BOR/BXOR  = land/lor/lxor (bitwise)
//   JUMP           = ljmp
//   JUMP_FALSE     = ljpf
//   SPAWN          = lcal
//   RETURN         = lret
//   FRAME          = lfrm
//   FRAME_N        = lfrf
//   CASE_EQ        = lcse
//   CASE_RNG       = lcsr
//   DUP            = ldup
// ============================================================================
enum class Op : int {
    NOP          = 0,

    // --- literais e variáveis escalares ---
    PUSH_CONST       = 1,   // operando: valor literal             → stack
    PUSH_FLOAT       = 2,   // operando: bits do float             → stack
    PUSH_STRING      = 3,   // operando: id da string              → stack
    LOAD_GLOBAL      = 4,   // operando: endereço absoluto         → globals[addr]
    STORE_GLOBAL     = 5,   // operando: endereço; pop             → globals[addr]
    LOAD_LOCAL       = 6,   // operando: offset no processo        → locals[off]
    STORE_LOCAL      = 7,   // operando: offset; pop               → locals[off]

    // --- variáveis com índice (arrays 1D) ---
    // Equivale ao acesso base+índice do DIV: mem[base + pop_idx]
    LOAD_GLOBAL_IDX  = 8,   // op: base; pop idx → globals[base+idx]
    STORE_GLOBAL_IDX = 9,   // op: base; pop val(TOS), pop idx(TOS-1) → globals[base+idx]=val
    LOAD_LOCAL_IDX   = 10,  // op: base; pop idx → locals[base+idx]
    STORE_LOCAL_IDX  = 11,  // op: base; pop val(TOS), pop idx(TOS-1) → locals[base+idx]=val

    // --- aritmética (= lsum, lres, lmul, ldiv, lmod, lneg do DIV) ---
    ADD  = 12,
    SUB  = 13,
    MUL  = 14,
    DIV  = 15,
    NEG  = 16,   // negação unária (-top)
    MOD  = 17,   // módulo (= lmod do DIV)

    // --- bitwise (= land, lor, lxor, lnot do DIV) ---
    BAND = 18,   // bitwise AND  (&)
    BOR  = 19,   // bitwise OR   (|)
    BXOR = 20,   // bitwise XOR  (^)
    BNOT = 21,   // bitwise NOT  (~)

    // --- comparação (resultado: 1 = verdade, 0 = falso) ---
    // = ligu, ldis, lmay, lmen, lmei, lmai do DIV
    EQ   = 22,
    NEQ  = 23,
    GT   = 24,
    LT   = 25,
    GTE  = 26,
    LTE  = 27,

    // --- lógico (resultado 0 ou 1; em DIV usa-se & | ! em condições) ---
    AND  = 28,   // lógico AND  (&&)
    OR   = 29,   // lógico OR   (||)
    NOT  = 30,   // lógico NOT  (!)

    // --- saltos (= ljmp, ljpf do DIV) ---
    JUMP       = 32,   // operando: endereço (incondicional)
    JUMP_FALSE = 33,   // operando: endereço (se TOS == 0)

    // --- processos (= lcal, lret, lfrm, lfrf, lcid do DIV) ---
    SPAWN      = 40,   // operandos: entry, priority → filho na stack
    RETURN     = 41,   // mata processo
    FRAME      = 42,   // yield normal (= lfrm)
    FRAME_N    = 43,   // operando: n; acumula, yield real quando >=100 (= lfrf)
    LOAD_ID    = 44,   // push id do processo corrente (= lcid)
    SIGNAL     = 45,   // operando: tipo; pop=id → sinal

    // --- misc ---
    CALL_BUILTIN = 50, // operando: índice built-in
    POP          = 51, // descarta TOS
    DUP          = 52, // duplica TOS (stack[sp+1]=stack[sp]; sp++)

    // --- switch/case (= lcse, lcsr do DIV) ---
    // CASE_EQ  addr: pop case_val; se TOS!=case_val → jump addr (TOS=switch_val permanece)
    // CASE_RNG addr: pop hi; pop lo; se TOS fora [lo,hi] → jump addr
    CASE_EQ  = 60,
    CASE_RNG = 61,

    // --- funções inline (não são processos, não fazem yield) ---
    // FUNC_CALL addr nargs: guarda ip+locals, carrega args, salta para addr
    // FUNC_RET:             restaura ip+locals, deixa retval na stack
    FUNC_CALL = 62,
    FUNC_RET  = 63,

    // --- inicialização de variáveis private (= lpri do DIV) ---
    // LPRI jump_addr [v0 v1 ... vN-1]
    // N = jump_addr - ip - 1
    // Copia N ints do bytecode → me->locals[inicio_privadas..]
    // Depois salta para jump_addr.
    // DIV kernel: memcpy(&mem[id+inicio_privadas],&mem[ip+1],(mem[ip]-ip-1)<<2); ip=mem[ip];
    LPRI = 64,

    // --- acesso remoto (= laid + lptr do DIV) ---
    // Equivale a: pegar o id de um processo (= endereço base em mem[]) e
    // aceder/escrever os seus locals pelo offset.
    //
    // No DIV: lcar offset; laid; lptr   (read)
    //         lcar offset; laid; ... lasi (write)
    //
    // Aqui encapsulamos em dois opcodes:
    //   LOAD_ID_LOCAL  idx : pop id → push find(id)->locals[idx]
    //   STORE_ID_LOCAL idx : pop value, pop id → find(id)->locals[idx] = value
    LOAD_ID_LOCAL  = 70,
    STORE_ID_LOCAL = 71,
};
