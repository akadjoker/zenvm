#pragma once

// Opcodes da VM — mesma lógica do DIV original (inter.h)
// mas com nomes em inglês claro em vez de abreviaturas espanholas

enum class Op : int {
    NOP          = 0,  // não faz nada

    // --- stack / literais ---
    PUSH         = 1,  // empurra literal inteiro para a stack
    PUSH_STRING  = 2,  // empurra literal de string para a stack
    PUSH_FLOAT   = 20, // empurra literal float (bits inline) para a stack

    // --- memória ---
    LOAD_GLOBAL  = 3,
    STORE_GLOBAL = 4,
    LOAD_LOCAL   = 5,
    STORE_LOCAL  = 6,
    LOAD_PRIVATE = 7,
    STORE_PRIVATE = 8,

    // --- comparação (deixam 1 ou 0 na stack) ---
    EQ           = 9,
    NEQ          = 10,
    GT           = 11,
    LT           = 12,
    LTE          = 13,
    GTE          = 14,

    // --- aritmética ---
    ADD          = 15,
    SUB          = 16,
    MUL          = 17,
    DIV          = 18,
    NEG          = 19,

    // --- saltos ---
    JUMP         = 23,
    JUMP_FALSE   = 24,

    // --- processo / scheduler ---
    CALL_BUILTIN = 25,
    SPAWN        = 26,
    RETURN       = 27,
    POP          = 28,
    FRAME        = 29,
    FUNC_CALL    = 30,
    FUNC_RET     = 31,
};
