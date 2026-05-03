/*
** compiler_statements.cpp — Statement and declaration parsing.
** Handles: var, def, class, struct, if/elif/else, while, for,
** foreach, loop, do..while, switch, return, break, continue, frame, block.
*/

#include "compiler.h"
#include "vm.h"

namespace zen
{
    /* =========================================================
    ** Top-level dispatcher
    ** ========================================================= */

    void Compiler::declaration()
    {
        if (panic_mode_)
        {
            /* Synchronize: skip tokens until a statement boundary */
            panic_mode_ = false;
            while (!check(TOK_EOF))
            {
                if (previous_.type == TOK_SEMICOLON)
                    return;
                switch (current_.type)
                {
                case TOK_VAR:
                case TOK_DEF:
                case TOK_CLASS:
                case TOK_STRUCT:
                case TOK_IF:
                case TOK_WHILE:
                case TOK_FOR:
                case TOK_FOREACH:
                case TOK_LOOP:
                case TOK_DO:
                case TOK_RETURN:
                case TOK_IMPORT:
                case TOK_INCLUDE:
                    return;
                default:
                    advance();
                }
            }
            return;
        }

        if (check(TOK_VAR))
        {
            advance();
            var_declaration();
        }
        else if (check(TOK_DEF))
        {
            advance();
            fun_declaration();
        }
        else if (check(TOK_PROCESS))
        {
            advance();
            process_declaration();
        }
        else if (check(TOK_CLASS))
        {
            advance();
            class_declaration();
        }
        else if (check(TOK_STRUCT))
        {
            advance();
            struct_declaration();
        }
        else if (check(TOK_IMPORT))
        {
            advance();
            import_statement();
        }
        else if (check(TOK_USING))
        {
            advance();
            using_statement();
        }
        else if (check(TOK_INCLUDE))
        {
            advance();
            include_statement();
        }
        else
        {
            statement();
        }
    }

    /* =========================================================
    ** var declaration
    ** var x = expr;
    ** var (a, b, _) = expr;
    ** ========================================================= */

    void Compiler::var_declaration()
    {
        /* Multi-return: var (a, b, _) = expr; */
        if (match(TOK_LPAREN))
        {
            /* Collect names */
            Token names[16];
            int name_count = 0;
            do
            {
                if (check(TOK_UNDERSCORE))
                {
                    names[name_count].type = TOK_UNDERSCORE;
                    names[name_count].start = "_";
                    names[name_count].length = 1;
                    names[name_count].line = current_.line;
                    advance();
                }
                else
                {
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
            expression_results(base, name_count);
            if (state_->next_reg < base + name_count)
                state_->next_reg = base + name_count;
            if (state_->max_reg < state_->next_reg)
                state_->max_reg = state_->next_reg;

            /* Assign to locals */
            for (int i = 0; i < name_count; i++)
            {
                if (names[i].type == TOK_UNDERSCORE)
                    continue; /* discard */
                if (state_->scope_depth > 0)
                {
                    /* Local */
                    int reg = add_local(names[i]);
                    emit_move(reg, base + i);
                }
                else
                {
                    /* Global */
                    char buf[256];
                    int len = names[i].length < 255 ? names[i].length : 255;
                    memcpy(buf, names[i].start, len);
                    buf[len] = '\0';
                    int gidx = require_global_slot(buf, &names[i]);
                    if (gidx < 0)
                        continue;
                    state_->emitter.emit_abx(OP_SETGLOBAL, base + i, gidx, names[i].line);
                    if (ensure_global_class_hint(gidx))
                        global_class_hints_[gidx] = state_->reg_class_hints[base + i];
                }
            }
            consume(TOK_SEMICOLON, "Expected ';' after declaration.");
            return;
        }

        /* Simple: var x = expr; or var x; */
        consume(TOK_IDENTIFIER, "Expected variable name.");
        Token name = previous_;

        if (state_->scope_depth > 0)
        {
            /* Local variable — like Lua, the variable only becomes visible
               AFTER the RHS is compiled (so `var x = x` captures outer x). */
            int reg = alloc_reg();
            last_call_struct_def_ = nullptr;
            if (match(TOK_EQ))
            {
                expression(reg);
            }
            else
            {
                state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, name.line);
            }
            /* Now register it as a local (becomes visible to subsequent code) */
            declare_local(name);
            Local &local = state_->locals[state_->local_count++];
            local.name = name;
            local.depth = state_->scope_depth;
            local.reg = reg;
            local.captured = false;
            local.struct_type = last_call_struct_def_;
            local.class_type = last_call_class_def_ ? last_call_class_def_ : state_->reg_class_hints[reg];
            state_->reg_class_hints[reg] = local.class_type;
            last_call_struct_def_ = nullptr;
            last_call_class_def_ = nullptr;
        }
        else
        {
            /* Global variable */
            char buf[256];
            int len = name.length < 255 ? name.length : 255;
            memcpy(buf, name.start, len);
            buf[len] = '\0';
            int gidx = require_global_slot(buf, &name);

            if (match(TOK_EQ))
            {
                int reg = alloc_reg();
                expression(reg);
                if (gidx >= 0)
                {
                    state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
                    if (ensure_global_class_hint(gidx))
                        global_class_hints_[gidx] = last_call_class_def_ ? last_call_class_def_ : state_->reg_class_hints[reg];
                }
                free_reg(reg);
                last_call_struct_def_ = nullptr;
                last_call_class_def_ = nullptr;
            }
            else
            {
                /* nil by default — already set */
                if (gidx >= 0 && ensure_global_class_hint(gidx))
                    global_class_hints_[gidx] = nullptr;
            }
        }

        consume(TOK_SEMICOLON, "Expected ';' after variable declaration.");
    }

    /* =========================================================
    ** def — function declaration
    ** def name(params) { body }
    ** ========================================================= */

    void Compiler::fun_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected function name.");
        Token name = previous_;

        /* Create function in a new compiler state */
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

        char name_buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(name_buf, name.start, len);
        name_buf[len] = '\0';

        fn_state.emitter.begin(name_buf, 0, name_buf);

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters */
        consume(TOK_LPAREN, "Expected '(' after function name.");
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

        /* Copy upvalue descriptors to ObjFunc */
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

        /* Restore enclosing state */
        state_ = enclosing;

        /* Store function as constant and load it */
        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));

        if (state_->scope_depth > 0)
        {
            /* Local function */
            int reg = add_local(name);
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
        }
        else
        {
            /* Global function */
            int gidx = require_global_slot(name_buf, &name);
            if (gidx >= 0)
            {
                int reg = alloc_reg();
                state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
                state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
                free_reg(reg);
            }
        }
    }

    /* =========================================================
    ** process declaration (DIV Games Studio style)
    **
    ** process player(x, y) {
    **     // body — runs once per tick, auto-yields via FRAME
    **     x = x + vx;
    ** }
    **
    ** Compiles to a function whose body is wrapped in:
    **   LOOP_START:
    **     <user body>
    **     FRAME;           (implicit yield — one tick boundary)
    **     JMP LOOP_START;
    **
    ** The process is stored as a global closure.
    ** You spawn it via: spawn player(100, 200);
    ** ========================================================= */

    void Compiler::process_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected process name.");
        Token name = previous_;

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
        fn_state.is_process = true;

        char name_buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(name_buf, name.start, len);
        name_buf[len] = '\0';

        fn_state.emitter.begin(name_buf, 0, name_buf);

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters — if name matches a private, record the mapping.
        ** Non-private params become normal locals. */
        consume(TOK_LPAREN, "Expected '(' after process name.");
        int arity = 0;
        int8_t priv_map[16];
        memset(priv_map, -1, sizeof(priv_map));
        if (!check(TOK_RPAREN))
        {
            do
            {
                consume(TOK_IDENTIFIER, "Expected parameter name.");
                int pidx = VM::resolve_private(previous_.start, previous_.length);
                if (arity < 16) priv_map[arity] = (int8_t)pidx;
                add_local(previous_);
                arity++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after parameters.");

        /* Body — user writes their own loop { ... frame; } */
        consume(TOK_LBRACE, "Expected '{' before process body.");
        block();

        /* Implicit return nil */
        state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        fn->arity = arity;
        fn->is_process = 1;
        memcpy(fn->param_privates, priv_map, sizeof(fn->param_privates));

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

        /* Store as global (like fun_declaration) */
        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));

        if (state_->scope_depth > 0)
        {
            int reg = add_local(name);
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
        }
        else
        {
            int gidx = require_global_slot(name_buf, &name);
            if (gidx >= 0)
            {
                int reg = alloc_reg();
                state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);
                state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
                free_reg(reg);
            }
        }
    }

    /* =========================================================
    ** class declaration
    ** class Name { var field; def method() {} }
    ** class Child : Parent { ... }
    **
    ** Compiles to:
    **   1. OP_NEWCLASS → creates ObjClass, stores as global
    **   2. For each method: compile as closure, store in class.methods
    **
    ** At runtime, ClassName(args) → new_instance + call init
    ** ========================================================= */

    void Compiler::class_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected class name.");
        Token name_tok = previous_;

        char name_buf[128];
        int nlen = name_tok.length < 127 ? name_tok.length : 127;
        memcpy(name_buf, name_tok.start, nlen);
        name_buf[nlen] = '\0';

        /* Check for inheritance: class Child : Parent */
        char parent_buf[128];
        parent_buf[0] = '\0';
        if (match(TOK_COLON))
        {
            consume(TOK_IDENTIFIER, "Expected parent class name after ':'.");
            int plen = previous_.length < 127 ? previous_.length : 127;
            memcpy(parent_buf, previous_.start, plen);
            parent_buf[plen] = '\0';
        }

        consume(TOK_LBRACE, "Expected '{' before class body.");

        /* Collect fields and methods */
        char fields[64][64];
        int field_count = 0;

        /* Build ClassFieldInfo for compile-time field indexing in methods.
         * Parent fields come first, then local fields are added as parsed. */
        ClassFieldInfo cfi;
        cfi.count = 0;
        cfi.parent_field_count = 0;

        /* Resolve parent class early for field layout */
        ObjClass *parent_class = nullptr;
        if (parent_buf[0] != '\0')
        {
            int pidx = vm_->find_global(parent_buf);
            if (pidx < 0 || !is_class(vm_->get_global(pidx)))
            {
                error("Parent class not found.");
                return;
            }
            parent_class = as_class(vm_->get_global(pidx));
            /* Copy parent field names into cfi */
            for (int i = 0; i < parent_class->num_fields; i++)
            {
                int flen = parent_class->field_names[i]->length;
                if (flen > 63) flen = 63;
                memcpy(cfi.fields[cfi.count], parent_class->field_names[i]->chars, flen);
                cfi.fields[cfi.count][flen] = '\0';
                cfi.count++;
            }
            cfi.parent_field_count = cfi.count;
        }

        /* We'll store compiled methods temporarily */
        struct MethodInfo
        {
            char name[64];
            ObjFunc *func; /* compiled function */
        };
        MethodInfo *methods = nullptr;
        int method_count = 0;
        int method_capacity = 0;
        bool has_init_method = false;

        while (!check(TOK_RBRACE) && !check(TOK_EOF))
        {
            if (match(TOK_VAR))
            {
                /* Field declaration: var name; or var a, b, c; */
                do
                {
                    consume(TOK_IDENTIFIER, "Expected field name.");
                    int flen = previous_.length < 63 ? previous_.length : 63;
                    memcpy(fields[field_count], previous_.start, flen);
                    fields[field_count][flen] = '\0';
                    field_count++;
                    /* Also add to cfi for indexing */
                    memcpy(cfi.fields[cfi.count], previous_.start, flen);
                    cfi.fields[cfi.count][flen] = '\0';
                    cfi.count++;
                } while (match(TOK_COMMA));
                consume(TOK_SEMICOLON, "Expected ';' after field declaration.");
            }
            else if (match(TOK_DEF))
            {
                /* Method: def name(params) { body } */
                consume(TOK_IDENTIFIER, "Expected method name.");
                Token method_name = previous_;

                if (method_count >= method_capacity)
                {
                    int new_capacity = method_capacity == 0 ? 16 : method_capacity * 2;
                    MethodInfo *grown = (MethodInfo *)realloc(methods, sizeof(MethodInfo) * new_capacity);
                    if (!grown)
                    {
                        free(methods);
                        error("Out of memory while collecting class methods.");
                        return;
                    }
                    methods = grown;
                    method_capacity = new_capacity;
                }

                int mnlen = method_name.length < 63 ? method_name.length : 63;
                memcpy(methods[method_count].name, method_name.start, mnlen);
                methods[method_count].name[mnlen] = '\0';
                if (mnlen == 4 && memcmp(methods[method_count].name, "init", 4) == 0)
                    has_init_method = true;

                /* Compile method as a function with implicit 'self' at reg 0 */
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
                fn_state.is_method = true;
                fn_state.is_process = false;

                char fn_name[192];
                snprintf(fn_name, sizeof(fn_name), "%s.%s", name_buf, methods[method_count].name);
                fn_state.emitter.begin(fn_name, 0, fn_name);

                CompilerState *enclosing = state_;
                state_ = &fn_state;

                /* Enable field indexing for self.x inside methods */
                ClassFieldInfo *prev_cfi = current_class_fields_;
                current_class_fields_ = &cfi;

                begin_scope();

                /* Register 0 = self (implicit first parameter) */
                Token self_tok;
                self_tok.start = "self";
                self_tok.length = 4;
                self_tok.type = TOK_SELF;
                self_tok.line = method_name.line;
                add_local(self_tok);

                /* Parse explicit parameters */
                consume(TOK_LPAREN, "Expected '(' after method name.");
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
                consume(TOK_LBRACE, "Expected '{' before method body.");
                block();

                /* Implicit return nil (or self for init) */
                bool is_init = (mnlen == 4 && memcmp(methods[method_count].name, "init", 4) == 0);
                if (is_init)
                {
                    /* init returns self */
                    state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
                }
                else
                {
                    state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
                    state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
                }

                ObjFunc *fn = state_->emitter.end(state_->max_reg);
                fn->arity = arity; /* does NOT count self — VM adds it implicitly */

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
                current_class_fields_ = prev_cfi;

                /* Store function pointer directly */
                methods[method_count].func = fn;
                method_count++;
            }
            else
            {
                error("Expected 'var' or 'def' in class body.");
                advance();
            }
        }
        consume(TOK_RBRACE, "Expected '}' after class body.");
        if (field_count > 0 && !has_init_method)
        {
            error("Class with fields must define init().");
        }

        /* Build ObjClass now (compile-time, like structs) */
        ObjString *cls_name = intern_string(gc_, name_buf, nlen,
                                            hash_string(name_buf, nlen));

        /* Hot reload: reuse existing class if already defined (patch in-place) */
        ObjClass *klass = nullptr;
        int existing_idx = vm_->find_global(name_buf);
        if (existing_idx >= 0 && is_class(vm_->get_global(existing_idx)))
        {
            klass = as_class(vm_->get_global(existing_idx));
            /* Clear vtable for rebuild — methods will be re-registered below */
            for (int vi = 0; vi < klass->vtable_size; vi++)
                klass->vtable[vi] = val_nil();
            for (int oi = 0; oi < VM::SLOT_OPERATOR_COUNT; oi++)
                klass->operator_slots[oi] = val_nil();
            /* Clear methods map */
            if (klass->methods)
                map_clear(gc_, klass->methods);
        }
        else
        {
            klass = new_class(gc_, cls_name, parent_class);
        }

        /* Set up field_names — parent fields first, then local */
        int parent_fields = parent_class ? parent_class->num_fields : 0;
        int total_fields = parent_fields + field_count;
        /* Free old field_names on hot reload */
        if (klass->field_names && klass->num_fields > 0)
            zen_free(gc_, klass->field_names, sizeof(ObjString *) * klass->num_fields);
        klass->num_fields = total_fields;
        klass->field_names = nullptr;
        if (total_fields > 0)
        {
            klass->field_names = (ObjString **)zen_alloc(gc_, sizeof(ObjString *) * total_fields);
            /* Copy parent fields first */
            for (int i = 0; i < parent_fields; i++)
                klass->field_names[i] = parent_class->field_names[i];
            /* Then local fields */
            for (int i = 0; i < field_count; i++)
            {
                int flen = (int)strlen(fields[i]);
                klass->field_names[parent_fields + i] = intern_string(gc_, fields[i], flen,
                                                                      hash_string(fields[i], flen));
            }
        }

        /* Store methods — compile closures and put in klass->methods */
        for (int m = 0; m < method_count; m++)
        {
            ObjFunc *fn = methods[m].func;
            /* Create closure for the method (no upvalues typically) */
            ObjClosure *cl = (ObjClosure *)zen_alloc(gc_, sizeof(ObjClosure));
            cl->obj.type = OBJ_CLOSURE;
            cl->obj.color = GC_BLACK;
            cl->obj.interned = 0;
            cl->obj.hash = 0;
            cl->obj._pad = 0;
            cl->obj.gc_next = gc_->objects;
            gc_->objects = (Obj *)cl;
            cl->func = fn;
            cl->upvalue_count = 0;
            cl->upvalues = nullptr;

            int mnlen2 = (int)strlen(methods[m].name);
            ObjString *mname = intern_string(gc_, methods[m].name, mnlen2,
                                             hash_string(methods[m].name, mnlen2));
            map_set(gc_, klass->methods, val_obj((Obj *)mname), val_obj((Obj *)cl));

            int op_slot = mname->operator_slot_id;
            if (op_slot >= 0)
            {
                klass->operator_slots[op_slot] = val_obj((Obj *)cl);
            }
            else
            {
                /* Register method in vtable */
                int slot = vm_->intern_selector(methods[m].name, mnlen2);
                if (slot >= klass->vtable_size)
                {
                    /* Grow vtable to accommodate new slot */
                    int new_size = slot + 1;
                    Value *new_vt = (Value *)zen_alloc(gc_, sizeof(Value) * new_size);
                    for (int vi = 0; vi < klass->vtable_size; vi++)
                        new_vt[vi] = klass->vtable[vi];
                    for (int vi = klass->vtable_size; vi < new_size; vi++)
                        new_vt[vi] = val_nil();
                    if (klass->vtable)
                        zen_free(gc_, klass->vtable, sizeof(Value) * klass->vtable_size);
                    klass->vtable = new_vt;
                    klass->vtable_size = new_size;
                }
                klass->vtable[slot] = val_obj((Obj *)cl);
            }
        }

        /* Flatten parent vtable: copy slots not overridden */
        if (parent_class && parent_class->vtable_size > 0)
        {
            /* Ensure child vtable is at least as big as parent's */
            if (klass->vtable_size < parent_class->vtable_size)
            {
                int new_size = parent_class->vtable_size;
                Value *new_vt = (Value *)zen_alloc(gc_, sizeof(Value) * new_size);
                for (int vi = 0; vi < klass->vtable_size; vi++)
                    new_vt[vi] = klass->vtable[vi];
                for (int vi = klass->vtable_size; vi < new_size; vi++)
                    new_vt[vi] = val_nil();
                if (klass->vtable)
                    zen_free(gc_, klass->vtable, sizeof(Value) * klass->vtable_size);
                klass->vtable = new_vt;
                klass->vtable_size = new_size;
            }
            /* Copy parent slots where child has nil */
            for (int vi = 0; vi < parent_class->vtable_size; vi++)
            {
                if (is_nil(klass->vtable[vi]))
                    klass->vtable[vi] = parent_class->vtable[vi];
            }
        }
        if (parent_class)
        {
            for (int oi = 0; oi < VM::SLOT_OPERATOR_COUNT; oi++)
            {
                if (is_nil(klass->operator_slots[oi]))
                    klass->operator_slots[oi] = parent_class->operator_slots[oi];
            }
        }

        /* Register class as global */
        int gidx = require_global_slot(name_buf, &name_tok);
        if (gidx >= 0)
            vm_->set_global(gidx, val_obj((Obj *)klass));

        free(methods);
    }

    /* =========================================================
    ** struct declaration (stub)
    ** ========================================================= */

    void Compiler::struct_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected struct name.");
        Token name_tok = previous_;

        char name_buf[128];
        int nlen = name_tok.length < 127 ? name_tok.length : 127;
        memcpy(name_buf, name_tok.start, nlen);
        name_buf[nlen] = '\0';

        consume(TOK_LBRACE, "Expected '{' before struct body.");

        /* Collect field names */
        char fields[64][64];
        int field_count = 0;

        while (!check(TOK_RBRACE) && !check(TOK_EOF))
        {
            consume(TOK_IDENTIFIER, "Expected field name.");
            int flen = previous_.length < 63 ? previous_.length : 63;
            memcpy(fields[field_count], previous_.start, flen);
            fields[field_count][flen] = '\0';
            field_count++;

            if (!match(TOK_COMMA))
                break;
        }

        consume(TOK_RBRACE, "Expected '}' after struct body.");

        /* Create the struct def in the VM (registers as global) */
        VM::StructBuilder builder = vm_->def_struct(name_buf);
        for (int i = 0; i < field_count; i++)
            builder.field(fields[i]);
        builder.end();
    }

    /* =========================================================
    ** import (stub)
    ** ========================================================= */

    void Compiler::import_statement()
    {
        consume(TOK_IDENTIFIER, "Expected module name after 'import'.");
        Token name = previous_;
        consume(TOK_SEMICOLON, "Expected ';' after import.");

        /* Build name string */
        char mod_name[64];
        int len = name.length < 63 ? name.length : 63;
        memcpy(mod_name, name.start, len);
        mod_name[len] = '\0';

        /* Check not already imported */
        for (int i = 0; i < num_imports_; i++)
        {
            if (strcmp(imports_[i].lib->name, mod_name) == 0)
                return; /* already imported, skip */
        }

        /* Find in VM registry */
        const NativeLib *lib = vm_->find_lib(mod_name);
        if (!lib)
        {
            /* Try loading as plugin (.so/.dll/.dylib) */
            lib = vm_->try_load_plugin(mod_name);
        }
        if (!lib)
        {
            error("Unknown module.");
            return;
        }

        if (num_imports_ >= MAX_IMPORTS)
        {
            error("Too many imports.");
            return;
        }

        /* Register all functions/constants as globals */
        int base = vm_->open_lib_globals(lib);

        imports_[num_imports_].lib = lib;
        imports_[num_imports_].base_gidx = base;
        imports_[num_imports_].exposed = false;
        num_imports_++;
    }

    void Compiler::using_statement()
    {
        consume(TOK_IDENTIFIER, "Expected module name after 'using'.");
        Token name = previous_;
        consume(TOK_SEMICOLON, "Expected ';' after using.");

        /* Build name string */
        char mod_name[64];
        int len = name.length < 63 ? name.length : 63;
        memcpy(mod_name, name.start, len);
        mod_name[len] = '\0';

        /* Find in imported modules */
        for (int i = 0; i < num_imports_; i++)
        {
            if (strcmp(imports_[i].lib->name, mod_name) == 0)
            {
                imports_[i].exposed = true;
                return;
            }
        }

        /* Not imported yet? Try to import + expose */
        const NativeLib *lib = vm_->find_lib(mod_name);
        if (!lib)
            lib = vm_->try_load_plugin(mod_name);
        if (!lib)
        {
            error("Unknown module. Use 'import' first.");
            return;
        }

        if (num_imports_ >= MAX_IMPORTS)
        {
            error("Too many imports.");
            return;
        }

        int base = vm_->open_lib_globals(lib);
        imports_[num_imports_].lib = lib;
        imports_[num_imports_].base_gidx = base;
        imports_[num_imports_].exposed = true;
        num_imports_++;
    }

    void Compiler::include_statement()
    {
        consume(TOK_STRING, "Expected file path string after 'include'.");
        Token path_tok = previous_;
        consume(TOK_SEMICOLON, "Expected ';' after include path.");

        /* Extract path (strip quotes) */
        const char *path_start = path_tok.start + 1; /* skip opening " */
        int path_len = path_tok.length - 2;          /* strip both quotes */

        if (path_len <= 0)
        {
            error("Empty include path.");
            return;
        }

        /* Build path string from token */
        char filepath[1024];
        if (path_len >= (int)sizeof(filepath))
        {
            error("Include path too long.");
            return;
        }
        memcpy(filepath, path_start, (size_t)path_len);
        filepath[path_len] = '\0';

        /* Read file via VM (searches relative path + search paths) */
        char resolved[1024];
        resolved[0] = '\0';
        long size = 0;
        char *source = vm_->read_file(filepath, current_file_, &size,
                                      resolved, (int)sizeof(resolved));
        if (!source)
        {
            error("Cannot open include file.");
            return;
        }

        /* Check include depth */
        if (include_depth_ >= MAX_INCLUDE_DEPTH)
        {
            free(source);
            error("Include nested too deeply (circular include?).");
            return;
        }

        /* Check for circular include (against resolved paths) */
        for (int i = 0; i < include_depth_; i++)
        {
            if (strcmp(include_stack_[i], resolved) == 0)
            {
                free(source);
                error("Circular include detected.");
                return;
            }
        }

        /* Store buffer and resolved path for later cleanup */
        if (include_count_ >= MAX_INCLUDES)
        {
            free(source);
            error("Too many includes (max 64).");
            return;
        }
        include_sources_[include_count_] = source;
        int rlen = 0;
        while (resolved[rlen]) rlen++;
        char *path_copy = (char *)malloc((size_t)rlen + 1);
        memcpy(path_copy, resolved, (size_t)rlen + 1);
        include_paths_[include_count_] = path_copy;
        include_count_++;

        /* Save lexer state, switch to included source */
        LexerState saved = lexer_.save_state();
        const char *saved_file = current_file_;
        Token saved_current = current_;
        Token saved_previous = previous_;
        lexer_.set_source(source);
        current_file_ = path_copy;
        include_stack_[include_depth_++] = path_copy;

        /* Parse the included file's content */
        advance(); /* prime the first token from new source */
        while (!check(TOK_EOF))
        {
            declaration();
        }

        /* Restore lexer and compiler state to the including file */
        include_depth_--;
        lexer_.restore_state(saved);
        current_file_ = saved_file;
        current_ = saved_current;
        previous_ = saved_previous;
    }

    /* =========================================================
    ** Statement dispatcher
    ** ========================================================= */

    void Compiler::statement()
    {
        if (match(TOK_IF))
        {
            if_statement();
        }
        else if (match(TOK_WHILE))
        {
            while_statement();
        }
        else if (match(TOK_FOR))
        {
            for_statement();
        }
        else if (match(TOK_FOREACH))
        {
            foreach_statement();
        }
        else if (match(TOK_LOOP))
        {
            loop_statement();
        }
        else if (match(TOK_DO))
        {
            do_while_statement();
        }
        else if (match(TOK_SWITCH))
        {
            switch_statement();
        }
        else if (match(TOK_RETURN))
        {
            return_statement();
        }
        else if (match(TOK_BREAK))
        {
            break_statement();
        }
        else if (match(TOK_CONTINUE))
        {
            continue_statement();
        }
        else if (match(TOK_FRAME))
        {
            frame_statement();
        }
        else if (match(TOK_PRINT))
        {
            print_statement();
        }
        else if (match(TOK_LBRACE))
        {
            begin_scope();
            block();
            end_scope();
        }
        else
        {
            expression_statement();
        }
    }

    /* =========================================================
    ** Expression statement
    ** ========================================================= */

    void Compiler::expression_statement()
    {
        int saved_next_reg = state_->next_reg;
        int reg = expression(-1);
        free_reg(reg);
        consume(TOK_SEMICOLON, "Expected ';' after expression.");
        state_->next_reg = saved_next_reg;
    }

    /* =========================================================
    ** Block: { declarations... }
    ** Assumes opening '{' already consumed.
    ** ========================================================= */

    void Compiler::block()
    {
        while (!check(TOK_RBRACE) && !check(TOK_EOF))
        {
            declaration();
        }
        consume(TOK_RBRACE, "Expected '}' after block.");
    }

    /* =========================================================
    ** if / elif / else
    ** ========================================================= */

    void Compiler::if_statement()
    {
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
        while (match(TOK_ELIF))
        {
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
        if (match(TOK_ELSE))
        {
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

    void Compiler::while_statement()
    {
        int loop_start = state_->emitter.current_offset();

        /* Push loop context */
        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.start = loop_start;
        loop.continue_target = loop_start;
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;

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
    ** Numeric for — optimized path with FORPREP/FORLOOP.
    ** Detects: for (var i = <int>; i < <expr>; i = i + <int>)
    ** Also: i += <int>, i <= <expr>, step can be negative.
    ** Returns true if pattern matched and code was emitted.
    ** ========================================================= */

    bool Compiler::try_numeric_for()
    {
        /* We need: var IDENT = INT ; IDENT <|<= EXPR ; IDENT = IDENT + INT|IDENT += INT ) */
        /* Save state for rollback */
        LexerState saved_lex = lexer_.save_state();
        Token saved_current = current_;
        Token saved_previous = previous_;
        int saved_next_reg = state_->next_reg;
        int saved_local_count = state_->local_count;

        /* --- Phase 1: Pattern matching (can rollback) --- */
        Token loop_var;
        int32_t init_val = 0;
        bool use_le = false;

        /* Step 1: check for 'var IDENT = INT ;' */
        if (!check(TOK_VAR))
            return false;
        advance(); /* consume 'var' */

        if (!check(TOK_IDENTIFIER))
        {
            lexer_.restore_state(saved_lex);
            current_ = saved_current;
            previous_ = saved_previous;
            state_->next_reg = saved_next_reg;
            state_->local_count = saved_local_count;
            return false;
        }
        advance(); /* consume IDENT */
        loop_var = previous_;

        if (!match(TOK_EQ))
        {
            lexer_.restore_state(saved_lex);
            current_ = saved_current;
            previous_ = saved_previous;
            state_->next_reg = saved_next_reg;
            state_->local_count = saved_local_count;
            return false;
        }

        /* Must be an integer literal (or negative: -INT) */
        {
            bool neg_init = false;
            if (check(TOK_MINUS))
            {
                advance();
                neg_init = true;
            }
            if (!check(TOK_INT))
            {
                lexer_.restore_state(saved_lex);
                current_ = saved_current;
                previous_ = saved_previous;
                state_->next_reg = saved_next_reg;
                state_->local_count = saved_local_count;
                return false;
            }
            advance();
            init_val = (int32_t)strtol(previous_.start, nullptr, 10);
            if (neg_init)
                init_val = -init_val;
        }

        if (!match(TOK_SEMICOLON))
        {
            lexer_.restore_state(saved_lex);
            current_ = saved_current;
            previous_ = saved_previous;
            state_->next_reg = saved_next_reg;
            state_->local_count = saved_local_count;
            return false;
        }

        /* Step 2: check condition — IDENT < EXPR or IDENT <= EXPR */
        if (!check(TOK_IDENTIFIER) ||
            current_.length != loop_var.length ||
            memcmp(current_.start, loop_var.start, loop_var.length) != 0)
        {
            lexer_.restore_state(saved_lex);
            current_ = saved_current;
            previous_ = saved_previous;
            state_->next_reg = saved_next_reg;
            state_->local_count = saved_local_count;
            return false;
        }
        advance(); /* consume loop var in condition */

        if (check(TOK_LT))
        {
            advance();
            use_le = false;
        }
        else if (check(TOK_LT_EQ))
        {
            advance();
            use_le = true;
        }
        else
        {
            lexer_.restore_state(saved_lex);
            current_ = saved_current;
            previous_ = saved_previous;
            state_->next_reg = saved_next_reg;
            state_->local_count = saved_local_count;
            return false;
        }

        /* --- Phase 2: Committed — emit optimized for loop --- */

        /* Allocate 3 consecutive registers: counter, limit, step.
           All must be declared as locals so inner scopes don't reclaim them. */
        int base_reg = state_->next_reg; /* R[A] = counter = loop var */

        /* Declare loop var as local at base_reg */
        declare_local(loop_var);
        Local &local_i = state_->locals[state_->local_count++];
        local_i.name = loop_var;
        local_i.depth = state_->scope_depth;
        local_i.reg = base_reg;
        local_i.captured = false;
        state_->next_reg++; /* reserve R[A] */

        /* Hidden locals for limit and step (names can't collide with user code) */
        static const char hidden_limit[] = "(limit)";
        static const char hidden_step[] = "(step)";
        Token limit_tok = {TOK_IDENTIFIER, hidden_limit, 7, loop_var.line};
        Token step_tok = {TOK_IDENTIFIER, hidden_step, 6, loop_var.line};

        int limit_reg = state_->next_reg;
        Local &local_lim = state_->locals[state_->local_count++];
        local_lim.name = limit_tok;
        local_lim.depth = state_->scope_depth;
        local_lim.reg = limit_reg;
        local_lim.captured = false;
        state_->next_reg++;

        int step_reg = state_->next_reg;
        Local &local_stp = state_->locals[state_->local_count++];
        local_stp.name = step_tok;
        local_stp.depth = state_->scope_depth;
        local_stp.reg = step_reg;
        local_stp.captured = false;
        state_->next_reg++;

        /* Emit init value into counter reg */
        state_->emitter.emit_asbx(OP_LOADI, base_reg, init_val, loop_var.line);

        /* Parse limit expression into limit_reg */
        expression(limit_reg);

        if (!match(TOK_SEMICOLON))
        {
            error_at_current("Expected ';' after for condition in numeric for.");
            return true;
        }

        /* Step 3: check step — IDENT = IDENT + INT or IDENT += INT */
        int32_t step_val = 1;

        if (!check(TOK_IDENTIFIER) ||
            current_.length != loop_var.length ||
            memcmp(current_.start, loop_var.start, loop_var.length) != 0)
        {
            error_at_current("Expected loop variable in for step.");
            return true;
        }
        advance(); /* consume loop var */

        if (match(TOK_PLUS_EQ))
        {
            /* i += INT */
            bool neg_step = false;
            if (check(TOK_MINUS))
            {
                advance();
                neg_step = true;
            }
            if (!check(TOK_INT))
            {
                error_at_current("Numeric for step must be integer literal.");
                return true;
            }
            advance();
            step_val = (int32_t)strtol(previous_.start, nullptr, 10);
            if (neg_step)
                step_val = -step_val;
        }
        else if (match(TOK_EQ))
        {
            /* i = i + INT or i = i - INT */
            if (!check(TOK_IDENTIFIER) ||
                current_.length != loop_var.length ||
                memcmp(current_.start, loop_var.start, loop_var.length) != 0)
            {
                error_at_current("Expected loop variable in for step.");
                return true;
            }
            advance(); /* consume 'i' on RHS */

            if (match(TOK_PLUS))
            {
                bool neg_step = false;
                if (check(TOK_MINUS))
                {
                    advance();
                    neg_step = true;
                }
                if (!check(TOK_INT))
                {
                    error_at_current("Numeric for step must be integer literal.");
                    return true;
                }
                advance();
                step_val = (int32_t)strtol(previous_.start, nullptr, 10);
                if (neg_step)
                    step_val = -step_val;
            }
            else if (match(TOK_MINUS))
            {
                if (!check(TOK_INT))
                {
                    error_at_current("Numeric for step must be integer literal.");
                    return true;
                }
                advance();
                step_val = -(int32_t)strtol(previous_.start, nullptr, 10);
            }
            else
            {
                error_at_current("Expected '+' or '-' in for step.");
                return true;
            }
        }
        else
        {
            error_at_current("Expected '+=' or '=' in for step.");
            return true;
        }

        consume(TOK_RPAREN, "Expected ')' after for clauses.");

        /* Emit step value */
        state_->emitter.emit_asbx(OP_LOADI, step_reg, step_val, previous_.line);

        /* If using <=, adjust: add 1 to limit for < comparison in FORLOOP
           (FORLOOP checks counter < limit, so for <= we need limit+1) */
        if (use_le)
        {
            state_->emitter.emit_abc(OP_ADDI, limit_reg, limit_reg, 1, previous_.line);
        }

        /* FORPREP: R[A] -= step; jump to FORLOOP */
        int forprep = state_->emitter.emit_jump(OP_FORPREP, base_reg, previous_.line);

        /* Loop context for break/continue */
        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;
        loop.continue_target = -1; /* patch later — FORLOOP not yet emitted */

        int body_start = state_->emitter.current_offset();
        loop.start = body_start;

        /* Body */
        consume(TOK_LBRACE, "Expected '{' after for clauses.");
        begin_scope();
        block();
        end_scope();

        /* FORLOOP: R[A] += step; if R[A] < R[A+1]: jump to body_start */
        int forloop_offset = state_->emitter.current_offset();

        int loop_back = body_start - (forloop_offset + 1);
        state_->emitter.emit_asbx(OP_FORLOOP, base_reg, loop_back, previous_.line);

        /* Patch FORPREP to jump to FORLOOP */
        state_->emitter.patch_jump_to(forprep, forloop_offset);

        /* Patch breaks to after FORLOOP */
        for (int i = 0; i < loop.break_count; i++)
            state_->emitter.patch_jump(loop.breaks[i]);

        /* Patch continues to FORLOOP */
        for (int i = 0; i < loop.continue_count; i++)
            state_->emitter.patch_jump_to(loop.continues[i], forloop_offset);

        state_->loop_depth--;
        return true;
    }

    /* =========================================================
    ** for (init; cond; step) { body }
    ** ========================================================= */

    void Compiler::for_statement()
    {
        begin_scope();
        consume(TOK_LPAREN, "Expected '(' after 'for'.");

        /* Try optimized numeric for path first */
        if (try_numeric_for())
        {
            end_scope();
            return;
        }

        /* --- General for (fallback) --- */

        /* Initializer */
        if (match(TOK_SEMICOLON))
        {
            /* No init */
        }
        else if (check(TOK_VAR))
        {
            advance();
            var_declaration();
        }
        else
        {
            expression_statement();
        }

        int loop_start = state_->emitter.current_offset();

        /* Push loop context */
        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.start = loop_start;
        loop.continue_target = loop_start;
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;

        /* Condition */
        int exit_jump = -1;
        if (!match(TOK_SEMICOLON))
        {
            int cond_reg = expression(-1);
            consume(TOK_SEMICOLON, "Expected ';' after for condition.");
            exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond_reg, previous_.line);
            free_reg(cond_reg);
        }

        /* Step — parse but emit after body */
        int body_jump = -1;
        int step_start = -1;
        if (!check(TOK_RPAREN))
        {
            body_jump = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
            step_start = state_->emitter.current_offset();

            /* Save next_reg before step — step may return a local reg
               that we must NOT free (e.g. i += 1 returns local i) */
            int save_reg = state_->next_reg;
            int step_reg = expression(-1);
            (void)step_reg;
            state_->next_reg = save_reg; /* restore — don't clobber loop locals */

            state_->emitter.emit_loop(loop_start, 0, previous_.line);
            loop.start = step_start; /* continue goes to step */
            loop.continue_target = step_start;
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
        if (step_start >= 0)
        {
            state_->emitter.emit_loop(step_start, 0, previous_.line);
        }
        else
        {
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

    void Compiler::foreach_statement()
    {
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

        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.start = loop_start;
        loop.continue_target = loop_start;
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;

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

    void Compiler::loop_statement()
    {
        int loop_start = state_->emitter.current_offset();

        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.start = loop_start;
        loop.continue_target = loop_start;
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;

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

    void Compiler::do_while_statement()
    {
        int loop_start = state_->emitter.current_offset();

        LoopCtx &loop = state_->loops[state_->loop_depth++];
        loop.start = loop_start;
        loop.continue_target = -1; /* patch later — condition comes after body */
        loop.scope_depth = state_->scope_depth;
        loop.break_count = 0;
        loop.continue_count = 0;

        consume(TOK_LBRACE, "Expected '{' after 'do'.");
        begin_scope();
        block();
        end_scope();

        /* Patch continue jumps to here (the condition) */
        for (int i = 0; i < loop.continue_count; i++)
            state_->emitter.patch_jump(loop.continues[i]);

        consume(TOK_WHILE, "Expected 'while' after do block.");
        consume(TOK_LPAREN, "Expected '(' after 'while'.");
        int cond_reg = expression(-1);
        consume(TOK_RPAREN, "Expected ')' after condition.");
        consume(TOK_SEMICOLON, "Expected ';' after do-while.");

        /* If condition is true, jump back to loop_start */
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

    void Compiler::switch_statement()
    {
        consume(TOK_LPAREN, "Expected '(' after 'switch'.");
        int expr_reg = expression(-1);
        consume(TOK_RPAREN, "Expected ')' after switch expression.");
        consume(TOK_LBRACE, "Expected '{' after switch.");

        /* Protect the switch expression register throughout */
        int base_reg = expr_reg + 1;

        int end_jumps[64];
        int end_count = 0;

        while (match(TOK_CASE))
        {
            /* Ensure expr_reg is protected */
            state_->next_reg = base_reg;

            /* case value: { body } */
            int case_val_reg = expression(-1);
            consume(TOK_COLON, "Expected ':' after case value.");

            /* Compare: expr == case_val */
            int cmp_reg = alloc_reg();
            state_->emitter.emit_abc(OP_EQ, cmp_reg, expr_reg, case_val_reg, previous_.line);
            int skip_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, previous_.line);

            /* Reset temps for body */
            state_->next_reg = base_reg;

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
        if (match(TOK_DEFAULT))
        {
            state_->next_reg = base_reg;
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

        /* Release switch expression register */
        state_->next_reg = expr_reg;
    }

    /* =========================================================
    ** return expr, expr, ...;
    ** ========================================================= */

    void Compiler::return_statement()
    {
        if (match(TOK_SEMICOLON))
        {
            /* return; → return nil */
            state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
            state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
            return;
        }

        /* Return values — put in consecutive regs starting at some base */
        int base = state_->next_reg;
        int count = 0;
        do
        {
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

    void Compiler::break_statement()
    {
        if (state_->loop_depth == 0)
        {
            error("'break' outside of loop.");
            consume(TOK_SEMICOLON, "Expected ';' after 'break'.");
            return;
        }
        consume(TOK_SEMICOLON, "Expected ';' after 'break'.");

        LoopCtx &loop = state_->loops[state_->loop_depth - 1];
        if (loop.break_count >= 64)
        {
            error("Too many 'break' statements in loop.");
            return;
        }
        loop.breaks[loop.break_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
    }

    void Compiler::continue_statement()
    {
        if (state_->loop_depth == 0)
        {
            error("'continue' outside of loop.");
            consume(TOK_SEMICOLON, "Expected ';' after 'continue'.");
            return;
        }
        consume(TOK_SEMICOLON, "Expected ';' after 'continue'.");

        LoopCtx &loop = state_->loops[state_->loop_depth - 1];
        if (loop.continue_target >= 0)
        {
            /* Target known (while, for) — emit backward jump */
            state_->emitter.emit_loop(loop.continue_target, 0, previous_.line);
        }
        else
        {
            /* Target not yet known (do-while) — emit forward jump, patch later */
            loop.continues[loop.continue_count++] =
                state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
        }
    }

    /* =========================================================
    ** frame; — game loop yield (speed=100)
    ** frame(expr); — yield with custom speed
    ** ========================================================= */

    void Compiler::frame_statement()
    {
        if (match(TOK_LPAREN))
        {
            /* frame(expr); → OP_FRAME_N */
            int reg = alloc_reg();
            expression(reg);
            consume(TOK_RPAREN, "Expected ')' after frame speed.");
            consume(TOK_SEMICOLON, "Expected ';' after 'frame(...)'.");
            state_->emitter.emit_abc(OP_FRAME_N, reg, 0, 0, previous_.line);
            free_reg(reg);
        }
        else
        {
            /* frame; → OP_FRAME */
            consume(TOK_SEMICOLON, "Expected ';' after 'frame'.");
            state_->emitter.emit_abc(OP_FRAME, 0, 0, 0, previous_.line);
        }
    }

    /* =========================================================
    ** print(a, b, c);
    ** Emits OP_PRINT for each arg. Multiple args separated by space.
    ** B=1 means "print newline after", B=0 means "no newline (more coming)".
    ** ========================================================= */

    void Compiler::print_statement()
    {
        consume(TOK_LPAREN, "Expected '(' after 'print'.");

        if (check(TOK_RPAREN))
        {
            /* print() — just newline */
            advance();
            consume(TOK_SEMICOLON, "Expected ';' after print.");
            int reg = alloc_reg();
            state_->emitter.emit_abc(OP_PRINT, reg, 1, 1, previous_.line); /* C=1: no value */
            free_reg(reg);
            return;
        }

        /* Collect args */
        do
        {
            int reg = expression(-1);
            bool last = check(TOK_RPAREN);
            state_->emitter.emit_abc(OP_PRINT, reg, last ? 1 : 0, 0, previous_.line);
            free_reg(reg);
        } while (match(TOK_COMMA));

        consume(TOK_RPAREN, "Expected ')' after print arguments.");
        consume(TOK_SEMICOLON, "Expected ';' after print.");
    }

} /* namespace zen */
