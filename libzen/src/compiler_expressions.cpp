/*
** compiler_expressions.cpp — Pratt expression parser.
** Handles all expression types: literals, variables, unary, binary,
** calls, indexing, field access, array/map literals, assignments.
*/

#include "compiler.h"
#include "vm.h"

namespace zen
{

    /* =========================================================
    ** Precedence table
    ** ========================================================= */

    Precedence Compiler::get_precedence(TokenType type)
    {
        switch (type)
        {
        /* Assignment operators are NOT infix — handled directly in variable/dot/index */
        case TOK_OR:
        case TOK_PIPE_PIPE:
            return PREC_OR;
        case TOK_AND:
        case TOK_AMP_AMP:
            return PREC_AND;
        case TOK_PIPE:
            return PREC_BITWISE_OR;
        case TOK_CARET:
        case TOK_XOR:
            return PREC_BITWISE_XOR;
        case TOK_AMP:
            return PREC_BITWISE_AND;
        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
            return PREC_EQUALITY;
        case TOK_LT:
        case TOK_GT:
        case TOK_LT_EQ:
        case TOK_GT_EQ:
            return PREC_COMPARISON;
        case TOK_LT_LT:
        case TOK_GT_GT:
            return PREC_SHIFT;
        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_TERM;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
            return PREC_FACTOR;
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_DOT:
            return PREC_CALL;
        default:
            return PREC_NONE;
        }
    }

    bool Compiler::is_prefix(TokenType type)
    {
        switch (type)
        {
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING:
        case TOK_VERBATIM_STRING:
        case TOK_INTERP_START:
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_NIL:
        case TOK_IDENTIFIER:
        case TOK_SELF:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_NOT:
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_LBRACE:
        case TOK_SET_LBRACE:
        /* Math builtins */
        case TOK_SIN:
        case TOK_COS:
        case TOK_TAN:
        case TOK_ASIN:
        case TOK_ACOS:
        case TOK_ATAN:
        case TOK_ATAN2:
        case TOK_SQRT:
        case TOK_POW:
        case TOK_LOG:
        case TOK_ABS:
        case TOK_FLOOR:
        case TOK_CEIL:
        case TOK_DEG:
        case TOK_RAD:
        case TOK_EXP:
        case TOK_CLOCK:
        case TOK_LEN:
        case TOK_SPAWN:
        case TOK_RESUME:
        case TOK_YIELD:
        case TOK_FATHER:
        case TOK_SON:
        case TOK_TYPE:
        case TOK_DEF:
            return true;
        default:
            return false;
        }
    }

    bool Compiler::looks_like_generic_call()
    {
        if (current_.type != TOK_LT)
            return false;

        LexerState saved = lexer_.save_state();
        Token type_name = lexer_.next_token();
        Token close = lexer_.next_token();
        Token call = lexer_.next_token();
        lexer_.restore_state(saved);

        return type_name.type == TOK_IDENTIFIER &&
               close.type == TOK_GT &&
               call.type == TOK_LPAREN;
    }

    int Compiler::generic_type_arg(int dest)
    {
        if (check(TOK_LT))
            advance();
        else if (previous_.type != TOK_LT)
            error("Expected '<' before generic type.");
        consume(TOK_IDENTIFIER, "Expected type name in generic call.");
        Token type_name = previous_;
        consume(TOK_GT, "Expected '>' after generic type.");

        char name_buf[256];
        int len = type_name.length < 255 ? type_name.length : 255;
        memcpy(name_buf, type_name.start, len);
        name_buf[len] = '\0';

        int gidx = vm_->find_global(name_buf);
        if (gidx < 0)
            gidx = vm_->def_global(name_buf, val_nil());

        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abx(OP_GETGLOBAL, reg, gidx, type_name.line);
        return reg;
    }

    /* =========================================================
    ** Expression entry point
    ** ========================================================= */

    int Compiler::expression(int dest)
    {
        return parse_precedence(PREC_ASSIGNMENT, dest);
    }

    int Compiler::expression_results(int dest, int nresults)
    {
        int saved = expected_results_;
        expected_results_ = nresults;
        int reg = parse_precedence(PREC_ASSIGNMENT, dest);
        expected_results_ = saved;
        return reg;
    }

    int Compiler::parse_precedence(Precedence prec, int dest)
    {
        Token token = current_;
        advance();

        if (!is_prefix(token.type))
        {
            error("Expected expression.");
            return dest >= 0 ? dest : alloc_reg();
        }

        bool canAssign = (prec <= PREC_ASSIGNMENT);

        /* Prefix: parse the left-hand side */
        int reg = prefix_rule(token, dest, canAssign);

        /* Infix: keep consuming operators at this precedence or higher */
        for (;;)
        {
            Precedence op_prec = get_precedence(current_.type);
            bool is_generic_call = current_.type == TOK_LT && looks_like_generic_call();
            if (is_generic_call)
                op_prec = PREC_CALL;
            if (prec > op_prec)
                break;

            Token op = current_;
            advance();
            if (is_generic_call)
                reg = generic_call_expr(reg, dest);
            else
                reg = infix_rule(op, reg, dest, canAssign);
        }

        /* If we can assign but '=' is still sitting there, it's an error.
           e.g. "a + b = c" — '=' not consumed because PREC_TERM > PREC_ASSIGNMENT */
        if (canAssign && match(TOK_EQ))
        {
            error("Invalid assignment target.");
        }

        return reg;
    }

    /* =========================================================
    ** Prefix rules
    ** ========================================================= */

    int Compiler::prefix_rule(Token token, int dest, bool canAssign)
    {
        switch (token.type)
        {
        case TOK_INT:
        case TOK_FLOAT:
            return number(token, dest);
        case TOK_STRING:
            return string_literal(token, dest);
        case TOK_VERBATIM_STRING:
            return verbatim_string_literal(token, dest);
        case TOK_INTERP_START:
            return interp_string(token, dest);
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_NIL:
            return literal(token, dest);
        case TOK_IDENTIFIER:
        case TOK_SELF:
            return variable(token, dest, canAssign);
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_NOT:
            return unary(token, dest);
        case TOK_LPAREN:
            return grouping(dest);
        case TOK_LBRACKET:
            return array_literal(dest);
        case TOK_LBRACE:
            return map_literal(dest);
        case TOK_SET_LBRACE:
            return set_literal(dest);
        /* Math builtins */
        case TOK_SIN:
        case TOK_COS:
        case TOK_TAN:
        case TOK_ASIN:
        case TOK_ACOS:
        case TOK_ATAN:
        case TOK_ATAN2:
        case TOK_SQRT:
        case TOK_POW:
        case TOK_LOG:
        case TOK_ABS:
        case TOK_FLOOR:
        case TOK_CEIL:
        case TOK_DEG:
        case TOK_RAD:
        case TOK_EXP:
        case TOK_CLOCK:
        case TOK_LEN:
            return math_builtin(token, dest);
        case TOK_SPAWN:
            return spawn_expression(dest);
        case TOK_RESUME:
            return resume_expression(dest);
        case TOK_YIELD:
            return yield_expression(dest);
        case TOK_FATHER:
        case TOK_SON:
            return process_field_expr(token, dest);
        case TOK_TYPE:
        {
            int reg = (dest >= 0) ? dest : alloc_reg();
            state_->emitter.emit_abc(OP_PROC_GET, reg, 0, VM::PRIV_TYPE, previous_.line);
            return reg;
        }
        case TOK_DEF:
            return anonymous_function(dest);
        default:
            error("Expected expression.");
            return dest >= 0 ? dest : alloc_reg();
        }
    }

    /* =========================================================
    ** Infix rules
    ** ========================================================= */

    int Compiler::infix_rule(Token op, int left, int dest, bool canAssign)
    {
        switch (op.type)
        {
        /* Binary arithmetic/comparison/bitwise */
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
        case TOK_LT:
        case TOK_GT:
        case TOK_LT_EQ:
        case TOK_GT_EQ:
        case TOK_LT_LT:
        case TOK_GT_GT:
        case TOK_AMP:
        case TOK_PIPE:
        case TOK_CARET:
        case TOK_XOR:
            return binary(op, left, dest);

        /* Short-circuit logical */
        case TOK_AND:
        case TOK_AMP_AMP:
            return and_expr(left, dest);
        case TOK_OR:
        case TOK_PIPE_PIPE:
            return or_expr(left, dest);

        /* Call: f(args) */
        case TOK_LPAREN:
            return call_expr(left, dest);

        /* Index: a[i] — may be assignment target */
        case TOK_LBRACKET:
            return index_expr(left, dest, canAssign);

        /* Field: a.b — may be assignment target */
        case TOK_DOT:
            return dot_expr(left, dest, canAssign);

        default:
            return left;
        }
    }

    /* =========================================================
    ** Prefix handlers — Literals
    ** ========================================================= */

    int Compiler::number(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();

        if (token.type == TOK_INT)
        {
            /* Try small int (fits in sBx = -32768..32767) */
            long long val;
            if (token.length > 2 && token.start[0] == '0' && (token.start[1] == 'x' || token.start[1] == 'X'))
            {
                val = strtoll(token.start, nullptr, 16);
            }
            else
            {
                val = strtoll(token.start, nullptr, 10);
            }
            if (val >= -32768 && val <= 32767)
            {
                state_->emitter.emit_asbx(OP_LOADI, reg, (int)val, token.line);
            }
            else
            {
                int ki = state_->emitter.add_constant(val_int((int64_t)val));
                state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
            }
        }
        else
        {
            /* Float — always via constant pool */
            double val = strtod(token.start, nullptr);
            int ki = state_->emitter.add_constant(val_float(val));
            state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        }

        return reg;
    }

    int Compiler::string_literal(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        /* Skip quotes: token.start+1, token.length-2 */
        state_->emitter.clear_escape_error();
        int ki = state_->emitter.add_escaped_string_constant(token.start + 1, token.length - 2);
        if (state_->emitter.has_escape_error())
        {
            error(state_->emitter.escape_error());
        }
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    int Compiler::verbatim_string_literal(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        /* Skip @" prefix and closing " → token.start+2, token.length-3 */
        int ki = state_->emitter.add_verbatim_string_constant(token.start + 2, token.length - 3);
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    /* =========================================================
    ** Interpolated string: "text {expr} text {expr} text"
    ** Token sequence: INTERP_START expr INTERP_MID expr INTERP_END
    ** Compiles to: LOADK seg0, TOSTRING expr0, CONCAT, LOADK seg1, ...
    ** ========================================================= */

    int Compiler::interp_string(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();

        /* First segment: skip leading " — token includes the opening quote */
        int seg_start = 1;                      /* skip opening " */
        int seg_len = token.length - seg_start; /* text before { */
        int ki = state_->emitter.add_escaped_string_constant(token.start + seg_start, seg_len);
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);

        /* Compile first expression */
        int tmp = alloc_reg();
        expression(tmp);
        /* Convert expression to string */
        ObjClass *tmp_class = class_hint_for_reg(tmp);
        int str_slot = vm_->find_selector("__str__", 7);
        bool has_str = tmp_class && str_slot >= 0 && str_slot < tmp_class->vtable_size &&
                       !is_nil(tmp_class->vtable[str_slot]);
        state_->emitter.emit_abc(has_str ? OP_TOSTRING_OBJ : OP_TOSTRING, tmp, tmp, 0, token.line);
        /* Concat: reg = reg .. tmp */
        state_->emitter.emit_abc(OP_CONCAT, reg, reg, tmp, token.line);
        free_reg(tmp);

        /* Process middle segments and final segment */
        while (current_.type == TOK_INTERP_MID)
        {
            Token mid = current_;
            advance();

            /* Emit this text segment */
            if (mid.length > 0)
            {
                int tmp2 = alloc_reg();
                int ki2 = state_->emitter.add_escaped_string_constant(mid.start, mid.length);
                state_->emitter.emit_abx(OP_LOADK, tmp2, ki2, mid.line);
                state_->emitter.emit_abc(OP_CONCAT, reg, reg, tmp2, mid.line);
                free_reg(tmp2);
            }

            /* Compile next expression */
            tmp = alloc_reg();
            expression(tmp);
            tmp_class = class_hint_for_reg(tmp);
            has_str = tmp_class && str_slot >= 0 && str_slot < tmp_class->vtable_size &&
                      !is_nil(tmp_class->vtable[str_slot]);
            state_->emitter.emit_abc(has_str ? OP_TOSTRING_OBJ : OP_TOSTRING, tmp, tmp, 0, token.line);
            state_->emitter.emit_abc(OP_CONCAT, reg, reg, tmp, token.line);
            free_reg(tmp);
        }

        /* Final segment (INTERP_END) — text after last } up to closing " */
        if (current_.type == TOK_INTERP_END)
        {
            Token end_tok = current_;
            advance();
            if (end_tok.length > 0)
            {
                int tmp3 = alloc_reg();
                int ki3 = state_->emitter.add_escaped_string_constant(end_tok.start, end_tok.length);
                state_->emitter.emit_abx(OP_LOADK, tmp3, ki3, end_tok.line);
                state_->emitter.emit_abc(OP_CONCAT, reg, reg, tmp3, end_tok.line);
                free_reg(tmp3);
            }
        }
        else
        {
            error("Unterminated interpolated string.");
        }

        return reg;
    }

    int Compiler::literal(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        switch (token.type)
        {
        case TOK_TRUE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 1, 0, token.line);
            break;
        case TOK_FALSE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 0, 0, token.line);
            break;
        case TOK_NIL:
            state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, token.line);
            break;
        default:
            break;
        }
        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Variable
    ** ========================================================= */

    int Compiler::variable(Token token, int dest, bool canAssign)
    {
        /* Check for typed buffer constructors: Int32Array(n) etc. */
        if (token.type == TOK_IDENTIFIER && check(TOK_LPAREN))
        {
            int btype = match_buffer_type(token);
            if (btype >= 0)
                return buffer_constructor((BufferType)btype, dest);
        }

        /* Check for module dot-access: math.random(), math.PI */
        if (token.type == TOK_IDENTIFIER && check(TOK_DOT))
        {
            char mod_name[64];
            int mlen = token.length < 63 ? token.length : 63;
            memcpy(mod_name, token.start, mlen);
            mod_name[mlen] = '\0';

            for (int i = 0; i < num_imports_; i++)
            {
                if (strcmp(imports_[i].lib->name, mod_name) == 0)
                {
                    advance(); /* consume '.' */
                    /* Accept identifier or keyword as member name */
                    if (current_.type == TOK_IDENTIFIER ||
                        (current_.type >= TOK_VAR && current_.type <= TOK_CLOCK))
                        advance();
                    else
                        error_at_current("Expected member name after '.'.");
                    Token member = previous_;
                    char mem_name[64];
                    int memlen = member.length < 63 ? member.length : 63;
                    memcpy(mem_name, member.start, memlen);
                    mem_name[memlen] = '\0';

                    const NativeLib *lib = imports_[i].lib;
                    int base = imports_[i].base_gidx;

                    /* Search functions */
                    for (int f = 0; f < lib->num_functions; f++)
                    {
                        if (strcmp(lib->functions[f].name, mem_name) == 0)
                        {
                            int fn_gidx = base + f;
                            /* Must be a call */
                            if (!check(TOK_LPAREN))
                            {
                                /* Return function as value */
                                int reg = dest >= 0 ? dest : alloc_reg();
                                state_->emitter.emit_abx(OP_GETGLOBAL, reg, fn_gidx, member.line);
                                return reg;
                            }
                            advance(); /* consume '(' */
                            int fn_reg = alloc_reg();
                            state_->emitter.emit_abx(OP_GETGLOBAL, fn_reg, fn_gidx, member.line);
                            int save_next = state_->next_reg;
                            int call_results = expected_results_ > 0 ? expected_results_ : 1;
                            int saved_expected_results = expected_results_;
                            expected_results_ = 1;
                            int arg_count = 0;
                            if (!check(TOK_RPAREN))
                            {
                                do
                                {
                                    int arg_reg = alloc_reg();
                                    expression(arg_reg);
                                    state_->next_reg = arg_reg + 1;
                                    arg_count++;
                                } while (match(TOK_COMMA));
                            }
                            consume(TOK_RPAREN, "Expected ')' after arguments.");
                            expected_results_ = saved_expected_results;
                            state_->emitter.emit_abc(OP_CALL, fn_reg, arg_count, call_results, member.line);
                            state_->next_reg = save_next;
                            int result = dest >= 0 ? dest : fn_reg;
                            if (result != fn_reg)
                                emit_move(result, fn_reg);
                            return result;
                        }
                    }
                    /* Search constants */
                    for (int c = 0; c < lib->num_constants; c++)
                    {
                        if (strcmp(lib->constants[c].name, mem_name) == 0)
                        {
                            int const_gidx = base + lib->num_functions + c;
                            int reg = dest >= 0 ? dest : alloc_reg();
                            state_->emitter.emit_abx(OP_GETGLOBAL, reg, const_gidx, member.line);
                            return reg;
                        }
                    }
                    error("Unknown module member.");
                    return dest >= 0 ? dest : alloc_reg();
                }
            }
        }

        /* In a process body, check privates FIRST (before locals) */
        if (state_->is_process && token.type == TOK_IDENTIFIER)
        {
            int pidx = VM::resolve_private(token.start, token.length);
            if (pidx >= 0)
            {
                int reg = dest >= 0 ? dest : alloc_reg();
                /* Assignment: x = expr, x += expr, etc. */
                if (canAssign && (check(TOK_EQ) || check(TOK_PLUS_EQ) ||
                                  check(TOK_MINUS_EQ) || check(TOK_STAR_EQ) ||
                                  check(TOK_SLASH_EQ)))
                {
                    TokenType assign_op = current_.type;
                    advance();
                    if (assign_op == TOK_EQ)
                    {
                        int val = expression(reg);
                        if (val != reg) emit_move(reg, val);
                        state_->emitter.emit_abc(OP_PROC_SET, reg, 0, pidx, token.line);
                        return reg;
                    }
                    else
                    {
                        /* Compound: load current value, compute, store back */
                        state_->emitter.emit_abc(OP_PROC_GET, reg, 0, pidx, token.line);
                        int rhs = alloc_reg();
                        expression(rhs);
                        OpCode op;
                        switch (assign_op) {
                            case TOK_PLUS_EQ:  op = OP_ADD; break;
                            case TOK_MINUS_EQ: op = OP_SUB; break;
                            case TOK_STAR_EQ:  op = OP_MUL; break;
                            case TOK_SLASH_EQ: op = OP_DIV; break;
                            default:           op = OP_ADD; break;
                        }
                        state_->emitter.emit_abc(op, reg, reg, rhs, token.line);
                        free_reg(rhs);
                        state_->emitter.emit_abc(OP_PROC_SET, reg, 0, pidx, token.line);
                        return reg;
                    }
                }
                /* Read */
                state_->emitter.emit_abc(OP_PROC_GET, reg, 0, pidx, token.line);
                return reg;
            }
        }

        /* Resolve where the variable lives */
        int local_reg = resolve_local(state_, &token);
        int upval = (local_reg == -1) ? resolve_upvalue(state_, &token) : -1;
        int gidx = -1;
        char name_buf[256];

        if (local_reg == -1 && upval == -1)
        {
            int len = token.length < 255 ? token.length : 255;
            memcpy(name_buf, token.start, len);
            name_buf[len] = '\0';

            /* Check exposed modules (using) for bare name resolution */
            for (int i = 0; i < num_imports_; i++)
            {
                if (!imports_[i].exposed)
                    continue;
                const NativeLib *lib = imports_[i].lib;
                int base = imports_[i].base_gidx;
                for (int f = 0; f < lib->num_functions; f++)
                {
                    if (strcmp(lib->functions[f].name, name_buf) == 0)
                    {
                        gidx = base + f;
                        goto resolved;
                    }
                }
                for (int c = 0; c < lib->num_constants; c++)
                {
                    if (strcmp(lib->constants[c].name, name_buf) == 0)
                    {
                        gidx = base + lib->num_functions + c;
                        goto resolved;
                    }
                }
            }

            gidx = vm_->find_global(name_buf);
            if (gidx < 0)
                gidx = vm_->def_global(name_buf, val_nil());
        resolved:;
        }

        /* --- Assignment: = += -= *= /= --- */
        if (canAssign && (check(TOK_EQ) || check(TOK_PLUS_EQ) ||
                          check(TOK_MINUS_EQ) || check(TOK_STAR_EQ) ||
                          check(TOK_SLASH_EQ)))
        {
            TokenType assign_op = current_.type;
            advance(); /* consume the assignment operator */

            if (assign_op == TOK_EQ)
            {
                /* Simple assignment: var = expr */
                if (local_reg != -1)
                {
                    /* Compile RHS to a temp, then move to local.
                       Cannot pass local_reg as dest because the RHS might
                       reference this same local (e.g. b = a % b). */
                    int save_reg = state_->next_reg;
                    int rhs = expression(-1);
                    if (rhs != local_reg)
                    {
                        emit_move(local_reg, rhs);
                    }
                    /* Restore next_reg — we don't want to permanently consume
                       a temp register (if rhs was a temp) or accidentally free
                       a local (if rhs was a local). */
                    state_->next_reg = save_reg;
                    return local_reg;
                }
                int val_reg = alloc_reg();
                expression(val_reg);
                if (upval != -1)
                {
                    state_->emitter.emit_abc(OP_SETUPVAL, val_reg, upval, 0, token.line);
                }
                else
                {
                    state_->emitter.emit_abx(OP_SETGLOBAL, val_reg, gidx, token.line);
                    if (gidx >= 0 && gidx < MAX_GLOBALS)
                        global_class_hints_[gidx] = last_call_class_def_ ? last_call_class_def_ : state_->reg_class_hints[val_reg];
                }
                if (dest >= 0 && dest != val_reg)
                {
                    emit_move(dest, val_reg);
                    free_reg(val_reg);
                    return dest;
                }
                /* val_reg holds the assigned value — return it as result */
                return val_reg;
            }
            else
            {
                /* Compound assignment: += -= *= /= */
                /* Load current value */
                int cur_reg;
                if (local_reg != -1)
                {
                    cur_reg = local_reg;
                }
                else
                {
                    cur_reg = alloc_reg();
                    if (upval != -1)
                        state_->emitter.emit_abc(OP_GETUPVAL, cur_reg, upval, 0, token.line);
                    else
                        state_->emitter.emit_abx(OP_GETGLOBAL, cur_reg, gidx, token.line);
                }

                /* Compile RHS */
                int rhs_reg = alloc_reg();
                expression(rhs_reg);

                /* Perform operation */
                OpCode op;
                switch (assign_op)
                {
                case TOK_PLUS_EQ:
                    op = OP_ADD;
                    break;
                case TOK_MINUS_EQ:
                    op = OP_SUB;
                    break;
                case TOK_STAR_EQ:
                    op = OP_MUL;
                    break;
                case TOK_SLASH_EQ:
                    op = OP_DIV;
                    break;
                default:
                    op = OP_ADD;
                    break;
                }
                /* When dest == left operand and it's a local, use OP_STRADD
                   for += to enable in-place string append optimization.
                   OP_STRADD falls back to numeric ADD if not strings. */
                if (assign_op == TOK_PLUS_EQ && local_reg != -1)
                {
                    state_->emitter.emit_abc(OP_STRADD, cur_reg, rhs_reg, 0, token.line);
                }
                else
                {
                    state_->emitter.emit_abc(op, cur_reg, cur_reg, rhs_reg, token.line);
                }
                free_reg(rhs_reg);

                /* Store back if not local */
                if (local_reg == -1)
                {
                    if (upval != -1)
                        state_->emitter.emit_abc(OP_SETUPVAL, cur_reg, upval, 0, token.line);
                    else
                        state_->emitter.emit_abx(OP_SETGLOBAL, cur_reg, gidx, token.line);
                    free_reg(cur_reg);
                }
                return dest >= 0 ? dest : (local_reg != -1 ? local_reg : alloc_reg());
            }
        }

        /* --- Not assignment: emit GET --- */
        int reg = dest >= 0 ? dest : -1;

        if (local_reg != -1)
        {
            if (reg >= 0 && reg != local_reg)
            {
                emit_move(reg, local_reg);
                return reg;
            }
            return local_reg;
        }

        if (upval != -1)
        {
            if (reg < 0)
                reg = alloc_reg();
            state_->emitter.emit_abc(OP_GETUPVAL, reg, upval, 0, token.line);
            return reg;
        }

        /* Global */
        if (reg < 0)
            reg = alloc_reg();
        state_->emitter.emit_abx(OP_GETGLOBAL, reg, gidx, token.line);
        if (gidx >= 0 && gidx < MAX_GLOBALS && global_class_hints_[gidx])
            state_->reg_class_hints[reg] = global_class_hints_[gidx];

        /* Track struct def for type inference (used by var_declaration) */
        Value gval = vm_->get_global(gidx);
        if (is_struct_def(gval))
            last_call_struct_def_ = as_struct_def(gval);
        else
            last_call_struct_def_ = nullptr;
        if (gidx >= 0 && gidx < MAX_GLOBALS && global_class_hints_[gidx])
            last_call_class_def_ = global_class_hints_[gidx];
        else if (is_class(gval))
            last_call_class_def_ = as_class(gval);
        else
            last_call_class_def_ = nullptr;

        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Unary
    ** ========================================================= */

    int Compiler::unary(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();

        /* Parse operand at unary precedence */
        int operand = parse_precedence(PREC_UNARY, -1);

        switch (token.type)
        {
        case TOK_MINUS:
            state_->emitter.emit_abc(class_hint_for_reg(operand) ? OP_NEG_OBJ : OP_NEG,
                                     reg, operand, 0, token.line);
            last_call_class_def_ = class_hint_for_reg(operand);
            break;
        case TOK_BANG:
        case TOK_NOT:
            state_->emitter.emit_abc(OP_NOT, reg, operand, 0, token.line);
            break;
        case TOK_TILDE:
            state_->emitter.emit_abc(OP_BNOT, reg, operand, 0, token.line);
            break;
        default:
            break;
        }

        if (operand != reg)
            free_reg(operand);
        if (reg >= 0 && reg < 256)
            state_->reg_class_hints[reg] = last_call_class_def_;
        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Grouping: ( expr )
    ** ========================================================= */

    int Compiler::grouping(int dest)
    {
        int reg = expression(dest);
        consume(TOK_RPAREN, "Expected ')' after expression.");
        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Array literal: [a, b, c]
    ** ========================================================= */

    int Compiler::array_literal(int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, previous_.line);

        if (!check(TOK_RBRACKET))
        {
            do
            {
                int val_reg = expression(-1);
                state_->emitter.emit_abc(OP_APPEND, reg, val_reg, 0, previous_.line);
                free_reg(val_reg);
            } while (match(TOK_COMMA));
        }
        consume(TOK_RBRACKET, "Expected ']' after array literal.");
        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Map literal: {key: val, ...}
    ** ========================================================= */

    int Compiler::map_literal(int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, previous_.line);

        if (!check(TOK_RBRACE))
        {
            do
            {
                int key_reg = expression(-1);
                consume(TOK_COLON, "Expected ':' after map key.");
                int val_reg = expression(-1);
                state_->emitter.emit_abc(OP_SETINDEX, reg, key_reg, val_reg, previous_.line);
                free_reg(val_reg);
                free_reg(key_reg);
            } while (match(TOK_COMMA));
        }
        consume(TOK_RBRACE, "Expected '}' after map literal.");
        return reg;
    }

    /* =========================================================
    ** Prefix handlers — Set literal: #{a, b, c}
    ** ========================================================= */

    int Compiler::set_literal(int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_NEWSET, reg, 0, 0, previous_.line);

        if (!check(TOK_RBRACE))
        {
            do
            {
                int val_reg = expression(-1);
                state_->emitter.emit_abc(OP_SETADD, reg, val_reg, 0, previous_.line);
                free_reg(val_reg);
            } while (match(TOK_COMMA));
        }
        consume(TOK_RBRACE, "Expected '}' after set literal.");
        return reg;
    }

    /* =========================================================
    ** Buffer constructors: Int32Array(n) / Float64Array([1,2,3])
    ** ========================================================= */

    int Compiler::match_buffer_type(Token token)
    {
        struct
        {
            const char *name;
            int len;
            BufferType type;
        } types[] = {
            {"Int8Array", 9, BUF_INT8},
            {"Int16Array", 10, BUF_INT16},
            {"Int32Array", 10, BUF_INT32},
            {"Uint8Array", 10, BUF_UINT8},
            {"Uint16Array", 11, BUF_UINT16},
            {"Uint32Array", 11, BUF_UINT32},
            {"Float32Array", 12, BUF_FLOAT32},
            {"Float64Array", 12, BUF_FLOAT64},
        };
        for (int i = 0; i < 8; i++)
        {
            if (token.length == types[i].len &&
                memcmp(token.start, types[i].name, types[i].len) == 0)
                return (int)types[i].type;
        }
        return -1;
    }

    int Compiler::buffer_constructor(BufferType btype, int dest)
    {
        /* Already checked that next is '(' */
        advance(); /* consume '(' */
        int arg_reg = expression(-1);
        consume(TOK_RPAREN, "Expected ')' after buffer constructor argument.");
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_NEWBUFFER, reg, arg_reg, (int)btype, previous_.line);
        if (arg_reg != reg)
            free_reg(arg_reg);
        return reg;
    }

    /* =========================================================
    ** Infix handlers — Binary operators
    ** ========================================================= */

    int Compiler::binary(Token op, int left, int dest)
    {
        /* NEVER reuse left as dest — it may be a local/parameter that's needed later */
        int reg = dest >= 0 ? dest : alloc_reg();

        /* Parse right operand at one higher precedence (left-associative) */
        Precedence prec = (Precedence)(get_precedence(op.type) + 1);
        int right = parse_precedence(prec, -1);
        ObjClass *left_class = class_hint_for_reg(left);
        ObjClass *right_class = class_hint_for_reg(right);
        bool use_obj_op = left_class || right_class;

        OpCode opcode;
        switch (op.type)
        {
        case TOK_PLUS:
            opcode = use_obj_op ? OP_ADD_OBJ : OP_ADD;
            break;
        case TOK_MINUS:
            opcode = use_obj_op ? OP_SUB_OBJ : OP_SUB;
            break;
        case TOK_STAR:
            opcode = use_obj_op ? OP_MUL_OBJ : OP_MUL;
            break;
        case TOK_SLASH:
            opcode = use_obj_op ? OP_DIV_OBJ : OP_DIV;
            break;
        case TOK_PERCENT:
            opcode = use_obj_op ? OP_MOD_OBJ : OP_MOD;
            break;
        case TOK_EQ_EQ:
            opcode = use_obj_op ? OP_EQ_OBJ : OP_EQ;
            break;
        case TOK_BANG_EQ:
            opcode = use_obj_op ? OP_EQ_OBJ : OP_EQ;
            break; /* EQ + NOT */
        case TOK_LT:
            opcode = use_obj_op ? OP_LT_OBJ : OP_LT;
            break;
        case TOK_GT:
            opcode = use_obj_op ? OP_LT_OBJ : OP_LT;
            break; /* swap operands */
        case TOK_LT_EQ:
            opcode = use_obj_op ? OP_LE_OBJ : OP_LE;
            break;
        case TOK_GT_EQ:
            opcode = use_obj_op ? OP_LE_OBJ : OP_LE;
            break; /* swap operands */
        case TOK_LT_LT:
            opcode = OP_SHL;
            break;
        case TOK_GT_GT:
            opcode = OP_SHR;
            break;
        case TOK_AMP:
            opcode = OP_BAND;
            break;
        case TOK_PIPE:
            opcode = OP_BOR;
            break;
        case TOK_CARET:
        case TOK_XOR:
            opcode = OP_BXOR;
            break;
        default:
            opcode = OP_ADD;
            break;
        }

        /* Handle swapped operands for > and >= */
        if (op.type == TOK_GT || op.type == TOK_GT_EQ)
        {
            state_->emitter.emit_abc(opcode, reg, right, left, op.line);
        }
        else
        {
            state_->emitter.emit_abc(opcode, reg, left, right, op.line);
        }

        /* != is EQ + NOT */
        if (op.type == TOK_BANG_EQ)
        {
            state_->emitter.emit_abc(OP_NOT, reg, reg, 0, op.line);
        }

        bool result_is_bool = op.type == TOK_EQ_EQ || op.type == TOK_BANG_EQ ||
                              op.type == TOK_LT || op.type == TOK_GT ||
                              op.type == TOK_LT_EQ || op.type == TOK_GT_EQ;
        last_call_class_def_ = (use_obj_op && !result_is_bool) ? (left_class ? left_class : right_class) : nullptr;
        if (reg >= 0 && reg < 256)
            state_->reg_class_hints[reg] = last_call_class_def_;

        if (right != reg)
            free_reg(right);
        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Infix handlers — Short-circuit: and / or
    ** ========================================================= */

    int Compiler::and_expr(int left, int dest)
    {
        int reg = dest >= 0 ? dest : left;
        if (reg != left)
            emit_move(reg, left);

        /* If falsy, short-circuit (jump over right side) */
        int jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, previous_.line);

        /* Parse right side into same register */
        int right = parse_precedence(PREC_AND, reg);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->emitter.patch_jump(jump);
        if (left != reg)
            free_reg(left);
        return reg;
    }

    int Compiler::or_expr(int left, int dest)
    {
        int reg = dest >= 0 ? dest : left;
        if (reg != left)
            emit_move(reg, left);

        /* If truthy, short-circuit */
        int jump = state_->emitter.emit_jump(OP_JMPIF, reg, previous_.line);

        /* Parse right side */
        int right = parse_precedence(PREC_OR, reg);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->emitter.patch_jump(jump);
        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Infix handlers — Function call: f(a, b, c)
    ** ========================================================= */

    int Compiler::call_expr(int func_reg, int dest)
    {
        /* If func_reg is a local variable register, copy to temp to avoid
           OP_CALL overwriting the local with the return value */
        int base;
        int save_next = state_->next_reg;

        /* Save struct def hint — argument expressions must not clobber it */
        ObjStructDef *saved_struct_def = last_call_struct_def_;

        if (is_local_reg(func_reg))
        {
            if (state_->next_reg <= func_reg)
                state_->next_reg = func_reg + 1;
            base = alloc_reg();
            emit_move(base, func_reg);
        }
        else
        {
            base = func_reg;
        }

        /* Arguments go in consecutive registers after base */
        int call_results = expected_results_ > 0 ? expected_results_ : 1;
        int saved_expected_results = expected_results_;
        expected_results_ = 1;

        int arg_count = 0;

        if (state_->next_reg <= base)
            state_->next_reg = base + 1;

        if (!check(TOK_RPAREN))
        {
            do
            {
                int arg_reg = alloc_reg();
                expression(arg_reg);
                /* Force next_reg back to arg_reg+1 so next argument is
                   consecutive. Sub-expressions (nested calls) may have
                   bumped next_reg to higher temp registers. */
                state_->next_reg = arg_reg + 1;
                arg_count++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* Emit CALL: R[base] = func, args are R[base+1]..R[base+arg_count] */
        /* Result goes back into R[base] (or dest) */
        int result_reg = dest >= 0 ? dest : base;
        expected_results_ = saved_expected_results;
        state_->emitter.emit_abc(OP_CALL, base, arg_count, call_results, previous_.line);

        /* Restore register allocation */
        state_->next_reg = save_next > base + 1 ? save_next : base + 1;

        /* Restore struct def hint (argument expressions may have clobbered it) */
        last_call_struct_def_ = saved_struct_def;

        if (result_reg != base)
        {
            emit_move(result_reg, base);
        }
        if (result_reg >= 0 && result_reg < 256)
            state_->reg_class_hints[result_reg] = last_call_class_def_;
        return result_reg;
    }

    int Compiler::generic_call_expr(int func_reg, int dest)
    {
        int base;
        int save_next = state_->next_reg;
        ObjStructDef *saved_struct_def = last_call_struct_def_;

        if (is_local_reg(func_reg))
        {
            if (state_->next_reg <= func_reg)
                state_->next_reg = func_reg + 1;
            base = alloc_reg();
            emit_move(base, func_reg);
        }
        else
        {
            base = func_reg;
        }

        if (state_->next_reg <= base)
            state_->next_reg = base + 1;

        int arg_count = 0;
        int type_reg = alloc_reg();
        if (type_reg != base + 1)
        {
            /* Keep call arguments consecutive even if the allocator had moved. */
            state_->next_reg = base + 1;
            type_reg = alloc_reg();
        }
        generic_type_arg(type_reg);
        arg_count++;

        consume(TOK_LPAREN, "Expected '(' after generic type.");
        if (!check(TOK_RPAREN))
        {
            do
            {
                int arg_reg = alloc_reg();
                expression(arg_reg);
                state_->next_reg = arg_reg + 1;
                arg_count++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        int result_reg = dest >= 0 ? dest : base;
        state_->emitter.emit_abc(OP_CALL, base, arg_count, 1, previous_.line);
        state_->next_reg = save_next > base + 1 ? save_next : base + 1;
        last_call_struct_def_ = saved_struct_def;

        if (result_reg != base)
            emit_move(result_reg, base);
        return result_reg;
    }

    /* =========================================================
    ** Infix handlers — Index: a[i]
    ** ========================================================= */

    int Compiler::index_expr(int obj_reg, int dest, bool canAssign)
    {
        int idx_reg = expression(-1);
        consume(TOK_RBRACKET, "Expected ']' after index.");

        /* Check for assignment: a[i] = expr or a[i] += expr */
        if (canAssign && (check(TOK_EQ) || check(TOK_PLUS_EQ) ||
                          check(TOK_MINUS_EQ) || check(TOK_STAR_EQ) ||
                          check(TOK_SLASH_EQ)))
        {
            TokenType assign_op = current_.type;
            advance();

            if (assign_op == TOK_EQ)
            {
                /* a[i] = expr */
                int val_reg = alloc_reg();
                expression(val_reg);
                state_->emitter.emit_abc(OP_SETINDEX, obj_reg, idx_reg, val_reg, previous_.line);
                free_reg(val_reg);
            }
            else
            {
                /* a[i] += expr — load, operate, store */
                int cur_reg = alloc_reg();
                state_->emitter.emit_abc(OP_GETINDEX, cur_reg, obj_reg, idx_reg, previous_.line);
                int rhs_reg = alloc_reg();
                expression(rhs_reg);
                OpCode op;
                switch (assign_op)
                {
                case TOK_PLUS_EQ:
                    op = OP_ADD;
                    break;
                case TOK_MINUS_EQ:
                    op = OP_SUB;
                    break;
                case TOK_STAR_EQ:
                    op = OP_MUL;
                    break;
                case TOK_SLASH_EQ:
                    op = OP_DIV;
                    break;
                default:
                    op = OP_ADD;
                    break;
                }
                state_->emitter.emit_abc(op, cur_reg, cur_reg, rhs_reg, previous_.line);
                free_reg(rhs_reg);
                state_->emitter.emit_abc(OP_SETINDEX, obj_reg, idx_reg, cur_reg, previous_.line);
                free_reg(cur_reg);
            }
            free_reg(idx_reg);
            if (obj_reg != dest)
                free_reg(obj_reg);
            return dest >= 0 ? dest : alloc_reg();
        }

        /* Not assignment — emit GETINDEX */
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_GETINDEX, reg, obj_reg, idx_reg, previous_.line);
        free_reg(idx_reg);
        if (obj_reg != reg)
            free_reg(obj_reg);
        return reg;
    }

    /* =========================================================
    ** Infix handlers — Field access: a.b
    ** ========================================================= */

    int Compiler::dot_expr(int obj_reg, int dest, bool canAssign)
    {
        /* After '.', any identifier or keyword is valid as a method/field name */
        if (current_.type == TOK_IDENTIFIER ||
            (current_.type >= TOK_VAR && current_.type <= TOK_CLOCK))
            advance();
        else
            error_at_current("Expected field/method name after '.'.");
        Token field_tok = previous_;
        int name_ki = state_->emitter.add_string_constant(field_tok.start, field_tok.length);

        /* Try to resolve struct field index at compile time */
        int field_idx = -1;
        Local *loc = find_local_by_reg(obj_reg);
        if (loc && loc->struct_type)
        {
            ObjStructDef *def = loc->struct_type;
            for (int i = 0; i < def->num_fields; i++)
            {
                if (def->field_names[i]->length == field_tok.length &&
                    memcmp(def->field_names[i]->chars, field_tok.start, field_tok.length) == 0)
                {
                    field_idx = i;
                    break;
                }
            }
        }
        /* Try to resolve class field index (self.x inside a method) */
        if (field_idx < 0 && current_class_fields_ && state_->is_method && obj_reg == 0)
        {
            for (int i = 0; i < current_class_fields_->count; i++)
            {
                int flen = (int)strlen(current_class_fields_->fields[i]);
                if (flen == field_tok.length &&
                    memcmp(current_class_fields_->fields[i], field_tok.start, field_tok.length) == 0)
                {
                    field_idx = i;
                    break;
                }
            }
        }

        /* Check for assignment: a.b = expr or a.b += expr */
        if (canAssign && (check(TOK_EQ) || check(TOK_PLUS_EQ) ||
                          check(TOK_MINUS_EQ) || check(TOK_STAR_EQ) ||
                          check(TOK_SLASH_EQ)))
        {
            TokenType assign_op = current_.type;
            advance();

            if (assign_op == TOK_EQ)
            {
                /* a.b = expr */
                int val_reg = alloc_reg();
                expression(val_reg);
                if (field_idx >= 0)
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj_reg, field_idx, val_reg, previous_.line);
                else
                    state_->emitter.emit_abc(OP_SETFIELD, obj_reg, name_ki, val_reg, previous_.line);
                free_reg(val_reg);
            }
            else
            {
                /* a.b += expr */
                int cur_reg = alloc_reg();
                if (field_idx >= 0)
                    state_->emitter.emit_abc(OP_GETFIELD_IDX, cur_reg, obj_reg, field_idx, previous_.line);
                else
                    state_->emitter.emit_abc(OP_GETFIELD, cur_reg, obj_reg, name_ki, previous_.line);
                int rhs_reg = alloc_reg();
                expression(rhs_reg);
                OpCode op;
                switch (assign_op)
                {
                case TOK_PLUS_EQ:
                    op = OP_ADD;
                    break;
                case TOK_MINUS_EQ:
                    op = OP_SUB;
                    break;
                case TOK_STAR_EQ:
                    op = OP_MUL;
                    break;
                case TOK_SLASH_EQ:
                    op = OP_DIV;
                    break;
                default:
                    op = OP_ADD;
                    break;
                }
                state_->emitter.emit_abc(op, cur_reg, cur_reg, rhs_reg, previous_.line);
                free_reg(rhs_reg);
                if (field_idx >= 0)
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj_reg, field_idx, cur_reg, previous_.line);
                else
                    state_->emitter.emit_abc(OP_SETFIELD, obj_reg, name_ki, cur_reg, previous_.line);
                free_reg(cur_reg);
            }
            if (obj_reg != dest)
                free_reg(obj_reg);
            return dest >= 0 ? dest : alloc_reg();
        }

        /* Not assignment — check for method call: a.b(args) or a.b<T>(args) */
        bool has_generic_arg = check(TOK_LT) && looks_like_generic_call();
        if (has_generic_arg || check(TOK_LPAREN))
        {
            /* Method invocation — emit OP_INVOKE (2-word) */

            /* Put receiver at base register */
            int base = alloc_reg();
            emit_move(base, obj_reg);
            if (obj_reg != base)
                free_reg(obj_reg);

            /* Parse arguments into consecutive registers after base */
            int arg_count = 0;
            int save_next = state_->next_reg;
            if (state_->next_reg <= base)
                state_->next_reg = base + 1;

            if (has_generic_arg)
            {
                int type_reg = alloc_reg();
                if (type_reg != base + 1)
                {
                    state_->next_reg = base + 1;
                    type_reg = alloc_reg();
                }
                generic_type_arg(type_reg);
                arg_count++;
            }

            consume(TOK_LPAREN, "Expected '(' after generic type.");
            if (!check(TOK_RPAREN))
            {
                do
                {
                    int arg_reg = alloc_reg();
                    expression(arg_reg);
                    state_->next_reg = arg_reg + 1;
                    arg_count++;
                } while (match(TOK_COMMA));
            }
            consume(TOK_RPAREN, "Expected ')' after arguments.");

            /* Try vtable dispatch if we know the class at compile time */
            ObjClass *known_class = nullptr;
            if (loc && loc->class_type)
                known_class = loc->class_type;
            else if (current_class_fields_ && state_->is_method && obj_reg == 0)
            {
                /* self inside a method — find the class being compiled */
                /* We look up the class by the method's enclosing class name */
                /* For now, use the general approach: check if current_class_fields_ is set */
                /* We need the actual ObjClass*. It's built AFTER methods, so we can't
                   use it here. For self.method() inside methods, fall back to OP_INVOKE
                   for now. The vtable win is for external calls (var c = C(); c.tick()). */
            }

            if (known_class)
            {
                int slot = vm_->find_selector(field_tok.start, field_tok.length);
                if (slot >= 0 && slot < known_class->vtable_size &&
                    !is_nil(known_class->vtable[slot]))
                {
                    /* Emit single-word OP_INVOKE_VT */
                    state_->emitter.emit_abc(OP_INVOKE_VT, base, arg_count, slot, previous_.line);
                }
                else
                {
                    /* Fallback: method not in vtable (maybe added later) */
                    state_->emitter.emit_abc(OP_INVOKE, base, arg_count, 0, previous_.line);
                    state_->emitter.emit((uint32_t)name_ki, previous_.line);
                }
            }
            else
            {
                /* No type info — emit 2-word OP_INVOKE */
                state_->emitter.emit_abc(OP_INVOKE, base, arg_count, 0, previous_.line);
                state_->emitter.emit((uint32_t)name_ki, previous_.line);
            }

            /* Result is in R[base] */
            state_->next_reg = save_next > base + 1 ? save_next : base + 1;
            int result_reg = dest >= 0 ? dest : base;
            if (result_reg != base)
                emit_move(result_reg, base);
            return result_reg;
        }

        /* Not assignment, not call — emit GETFIELD or GETFIELD_IDX */
        int reg = dest >= 0 ? dest : alloc_reg();
        if (field_idx >= 0)
            state_->emitter.emit_abc(OP_GETFIELD_IDX, reg, obj_reg, field_idx, previous_.line);
        else
            state_->emitter.emit_abc(OP_GETFIELD, reg, obj_reg, name_ki, previous_.line);
        if (obj_reg != reg)
            free_reg(obj_reg);
        return reg;
    }

    /* =========================================================
    ** Math builtins — keyword-level, compiled to opcodes
    ** ========================================================= */

    int Compiler::math_builtin(Token token, int dest)
    {
        int reg = dest >= 0 ? dest : alloc_reg();

        /* clock() takes no args */
        if (token.type == TOK_CLOCK)
        {
            consume(TOK_LPAREN, "Expected '(' after 'clock'.");
            consume(TOK_RPAREN, "Expected ')' after 'clock('.");
            state_->emitter.emit_abc(OP_CLOCK, reg, 0, 0, token.line);
            return reg;
        }

        /* Two-arg builtins: atan2(y, x), pow(base, exp) */
        if (token.type == TOK_ATAN2 || token.type == TOK_POW)
        {
            consume(TOK_LPAREN, "Expected '(' after builtin.");
            int arg1 = alloc_reg();
            expression(arg1);
            consume(TOK_COMMA, "Expected ',' between arguments.");
            int arg2 = alloc_reg();
            expression(arg2);
            consume(TOK_RPAREN, "Expected ')' after arguments.");

            OpCode op = (token.type == TOK_ATAN2) ? OP_ATAN2 : OP_POW;
            state_->emitter.emit_abc(op, reg, arg1, arg2, token.line);
            free_reg(arg2);
            free_reg(arg1);
            return reg;
        }

        /* Single-arg builtins: sin(x), cos(x), etc. */
        consume(TOK_LPAREN, "Expected '(' after builtin.");
        int arg = alloc_reg();
        expression(arg);
        consume(TOK_RPAREN, "Expected ')' after argument.");

        OpCode op;
        switch (token.type)
        {
        case TOK_SIN:
            op = OP_SIN;
            break;
        case TOK_COS:
            op = OP_COS;
            break;
        case TOK_TAN:
            op = OP_TAN;
            break;
        case TOK_ASIN:
            op = OP_ASIN;
            break;
        case TOK_ACOS:
            op = OP_ACOS;
            break;
        case TOK_ATAN:
            op = OP_ATAN;
            break;
        case TOK_SQRT:
            op = OP_SQRT;
            break;
        case TOK_LOG:
            op = OP_LOG;
            break;
        case TOK_ABS:
            op = OP_ABS;
            break;
        case TOK_FLOOR:
            op = OP_FLOOR;
            break;
        case TOK_CEIL:
            op = OP_CEIL;
            break;
        case TOK_DEG:
            op = OP_DEG;
            break;
        case TOK_RAD:
            op = OP_RAD;
            break;
        case TOK_EXP:
            op = OP_EXP;
            break;
        case TOK_LEN:
            op = OP_LEN;
            break;
        default:
            op = OP_SIN;
            break; /* unreachable */
        }
        state_->emitter.emit_abc(op, reg, arg, 0, token.line);
        free_reg(arg);
        return reg;
    }

    /* =========================================================
    ** spawn expr  → OP_NEWFIBER  R[A] = Fiber.new(R[B])
    ** ========================================================= */
    /* =========================================================
    ** father.field / son.field → OP_PROC_GET / OP_PROC_SET
    ** Resolves field name as a parameter register index in current scope.
    ** ========================================================= */
    int Compiler::process_field_expr(Token token, int dest)
    {
        int mode = (token.type == TOK_FATHER) ? 1 : 2;

        consume(TOK_DOT, (mode == 1) ? "Expected '.' after 'father'."
                                      : "Expected '.' after 'son'.");
        consume(TOK_IDENTIFIER, "Expected field name.");

        /* Resolve field name as a process private */
        Token field_name = previous_;
        int field_idx = VM::resolve_private(field_name.start, field_name.length);
        if (field_idx < 0)
        {
            error("Unknown process private field.");
            return dest >= 0 ? dest : alloc_reg();
        }

        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Check for assignment: father.x = expr */
        if (match(TOK_EQ))
        {
            int val_reg = expression(reg);
            if (val_reg != reg)
                state_->emitter.emit_abc(OP_MOVE, reg, val_reg, 0, previous_.line);
            state_->emitter.emit_abc(OP_PROC_SET, reg, mode, field_idx, previous_.line);
            return reg;
        }

        /* Read: father.x */
        state_->emitter.emit_abc(OP_PROC_GET, reg, mode, field_idx, previous_.line);
        return reg;
    }

    int Compiler::spawn_expression(int dest)
    {
        /* spawn expr
        ** Compiles expr to a closure and wraps it in a Fiber.
        ** Process declarations do not use this path: calling a process
        ** closure already spawns a process in OP_CALL. */
        int reg = (dest >= 0) ? dest : alloc_reg();

        int closure_reg = expression(reg);
        if (closure_reg != reg)
        {
            emit_move(reg, closure_reg);
            free_reg(closure_reg);
        }
        state_->emitter.emit_abc(OP_NEWFIBER, reg, reg, 0, previous_.line);

        return reg;
    }

    /* =========================================================
    ** resume(fiber, val)  → OP_RESUME  R[A] = R[B].resume(R[C])
    ** resume(fiber)       → resume with nil
    ** ========================================================= */
    int Compiler::resume_expression(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        consume(TOK_LPAREN, "Expected '(' after 'resume'.");
        int fiber_reg = expression(-1);
        int val_reg;
        if (match(TOK_COMMA))
        {
            val_reg = expression(-1);
        }
        else
        {
            val_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LOADNIL, val_reg, 0, 0, previous_.line);
        }
        consume(TOK_RPAREN, "Expected ')' after resume arguments.");
        state_->emitter.emit_abc(OP_RESUME, reg, fiber_reg, val_reg, previous_.line);
        free_reg(val_reg);
        free_reg(fiber_reg);
        return reg;
    }

    /* =========================================================
    ** yield expr  → OP_YIELD A, B
    **   Yields R[B] to caller. On resume, R[A] = value sent by caller.
    **   yield;  → yield nil
    ** ========================================================= */
    int Compiler::yield_expression(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        int val_reg;
        /* Check if there's an expression to yield or just semicolon */
        if (check(TOK_SEMICOLON) || check(TOK_EOF))
        {
            val_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LOADNIL, val_reg, 0, 0, previous_.line);
        }
        else
        {
            val_reg = expression(-1);
        }
        state_->emitter.emit_abc(OP_YIELD, reg, val_reg, 0, previous_.line);
        if (val_reg != reg)
            free_reg(val_reg);
        return reg;
    }

    /* =========================================================
    ** def(params) { body } — anonymous function expression
    ** ========================================================= */
    int Compiler::anonymous_function(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        CompilerState fn_state;
        fn_state.parent = state_;
        fn_state.function = new_func(gc_);
        fn_state.emitter = Emitter(gc_);
        fn_state.local_count = 0;
        memset(fn_state.reg_class_hints, 0, sizeof(fn_state.reg_class_hints));
        fn_state.scope_depth = 0;
        fn_state.next_reg = 0;
        fn_state.max_reg = 0;
        fn_state.upvalue_count = 0;
        fn_state.loop_depth = 0;
        fn_state.is_method = false;
        fn_state.is_process = false;

        fn_state.emitter.begin("<anon>", 0, "<anon>");

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters */
        consume(TOK_LPAREN, "Expected '(' after 'def'.");
        int arity = 0;
        if (!check(TOK_RPAREN))
        {
            do
            {
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

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        fn->arity = arity;

        /* Copy upvalue descriptors */
        int nuv = state_->upvalue_count;
        fn->upvalue_count = nuv;
        if (nuv > 0)
        {
            fn->upval_descs = (UpvalDesc *)zen_alloc(gc_, nuv * sizeof(UpvalDesc));
            for (int i = 0; i < nuv; i++)
            {
                fn->upval_descs[i].index = (uint8_t)state_->upvalues[i].index;
                fn->upval_descs[i].is_local = state_->upvalues[i].is_local ? 1 : 0;
            }
        }

        state_ = enclosing;

        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));
        state_->emitter.emit_abx(OP_CLOSURE, reg, ki, previous_.line);
        return reg;
    }

} /* namespace zen */
