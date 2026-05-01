#ifndef ZEN_COMPILER_H
#define ZEN_COMPILER_H

#include "lexer.h"
#include "emitter.h"

namespace zen
{

    /*
    ** Compiler — Single-pass recursive descent + Pratt expression parser.
    ** Compiles zen source → ObjFunc bytecode.
    **
    ** Architecture:
    **   - Pratt parser for expressions (precedence climbing).
    **   - Locals tracked on a compile-time stack (resolved to registers).
    **   - Nested functions handled via CompilerState chain (parent_).
    **
    ** Register allocation:
    **   - Locals live in registers [0 .. num_locals-1].
    **   - Temporaries allocated above locals.
    **   - Each expression returns the register holding its result.
    */

    /* Precedence levels for Pratt parser */
    enum Precedence : uint8_t
    {
        PREC_NONE,
        PREC_ASSIGNMENT,  /* =              */
        PREC_OR,          /* or ||          */
        PREC_AND,         /* and &&         */
        PREC_BITWISE_OR,  /* |              */
        PREC_BITWISE_XOR, /* ^              */
        PREC_BITWISE_AND, /* &              */
        PREC_EQUALITY,    /* == !=          */
        PREC_COMPARISON,  /* < > <= >=      */
        PREC_SHIFT,       /* << >>          */
        PREC_TERM,        /* + -            */
        PREC_FACTOR,      /* * / %          */
        PREC_UNARY,       /* - ! ~ not      */
        PREC_CALL,        /* . () []        */
        PREC_PRIMARY,
    };

    /* Local variable (compile-time only) */
    struct Local
    {
        Token name;    /* identifier token */
        int depth;     /* scope depth (0 = global scope) */
        int reg;       /* register index */
        bool captured; /* captured by a closure? */
    };

    /* Upvalue descriptor (compile-time) */
    struct UpvalueDesc
    {
        int index;     /* local reg in enclosing, or upvalue index in enclosing */
        bool is_local; /* true = captures from immediate parent's locals */
    };

    /* Loop context for break/continue */
    struct LoopCtx
    {
        int start;            /* offset of loop start (for back-jump) */
        int continue_target;  /* where continue jumps (-1 = patch later) */
        int scope_depth;      /* scope depth at loop start */
        int breaks[64];       /* break jump offsets to patch */
        int break_count;
        int continues[64];    /* continue jump offsets to patch (do-while) */
        int continue_count;
    };

    /* Compiler state — one per function being compiled */
    struct CompilerState
    {
        CompilerState *parent; /* enclosing function (nullptr for script) */
        ObjFunc *function;     /* function being built */
        Emitter emitter;       /* bytecode emitter */

        Local locals[256];
        int local_count;
        int scope_depth;
        int next_reg; /* next available register */
        int max_reg;  /* high water mark */

        UpvalueDesc upvalues[256];
        int upvalue_count;

        LoopCtx loops[16];
        int loop_depth;

        bool is_method; /* are we inside a class method? */
    };

    class Compiler
    {
    public:
        Compiler();

        /* Compile source to a top-level script function */
        ObjFunc *compile(GC *gc, VM *vm, const char *source, const char *filename = "<script>");

        /* Error state */
        bool had_error() const { return had_error_; }

    private:
        /* --- Parsing infrastructure --- */
        void advance();
        void consume(TokenType type, const char *msg);
        bool check(TokenType type);
        bool match(TokenType type);

        /* --- Error reporting --- */
        void error_at(Token *token, const char *msg);
        void error(const char *msg);
        void error_at_current(const char *msg);

        /* --- Statements --- */
        void declaration();
        void var_declaration();
        void fun_declaration();
        void class_declaration();
        void struct_declaration();
        void statement();
        void expression_statement();
        void if_statement();
        void while_statement();
        void for_statement();
        bool try_numeric_for(); /* optimized numeric for with FORPREP/FORLOOP */
        void foreach_statement();
        void loop_statement();
        void do_while_statement();
        void switch_statement();
        void return_statement();
        void break_statement();
        void continue_statement();
        void frame_statement();
        void print_statement();
        void block();
        void import_statement();

        /* --- Expressions (Pratt parser) --- */
        int expression(int dest = -1); /* returns register with result */
        int parse_precedence(Precedence prec, int dest);
        int prefix_rule(Token token, int dest, bool canAssign);
        int infix_rule(Token op_token, int left_reg, int dest, bool canAssign);

        /* Prefix handlers */
        int number(Token token, int dest);
        int string_literal(Token token, int dest);
        int verbatim_string_literal(Token token, int dest);
        int interp_string(Token token, int dest);
        int literal(Token token, int dest); /* true/false/nil */
        int variable(Token token, int dest, bool canAssign);
        int unary(Token token, int dest);
        int grouping(int dest);
        int array_literal(int dest);
        int map_literal(int dest);

        /* Infix handlers */
        int binary(Token op, int left, int dest);
        int call_expr(int func_reg, int dest);
        int index_expr(int obj_reg, int dest, bool canAssign);
        int dot_expr(int obj_reg, int dest, bool canAssign);
        int and_expr(int left, int dest);
        int or_expr(int left, int dest);

        /* Builtin handlers */
        int math_builtin(Token token, int dest);
        int spawn_expression(int dest);
        int resume_expression(int dest);
        int yield_expression(int dest);
        int anonymous_function(int dest);

        /* --- Helpers --- */
        Precedence get_precedence(TokenType type);
        bool is_prefix(TokenType type);

        /* --- Variable resolution --- */
        int resolve_local(CompilerState *state, Token *name);
        int resolve_upvalue(CompilerState *state, Token *name);
        int add_upvalue(CompilerState *state, int index, bool is_local);
        void declare_local(Token name);
        int add_local(Token name);
        void begin_scope();
        void end_scope();

        /* --- Register management --- */
        int alloc_reg();
        void free_reg(int reg);
        void set_next_reg(int reg); /* reset temp reg to a specific point */
        bool is_local_reg(int reg);

        /* --- Code emission shortcuts --- */
        void emit_move(int dst, int src);

        /* --- Compiler state --- */
        GC *gc_;
        VM *vm_;
        Lexer lexer_;
        Token current_;
        Token previous_;
        CompilerState *state_; /* current function being compiled */
        bool had_error_;
        bool panic_mode_;
    };

} /* namespace zen */

#endif /* ZEN_COMPILER_H */
