/*
** compiler.cpp — Infrastructure: init, advance, consume, errors,
** variable resolution, register management, scope, top-level compile().
**
** Split:
**   compiler.cpp              — this file (infrastructure)
**   compiler_expressions.cpp  — Pratt expression parser
**   compiler_statements.cpp   — statement/declaration parsing
*/

#include "compiler.h"
#include "vm.h"

namespace zen
{

    /* =========================================================
    ** Constructor
    ** ========================================================= */

    Compiler::Compiler()
        : gc_(nullptr), vm_(nullptr), state_(nullptr),
          had_error_(false), panic_mode_(false)
    {
        current_.type = TOK_EOF;
        previous_.type = TOK_EOF;
    }

    /* =========================================================
    ** Top-level compile entry point
    ** ========================================================= */

    ObjFunc *Compiler::compile(GC *gc, VM *vm, const char *source, const char *filename)
    {
        gc_ = gc;
        vm_ = vm;
        had_error_ = false;
        panic_mode_ = false;

        lexer_.init(source);

        /* Set up the top-level script function state */
        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc);
        script_state.emitter = Emitter(gc);
        script_state.local_count = 0;
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;

        script_state.emitter.begin(filename, 0, filename);
        state_ = &script_state;

        /* Kick off parsing */
        advance();
        while (!check(TOK_EOF))
        {
            declaration();
        }
        consume(TOK_EOF, "Expected end of file.");

        /* Emit final HALT */
        state_->emitter.emit_abc(OP_HALT, 0, 0, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        state_ = nullptr;

        return had_error_ ? nullptr : fn;
    }

    /* =========================================================
    ** Parsing infrastructure
    ** ========================================================= */

    void Compiler::advance()
    {
        previous_ = current_;
        for (;;)
        {
            current_ = lexer_.next_token();
            if (current_.type != TOK_ERROR)
                break;
            error_at_current(current_.start);
        }
    }

    void Compiler::consume(TokenType type, const char *msg)
    {
        if (current_.type == type)
        {
            advance();
            return;
        }
        error_at_current(msg);
    }

    bool Compiler::check(TokenType type)
    {
        return current_.type == type;
    }

    bool Compiler::match(TokenType type)
    {
        if (!check(type))
            return false;
        advance();
        return true;
    }

    /* =========================================================
    ** Error reporting
    ** ========================================================= */

    void Compiler::error_at(Token *token, const char *msg)
    {
        if (panic_mode_)
            return;
        panic_mode_ = true;
        had_error_ = true;

        fprintf(stderr, "[line %d] Error", token->line);
        if (token->type == TOK_EOF)
        {
            fprintf(stderr, " at end");
        }
        else if (token->type != TOK_ERROR)
        {
            fprintf(stderr, " at '%.*s'", token->length, token->start);
        }
        fprintf(stderr, ": %s\n", msg);
    }

    void Compiler::error(const char *msg)
    {
        error_at(&previous_, msg);
    }

    void Compiler::error_at_current(const char *msg)
    {
        error_at(&current_, msg);
    }

    /* =========================================================
    ** Scope management
    ** ========================================================= */

    void Compiler::begin_scope()
    {
        state_->scope_depth++;
    }

    void Compiler::end_scope()
    {
        state_->scope_depth--;

        /* Pop locals that went out of scope */
        while (state_->local_count > 0 &&
               state_->locals[state_->local_count - 1].depth > state_->scope_depth)
        {
            Local &local = state_->locals[state_->local_count - 1];
            if (local.captured)
            {
                /* Emit CLOSE to capture into upvalue */
                state_->emitter.emit_abc(OP_CLOSE, local.reg, 0, 0, previous_.line);
            }
            state_->local_count--;
        }

        /* Shrink register window back */
        if (state_->local_count > 0)
        {
            state_->next_reg = state_->locals[state_->local_count - 1].reg + 1;
        }
        else
        {
            state_->next_reg = 0;
        }
    }

    /* =========================================================
    ** Variable resolution
    ** ========================================================= */

    static bool identifiers_equal(Token *a, Token *b)
    {
        if (a->length != b->length)
            return false;
        return memcmp(a->start, b->start, a->length) == 0;
    }

    int Compiler::resolve_local(CompilerState *state, Token *name)
    {
        for (int i = state->local_count - 1; i >= 0; i--)
        {
            if (identifiers_equal(&state->locals[i].name, name))
            {
                return state->locals[i].reg;
            }
        }
        return -1;
    }

    int Compiler::add_upvalue(CompilerState *state, int index, bool is_local)
    {
        /* Check if already captured */
        for (int i = 0; i < state->upvalue_count; i++)
        {
            if (state->upvalues[i].index == index &&
                state->upvalues[i].is_local == is_local)
            {
                return i;
            }
        }
        if (state->upvalue_count >= 256)
        {
            error("Too many closure variables in function.");
            return 0;
        }
        state->upvalues[state->upvalue_count].index = index;
        state->upvalues[state->upvalue_count].is_local = is_local;
        return state->upvalue_count++;
    }

    int Compiler::resolve_upvalue(CompilerState *state, Token *name)
    {
        if (state->parent == nullptr)
            return -1;

        /* Try local in enclosing */
        int local = resolve_local(state->parent, name);
        if (local != -1)
        {
            /* Mark as captured in the parent */
            for (int i = 0; i < state->parent->local_count; i++)
            {
                if (state->parent->locals[i].reg == local)
                {
                    state->parent->locals[i].captured = true;
                    break;
                }
            }
            return add_upvalue(state, local, true);
        }

        /* Try upvalue in enclosing (recursive) */
        int upvalue = resolve_upvalue(state->parent, name);
        if (upvalue != -1)
        {
            return add_upvalue(state, upvalue, false);
        }

        return -1;
    }

    void Compiler::declare_local(Token name)
    {
        if (state_->local_count >= 256)
        {
            error("Too many local variables in function.");
            return;
        }
        /* Check for redeclaration in same scope */
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            Local &local = state_->locals[i];
            if (local.depth != -1 && local.depth < state_->scope_depth)
                break;
            if (identifiers_equal(&local.name, &name))
            {
                error("Variable already declared in this scope.");
                return;
            }
        }
    }

    int Compiler::add_local(Token name)
    {
        declare_local(name);
        int reg = alloc_reg();
        Local &local = state_->locals[state_->local_count++];
        local.name = name;
        local.depth = state_->scope_depth;
        local.reg = reg;
        local.captured = false;
        return reg;
    }

    /* =========================================================
    ** Register management
    ** ========================================================= */

    int Compiler::alloc_reg()
    {
        int reg = state_->next_reg++;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;
        if (reg >= kMaxRegs)
        {
            error("Too many registers needed (expression too complex).");
        }
        return reg;
    }

    void Compiler::free_reg(int reg)
    {
        /* Only free if it's a temp register above all locals */
        if (reg == state_->next_reg - 1 && reg >= state_->local_count)
        {
            state_->next_reg--;
        }
    }

    void Compiler::set_next_reg(int reg)
    {
        state_->next_reg = reg;
    }

    bool Compiler::is_local_reg(int reg)
    {
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].reg == reg)
                return true;
        }
        return false;
    }

    /* =========================================================
    ** Emission helpers
    ** ========================================================= */

    void Compiler::emit_move(int dst, int src)
    {
        if (dst != src)
        {
            state_->emitter.emit_abc(OP_MOVE, dst, src, 0, previous_.line);
        }
    }

} /* namespace zen */
