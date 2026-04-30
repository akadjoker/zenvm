#pragma once
#include <array>
#include <vector>
#include <string>

#include "value.hpp"

static constexpr int LOCALS_SIZE     = 64;
static constexpr int PROC_STACK_SIZE = 1024;
static constexpr int MAX_CALL_DEPTH  = 128;

static constexpr int LOC_X          = 0;
static constexpr int LOC_Y          = 1;
static constexpr int LOC_ANGLE      = 2;
static constexpr int LOC_GRAPH      = 3;
static constexpr int LOC_SIZE       = 4;
static constexpr int LOC_FLAGS      = 5;
static constexpr int LOC_FILE       = 6;
// = _Father/_Son do DIV (mem[id+_Father], mem[id+_Son])
static constexpr int LOC_FATHER     = 7;
static constexpr int LOC_SON        = 8;
static constexpr int LOC_USER_START = 9;  // primeiro slot de user private/local

enum class ProcessStatus { ALIVE=2, DEAD=0, SLEEPING=3, FROZEN=4 };

// Frame de chamada de função — guardado no call_stack do processo
// quando se executa FUNC_CALL, restaurado em FUNC_RET.
struct CallFrame {
    int ret_ip = 0;
    Value* slots = nullptr;
};

struct Process {
    int           id     = -1;
    std::string   name;
    ProcessStatus status = ProcessStatus::ALIVE;

    int  priority            = 100;
    bool executed_this_frame = false;
    int  frame_accum         = 0;

    int saved_ip = 0;
    int saved_sp = -1;

    // inicio_privadas — equivale à variável global do DIV com o mesmo nome.
    // Após copiar as local vars, aponta para onde as private devem ir.
    // LPRI copia para locals[inicio_privadas] e incrementa.
    // (= inicio_privadas=iloc_pub_len no ltyp do DIV)
    int inicio_privadas = LOC_USER_START;

    std::array<Value, PROC_STACK_SIZE> stack{};
    std::array<Value, LOCALS_SIZE> locals{};
    std::vector<CallFrame> call_stack;

    int father_id = -1;
    int caller_id = -1;

    Process() {
        locals[LOC_SIZE] = Value::make_int(100);
        call_stack.reserve(MAX_CALL_DEPTH);
    }
};
