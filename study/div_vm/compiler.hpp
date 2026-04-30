#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "vm.hpp"

// ============================================================================
// Compiler — lexer + parser + codegen num único passo, estilo DIV
//
// Equivalências com divc.cpp:
//   pieza   → Compiler::pieza   (token actual)
//   lexico()→ Compiler::lexico()
//   g1/g2() → Compiler::g1/g2()
//   imem    → vm.code.size() / cursor
//   im1,im2 → variáveis locais nas funções do parser (backpatch holes)
//   tbreak[]/tcont[] → break_holes / cont_holes
//   obj[]   → Symbol struct em syms[]
// ============================================================================

// ---- Tokens (= "pieza" no DIV) ----
enum class Token {
    // palavras reservadas
    PROGRAM, PROCESS, FUNCTION, GLOBAL, LOCAL, PRIVATE, CONST,
    BEGIN, END, IF, ELSE, WHILE, LOOP, REPEAT, UNTIL,
    FROM, TO, STEP, FOR,
    SWITCH, CASE, DEFAULT,
    BREAK, CONTINUE,
    FRAME, RETURN,
    AND_KW, OR_KW, NOT_KW,    // palavras and/or/not
    // tipos
    INT_KW, FLOAT_KW, STRING_KW,
    // literais / identifiers
    NUMBER, FLOAT_NUMBER, STRING_LITERAL, IDENT,
    // operadores de atribuição
    ASSIGN,                              // =
    ADD_ASSIGN, SUB_ASSIGN,              // += -=
    MUL_ASSIGN, DIV_ASSIGN, MOD_ASSIGN,  // *= /= %=
    INC, DEC,                            // ++ --
    // operadores relacionais
    EQ_OP, NEQ, GT, LT, GTE, LTE,        // == != > < >= <=
    // operadores aritméticos
    PLUS, MINUS, STAR, SLASH, MOD_OP,   // + - * / %
    // operadores lógicos/bitwise
    AND_OP, OR_OP, NOT_OP,               // && || !
    BAND_OP, BOR_OP, BXOR_OP,           // & | ^
    // pontuação
    LPAREN, RPAREN,
    LBRACKET, RBRACKET,                  // [ ]
    SEMICOLON, COMMA, COLON,             // ; , :
    DOTDOT,                              // .. (range em switch: case 1..5)
    DOT,                                 // .  (acesso remoto: father.x, son.y)
    // fim
    END_OF_FILE,
};

// ---- Tipos de símbolo (= tipo em struct objeto do DIV) ----
enum class SymKind {
    GLOBAL,
    LOCAL,
    PROCESS,
    FUNCTION,  // função inline (tem return value, não faz spawn)
    BUILTIN,
    NONE,
};

struct Symbol {
    std::string name;
    SymKind     kind      = SymKind::NONE;
    int         offset    = 0;    // globals: índice em globals[]; locals: índice em locals[]; process: entry
    int         nparams   = 0;
    int         array_len = -1;   // -1 = escalar; >=0 = array
    // para PROCESS: fatia de loc_init[] (= iloc_pub_len do DIV)
    int         loc_offset = 0;
    int         loc_size   = 0;
};

// ============================================================================
class Compiler {
public:
    explicit Compiler(VM& vm);

    bool trace_parse = false;
    bool show_warnings = true;

    // Regista uma função built-in com um nome DIV (ex: "write")
    // Deve chamar-se ANTES de compile().
    void register_builtin(const std::string& name, int builtin_index);

    // Compila source → preenche vm.code e vm.globals
    int compile(const std::string& source);

    // Devolve o entry point de um processo pelo nome (-1 se não encontrado)
    int entry_of(const std::string& name) {
        Symbol* s = find_sym(name);
        if (s && s->kind == SymKind::PROCESS) return s->offset;
        return -1;
    }

private:
    VM& vm;

    // ---- estado do lexer (= source_ptr, pieza, ... do DIV) ----
    const char* source_begin = nullptr;
    const char* token_start = nullptr;
    const char* src   = nullptr;   // cursor no source
    Token       pieza = Token::END_OF_FILE;   // token actual
    int         num_val = 0;       // valor se pieza==NUMBER
    float       float_val = 0.0f;  // valor se pieza==FLOAT_NUMBER
    std::string str_val;           // valor se pieza==STRING_LITERAL
    std::string id_val;            // nome se pieza==IDENT

    // ---- symbol table simples (= obj[] do DIV) ----
    std::vector<Symbol> syms;

    // ---- contadores de alocação (= imem, iloc do DIV) ----
    int next_global = 0;   // próximo offset em globals[]
    int next_local  = 0;   // próximo offset em locals[] do processo corrente

    // ---- backpatch para break/continue (= tbreak[], tcont[] do DIV) ----
    std::vector<int> break_holes;
    std::vector<int> cont_holes;

    // ---- processo/função corrente (para scoping de locais) ----
    std::string current_process;
    std::string current_scope_label = "program";
    bool        in_function = false;  // true quando dentro de 'function ... end'

    // ====================================================================
    // Lexer — equivale ao lexico() do DIV
    // ====================================================================
    void lexico();                           // avança para o próximo token

    // ====================================================================
    // Emissão de bytecode — equivale a g1()/g2() do DIV
    //   g1(op)       → code.push_back(op)
    //   g2(op, val)  → code.push_back(op); code.push_back(val)
    // ====================================================================
    int  here()  const { return (int)vm.code.size(); }   // = imem
    void g1(Op op);
    void g2(Op op, int val);
    void g3(Op op, int v1, int v2);
    // buraco (hole) — emite 0 e devolve o índice para backpatch depois
    int  hole(Op op);

    // ====================================================================
    // Symbol table
    // ====================================================================
    Symbol* find_sym(const std::string& name, SymKind scope_kind = SymKind::NONE);
    Symbol& add_global(const std::string& name, int array_len = -1);
    Symbol& add_local (const std::string& name, int array_len = -1);
    Symbol& add_process(const std::string& name, int entry, int loc_offset, int loc_size, int nparams);

    // Forward references — equivale ao sistema usado/offset como linked list do DIV
    // Quando um processo é chamado antes de ser definido, os holes ficam aqui.
    // Quando o processo é compilado, todos os holes são patched.
    std::unordered_map<std::string, std::vector<int>> forward_holes;

    // ====================================================================
    // Parser — recursive descent, gera bytecode directamente
    // ====================================================================
    void parse_program();
    void parse_global_section();
    void parse_process();
    void parse_function();          // function nome(a,b) begin...end
    void parse_local_section();
    void parse_private_section();

    // statements
    void sentencia();       // dispatcha para o statement correcto
    void stmt_if();
    void stmt_while();
    void stmt_loop();
    void stmt_repeat();     // repeat ... until (cond)   — = p_repeat do DIV
    void stmt_from();
    void stmt_for();        // for (init; cond; incr) ... end — = p_for do DIV
    void stmt_switch();     // switch (expr) case N: ... end ... end — = p_switch do DIV
    void stmt_frame();
    void stmt_return();
    void stmt_break();
    void stmt_continue();
    void stmt_assign_or_call();  // x=expr, x[i]=expr, x+=expr, x++, proc()

    // expressões (recursive descent, emite código)
    void condicion();       // expr com AND/OR chains
    void condicion_atom();  // expr + operador relacional (átomo de condição)
    void expr();            // adição/subtracção/BOR
    void term();            // multiplicação/divisão/MOD/BAND/BXOR
    void factor();          // literal, variável, array[idx], (expr), NOT, -

    // helpers
    void expect(Token t, const char* msg);
    bool check(Token t) const { return pieza == t; }
    bool match(Token t);
    void patch(int hole_idx) { vm.code[hole_idx] = here(); }
    void error(const std::string& msg);
    void warning(const std::string& msg) const;
    void trace(const std::string& msg) const;
    std::string current_token_text() const;
    std::string current_source_context() const;
    void current_source_position(int& line, int& column, std::string* line_text = nullptr) const;
    void record_debug_info(int bytecode_offset) const;
    Value parse_scalar_literal(const char* msg, bool allow_string = true, bool allow_float = true);
    void emit_scalar_literal();
};
