#pragma once

#include <vector>

#include "value.hpp"

// Estado de vida de um processo
enum class ProcessStatus {
    ALIVE,    // a correr / pronto para correr
    DEAD,     // morreu (lret executado)
    SLEEPING, // à espera de sinal
    FROZEN,   // congelado por signal(id, S_FREEZE)
};

static constexpr int DEFAULT_PROCESS_STACK_SIZE = 256;

struct CallFrame {
    int ret_ip = 0;
    std::vector<Value> saved_locals;
};

// Cada processo é uma struct com campos nomeados.
// No DIV original isto era um bloco de ints em mem[] com offsets
// (_Id=0, _Status=4, _IP=7, ...).  Aqui usamos campos normais.
struct Process {
    int           id = -1;               // identificador único (= _Id)
    ProcessStatus status = ProcessStatus::ALIVE; // estado actual (= _Status)

    // --- estado salvo ao fazer FRAME (yield) ---
    int           saved_ip = 0;         // onde retomar        (= _IP)
    int           saved_sp = -1;        // topo de stack salvo (= _SP)

    // --- scheduling ---
    int           priority = 100;       // prioridade (maior = corre primeiro)
    bool          executed_this_frame = false;  // já correu nesta frame?

    // --- árvore de processos ---
    int           father_id = -1;       // quem criou este processo (= _Father)
    int           caller_id = -1;       // quem o invocou           (= _Caller)

    // --- memória do processo ---
    std::vector<Value> stack;
    std::vector<Value> locals;
    std::vector<Value> privates;
    std::vector<CallFrame> call_stack;
};
