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
          had_error_(false), panic_mode_(false), current_file_(nullptr), include_count_(0), include_depth_(0), num_imports_(0), expected_results_(1),
          last_call_struct_def_(nullptr), last_call_class_def_(nullptr),
          global_class_hints_(nullptr), global_class_hints_capacity_(0),
          global_struct_hints_(nullptr), global_struct_hints_capacity_(0),
          global_return_struct_(nullptr), global_return_class_(nullptr), global_return_hints_capacity_(0),
          global_generic_arity_(nullptr), global_generic_arity_capacity_(0),
          global_uses_(nullptr), global_uses_capacity_(0), initial_global_count_(0),
          recursion_depth_(0),
          current_class_fields_(nullptr)
    {
        current_.type = TOK_EOF;
        previous_.type = TOK_EOF;
        ensure_global_class_hint(kInitGlobalCapacity - 1);
        ensure_global_struct_hint(kInitGlobalCapacity - 1);
    }

    Compiler::~Compiler()
    {
        free(global_class_hints_);
        free(global_struct_hints_);
        free(global_return_struct_);
        free(global_return_class_);
        free(global_generic_arity_);
        free(global_uses_);
        for (int i = 0; i < include_count_; i++)
        {
            free(include_sources_[i]);
            free(include_paths_[i]);
        }
    }

    bool Compiler::ensure_global_class_hint(int idx)
    {
        if (idx < 0)
            return false;
        if (idx < global_class_hints_capacity_)
            return true;
        if (idx >= kMaxGlobalsHard)
        {
            error("Too many globals.");
            return false;
        }

        int new_cap = global_class_hints_capacity_ > 0 ? global_class_hints_capacity_ : kInitGlobalCapacity;
        while (new_cap <= idx && new_cap < kMaxGlobalsHard)
            new_cap *= 2;
        if (new_cap <= idx)
            new_cap = kMaxGlobalsHard;

        ObjClass **grown = (ObjClass **)realloc(global_class_hints_, sizeof(ObjClass *) * (size_t)new_cap);
        if (!grown)
        {
            error("Out of memory growing global class hints.");
            return false;
        }
        for (int i = global_class_hints_capacity_; i < new_cap; i++)
            grown[i] = nullptr;
        global_class_hints_ = grown;
        global_class_hints_capacity_ = new_cap;
        return true;
    }

    bool Compiler::ensure_global_struct_hint(int idx)
    {
        if (idx < 0)
            return false;
        if (idx < global_struct_hints_capacity_)
            return true;
        if (idx >= kMaxGlobalsHard)
        {
            error("Too many globals.");
            return false;
        }

        int new_cap = global_struct_hints_capacity_ > 0 ? global_struct_hints_capacity_ : kInitGlobalCapacity;
        while (new_cap <= idx && new_cap < kMaxGlobalsHard)
            new_cap *= 2;
        if (new_cap <= idx)
            new_cap = kMaxGlobalsHard;

        ObjStructDef **grown = (ObjStructDef **)realloc(global_struct_hints_, sizeof(ObjStructDef *) * (size_t)new_cap);
        if (!grown)
        {
            error("Out of memory growing global struct hints.");
            return false;
        }
        for (int i = global_struct_hints_capacity_; i < new_cap; i++)
            grown[i] = nullptr;
        global_struct_hints_ = grown;
        global_struct_hints_capacity_ = new_cap;
        return true;
    }

    void Compiler::set_global_return_hint(int gidx, ObjStructDef *s, ObjClass *c)
    {
        if (gidx < 0)
            return;
        if (gidx >= global_return_hints_capacity_)
        {
            int new_cap = global_return_hints_capacity_ > 0 ? global_return_hints_capacity_ : kInitGlobalCapacity;
            while (new_cap <= gidx && new_cap < kMaxGlobalsHard)
                new_cap *= 2;
            global_return_struct_ = (ObjStructDef **)realloc(global_return_struct_, sizeof(ObjStructDef *) * (size_t)new_cap);
            global_return_class_ = (ObjClass **)realloc(global_return_class_, sizeof(ObjClass *) * (size_t)new_cap);
            for (int i = global_return_hints_capacity_; i < new_cap; i++)
            {
                global_return_struct_[i] = nullptr;
                global_return_class_[i] = nullptr;
            }
            global_return_hints_capacity_ = new_cap;
        }
        global_return_struct_[gidx] = s;
        global_return_class_[gidx] = c;
    }

    int Compiler::global_generic_arity_raw(int gidx) const
    {
        if (gidx < 0 || gidx >= global_generic_arity_capacity_)
            return 0;
        return global_generic_arity_[gidx];
    }

    int Compiler::global_generic_arity(int gidx) const
    {
        int raw = global_generic_arity_raw(gidx);
        return raw == kNonGenericDef ? 0 : raw;
    }

    void Compiler::set_global_generic_arity(int gidx, int arity)
    {
        if (gidx < 0)
            return;
        if (gidx >= global_generic_arity_capacity_)
        {
            int new_cap = global_generic_arity_capacity_ > 0 ? global_generic_arity_capacity_ : kInitGlobalCapacity;
            while (new_cap <= gidx && new_cap < kMaxGlobalsHard)
                new_cap *= 2;
            if (new_cap <= gidx)
                new_cap = kMaxGlobalsHard;
            int *grown = (int *)realloc(global_generic_arity_, sizeof(int) * (size_t)new_cap);
            if (!grown)
            {
                error("Out of memory growing global generic arities.");
                return;
            }
            for (int i = global_generic_arity_capacity_; i < new_cap; i++)
                grown[i] = 0;
            global_generic_arity_ = grown;
            global_generic_arity_capacity_ = new_cap;
        }
        global_generic_arity_[gidx] = arity;
    }

    /* Generic arity of a bare callee name. Only a global `def` can be generic —
    ** a local or an upvalue holding a function value carries no compile-time
    ** signature, so `<` after it stays a comparison. */
    int Compiler::generic_arity_of_callee(const Token &name)
    {
        Token tok = name;
        if (resolve_local(state_, &tok) != -1 || resolve_upvalue(state_, &tok) != -1)
            return 0;
        char buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(buf, name.start, len);
        buf[len] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return 0;
        return global_generic_arity(gidx);
    }

    /* A script `def` only fills its global slot at RUNTIME (OP_CLOSURE +
    ** OP_SETGLOBAL), so at compile time the slot still holds nil — the value
    ** can't answer "is this a function". global_generic_arity_ therefore marks
    ** every global def with kNonGenericDef instead of leaving it 0, so a
    ** non-generic def is still distinguishable from an unknown name. */
    bool Compiler::callee_is_known_def(const Token &name)
    {
        Token tok = name;
        if (resolve_local(state_, &tok) != -1 || resolve_upvalue(state_, &tok) != -1)
            return false;
        char buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(buf, name.start, len);
        buf[len] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return false;
        if (global_generic_arity_raw(gidx) != 0)
            return true;
        /* Natives (and anything else already resolved) can be read directly. */
        Value gval = vm_->get_global(gidx);
        return is_closure(gval) || is_func(gval) || is_native(gval);
    }

    /* Generic arity of `klass.method`, read straight off the flattened vtable.
    ** Works uniformly for a script method (ObjFunc::generic_arity, set when the
    ** class body was compiled) and for a native one registered through
    ** ClassBuilder::generic_method() (ObjNative::generic_arity). */
    int Compiler::generic_arity_of_method(ObjClass *klass, const Token &method)
    {
        if (!klass)
            return 0;
        int slot = vm_->find_selector(method.start, method.length);
        if (slot < 0 || slot >= klass->vtable_size)
            return 0;
        Value mval = klass->vtable[slot];
        if (is_closure(mval))
            return as_closure(mval)->func->generic_arity;
        if (is_native(mval))
            return as_native(mval)->generic_arity;
        return 0;
    }

    /* This compiler is genuinely single-pass, so `def later<T>(...)` only
    ** becomes visible to callee_is_known_def()/global_generic_arity() once
    ** fun_declaration() actually reaches it. A call site EARLIER in the file
    ** — including two generics recursing on each other — would otherwise see
    ** generic arity 0 and silently read `f<A>(x)` as the comparison chain
    ** `(f < A) > (x)`, with no error. This pass runs before real compilation
    ** starts and fixes exactly that: find every top-level `def NAME<...>`
    ** and register its arity up front, using a throwaway Lexer so nothing
    ** here touches lexer_/current_/previous_/had_error_.
    **
    ** Deliberately dumb: only tracks brace depth to skip over bodies (so a
    ** nested/local def, or anything mentioning `def` in a string or inside a
    ** class, is ignored — matching fun_declaration()'s own rule that only a
    ** scope_depth==0 def is ever treated as generic) and bails out silently
    ** on anything that doesn't look like `def IDENT < IDENT (, IDENT)* >` at
    ** brace depth 0. Malformed input is not this pass's problem — the real
    ** parser below will report the actual error when it gets there. */
    void Compiler::prescan_generic_defs(const char *source)
    {
        Lexer scan;
        scan.init(source);
        int brace_depth = 0;
        for (;;)
        {
            Token tok = scan.next_token();
            if (tok.type == TOK_EOF)
                break;
            if (tok.type == TOK_LBRACE)
            {
                brace_depth++;
                continue;
            }
            if (tok.type == TOK_RBRACE)
            {
                if (brace_depth > 0)
                    brace_depth--;
                continue;
            }
            if (tok.type != TOK_DEF || brace_depth != 0)
                continue;

            Token name = scan.next_token();
            if (name.type != TOK_IDENTIFIER)
                continue;
            Token after_name = scan.next_token();
            if (after_name.type != TOK_LT)
                continue; /* `def f(...)`, not generic — fun_declaration() marks it kNonGenericDef in the real pass */

            int count = 0;
            bool ok = true;
            for (;;)
            {
                Token p = scan.next_token();
                if (p.type != TOK_IDENTIFIER)
                {
                    ok = false;
                    break;
                }
                count++;
                Token sep = scan.next_token();
                if (sep.type == TOK_GT)
                    break;
                if (sep.type != TOK_COMMA)
                {
                    ok = false;
                    break;
                }
            }
            if (!ok || count == 0)
                continue;

            char gname[256];
            int gnlen = name.length < 255 ? name.length : 255;
            memcpy(gname, name.start, gnlen);
            gname[gnlen] = '\0';
            /* No `error()` on failure here — a slot that can't be allocated
            ** during the prescan will fail again, loudly, during the real
            ** pass moments later. */
            int gidx = vm_->find_global(gname);
            if (gidx < 0)
                gidx = vm_->def_global(gname, val_nil());
            if (gidx >= 0)
                set_global_generic_arity(gidx, count > kMaxGenericParams ? kMaxGenericParams : count);
        }
    }

    int Compiler::require_global_slot(const char *name, Token *token)
    {
        int gidx = vm_->find_global(name);
        if (gidx >= 0)
            return gidx;

        gidx = vm_->def_global(name, val_nil());
        if (gidx < 0)
        {
            if (token)
                error_at(token, "Failed to allocate global slot.");
            else
                error("Failed to allocate global slot.");
        }
        return gidx;
    }

    /* =========================================================
    ** Undefined-global detection (compile-time)
    ** ========================================================= */

    void Compiler::ensure_global_use(int gidx)
    {
        if (gidx < 0)
            return;
        if (gidx < global_uses_capacity_)
            return;
        int new_cap = global_uses_capacity_ ? global_uses_capacity_ * 2 : 64;
        while (new_cap <= gidx)
            new_cap *= 2;
        global_uses_ = (GlobalUse *)realloc(global_uses_, new_cap * sizeof(GlobalUse));
        for (int i = global_uses_capacity_; i < new_cap; i++)
        {
            /* Globals that already existed when compilation started (builtins,
               native libs, prior REPL definitions) count as defined. */
            global_uses_[i].defined = (i < initial_global_count_) ? 1 : 0;
            global_uses_[i].has_read = 0;
            global_uses_[i].read_tok = Token{};
        }
        global_uses_capacity_ = new_cap;
    }

    void Compiler::mark_global_defined(int gidx)
    {
        ensure_global_use(gidx);
        if (gidx >= 0)
            global_uses_[gidx].defined = 1;
    }

    void Compiler::mark_global_read(int gidx, Token tok)
    {
        ensure_global_use(gidx);
        if (gidx < 0)
            return;
        if (!global_uses_[gidx].defined && !global_uses_[gidx].has_read)
        {
            global_uses_[gidx].has_read = 1;
            global_uses_[gidx].read_tok = tok;
        }
    }

    void Compiler::check_undefined_globals()
    {
        for (int gidx = 0; gidx < global_uses_capacity_; gidx++)
        {
            if (!global_uses_[gidx].has_read || global_uses_[gidx].defined)
                continue;
            /* Double-guard: a slot that holds a non-nil value at compile end was
               defined by some mechanism we didn't track (native fn, module
               constant, struct/class def). Only a genuinely undefined name is
               still nil here. Forward-referenced script defs are nil now but are
               flagged 'defined' by their def/class/var statement. */
            if (!is_nil(vm_->get_global(gidx)))
                continue;
            Token t = global_uses_[gidx].read_tok;
            error_at(&t, "undefined variable (never declared with var/def/class)");
        }
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
        current_file_ = filename;

        /* Snapshot pre-existing globals (builtins, native libs, prior REPL
           definitions) and reset the undefined-global tracker for this compile. */
        initial_global_count_ = vm->num_globals();
        global_uses_capacity_ = 0;
        recursion_depth_ = 0;

        /* Before the real single-pass compile starts: find every top-level
           generic def so a call site earlier in the file (or mutual
           recursion between two generics) sees it as generic too. Slots this
           allocates are ordinary global defs as far as the rest of compile()
           is concerned — fun_declaration() will find them already present
           via require_global_slot() and just reuse them. */
        prescan_generic_defs(source);

        lexer_.init(source);

        /* Pause the GC for the whole compile. The half-built ObjFunc / constant
           strings are referenced only from the C++ compiler stack, not from GC
           roots, so a collection triggered mid-emit (by the allocation
           threshold, e.g. on a very large function) would free the code buffer
           out from under grow_code(). Resume on every exit path. */
        gc_pause(gc);

        /* Set up the top-level script function state */
        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc);
        script_state.emitter = Emitter(gc);
        script_state.local_count = 0;
        memset(script_state.reg_class_hints, 0, sizeof(script_state.reg_class_hints));
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;
        script_state.is_process = false;

        script_state.emitter.begin(filename, 0, filename);
        state_ = &script_state;

        /* Kick off parsing */
        advance();
        while (!check(TOK_EOF) && !had_error_)
        {
            declaration();
        }
        consume(TOK_EOF, "Expected end of file.");

        /* Now that the whole program is parsed, forward references are resolved:
           any global still read-but-never-defined is a typo. */
        if (!had_error_)
            check_undefined_globals();

        if (had_error_)
        {
            state_ = nullptr;
            gc_resume(gc);
            return nullptr;
        }

        /* Emit final HALT */
        state_->emitter.emit_abc(OP_HALT, 0, 0, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        state_ = nullptr;

        gc_resume(gc);
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

        /* --- header line: file, line number, token --- */
        const char *file = current_file_ ? current_file_ : "<input>";
        fprintf(stderr, "File \"%s\", line %d\n", file, token->line);

        /* --- source snippet: find the line start by walking back from token ---
           TOK_ERROR tokens carry the error *message* in start (not a pointer
           into the source buffer), so walking the line from it reads out of
           bounds — skip the snippet for those. */
        if (token->start && token->type != TOK_ERROR)
        {
            /* walk back to start of line */
            const char *line_start = token->start;
            while (line_start > lexer_.save_state().source && *(line_start - 1) != '\n')
                line_start--;

            /* find end of line */
            const char *line_end = token->start;
            while (*line_end && *line_end != '\n')
                line_end++;

            int line_len = (int)(line_end - line_start);
            fprintf(stderr, "  %.*s\n", line_len, line_start);

            /* caret pointing at the token */
            if (token->type != TOK_EOF && token->type != TOK_ERROR)
            {
                int col = (int)(token->start - line_start);
                int tok_len = token->length > 0 ? token->length : 1;
                for (int i = 0; i < col + 2; i++) fputc(' ', stderr); /* 2 = "  " indent */
                for (int i = 0; i < tok_len; i++) fputc('^', stderr);
                fputc('\n', stderr);
            }
        }

        /* --- error message --- */
        if (token->type == TOK_EOF)
            fprintf(stderr, "Error at end: %s\n", msg);
        else if (token->type == TOK_ERROR)
            fprintf(stderr, "Error: %s\n", msg);
        else
            fprintf(stderr, "Error at '%.*s': %s\n", token->length, token->start, msg);
    }

    void Compiler::error(const char *msg)
    {
        error_at(&previous_, msg);
    }

    void Compiler::error_at_current(const char *msg)
    {
        error_at(&current_, msg);
    }

    bool Compiler::function_nesting_ok()
    {
        int nest = 0;
        for (CompilerState *s = state_; s; s = s->parent)
            nest++;
        if (nest <= kMaxFuncNesting)
            return true;
        error_at_current("functions nested too deeply");
        return false;
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

    /* Reserve the next local slot. If the function already has the maximum
       number of locals, report the error once and return the last slot as a
       throw-away scratch — compilation is already doomed (had_error), so it
       is never executed, and we never index past the fixed-size array. */
    Local &Compiler::next_local()
    {
        if (state_->local_count >= 256)
        {
            error("Too many local variables in function.");
            return state_->locals[255];
        }
        return state_->locals[state_->local_count++];
    }

    int Compiler::add_local(Token name)
    {
        declare_local(name);
        int reg = alloc_reg();
        Local &local = next_local();
        local.name = name;
        local.depth = state_->scope_depth;
        local.reg = reg;
        local.captured = false;
        local.struct_type = nullptr;
        local.class_type = nullptr;
        state_->reg_class_hints[reg] = nullptr;
        return reg;
    }

    void Compiler::try_parse_type_hint()
    {
        if (!match(TOK_COLON))
            return;
        consume(TOK_IDENTIFIER, "Expected type name after ':'.");
        Token type_tok = previous_;

        /* Look up the type name as a global */
        char buf[128];
        int len = type_tok.length < 127 ? type_tok.length : 127;
        memcpy(buf, type_tok.start, len);
        buf[len] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return; /* type not found — silently ignore (forward decl) */

        Value gval = vm_->get_global(gidx);
        Local &local = state_->locals[state_->local_count - 1];
        if (is_struct_def(gval))
        {
            local.struct_type = as_struct_def(gval);
        }
        else if (is_class(gval))
        {
            local.class_type = as_class(gval);
            state_->reg_class_hints[local.reg] = as_class(gval);
        }
    }

    /* =========================================================
    ** Register management
    ** ========================================================= */

    int Compiler::alloc_reg()
    {
        int reg = state_->next_reg++;
        if (reg >= kMaxRegs)
        {
            error("Too many registers needed (expression too complex).");
            /* Clamp so callers that index reg_class_hints[reg] / emit on this
               register never write out of bounds. Compilation already failed
               (had_error), so the clamped register is never executed. */
            state_->next_reg = kMaxRegs;
            reg = kMaxRegs - 1;
        }
        if (reg >= 0 && reg < 256)
            state_->reg_class_hints[reg] = nullptr;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;
        return reg;
    }

    void Compiler::free_reg(int reg)
    {
        /* Only free if it's the most recent allocation AND not a named local.
           This used to test `reg >= local_count` — a register index against a
           COUNT, which only holds while locals occupy 0..local_count-1 with no
           gaps. `var (a, b, c) = f()` breaks that: the call's results are
           allocated as temps first and the locals are declared ABOVE them, so
           a local's register can exceed local_count and free_reg() then handed
           a live local back to the allocator, letting the next `var` land on
           top of it. Ask who owns the register instead of counting. */
        if (reg == state_->next_reg - 1 && !is_local_reg(reg))
        {
            state_->next_reg--;
        }
    }

    void Compiler::set_next_reg(int reg)
    {
        state_->next_reg = reg;
    }

    /* =========================================================
    ** Branch on a condition, fusing the comparison when possible.
    **
    ** `while (i < n)` normally compiles to LT into a temporary, then
    ** JMPIFNOT on that temporary: two instructions and a register that
    ** exists only to be tested once. OP_LTJMPIFNOT/OP_LEJMPIFNOT do both
    ** in one 2-word instruction and never materialise the boolean.
    **
    ** Only safe when the comparison is the instruction we just emitted and
    ** its result register is a temporary: if it is a named local, something
    ** else may read it later.
    ** ========================================================= */
    int Compiler::emit_cond_false_jump(int cond_reg, int line, bool &fused)
    {
        Emitter &e = state_->emitter;
        int off = e.current_offset() - 1;
        fused = false;

        /* A jump patched to land exactly here — the short-circuit JMPIF of
        ** `a() || b < c`, or the equivalent for && — expects the branch to
        ** start at this offset. Fusing replaces the comparison with a 2-word
        ** instruction, so the branch word moves and that jump would land on
        ** the sBx word instead of an opcode. Leave those alone. */
        bool targeted = e.last_patched_target() >= e.current_offset();

        if (off >= 0 && !targeted && !is_local_reg(cond_reg))
        {
            Instruction ins = e.instruction_at(off);
            OpCode op = (OpCode)ZEN_OP(ins);
            if ((op == OP_LT || op == OP_LE || op == OP_EQ) && ZEN_A(ins) == cond_reg)
            {
                int b = ZEN_B(ins), c = ZEN_C(ins);
                /* The operands must outlive the comparison we are deleting.
                ** A temporary above the condition register was produced by
                ** the comparison's own operand evaluation and is still live
                ** here, so both reads stay valid. */
                e.rewind_to(off);
                fused = true;
                free_reg(cond_reg);
                if (op == OP_EQ)
                    return e.emit_cmp_jmpifnot(OP_EQJMPIFNOT, b, c, line);
                return op == OP_LT ? e.emit_lt_jmpifnot(b, c, line)
                                   : e.emit_le_jmpifnot(b, c, line);
            }

            /* `a != b` is EQ then NOT on the same register — the only shape
            ** that produces it — so the pair collapses into NEJMPIFNOT.
            ** off-1 must be the EQ's own word: NOT is one word, so the
            ** instruction before it starts there. */
            if (op == OP_NOT && ZEN_A(ins) == cond_reg && ZEN_B(ins) == cond_reg && off >= 1)
            {
                Instruction cmp = e.instruction_at(off - 1);
                if (ZEN_OP(cmp) == OP_EQ && ZEN_A(cmp) == cond_reg)
                {
                    int b = ZEN_B(cmp), c = ZEN_C(cmp);
                    e.rewind_to(off - 1);
                    fused = true;
                    free_reg(cond_reg);
                    return e.emit_cmp_jmpifnot(OP_NEJMPIFNOT, b, c, line);
                }
            }
        }

        int j = e.emit_jump(OP_JMPIFNOT, cond_reg, line);
        free_reg(cond_reg);
        return j;
    }

    /* =========================================================
    ** `s = s + i` compiles the addition into a temporary and then moves it
    ** to the local, because the right-hand side may read the local being
    ** assigned. But when the value came from a single instruction that
    ** writes its result to A and has already read its operands, pointing
    ** that instruction at the local is equivalent and drops the MOVE.
    **
    ** Restricted to one-word arithmetic/logic opcodes whose only effect is
    ** R[A] = f(R[B], R[C]): anything with a second word, a jump, a call, or
    ** a side effect is left alone. The instruction must also be the last
    ** one emitted, so nothing has read the temporary in between.
    ** ========================================================= */
    static bool is_retargetable_producer(OpCode op)
    {
        switch (op)
        {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_MOD: case OP_IDIV: case OP_POW:
        case OP_ADDI: case OP_SUBI:
        case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
        case OP_LT: case OP_LE: case OP_EQ:
        case OP_NEG: case OP_NOT: case OP_BNOT:
            return true;
        default:
            return false;
        }
    }

    bool Compiler::retarget_last_producer(int src, int dest)
    {
        if (src == dest || is_local_reg(src))
            return false;
        Emitter &e = state_->emitter;
        int off = e.current_offset() - 1;
        if (off < 0)
            return false;
        Instruction ins = e.instruction_at(off);
        if (!is_retargetable_producer((OpCode)ZEN_OP(ins)) || ZEN_A(ins) != src)
            return false;
        e.rewrite_a_at(off, dest);
        return true;
    }

    void Compiler::patch_cond_jump(int offset, bool fused)
    {
        if (fused)
            state_->emitter.patch_fused_jump(offset);
        else
            state_->emitter.patch_jump(offset);
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

    Local *Compiler::find_local_by_reg(int reg)
    {
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            if (state_->locals[i].reg == reg)
                return &state_->locals[i];
        }
        return nullptr;
    }

    ObjClass *Compiler::class_hint_for_reg(int reg)
    {
        Local *loc = find_local_by_reg(reg);
        if (loc && loc->class_type)
            return loc->class_type;
        if (reg >= 0 && reg < 256)
            return state_->reg_class_hints[reg];
        return nullptr;
    }

    /* =========================================================
    ** Emission helpers
    ** ========================================================= */

    void Compiler::emit_move(int dst, int src)
    {
        if (dst != src)
        {
            state_->emitter.emit_abc(OP_MOVE, dst, src, 0, previous_.line);
            if (dst >= 0 && dst < 256)
                state_->reg_class_hints[dst] = class_hint_for_reg(src);
        }
    }

} /* namespace zen */
