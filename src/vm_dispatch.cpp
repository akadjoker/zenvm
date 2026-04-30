/*
** vm_dispatch.cpp — Dispatch loop da VM zen.
**
** Um único ficheiro com #ifdef ZEN_COMPUTED_GOTO para seleccionar:
**   - Computed goto (GCC/Clang) — ~45% mais rápido
**   - Switch (portável, MSVC)
**
** O código dos opcodes é IDÊNTICO em ambos os modos.
** Macros CASE/NEXT/DISPATCH adaptam-se automaticamente.
*/

#include "vm.h"
#include "debug.h"

namespace zen {

void VM::execute(ObjFiber* fiber) {
    /* Cache hot state em locals */
    CallFrame* frame = &fiber->frames[fiber->frame_count - 1];
    Instruction* ip = frame->ip;
    Value* R = frame->base;
    Value* K = frame->func->constants;
    ObjUpvalue** UV = frame->closure ? frame->closure->upvalues : nullptr;

    /* Macro para reload após CALL/RETURN (frame mudou) */
    #define LOAD_STATE()                                       \
        frame = &fiber->frames[fiber->frame_count - 1];        \
        ip    = frame->ip;                                     \
        R     = frame->base;                                   \
        K     = frame->func->constants;                        \
        UV    = frame->closure ? frame->closure->upvalues : nullptr

    #define SAVE_IP() frame->ip = ip

    /* Aritmética helpers */
    #define NUM_BINOP(op)                                          \
        do {                                                        \
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];              \
            if (vb.type == VAL_INT && vc.type == VAL_INT)           \
                R[ZEN_A(i)] = val_int(vb.as.integer op vc.as.integer); \
            else                                                    \
                R[ZEN_A(i)] = val_float(to_number(vb) op to_number(vc)); \
        } while(0)

/* =================================================================
**  DISPATCH SETUP
** ================================================================= */

#ifdef ZEN_COMPUTED_GOTO

    /* --- Computed goto (GCC/Clang) --- */
    static const void* dispatch_table[] = {
        &&lbl_OP_LOADNIL,   &&lbl_OP_LOADBOOL,  &&lbl_OP_LOADK,     &&lbl_OP_LOADI,
        &&lbl_OP_MOVE,
        &&lbl_OP_GETGLOBAL, &&lbl_OP_SETGLOBAL,
        &&lbl_OP_ADD,       &&lbl_OP_SUB,       &&lbl_OP_MUL,       &&lbl_OP_DIV,
        &&lbl_OP_MOD,       &&lbl_OP_NEG,       &&lbl_OP_ADDI,      &&lbl_OP_SUBI,
        &&lbl_OP_BAND,      &&lbl_OP_BOR,       &&lbl_OP_BXOR,      &&lbl_OP_BNOT,
        &&lbl_OP_SHL,       &&lbl_OP_SHR,
        &&lbl_OP_EQ,        &&lbl_OP_LT,        &&lbl_OP_LE,        &&lbl_OP_NOT,
        &&lbl_OP_JMP,       &&lbl_OP_JMPIF,     &&lbl_OP_JMPIFNOT,
        &&lbl_OP_CALL,      &&lbl_OP_CALLGLOBAL, &&lbl_OP_RETURN,
        &&lbl_OP_CLOSURE,   &&lbl_OP_GETUPVAL,  &&lbl_OP_SETUPVAL,  &&lbl_OP_CLOSE,
        &&lbl_OP_NEWFIBER,  &&lbl_OP_RESUME,    &&lbl_OP_YIELD,
        &&lbl_OP_FRAME,     &&lbl_OP_FRAME_N,
        &&lbl_OP_NEWARRAY,  &&lbl_OP_NEWMAP,
        &&lbl_OP_GETFIELD,  &&lbl_OP_SETFIELD,  &&lbl_OP_GETINDEX,  &&lbl_OP_SETINDEX,
        &&lbl_OP_NEWCLASS,  &&lbl_OP_NEWINSTANCE, &&lbl_OP_GETMETHOD,
        &&lbl_OP_CONCAT,    &&lbl_OP_LEN,       &&lbl_OP_PRINT,
        &&lbl_OP_LTJMPIFNOT, &&lbl_OP_LEJMPIFNOT,
        &&lbl_OP_HALT,
    };

    #define DISPATCH()  goto *dispatch_table[ZEN_OP(*ip)]
    #define CASE(op)    lbl_##op:
    #define NEXT()      do { ++ip; DISPATCH(); } while(0)

    DISPATCH();

#else

    /* --- Switch (portável) --- */
    #define DISPATCH()  continue
    #define CASE(op)    case op:
    #define NEXT()      do { ++ip; continue; } while(0)

    for (;;) {

    #define SWITCH_TOP switch(ZEN_OP(*ip)) {
    SWITCH_TOP

#endif

/* =================================================================
**  OPCODES
** ================================================================= */

    CASE(OP_LOADNIL) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_nil();
        NEXT();
    }

    CASE(OP_LOADBOOL) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_bool(ZEN_B(i) != 0);
        if (ZEN_C(i)) ++ip;  /* skip next if C */
        NEXT();
    }

    CASE(OP_LOADK) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = K[ZEN_BX(i)];
        NEXT();
    }

    CASE(OP_LOADI) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(ZEN_SBX(i));
        NEXT();
    }

    CASE(OP_MOVE) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = R[ZEN_B(i)];
        NEXT();
    }

    CASE(OP_GETGLOBAL) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = globals_[ZEN_BX(i)];
        NEXT();
    }

    CASE(OP_SETGLOBAL) {
        uint32_t i = *ip;
        globals_[ZEN_BX(i)] = R[ZEN_A(i)];
        NEXT();
    }

    /* --- Aritmética --- */
    CASE(OP_ADD) { uint32_t i = *ip; NUM_BINOP(+); NEXT(); }
    CASE(OP_SUB) { uint32_t i = *ip; NUM_BINOP(-); NEXT(); }
    CASE(OP_MUL) { uint32_t i = *ip; NUM_BINOP(*); NEXT(); }
    CASE(OP_DIV) {
        uint32_t i = *ip;
        /* Divisão sempre float (evitar div by zero int) */
        R[ZEN_A(i)] = val_float(to_number(R[ZEN_B(i)]) / to_number(R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_MOD) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
        if (vb.type == VAL_INT && vc.type == VAL_INT)
            R[ZEN_A(i)] = val_int(vb.as.integer % vc.as.integer);
        else {
            double a = to_number(vb), b = to_number(vc);
            R[ZEN_A(i)] = val_float(a - (int64_t)(a/b) * b);
        }
        NEXT();
    }
    CASE(OP_NEG) {
        uint32_t i = *ip;
        Value v = R[ZEN_B(i)];
        if (v.type == VAL_INT) R[ZEN_A(i)] = val_int(-v.as.integer);
        else R[ZEN_A(i)] = val_float(-to_number(v));
        NEXT();
    }

    /* --- Superinstruções imediatas --- */
    CASE(OP_ADDI) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)];
        int8_t imm = (int8_t)ZEN_C(i);
        if (vb.type == VAL_INT) R[ZEN_A(i)] = val_int(vb.as.integer + imm);
        else R[ZEN_A(i)] = val_float(to_number(vb) + imm);
        NEXT();
    }
    CASE(OP_SUBI) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)];
        int8_t imm = (int8_t)ZEN_C(i);
        if (vb.type == VAL_INT) R[ZEN_A(i)] = val_int(vb.as.integer - imm);
        else R[ZEN_A(i)] = val_float(to_number(vb) - imm);
        NEXT();
    }

    /* --- Bitwise --- */
    CASE(OP_BAND) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) & to_integer(R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_BOR) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) | to_integer(R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_BXOR) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) ^ to_integer(R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_BNOT) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(~to_integer(R[ZEN_B(i)]));
        NEXT();
    }
    CASE(OP_SHL) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) << to_integer(R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_SHR) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) >> to_integer(R[ZEN_C(i)]));
        NEXT();
    }

    /* --- Comparação --- */
    CASE(OP_EQ) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_bool(values_equal(R[ZEN_B(i)], R[ZEN_C(i)]));
        NEXT();
    }
    CASE(OP_LT) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
        if (vb.type == VAL_INT && vc.type == VAL_INT)
            R[ZEN_A(i)] = val_bool(vb.as.integer < vc.as.integer);
        else
            R[ZEN_A(i)] = val_bool(to_number(vb) < to_number(vc));
        NEXT();
    }
    CASE(OP_LE) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
        if (vb.type == VAL_INT && vc.type == VAL_INT)
            R[ZEN_A(i)] = val_bool(vb.as.integer <= vc.as.integer);
        else
            R[ZEN_A(i)] = val_bool(to_number(vb) <= to_number(vc));
        NEXT();
    }
    CASE(OP_NOT) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = val_bool(!is_truthy(R[ZEN_B(i)]));
        NEXT();
    }

    /* --- Jumps --- */
    CASE(OP_JMP) {
        uint32_t i = *ip;
        ip += ZEN_SBX(i);
        NEXT();
    }
    CASE(OP_JMPIF) {
        uint32_t i = *ip;
        if (is_truthy(R[ZEN_A(i)])) ip += ZEN_SBX(i);
        NEXT();
    }
    CASE(OP_JMPIFNOT) {
        uint32_t i = *ip;
        if (!is_truthy(R[ZEN_A(i)])) ip += ZEN_SBX(i);
        NEXT();
    }

    /* --- Funções --- */
    CASE(OP_CALL) {
        uint32_t i = *ip;
        int a = ZEN_A(i);
        int nargs = ZEN_B(i);
        int nresults = ZEN_C(i);
        ++ip;
        SAVE_IP();

        Value callee = R[a];
        if (is_closure(callee)) {
            /* Script closure — hot path */
            ObjClosure* cl = as_closure(callee);
            ObjFunc* fn = cl->func;
            CallFrame* new_frame = &fiber->frames[fiber->frame_count++];
            new_frame->closure = cl;
            new_frame->func = fn;
            new_frame->ip = fn->code;
            new_frame->base = &R[a + 1];
            new_frame->ret_reg = a;
            new_frame->ret_count = nresults;
            fiber->stack_top = new_frame->base + fn->num_regs;
            LOAD_STATE();
            DISPATCH();
        }
        if (is_native(callee)) {
            ObjNative* nat = as_native(callee);
            int nret = nat->fn(this, &R[a + 1], nargs);
            if (nret > 0) R[a] = R[a + 1];
            else R[a] = val_nil();
            DISPATCH();
        }
        runtime_error("attempt to call non-function");
        return;
    }

    CASE(OP_CALLGLOBAL) {
        uint32_t i = *ip;
        int a = ZEN_A(i);
        int nargs = ZEN_B(i);
        int nresults = ZEN_C(i);
        ++ip;
        int gidx = ZEN_BX(*ip); /* word 2: global index */
        ++ip;
        SAVE_IP();

        Value callee = globals_[gidx];
        if (is_closure(callee)) {
            ObjClosure* cl = as_closure(callee);
            ObjFunc* fn = cl->func;
            CallFrame* new_frame = &fiber->frames[fiber->frame_count++];
            new_frame->closure = cl;
            new_frame->func = fn;
            new_frame->ip = fn->code;
            new_frame->base = &R[a + 1];
            new_frame->ret_reg = a;
            new_frame->ret_count = nresults;
            fiber->stack_top = new_frame->base + fn->num_regs;
            LOAD_STATE();
            DISPATCH();
        }
        if (is_native(callee)) {
            ObjNative* nat = as_native(callee);
            int nret = nat->fn(this, &R[a + 1], nargs);
            if (nret > 0) R[a] = R[a + 1];
            else R[a] = val_nil();
            DISPATCH();
        }
        runtime_error("attempt to call non-function");
        return;
    }

    CASE(OP_RETURN) {
        uint32_t i = *ip;
        int a = ZEN_A(i);
        int nresults = ZEN_B(i);

        /* Close upvalues do frame actual — skip se não há nenhum aberto */
        if (fiber->open_upvalues && fiber->open_upvalues->location >= frame->base)
            close_upvalues(fiber, frame->base);

        /* Copiar resultados para o caller */
        int ret_reg = frame->ret_reg;
        int ret_count = frame->ret_count;
        Value* dest = frame->base - 1;  /* posição do callable no caller */

        fiber->frame_count--;
        if (fiber->frame_count == 0) {
            /* Retorno do top-level */
            fiber->state = FIBER_DONE;
            if (fiber->caller) {
                fiber->caller->transfer_value = nresults > 0 ? R[a] : val_nil();
                fiber->caller->state = FIBER_RUNNING;
                current_fiber_ = fiber->caller;
            }
            return;
        }

        /* Copiar resultados */
        CallFrame* caller_frame = &fiber->frames[fiber->frame_count - 1];
        Value* caller_base = caller_frame->base;
        int copy_count = ret_count < 0 ? nresults : ret_count;
        if (copy_count > nresults) copy_count = nresults;
        for (int j = 0; j < copy_count; j++) {
            caller_base[ret_reg + j] = R[a + j];
        }
        /* Nil-fill remaining */
        for (int j = copy_count; j < ret_count && ret_count > 0; j++) {
            caller_base[ret_reg + j] = val_nil();
        }

        fiber->stack_top = caller_base + caller_frame->func->num_regs;
        LOAD_STATE();
        DISPATCH();
    }

    /* --- Closures / Upvalues --- */
    CASE(OP_CLOSURE) {
        uint32_t i = *ip;
        int a = ZEN_A(i);
        ObjFunc* fn = as_func(K[ZEN_BX(i)]);

        ObjClosure* cl = (ObjClosure*)zen_alloc(&gc_, sizeof(ObjClosure));
        cl->obj.type = OBJ_CLOSURE;
        cl->obj.color = GC_WHITE;
        cl->obj.hash = 0;
        cl->obj.gc_next = gc_.objects;
        gc_.objects = (Obj*)cl;
        cl->func = fn;
        cl->upvalue_count = 0;  /* TODO: ler upvalue descriptors */
        cl->upvalues = nullptr;

        R[a] = val_obj((Obj*)cl);
        NEXT();
    }

    CASE(OP_GETUPVAL) {
        uint32_t i = *ip;
        R[ZEN_A(i)] = *UV[ZEN_B(i)]->location;
        NEXT();
    }

    CASE(OP_SETUPVAL) {
        uint32_t i = *ip;
        *UV[ZEN_B(i)]->location = R[ZEN_A(i)];
        NEXT();
    }

    CASE(OP_CLOSE) {
        uint32_t i = *ip;
        close_upvalues(fiber, &R[ZEN_A(i)]);
        NEXT();
    }

    /* --- Fibers --- */
    CASE(OP_NEWFIBER) {
        uint32_t i = *ip;
        ObjClosure* cl = as_closure(R[ZEN_B(i)]);
        ObjFiber* f = new_fiber(cl, 256);
        R[ZEN_A(i)] = val_obj((Obj*)f);
        NEXT();
    }

    CASE(OP_RESUME) {
        uint32_t i = *ip;
        ObjFiber* target = as_fiber(R[ZEN_B(i)]);
        Value send_val = R[ZEN_C(i)];
        ++ip; SAVE_IP();

        if (target->state == FIBER_DONE || target->state == FIBER_RUNNING) {
            runtime_error("cannot resume fiber");
            return;
        }

        target->transfer_value = send_val;
        target->caller = fiber;
        target->state = FIBER_RUNNING;
        fiber->state = FIBER_SUSPENDED;
        current_fiber_ = target;

        execute(target);

        /* Voltámos — target fez yield ou terminou */
        fiber->state = FIBER_RUNNING;
        current_fiber_ = fiber;
        R[ZEN_A(i - 1)] = target->transfer_value;
        LOAD_STATE();
        DISPATCH();
    }

    CASE(OP_YIELD) {
        uint32_t i = *ip;
        ++ip; SAVE_IP();

        if (!fiber->caller) {
            runtime_error("yield outside of fiber");
            return;
        }

        fiber->transfer_value = R[ZEN_A(i)];
        fiber->state = FIBER_SUSPENDED;
        /* Retorna ao execute() do caller (que está em OP_RESUME) */
        return;
    }

    CASE(OP_FRAME) {
        ++ip; SAVE_IP();
        fiber->frame_speed = 100;
        fiber->state = FIBER_SUSPENDED;
        return;
    }

    CASE(OP_FRAME_N) {
        uint32_t i = *ip;
        ++ip; SAVE_IP();
        fiber->frame_speed = R[ZEN_A(i)].as.integer;
        fiber->state = FIBER_SUSPENDED;
        return;
    }

    /* --- Collections --- */
    CASE(OP_NEWARRAY) {
        uint32_t i = *ip;
        ObjArray* arr = new_array(&gc_);
        R[ZEN_A(i)] = val_obj((Obj*)arr);
        NEXT();
    }

    CASE(OP_NEWMAP) {
        uint32_t i = *ip;
        ObjMap* map = new_map(&gc_);
        R[ZEN_A(i)] = val_obj((Obj*)map);
        NEXT();
    }

    /* --- Field/Index access (stubs) --- */
    CASE(OP_GETFIELD) { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }
    CASE(OP_SETFIELD) { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }
    CASE(OP_GETINDEX) { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }
    CASE(OP_SETINDEX) { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }

    /* --- Classes (stubs) --- */
    CASE(OP_NEWCLASS)    { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }
    CASE(OP_NEWINSTANCE) { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }
    CASE(OP_GETMETHOD)   { uint32_t i = *ip; (void)i; /* TODO */ NEXT(); }

    /* --- Misc --- */
    CASE(OP_CONCAT) {
        uint32_t i = *ip;
        /* TODO: string concat via new_string_concat */
        (void)i;
        NEXT();
    }

    CASE(OP_LEN) {
        uint32_t i = *ip;
        Value v = R[ZEN_B(i)];
        if (is_string(v)) R[ZEN_A(i)] = val_int(as_string(v)->length);
        else if (is_array(v)) R[ZEN_A(i)] = val_int(arr_count(as_array(v)));
        else if (is_map(v)) R[ZEN_A(i)] = val_int(as_map(v)->count);
        else R[ZEN_A(i)] = val_int(0);
        NEXT();
    }

    CASE(OP_PRINT) {
        uint32_t i = *ip;
        print_value(R[ZEN_A(i)]);
        printf("\n");
        NEXT();
    }

    /* --- Fused comparison + jump superinstructions (2-word) --- */
    CASE(OP_LTJMPIFNOT) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
        bool less;
        if (vb.type == VAL_INT && vc.type == VAL_INT)
            less = vb.as.integer < vc.as.integer;
        else
            less = to_number(vb) < to_number(vc);
        ++ip; /* advance to the sBx word */
        if (!less) ip += ZEN_SBX(*ip);
        NEXT();
    }

    CASE(OP_LEJMPIFNOT) {
        uint32_t i = *ip;
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
        bool le;
        if (vb.type == VAL_INT && vc.type == VAL_INT)
            le = vb.as.integer <= vc.as.integer;
        else
            le = to_number(vb) <= to_number(vc);
        ++ip; /* advance to the sBx word */
        if (!le) ip += ZEN_SBX(*ip);
        NEXT();
    }

    CASE(OP_HALT) {
        SAVE_IP();
        fiber->state = FIBER_DONE;
        return;
    }

/* =================================================================
**  DISPATCH CLEANUP
** ================================================================= */

#ifdef ZEN_COMPUTED_GOTO
    /* Computed goto não precisa de close */
#else
    } /* switch */
    } /* for */
#endif

    #undef DISPATCH
    #undef CASE
    #undef NEXT
    #undef LOAD_STATE
    #undef SAVE_IP
    #undef NUM_BINOP
}

} /* namespace zen */
