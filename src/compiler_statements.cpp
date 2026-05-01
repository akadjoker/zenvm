/*
** compiler_statements.cpp — Statement and declaration parsing.
** Handles: var, def, class, struct, if/elif/else, while, for,
** foreach, loop, do..while, switch, return, break, continue, frame, block.
*/

#include "compiler.h"
#include "vm.h"

namespace zen {

/* =========================================================
** Top-level dispatcher
** ========================================================= */

void Compiler::declaration() {
    if (panic_mode_) {
        /* Synchronize: skip tokens until a statement boundary */
        panic_mode_ = false;
        while (!check(TOK_EOF)) {
            if (previous_.type == TOK_SEMICOLON) return;
            switch (current_.type) {
                case TOK_VAR: case TOK_DEF: case TOK_CLASS: case TOK_STRUCT:
                case TOK_IF: case TOK_WHILE: case TOK_FOR: case TOK_FOREACH:
                case TOK_LOOP: case TOK_DO: case TOK_RETURN: case TOK_IMPORT:
                    return;
                default:
                    advance();
            }
        }
        return;
    }

    if (check(TOK_VAR))         { advance(); var_declaration(); }
    else if (check(TOK_DEF))    { advance(); fun_declaration(); }
    else if (check(TOK_CLASS))  { advance(); class_declaration(); }
    else if (check(TOK_STRUCT)) { advance(); struct_declaration(); }
    else if (check(TOK_IMPORT)) { advance(); import_statement(); }
    else                        { statement(); }
}

/* =========================================================
** var declaration
** var x = expr;
** var (a, b, _) = expr;
** ========================================================= */

void Compiler::var_declaration() {
    /* Multi-return: var (a, b, _) = expr; */
    if (match(TOK_LPAREN)) {
        /* Collect names */
        Token names[16];
        int name_count = 0;
        do {
            if (check(TOK_UNDERSCORE)) {
                names[name_count].type = TOK_UNDERSCORE;
                names[name_count].start = "_";
                names[name_count].length = 1;
                names[name_count].line = current_.line;
                advance();
            } else {
                consume(TOK_IDENTIFIER, "Expected variable name.");
                names[name_count] = previous_;
            }
            name_count++;
        } while (match(TOK_COMMA));
        consume(TOK_RPAREN, "Expected ')' after variable list.");
        consume(TOK_EQ, "Expected '=' after variable list.");

        /* Evaluate RHS — multi-return call */
        /* For now: evaluate into consecutive registers */
        int base = state_->next_reg;
        expression(base);
        /* TODO: handle multi-return from calls properly */

        /* Assign to locals */
        for (int i = 0; i < name_count; i++) {
            if (names[i].type == TOK_UNDERSCORE) continue; /* discard */
            if (state_->scope_depth > 0) {
                /* Local */
                int reg = add_local(names[i]);
                emit_move(reg, base + i);
            } else {
                /* Global */
                char buf[256];
                int len = names[i].length < 255 ? names[i].length : 255;
                memcpy(buf, names[i].start, len);
                buf[len] = '\0';
                int gidx = vm_->find_global(buf);
                if (gidx < 0) gidx = vm_->def_global(buf, val_nil());
                state_->emitter.emit_abx(OP_SETGLOBAL, base + i, gidx, names[i].line);
            }
        }
        consume(TOK_SEMICOLON, "Expected ';' after declaration.");
        return;
    }

    /* Simple: var x = expr; or var x; */
    consume(TOK_IDENTIFIER, "Expected variable name.");
    Token name = previous_;

    if (state_->scope_depth > 0) {
        /* Local variable */
        int reg = add_local(name);
        if (match(TOK_EQ)) {
            expression(reg);
        } else {
            state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, name.line);
        }
    } else {
        /* Global variable */
        char buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(buf, name.start, len);
        buf[len] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0) gidx = vm_->def_global(buf, val_nil());

        if (match(TOK_EQ)) {
            int reg = alloc_reg();
            expression(reg);
            state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
            free_reg(reg);
        } else {
            /* nil by default — already set */
        }
    }

    consume(TOK_SEMICOLON, "Expected ';' after variable declaration.");
}

/* =========================================================
** def — function declaration
** def name(params) { body }
** ========================================================= */

void Compiler::fun_declaration() {
    consume(TOK_IDENTIFIER, "Expected function name.");
    Token name = previous_;

    /* Create function in a new compiler state */
    CompilerState fn_state;
    fn_state.parent       = state_;
    fn_state.function     = new_func(gc_);
    fn_state.emitter      = Emitter(gc_);
    fn_state.local_count  = 0;
    fn_state.scope_depth  = 0;
    fn_state.next_reg     = 0;
    fn_state.max_reg      = 0;
    fn_state.upvalue_count = 0;
    fn_state.loop_depth   = 0;
    fn_state.is_method    = false;

    char name_buf[256];
    int len = name.length < 255 ? name.length : 255;
    memcpy(name_buf, name.start, len);
    name_buf[len] = '\0';

    fn_state.emitter.begin(name_buf, 0, name_buf);

    CompilerState* enclosing = state_;
    state_ = &fn_state;

    begin_scope();

    /* Parameters */
    consume(TOK_LPAREN, "Expected '(' after function name.");
    int arity = 0;
    if (!check(TOK_RPAREN)) {
        do {
            consume(TOK_IDENTIFIER, "Expected parameter name.");
            add_local(previous_);
            arity++;
        } while (match(TOK_COMMA));
    }
    consume(TOK_RPAREN, "Expected ')' after parameters.");

    /* Body */
    consume(TOK_LBRACE, "Expected '{' before function body.");
    block();

    /* Implicit return nil */
    state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
    state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

    ObjFunc* fn = state_->emitter.end(state_->max_reg);
    fn->arity = arity;

    /* Restore enclosing state */
    state_ = enclosing;

    /* Store function as constant and load it */
    int ki = state_->emitter.add_constant(val_obj((Obj*)fn));

    if (state_->scope_depth > 0) {
        /* Local function */
        int reg = add_local(name);
        if (fn_state.upvalue_count > 0) {
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
        } else {
            state_->emitter.emit_abx(OP_LOADK, reg, ki, name.line);
        }
    } else {
        /* Global function */
        int gidx = vm_->find_global(name_buf);
        if (gidx < 0) gidx = vm_->def_global(name_buf, val_nil());
        int reg = alloc_reg();
        if (fn_state.upvalue_count > 0) {
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
        } else {
            state_->emitter.emit_abx(OP_LOADK, reg, ki, name.line);
        }
        state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
        free_reg(reg);
    }
}

/* =========================================================
** class declaration (stub — will expand)
** class Name { var field; def method() {} }
** ========================================================= */

void Compiler::class_declaration() {
    consume(TOK_IDENTIFIER, "Expected class name.");
    /* Token name = previous_; */
    /* TODO: full class compilation */
    consume(TOK_LBRACE, "Expected '{' before class body.");
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        advance(); /* skip for now */
    }
    consume(TOK_RBRACE, "Expected '}' after class body.");
    error("Class declarations not yet implemented.");
}

/* =========================================================
** struct declaration (stub)
** ========================================================= */

void Compiler::struct_declaration() {
    consume(TOK_IDENTIFIER, "Expected struct name.");
    consume(TOK_LBRACE, "Expected '{' before struct body.");
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        advance();
    }
    consume(TOK_RBRACE, "Expected '}' after struct body.");
    error("Struct declarations not yet implemented.");
}

/* =========================================================
** import (stub)
** ========================================================= */

void Compiler::import_statement() {
    consume(TOK_IDENTIFIER, "Expected module name after 'import'.");
    consume(TOK_SEMICOLON, "Expected ';' after import.");
    error("Import not yet implemented.");
}

/* =========================================================
** Statement dispatcher
** ========================================================= */

void Compiler::statement() {
    if (match(TOK_IF))       { if_statement(); }
    else if (match(TOK_WHILE))   { while_statement(); }
    else if (match(TOK_FOR))     { for_statement(); }
    else if (match(TOK_FOREACH)) { foreach_statement(); }
    else if (match(TOK_LOOP))    { loop_statement(); }
    else if (match(TOK_DO))      { do_while_statement(); }
    else if (match(TOK_SWITCH))  { switch_statement(); }
    else if (match(TOK_RETURN))  { return_statement(); }
    else if (match(TOK_BREAK))   { break_statement(); }
    else if (match(TOK_CONTINUE)){ continue_statement(); }
    else if (match(TOK_FRAME))   { frame_statement(); }
    else if (match(TOK_PRINT))   { print_statement(); }
    else if (match(TOK_LBRACE))  { begin_scope(); block(); end_scope(); }
    else                         { expression_statement(); }
}

/* =========================================================
** Expression statement
** ========================================================= */

void Compiler::expression_statement() {
    int reg = expression(-1);
    free_reg(reg);
    consume(TOK_SEMICOLON, "Expected ';' after expression.");
}

/* =========================================================
** Block: { declarations... }
** Assumes opening '{' already consumed.
** ========================================================= */

void Compiler::block() {
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        declaration();
    }
    consume(TOK_RBRACE, "Expected '}' after block.");
}

/* =========================================================
** if / elif / else
** ========================================================= */

void Compiler::if_statement() {
    consume(TOK_LPAREN, "Expected '(' after 'if'.");
    int cond_reg = expression(-1);
    consume(TOK_RPAREN, "Expected ')' after condition.");

    /* Jump over 'then' body if false */
    int then_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
    free_reg(cond_reg);

    /* Then body */
    consume(TOK_LBRACE, "Expected '{' after if condition.");
    begin_scope();
    block();
    end_scope();

    /* Jump over elif/else chain */
    int end_jump = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
    state_->emitter.patch_jump(then_jump);

    /* elif chain */
    while (match(TOK_ELIF)) {
        consume(TOK_LPAREN, "Expected '(' after 'elif'.");
        cond_reg = expression(-1);
        consume(TOK_RPAREN, "Expected ')' after elif condition.");

        int elif_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
        free_reg(cond_reg);

        consume(TOK_LBRACE, "Expected '{' after elif condition.");
        begin_scope();
        block();
        end_scope();

        /* Chain: jump to end */
        int next_end = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
        state_->emitter.patch_jump(elif_jump);

        /* Patch previous end_jump to here? No — we need a list */
        /* Simple approach: patch the previous end_jump, get new one */
        state_->emitter.patch_jump(end_jump);
        end_jump = next_end;
    }

    /* else */
    if (match(TOK_ELSE)) {
        consume(TOK_LBRACE, "Expected '{' after 'else'.");
        begin_scope();
        block();
        end_scope();
    }

    state_->emitter.patch_jump(end_jump);
}

/* =========================================================
** while (cond) { body }
** ========================================================= */

void Compiler::while_statement() {
    int loop_start = state_->emitter.current_offset();

    /* Push loop context */
    LoopCtx& loop = state_->loops[state_->loop_depth++];
    loop.start = loop_start;
    loop.scope_depth = state_->scope_depth;
    loop.break_count = 0;

    consume(TOK_LPAREN, "Expected '(' after 'while'.");
    int cond_reg = expression(-1);
    consume(TOK_RPAREN, "Expected ')' after condition.");

    int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
    free_reg(cond_reg);

    consume(TOK_LBRACE, "Expected '{' after while condition.");
    begin_scope();
    block();
    end_scope();

    /* Loop back */
    state_->emitter.emit_loop(loop_start, 0, previous_.line);
    state_->emitter.patch_jump(exit_jump);

    /* Patch breaks */
    for (int i = 0; i < loop.break_count; i++)
        state_->emitter.patch_jump(loop.breaks[i]);
    state_->loop_depth--;
}

/* =========================================================
** for (init; cond; step) { body }
** ========================================================= */

void Compiler::for_statement() {
    begin_scope();
    consume(TOK_LPAREN, "Expected '(' after 'for'.");

    /* Initializer */
    if (match(TOK_SEMICOLON)) {
        /* No init */
    } else if (check(TOK_VAR)) {
        advance();
        var_declaration();
    } else {
        expression_statement();
    }

    int loop_start = state_->emitter.current_offset();

    /* Push loop context */
    LoopCtx& loop = state_->loops[state_->loop_depth++];
    loop.start = loop_start;
    loop.scope_depth = state_->scope_depth;
    loop.break_count = 0;

    /* Condition */
    int exit_jump = -1;
    if (!match(TOK_SEMICOLON)) {
        int cond_reg = expression(-1);
        consume(TOK_SEMICOLON, "Expected ';' after for condition.");
        exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
        free_reg(cond_reg);
    }

    /* Step — parse but emit after body */
    int body_jump = -1;
    int step_start = -1;
    if (!check(TOK_RPAREN)) {
        body_jump = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
        step_start = state_->emitter.current_offset();
        int step_reg = expression(-1);
        free_reg(step_reg);
        state_->emitter.emit_loop(loop_start, 0, previous_.line);
        loop.start = step_start; /* continue goes to step */
    }
    consume(TOK_RPAREN, "Expected ')' after for clauses.");

    if (body_jump >= 0)
        state_->emitter.patch_jump(body_jump);

    /* Body */
    consume(TOK_LBRACE, "Expected '{' after for clauses.");
    begin_scope();
    block();
    end_scope();

    /* Loop back (to step if exists, else to condition) */
    if (step_start >= 0) {
        state_->emitter.emit_loop(step_start, 0, previous_.line);
    } else {
        state_->emitter.emit_loop(loop_start, 0, previous_.line);
    }

    if (exit_jump >= 0)
        state_->emitter.patch_jump(exit_jump);

    /* Patch breaks */
    for (int i = 0; i < loop.break_count; i++)
        state_->emitter.patch_jump(loop.breaks[i]);
    state_->loop_depth--;

    end_scope();
}

/* =========================================================
** foreach (x in expr) { body }
** ========================================================= */

void Compiler::foreach_statement() {
    begin_scope();
    consume(TOK_LPAREN, "Expected '(' after 'foreach'.");
    consume(TOK_IDENTIFIER, "Expected variable name.");
    Token iter_name = previous_;
    consume(TOK_IN, "Expected 'in' after variable name.");

    /* Evaluate iterable */
    int iterable_reg = expression(-1);
    consume(TOK_RPAREN, "Expected ')' after foreach expression.");

    /* Internal index counter */
    int idx_reg = alloc_reg();
    state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, previous_.line);

    /* Len of iterable */
    int len_reg = alloc_reg();
    state_->emitter.emit_abc(OP_LEN, len_reg, iterable_reg, 0, previous_.line);

    /* Loop variable */
    int var_reg = add_local(iter_name);

    int loop_start = state_->emitter.current_offset();

    LoopCtx& loop = state_->loops[state_->loop_depth++];
    loop.start = loop_start;
    loop.scope_depth = state_->scope_depth;
    loop.break_count = 0;

    /* Condition: idx < len */
    int cond_reg = alloc_reg();
    state_->emitter.emit_abc(OP_LT, cond_reg, idx_reg, len_reg, previous_.line);
    int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
    free_reg(cond_reg);

    /* Load current element: var = iterable[idx] */
    state_->emitter.emit_abc(OP_GETINDEX, var_reg, iterable_reg, idx_reg, previous_.line);

    /* Body */
    consume(TOK_LBRACE, "Expected '{' after foreach.");
    begin_scope();
    block();
    end_scope();

    /* idx = idx + 1 */
    state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, previous_.line);

    /* Loop back */
    state_->emitter.emit_loop(loop_start, 0, previous_.line);
    state_->emitter.patch_jump(exit_jump);

    /* Patch breaks */
    for (int i = 0; i < loop.break_count; i++)
        state_->emitter.patch_jump(loop.breaks[i]);
    state_->loop_depth--;

    free_reg(len_reg);
    free_reg(idx_reg);
    free_reg(iterable_reg);
    end_scope();
}

/* =========================================================
** loop { body } — infinite loop
** ========================================================= */

void Compiler::loop_statement() {
    int loop_start = state_->emitter.current_offset();

    LoopCtx& loop = state_->loops[state_->loop_depth++];
    loop.start = loop_start;
    loop.scope_depth = state_->scope_depth;
    loop.break_count = 0;

    consume(TOK_LBRACE, "Expected '{' after 'loop'.");
    begin_scope();
    block();
    end_scope();

    state_->emitter.emit_loop(loop_start, 0, previous_.line);

    /* Patch breaks */
    for (int i = 0; i < loop.break_count; i++)
        state_->emitter.patch_jump(loop.breaks[i]);
    state_->loop_depth--;
}

/* =========================================================
** do { body } while (cond);
** ========================================================= */

void Compiler::do_while_statement() {
    int loop_start = state_->emitter.current_offset();

    LoopCtx& loop = state_->loops[state_->loop_depth++];
    loop.start = loop_start;
    loop.scope_depth = state_->scope_depth;
    loop.break_count = 0;

    consume(TOK_LBRACE, "Expected '{' after 'do'.");
    begin_scope();
    block();
    end_scope();

    consume(TOK_WHILE, "Expected 'while' after do block.");
    consume(TOK_LPAREN, "Expected '(' after 'while'.");
    int cond_reg = expression(-1);
    consume(TOK_RPAREN, "Expected ')' after condition.");
    consume(TOK_SEMICOLON, "Expected ';' after do-while.");

    /* If condition is true, jump back to loop_start */
    /* emit_loop uses OP_JMP (unconditional). We emit JMPIF manually with negative offset. */
    int offset = loop_start - (state_->emitter.current_offset() + 1);
    state_->emitter.emit(ZEN_ENCODE_SBX(OP_JMPIF, cond_reg, offset), previous_.line);
    free_reg(cond_reg);

    /* Patch breaks */
    for (int i = 0; i < loop.break_count; i++)
        state_->emitter.patch_jump(loop.breaks[i]);
    state_->loop_depth--;
}

/* =========================================================
** switch (expr) { case val: { body } default: { body } }
** No fallthrough — each case is independent.
** ========================================================= */

void Compiler::switch_statement() {
    consume(TOK_LPAREN, "Expected '(' after 'switch'.");
    int expr_reg = expression(-1);
    consume(TOK_RPAREN, "Expected ')' after switch expression.");
    consume(TOK_LBRACE, "Expected '{' after switch.");

    int end_jumps[64];
    int end_count = 0;

    while (match(TOK_CASE)) {
        /* case value: { body } */
        int case_val_reg = expression(-1);
        consume(TOK_COLON, "Expected ':' after case value.");

        /* Compare: expr == case_val */
        int cmp_reg = alloc_reg();
        state_->emitter.emit_abc(OP_EQ, cmp_reg, expr_reg, case_val_reg, previous_.line);
        int skip_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, previous_.line);
        free_reg(cmp_reg);
        free_reg(case_val_reg);

        /* Body */
        consume(TOK_LBRACE, "Expected '{' after case.");
        begin_scope();
        block();
        end_scope();

        /* Jump to end of switch */
        if (end_count < 64)
            end_jumps[end_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);

        state_->emitter.patch_jump(skip_jump);
    }

    /* default */
    if (match(TOK_DEFAULT)) {
        consume(TOK_COLON, "Expected ':' after 'default'.");
        consume(TOK_LBRACE, "Expected '{' after default.");
        begin_scope();
        block();
        end_scope();
    }

    consume(TOK_RBRACE, "Expected '}' after switch body.");

    /* Patch all end jumps */
    for (int i = 0; i < end_count; i++)
        state_->emitter.patch_jump(end_jumps[i]);

    free_reg(expr_reg);
}

/* =========================================================
** return expr, expr, ...;
** ========================================================= */

void Compiler::return_statement() {
    if (match(TOK_SEMICOLON)) {
        /* return; → return nil */
        state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
        return;
    }

    /* Return values — put in consecutive regs starting at some base */
    int base = state_->next_reg;
    int count = 0;
    do {
        int reg = alloc_reg();
        expression(reg);
        count++;
    } while (match(TOK_COMMA));

    consume(TOK_SEMICOLON, "Expected ';' after return value.");

    state_->emitter.emit_abc(OP_RETURN, base, count, 0, previous_.line);
    state_->next_reg = base; /* free temps */
}

/* =========================================================
** break; / continue;
** ========================================================= */

void Compiler::break_statement() {
    if (state_->loop_depth == 0) {
        error("'break' outside of loop.");
        consume(TOK_SEMICOLON, "Expected ';' after 'break'.");
        return;
    }
    consume(TOK_SEMICOLON, "Expected ';' after 'break'.");

    LoopCtx& loop = state_->loops[state_->loop_depth - 1];
    if (loop.break_count >= 64) {
        error("Too many 'break' statements in loop.");
        return;
    }
    loop.breaks[loop.break_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
}

void Compiler::continue_statement() {
    if (state_->loop_depth == 0) {
        error("'continue' outside of loop.");
        consume(TOK_SEMICOLON, "Expected ';' after 'continue'.");
        return;
    }
    consume(TOK_SEMICOLON, "Expected ';' after 'continue'.");

    LoopCtx& loop = state_->loops[state_->loop_depth - 1];
    state_->emitter.emit_loop(loop.start, 0, previous_.line);
}

/* =========================================================
** frame; — game loop yield
** ========================================================= */

void Compiler::frame_statement() {
    consume(TOK_SEMICOLON, "Expected ';' after 'frame'.");
    state_->emitter.emit_abc(OP_FRAME, 0, 0, 0, previous_.line);
}

/* =========================================================
** print(a, b, c);  or  print a;
** Emits OP_PRINT for each arg. Multiple args separated by space.
** B=1 means "print newline after", B=0 means "no newline (more coming)".
** ========================================================= */

void Compiler::print_statement() {
    bool has_paren = match(TOK_LPAREN);

    if (has_paren && check(TOK_RPAREN)) {
        /* print() — just newline */
        advance();
        consume(TOK_SEMICOLON, "Expected ';' after print.");
        /* Emit a nil print with newline flag */
        int reg = alloc_reg();
        state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, previous_.line);
        state_->emitter.emit_abc(OP_PRINT, reg, 1, 0, previous_.line);
        free_reg(reg);
        return;
    }

    /* Collect args */
    int count = 0;
    do {
        int reg = expression(-1);
        count++;
        /* Peek: is there another arg? */
        bool last = has_paren ? (check(TOK_RPAREN)) : (!check(TOK_COMMA));
        /* B=1: print newline (last arg), B=0: print space (not last) */
        state_->emitter.emit_abc(OP_PRINT, reg, last ? 1 : 0, 0, previous_.line);
        free_reg(reg);
    } while (match(TOK_COMMA));

    if (has_paren) consume(TOK_RPAREN, "Expected ')' after print arguments.");
    consume(TOK_SEMICOLON, "Expected ';' after print.");
}

} /* namespace zen */
