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
            return true;
        default:
            return false;
        }
    }

    /* =========================================================
    ** Expression entry point
    ** ========================================================= */

    int Compiler::expression(int dest)
    {
        return parse_precedence(PREC_ASSIGNMENT, dest);
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
        while (prec <= get_precedence(current_.type))
        {
            Token op = current_;
            advance();
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
            long val;
            if (token.length > 2 && token.start[0] == '0' && (token.start[1] == 'x' || token.start[1] == 'X'))
            {
                val = strtol(token.start, nullptr, 16);
            }
            else
            {
                val = strtol(token.start, nullptr, 10);
            }
            if (val >= -32768 && val <= 32767)
            {
                state_->emitter.emit_asbx(OP_LOADI, reg, (int)val, token.line);
            }
            else
            {
                int ki = state_->emitter.add_constant(val_int((int32_t)val));
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
        int ki = state_->emitter.add_escaped_string_constant(token.start + 1, token.length - 2);
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
        state_->emitter.emit_abc(OP_TOSTRING, tmp, tmp, 0, token.line);
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
            state_->emitter.emit_abc(OP_TOSTRING, tmp, tmp, 0, token.line);
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
            gidx = vm_->find_global(name_buf);
            if (gidx < 0)
                gidx = vm_->def_global(name_buf, val_nil());
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
                    expression(local_reg);
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
                }
                free_reg(val_reg);
                return dest >= 0 ? dest : alloc_reg();
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
                state_->emitter.emit_abc(op, cur_reg, cur_reg, rhs_reg, token.line);
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
            state_->emitter.emit_abc(OP_NEG, reg, operand, 0, token.line);
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
                /* SETINDEX arr, count, val — but we use a push approach */
                /* For simplicity: emit SETINDEX with incrementing index */
                /* Actually, let's just emit sequential SETINDEX calls */
                /* Better: we don't have OP_PUSH. Use SETINDEX with LOADI for index */
                /* Simplest approach for now: build via temp registers */
                state_->emitter.emit_abc(OP_SETINDEX, reg, reg, val_reg, previous_.line);
                /* TODO: proper array literal with SETINDEX idx semantics */
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
    ** Infix handlers — Binary operators
    ** ========================================================= */

    int Compiler::binary(Token op, int left, int dest)
    {
        int reg = dest >= 0 ? dest : left;

        /* Parse right operand at one higher precedence (left-associative) */
        Precedence prec = (Precedence)(get_precedence(op.type) + 1);
        int right = parse_precedence(prec, -1);

        OpCode opcode;
        switch (op.type)
        {
        case TOK_PLUS:
            opcode = OP_ADD;
            break;
        case TOK_MINUS:
            opcode = OP_SUB;
            break;
        case TOK_STAR:
            opcode = OP_MUL;
            break;
        case TOK_SLASH:
            opcode = OP_DIV;
            break;
        case TOK_PERCENT:
            opcode = OP_MOD;
            break;
        case TOK_EQ_EQ:
            opcode = OP_EQ;
            break;
        case TOK_BANG_EQ:
            opcode = OP_EQ;
            break; /* EQ + NOT */
        case TOK_LT:
            opcode = OP_LT;
            break;
        case TOK_GT:
            opcode = OP_LT;
            break; /* swap operands */
        case TOK_LT_EQ:
            opcode = OP_LE;
            break;
        case TOK_GT_EQ:
            opcode = OP_LE;
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
        /* Arguments go in consecutive registers after func_reg */
        int base = func_reg;
        int arg_count = 0;

        /* Ensure args are in registers base+1, base+2, ... */
        int save_next = state_->next_reg;
        if (state_->next_reg <= base)
            state_->next_reg = base + 1;

        if (!check(TOK_RPAREN))
        {
            do
            {
                int arg_reg = alloc_reg();
                expression(arg_reg);
                arg_count++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* Emit CALL: R[base] = func, args are R[base+1]..R[base+arg_count] */
        /* Result goes back into R[base] (or dest) */
        int result_reg = dest >= 0 ? dest : base;
        state_->emitter.emit_abc(OP_CALL, base, arg_count, 1, previous_.line);

        /* Restore register allocation */
        state_->next_reg = save_next > base + 1 ? save_next : base + 1;

        if (result_reg != base)
        {
            emit_move(result_reg, base);
        }
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
        consume(TOK_IDENTIFIER, "Expected field name after '.'.");
        int name_ki = state_->emitter.add_string_constant(previous_.start, previous_.length);

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
                state_->emitter.emit_abc(OP_SETFIELD, obj_reg, name_ki, val_reg, previous_.line);
                free_reg(val_reg);
            }
            else
            {
                /* a.b += expr */
                int cur_reg = alloc_reg();
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
                state_->emitter.emit_abc(OP_SETFIELD, obj_reg, name_ki, cur_reg, previous_.line);
                free_reg(cur_reg);
            }
            if (obj_reg != dest)
                free_reg(obj_reg);
            return dest >= 0 ? dest : alloc_reg();
        }

        /* Not assignment — emit GETFIELD */
        int reg = dest >= 0 ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_GETFIELD, reg, obj_reg, name_ki, previous_.line);
        if (obj_reg != reg)
            free_reg(obj_reg);
        return reg;
    }

} /* namespace zen */
