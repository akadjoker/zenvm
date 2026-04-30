#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "opcodes.hpp"

// ============================================================================
// Assembler — monta bytecode em code[] com labels e backpatching
//
// Equivale ao que o compilador DIV faz internamente com imem, im1, im2
// e os arrays tbreak[]/tcont[] para resolver buracos.
//
// Uso:
//   Assembler as(vm.code);
//
//   as.label("loop_start");
//   as.emit(Op::LOAD_LOCAL, 0);   // load local[0]
//   as.emit(Op::PUSH_CONST, 10);
//   as.emit(Op::LT);
//   as.jump_false("loop_end");    // buraco resolvido depois
//   // ... body ...
//   as.jump("loop_start");
//   as.label("loop_end");
// ============================================================================
class Assembler {
public:
    std::vector<int>& code;   // referência para o code[] da VM

    explicit Assembler(std::vector<int>& code) : code(code) {}

    // --- emite um opcode sem operandos ---
    void emit(Op op) {
        code.push_back(static_cast<int>(op));
    }

    // --- emite um opcode + 1 operando ---
    void emit(Op op, int operand) {
        code.push_back(static_cast<int>(op));
        code.push_back(operand);
    }

    // --- emite um opcode + 2 operandos ---
    void emit(Op op, int op1, int op2) {
        code.push_back(static_cast<int>(op));
        code.push_back(op1);
        code.push_back(op2);
    }

    // --- define uma label na posição actual do código ---
    void label(const std::string& name) {
        int addr = (int)code.size();
        labels[name] = addr;
        // resolve buracos que estavam à espera desta label
        auto it = holes.find(name);
        if (it != holes.end()) {
            for (int hole : it->second)
                code[hole] = addr;           // backpatch
            holes.erase(it);
        }
    }

    // --- jump incondicional para label (= ljmp do DIV) ---
    void jump(const std::string& target) {
        code.push_back(static_cast<int>(Op::JUMP));
        emit_or_hole(target);
    }

    // --- jump se falso para label (= ljpf do DIV) ---
    void jump_false(const std::string& target) {
        code.push_back(static_cast<int>(Op::JUMP_FALSE));
        emit_or_hole(target);
    }

    // --- endereço actual (útil para passar ao SPAWN) ---
    int here() const { return (int)code.size(); }

    // --- verifica se ficaram buracos por resolver (erro de programação) ---
    bool all_resolved() const { return holes.empty(); }

private:
    std::unordered_map<std::string, int>              labels;
    std::unordered_map<std::string, std::vector<int>> holes;

    void emit_or_hole(const std::string& target) {
        auto it = labels.find(target);
        if (it != labels.end()) {
            code.push_back(it->second);          // já conhecemos o endereço
        } else {
            holes[target].push_back((int)code.size()); // buraco para resolver depois
            code.push_back(0);
        }
    }
};
