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
#include "zenconf.h"
#include <cmath>
#include <ctime>

namespace zen
{

    /* Fast int→string (replaces snprintf "%lld" — ~10-30x faster) */
    static inline int int_to_cstr(int64_t n, char *buf)
    {
        bool neg = n < 0;
        uint64_t u = neg ? -(uint64_t)n : (uint64_t)n;
        char tmp[21];
        int i = 20;
        tmp[i] = '\0';
        do { tmp[--i] = '0' + (char)(u % 10); u /= 10; } while (u);
        if (neg) tmp[--i] = '-';
        int len = 20 - i;
        memcpy(buf, tmp + i, (size_t)len + 1);
        return len;
    }

    void VM::execute(ObjFiber *fiber)
    {
        /* Cache hot state em locals */
        CallFrame *frame = &fiber->frames[fiber->frame_count - 1];
        Instruction *ip = frame->ip;
        Value *R = frame->base;
        Value *K = frame->func->constants;
        ObjUpvalue **UV = frame->closure ? frame->closure->upvalues : nullptr;

/* Macro para reload após CALL/RETURN (frame mudou) */
#define LOAD_STATE()                                \
    frame = &fiber->frames[fiber->frame_count - 1]; \
    ip = frame->ip;                                 \
    R = frame->base;                                \
    K = frame->func->constants;                     \
    UV = frame->closure ? frame->closure->upvalues : nullptr

#define SAVE_IP() frame->ip = ip

/* Aritmética helpers — use int64_t wrapping via unsigned cast to avoid UB */
#define NUM_BINOP(op)                                                         \
    do                                                                        \
    {                                                                         \
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];                             \
        if (vb.type == VAL_INT && vc.type == VAL_INT)                         \
            R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer           \
                                                op(uint64_t) vc.as.integer)); \
        else                                                                  \
            R[ZEN_A(i)] = val_float(to_number(vb) op to_number(vc));          \
    } while (0)

        /* =================================================================
        **  DISPATCH SETUP
        ** ================================================================= */

#ifdef ZEN_COMPUTED_GOTO

        /* --- Computed goto (GCC/Clang) --- */
        static const void *const dispatch_table[] = {
            &&lbl_OP_LOADNIL,
            &&lbl_OP_LOADBOOL,
            &&lbl_OP_LOADK,
            &&lbl_OP_LOADI,
            &&lbl_OP_MOVE,
            &&lbl_OP_GETGLOBAL,
            &&lbl_OP_SETGLOBAL,
            &&lbl_OP_ADD,
            &&lbl_OP_SUB,
            &&lbl_OP_MUL,
            &&lbl_OP_DIV,
            &&lbl_OP_MOD,
            &&lbl_OP_NEG,
            &&lbl_OP_ADDI,
            &&lbl_OP_SUBI,
            &&lbl_OP_BAND,
            &&lbl_OP_BOR,
            &&lbl_OP_BXOR,
            &&lbl_OP_BNOT,
            &&lbl_OP_SHL,
            &&lbl_OP_SHR,
            &&lbl_OP_EQ,
            &&lbl_OP_LT,
            &&lbl_OP_LE,
            &&lbl_OP_NOT,
            &&lbl_OP_JMP,
            &&lbl_OP_JMPIF,
            &&lbl_OP_JMPIFNOT,
            &&lbl_OP_CALL,
            &&lbl_OP_CALLGLOBAL,
            &&lbl_OP_RETURN,
            &&lbl_OP_CLOSURE,
            &&lbl_OP_GETUPVAL,
            &&lbl_OP_SETUPVAL,
            &&lbl_OP_CLOSE,
            &&lbl_OP_NEWFIBER,
            &&lbl_OP_RESUME,
            &&lbl_OP_YIELD,
            &&lbl_OP_FRAME,
            &&lbl_OP_FRAME_N,
            &&lbl_OP_NEWARRAY,
            &&lbl_OP_NEWMAP,
            &&lbl_OP_NEWSET,
            &&lbl_OP_NEWBUFFER,
            &&lbl_OP_APPEND,
            &&lbl_OP_SETADD,
            &&lbl_OP_GETFIELD,
            &&lbl_OP_SETFIELD,
            &&lbl_OP_GETFIELD_IDX,
            &&lbl_OP_SETFIELD_IDX,
            &&lbl_OP_GETINDEX,
            &&lbl_OP_SETINDEX,
            &&lbl_OP_INVOKE,
            &&lbl_OP_INVOKE_VT,
            &&lbl_OP_NEWCLASS,
            &&lbl_OP_NEWINSTANCE,
            &&lbl_OP_GETMETHOD,
            &&lbl_OP_CONCAT,
            &&lbl_OP_STRADD,
            &&lbl_OP_TOSTRING,
            &&lbl_OP_LEN,
            &&lbl_OP_PRINT,
            &&lbl_OP_SIN,
            &&lbl_OP_COS,
            &&lbl_OP_TAN,
            &&lbl_OP_ASIN,
            &&lbl_OP_ACOS,
            &&lbl_OP_ATAN,
            &&lbl_OP_ATAN2,
            &&lbl_OP_SQRT,
            &&lbl_OP_POW,
            &&lbl_OP_LOG,
            &&lbl_OP_ABS,
            &&lbl_OP_FLOOR,
            &&lbl_OP_CEIL,
            &&lbl_OP_DEG,
            &&lbl_OP_RAD,
            &&lbl_OP_EXP,
            &&lbl_OP_CLOCK,
            &&lbl_OP_LTJMPIFNOT,
            &&lbl_OP_LEJMPIFNOT,
            &&lbl_OP_FORPREP,
            &&lbl_OP_FORLOOP,
            &&lbl_OP_HALT,
        };

#define DISPATCH() goto *dispatch_table[ZEN_OP(*ip)]
#define CASE(op) lbl_##op:
#define NEXT()      \
    do              \
    {               \
        ++ip;       \
        DISPATCH(); \
    } while (0)

        DISPATCH();

#else

/* --- Switch (portável) --- */
#define DISPATCH() continue
#define CASE(op) case op:
#define NEXT()    \
    do            \
    {             \
        ++ip;     \
        continue; \
    } while (0)

        for (;;)
        {

#define SWITCH_TOP       \
    switch (ZEN_OP(*ip)) \
    {
            SWITCH_TOP

#endif

        /* =================================================================
        **  OPCODES
        ** ================================================================= */

        CASE(OP_LOADNIL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_nil();
            NEXT();
        }

        CASE(OP_LOADBOOL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_bool(ZEN_B(i) != 0);
            if (ZEN_C(i))
                ++ip; /* skip next if C */
            NEXT();
        }

        CASE(OP_LOADK)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = K[ZEN_BX(i)];
            NEXT();
        }

        CASE(OP_LOADI)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(ZEN_SBX(i));
            NEXT();
        }

        CASE(OP_MOVE)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = R[ZEN_B(i)];
            NEXT();
        }

        CASE(OP_GETGLOBAL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = globals_[ZEN_BX(i)];
            NEXT();
        }

        CASE(OP_SETGLOBAL)
        {
            uint32_t i = *ip;
            globals_[ZEN_BX(i)] = R[ZEN_A(i)];
            NEXT();
        }

        /* --- Aritmética --- */
        CASE(OP_ADD)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (is_string(vb) && is_string(vc))
            {
                R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, as_string(vb), as_string(vc)));
            }
            else if (is_string(vb) || is_string(vc))
            {
                /* Mixed string + non-string: coerce to string and concat */
                char buf[64], buf2[64];
                const char *sa; int la;
                const char *sb; int lb;
                if (is_string(vb)) { sa = as_cstring(vb); la = as_string(vb)->length; }
                else if (is_int(vb)) { la = int_to_cstr(vb.as.integer, buf); sa = buf; }
                else if (is_float(vb)) { la = snprintf(buf, sizeof(buf), "%g", vb.as.number); sa = buf; }
                else if (is_bool(vb)) { sa = vb.as.boolean ? "true" : "false"; la = vb.as.boolean ? 4 : 5; }
                else { sa = "nil"; la = 3; }

                if (is_string(vc)) { sb = as_cstring(vc); lb = as_string(vc)->length; }
                else if (is_int(vc)) { lb = int_to_cstr(vc.as.integer, buf2); sb = buf2; }
                else if (is_float(vc)) { lb = snprintf(buf2, sizeof(buf2), "%g", vc.as.number); sb = buf2; }
                else if (is_bool(vc)) { sb = vc.as.boolean ? "true" : "false"; lb = vc.as.boolean ? 4 : 5; }
                else { sb = "nil"; lb = 3; }

                int len = la + lb;
                char tmp[256];
                char *p = (len <= 256) ? tmp : (char *)malloc(len);
                memcpy(p, sa, la);
                memcpy(p + la, sb, lb);
                R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, p, len));
                if (p != tmp) free(p);
            }
            else
            {
                NUM_BINOP(+);
            }
            NEXT();
        }
        CASE(OP_SUB)
        {
            uint32_t i = *ip;
            NUM_BINOP(-);
            NEXT();
        }
        CASE(OP_MUL)
        {
            uint32_t i = *ip;
            NUM_BINOP(*);
            NEXT();
        }
        CASE(OP_DIV)
        {
            uint32_t i = *ip;
            /* Divisão sempre float (evitar div by zero int) */
            R[ZEN_A(i)] = val_float(to_number(R[ZEN_B(i)]) / to_number(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_MOD)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (vb.type == VAL_INT && vc.type == VAL_INT)
            {
                int32_t divisor = vc.as.integer;
                if (divisor == 0)
                    R[ZEN_A(i)] = val_int(0);
                else
                    R[ZEN_A(i)] = val_int(vb.as.integer % divisor);
            }
            else
            {
                double a = to_number(vb), b = to_number(vc);
                if (b == 0.0)
                    R[ZEN_A(i)] = val_float(__builtin_nan(""));
                else
                    R[ZEN_A(i)] = val_float(a - (int64_t)(a / b) * b);
            }
            NEXT();
        }
        CASE(OP_NEG)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (v.type == VAL_INT)
                R[ZEN_A(i)] = val_int((int64_t)(-(uint64_t)v.as.integer));
            else
                R[ZEN_A(i)] = val_float(-to_number(v));
            NEXT();
        }

        /* --- Superinstructions (immediate) --- */
        CASE(OP_ADDI)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)];
            int8_t imm = (int8_t)ZEN_C(i);
            if (vb.type == VAL_INT)
                R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer + (int64_t)imm));
            else
                R[ZEN_A(i)] = val_float(to_number(vb) + imm);
            NEXT();
        }
        CASE(OP_SUBI)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)];
            int8_t imm = (int8_t)ZEN_C(i);
            if (vb.type == VAL_INT)
                R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer - (int64_t)imm));
            else
                R[ZEN_A(i)] = val_float(to_number(vb) - imm);
            NEXT();
        }

        /* --- Bitwise --- */
        CASE(OP_BAND)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) & to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BOR)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) | to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BXOR)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) ^ to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BNOT)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(~to_integer(R[ZEN_B(i)]));
            NEXT();
        }
        CASE(OP_SHL)
        {
            uint32_t i = *ip;
            int64_t val = to_integer(R[ZEN_B(i)]);
            int shift = (int)(to_integer(R[ZEN_C(i)]) & 63);
            R[ZEN_A(i)] = val_int((int64_t)((uint64_t)val << shift));
            NEXT();
        }
        CASE(OP_SHR)
        {
            uint32_t i = *ip;
            int64_t val = to_integer(R[ZEN_B(i)]);
            int shift = (int)(to_integer(R[ZEN_C(i)]) & 63);
            /* Arithmetic right shift (sign-preserving) — portable implementation */
            R[ZEN_A(i)] = val_int((int64_t)(
                ((uint64_t)val >> shift) | (val < 0 ? ~(~(uint64_t)0 >> shift) : 0)));
            NEXT();
        }

        /* --- Comparação --- */
        CASE(OP_EQ)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_bool(values_equal(R[ZEN_B(i)], R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_LT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (is_string(vb) && is_string(vc))
                R[ZEN_A(i)] = val_bool(strcmp(as_cstring(vb), as_cstring(vc)) < 0);
            else if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i)] = val_bool(vb.as.integer < vc.as.integer);
            else
                R[ZEN_A(i)] = val_bool(to_number(vb) < to_number(vc));
            NEXT();
        }
        CASE(OP_LE)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (is_string(vb) && is_string(vc))
                R[ZEN_A(i)] = val_bool(strcmp(as_cstring(vb), as_cstring(vc)) <= 0);
            else if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i)] = val_bool(vb.as.integer <= vc.as.integer);
            else
                R[ZEN_A(i)] = val_bool(to_number(vb) <= to_number(vc));
            NEXT();
        }
        CASE(OP_NOT)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_bool(!is_truthy(R[ZEN_B(i)]));
            NEXT();
        }

        /* --- Jumps --- */
        CASE(OP_JMP)
        {
            uint32_t i = *ip;
            ip += ZEN_SBX(i);
            NEXT();
        }
        CASE(OP_JMPIF)
        {
            uint32_t i = *ip;
            if (is_truthy(R[ZEN_A(i)]))
                ip += ZEN_SBX(i);
            NEXT();
        }
        CASE(OP_JMPIFNOT)
        {
            uint32_t i = *ip;
            if (!is_truthy(R[ZEN_A(i)]))
                ip += ZEN_SBX(i);
            NEXT();
        }

        /* --- Funções --- */
        CASE(OP_CALL)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nargs = ZEN_B(i);
            int nresults = ZEN_C(i);
            ++ip;
            SAVE_IP();

            Value callee = R[a];
            if (is_closure(callee))
            {
                /* Script closure — hot path */
                ObjClosure *cl = as_closure(callee);
                ObjFunc *fn = cl->func;
                if (fiber->frame_count >= kMaxFrames)
                {
                    runtime_error("stack overflow");
                    return;
                }
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
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
            if (is_native(callee))
            {
                ObjNative *nat = as_native(callee);
                int nret = nat->fn(this, &R[a + 1], nargs);
                if (nret > 0)
                    R[a] = R[a + 1];
                else
                    R[a] = val_nil();
                DISPATCH();
            }
            if (is_struct_def(callee))
            {
                ObjStructDef *def = as_struct_def(callee);
                ObjStruct *s = (ObjStruct *)zen_alloc(&gc_, sizeof(ObjStruct));
                s->obj.type = OBJ_STRUCT;
                s->obj.color = GC_BLACK;
                s->obj.hash = 0;
                s->obj.interned = 0;
                s->obj._pad = 0;
                s->obj.gc_next = gc_.objects;
                gc_.objects = (Obj *)s;
                s->def = def;
                s->fields = (Value *)zen_alloc(&gc_, sizeof(Value) * def->num_fields);
                for (int32_t fi = 0; fi < def->num_fields; fi++)
                    s->fields[fi] = (fi < nargs) ? R[a + 1 + fi] : val_nil();
                R[a] = val_obj((Obj *)s);
                DISPATCH();
            }
            if (is_native_struct_def(callee))
            {
                NativeStructDef *def = as_native_struct_def(callee);
                ObjNativeStruct *ns = (ObjNativeStruct *)zen_alloc(&gc_, sizeof(ObjNativeStruct));
                ns->obj.type = OBJ_NATIVE_STRUCT;
                ns->obj.color = GC_BLACK;
                ns->obj.hash = 0;
                ns->obj.interned = 0;
                ns->obj._pad = 0;
                ns->obj.gc_next = gc_.objects;
                gc_.objects = (Obj *)ns;
                ns->def = def;
                ns->data = zen_alloc(&gc_, def->struct_size);
                memset(ns->data, 0, def->struct_size);
                if (def->ctor)
                    def->ctor(this, ns->data, nargs, &R[a + 1]);
                R[a] = val_obj((Obj *)ns);
                DISPATCH();
            }
            if (is_class(callee))
            {
                /* Class call → create instance + call init */
                ObjClass *klass = as_class(callee);

                /* Check if script is allowed to instantiate this class */
                if (!klass->constructable)
                {
                    runtime_error("class '%s' cannot be instantiated from script",
                                  klass->name->chars);
                    return;
                }

                ObjInstance *inst = new_instance(&gc_, klass);
                R[a] = val_obj((Obj *)inst);

                /* Call native constructor if the class (or parent) has one */
                ObjClass *ctor_src = klass;
                while (ctor_src && !ctor_src->native_ctor)
                    ctor_src = ctor_src->parent;
                if (ctor_src && ctor_src->native_ctor)
                {
                    inst->native_data = ctor_src->native_ctor(this, nargs, &R[a + 1]);
                }

                /* Look for init method */
                ObjString *s_init = intern_string(&gc_, "init", 4, hash_string("init", 4));

                bool found;
                Value init_method = map_get(klass->methods, val_obj((Obj *)s_init), &found);
                if (!found && klass->parent)
                    init_method = map_get(klass->parent->methods, val_obj((Obj *)s_init), &found);

                if (found && is_closure(init_method))
                {
                    ObjClosure *cl = as_closure(init_method);
                    ObjFunc *fn = cl->func;
                    /* Check arity (init's arity = user params, self is implicit) */
                    if (fn->arity >= 0 && nargs != fn->arity)
                    {
                        runtime_error("init() expects %d args but got %d", fn->arity, nargs);
                        return;
                    }
                    /* Set up frame: R[a] = instance (self), args at R[a+1..] */
                    if (fiber->frame_count >= kMaxFrames)
                    {
                        runtime_error("stack overflow");
                        return;
                    }
                    CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                    new_frame->closure = cl;
                    new_frame->func = fn;
                    new_frame->ip = fn->code;
                    new_frame->base = &R[a]; /* self at base[0], args at base[1..] */
                    new_frame->ret_reg = a;
                    new_frame->ret_count = nresults;
                    fiber->stack_top = new_frame->base + fn->num_regs;
                    LOAD_STATE();
                    DISPATCH();
                }
                else if (nargs > 0 && !ctor_src)
                {
                    /* Error only if no init AND no native_ctor handled the args */
                    runtime_error("class '%s' has no init() but received %d args",
                                  klass->name->chars, nargs);
                    return;
                }
                /* No init and no args (or native_ctor consumed them) — just return the instance */
                DISPATCH();
            }
            runtime_error("attempt to call non-function");
            return;
        }

        CASE(OP_CALLGLOBAL)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nargs = ZEN_B(i);
            int nresults = ZEN_C(i);
            ++ip;
            int gidx = ZEN_BX(*ip); /* word 2: global index */
            ++ip;
            SAVE_IP();

            Value callee = globals_[gidx];
            if (is_closure(callee))
            {
                ObjClosure *cl = as_closure(callee);
                ObjFunc *fn = cl->func;
                if (fiber->frame_count >= kMaxFrames)
                {
                    runtime_error("stack overflow");
                    return;
                }
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
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
            if (is_native(callee))
            {
                ObjNative *nat = as_native(callee);
                int nret = nat->fn(this, &R[a + 1], nargs);
                if (nret > 0)
                    R[a] = R[a + 1];
                else
                    R[a] = val_nil();
                DISPATCH();
            }
            runtime_error("attempt to call non-function");
            return;
        }

        CASE(OP_RETURN)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nresults = ZEN_B(i);

            /* Close upvalues do frame actual — skip se não há nenhum aberto */
            if (fiber->open_upvalues && fiber->open_upvalues->location >= frame->base)
                close_upvalues(fiber, frame->base);

            /* Copiar resultados para o caller */
            int ret_reg = frame->ret_reg;
            int ret_count = frame->ret_count;
            Value *dest = frame->base - 1; /* posição do callable no caller */

            fiber->frame_count--;
            if (fiber->frame_count == 0)
            {
                /* Retorno do top-level */
                fiber->state = FIBER_DONE;
                if (fiber->caller)
                {
                    fiber->caller->transfer_value = nresults > 0 ? R[a] : val_nil();
                    fiber->caller->state = FIBER_RUNNING;
                    current_fiber_ = fiber->caller;
                }
                return;
            }

            /* Copiar resultados */
            CallFrame *caller_frame = &fiber->frames[fiber->frame_count - 1];
            Value *caller_base = caller_frame->base;
            int copy_count = ret_count < 0 ? nresults : ret_count;
            if (copy_count > nresults)
                copy_count = nresults;

            /* Fast path: single return (most common), then void, then multi */
            if (__builtin_expect(copy_count == 1, 1))
            {
                caller_base[ret_reg] = R[a];
            }
            else if (__builtin_expect(copy_count <= 0, 0))
            {
                /* void return — nothing to copy */
            }
            else
            {
                for (int j = 0; j < copy_count; j++)
                    caller_base[ret_reg + j] = R[a + j];
            }
            /* Nil-fill remaining */
            for (int j = copy_count; j < ret_count && ret_count > 0; j++)
            {
                caller_base[ret_reg + j] = val_nil();
            }

            fiber->stack_top = caller_base + caller_frame->func->num_regs;
            LOAD_STATE();
            DISPATCH();
        }

        /* --- Closures / Upvalues --- */
        CASE(OP_CLOSURE)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            ObjFunc *fn = as_func(K[ZEN_BX(i)]);

            int nuv = fn->upvalue_count;
            ObjClosure *cl = (ObjClosure *)zen_alloc(&gc_, sizeof(ObjClosure));
            cl->obj.type = OBJ_CLOSURE;
            cl->obj.color = GC_BLACK;
            cl->obj.interned = 0;
            cl->obj.hash = 0;
            cl->obj.gc_next = gc_.objects;
            gc_.objects = (Obj *)cl;
            cl->func = fn;
            cl->upvalue_count = nuv;
            if (nuv > 0)
            {
                cl->upvalues = (ObjUpvalue **)zen_alloc(&gc_, nuv * sizeof(ObjUpvalue *));
                for (int j = 0; j < nuv; j++)
                {
                    UpvalDesc &desc = fn->upval_descs[j];
                    if (desc.is_local)
                    {
                        /* Capture from enclosing frame's registers */
                        cl->upvalues[j] = capture_upvalue(fiber, &R[desc.index]);
                    }
                    else
                    {
                        /* Copy from enclosing closure's upvalue */
                        cl->upvalues[j] = UV[desc.index];
                    }
                }
            }
            else
            {
                cl->upvalues = nullptr;
            }

            R[a] = val_obj((Obj *)cl);
            NEXT();
        }

        CASE(OP_GETUPVAL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = *UV[ZEN_B(i)]->location;
            NEXT();
        }

        CASE(OP_SETUPVAL)
        {
            uint32_t i = *ip;
            *UV[ZEN_B(i)]->location = R[ZEN_A(i)];
            NEXT();
        }

        CASE(OP_CLOSE)
        {
            uint32_t i = *ip;
            close_upvalues(fiber, &R[ZEN_A(i)]);
            NEXT();
        }

        /* --- Fibers --- */
        CASE(OP_NEWFIBER)
        {
            uint32_t i = *ip;
            ObjClosure *cl = as_closure(R[ZEN_B(i)]);
            ObjFiber *f = new_fiber(cl, 256);
            R[ZEN_A(i)] = val_obj((Obj *)f);
            NEXT();
        }

        CASE(OP_RESUME)
        {
            uint32_t i = *ip;
            ObjFiber *target = as_fiber(R[ZEN_B(i)]);
            Value send_val = R[ZEN_C(i)];
            ++ip;
            SAVE_IP();

            if (target->state == FIBER_DONE)
            {
                R[ZEN_A(i)] = val_nil();
                DISPATCH();
            }
            if (target->state == FIBER_RUNNING)
            {
                runtime_error("cannot resume running fiber");
                return;
            }

            target->transfer_value = send_val;
            target->caller = fiber;
            target->state = FIBER_RUNNING;
            fiber->state = FIBER_SUSPENDED;
            current_fiber_ = target;

            /* If resuming a suspended fiber (not first time), write
               transfer_value into the yield_dest register */
            if (target->yield_dest >= 0)
            {
                CallFrame &tf = target->frames[target->frame_count - 1];
                Value *target_regs = tf.base;
                target_regs[target->yield_dest] = send_val;
                target->yield_dest = -1;
            }

            if (fiber_depth_ >= kMaxFiberDepth)
            {
                runtime_error("fiber resume depth exceeded");
                return;
            }
            ++fiber_depth_;
            execute(target);
            --fiber_depth_;

            /* Voltámos — target fez yield ou terminou */
            fiber->state = FIBER_RUNNING;
            current_fiber_ = fiber;
            R[ZEN_A(i)] = target->transfer_value;
            LOAD_STATE();
            DISPATCH();
        }

        CASE(OP_YIELD)
        {
            uint32_t i = *ip;
            ++ip;
            SAVE_IP();

            if (!fiber->caller)
            {
                runtime_error("yield outside of fiber");
                return;
            }

            fiber->transfer_value = R[ZEN_B(i)];
            fiber->yield_dest = ZEN_A(i);
            fiber->state = FIBER_SUSPENDED;
            /* Retorna ao execute() do caller (que está em OP_RESUME) */
            return;
        }

        CASE(OP_FRAME)
        {
            ++ip;
            SAVE_IP();
            fiber->frame_speed = 100;
            fiber->state = FIBER_SUSPENDED;
            return;
        }

        CASE(OP_FRAME_N)
        {
            uint32_t i = *ip;
            ++ip;
            SAVE_IP();
            Value fv = R[ZEN_A(i)];
            fiber->frame_speed = (fv.type == VAL_INT) ? fv.as.integer : 100;
            fiber->state = FIBER_SUSPENDED;
            return;
        }

        /* --- Collections --- */
        CASE(OP_NEWARRAY)
        {
            uint32_t i = *ip;
            ObjArray *arr = new_array(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)arr);
            NEXT();
        }

        CASE(OP_NEWMAP)
        {
            uint32_t i = *ip;
            ObjMap *map = new_map(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)map);
            NEXT();
        }

        CASE(OP_NEWSET)
        {
            uint32_t i = *ip;
            ObjSet *set = new_set(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)set);
            NEXT();
        }

        CASE(OP_NEWBUFFER)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int b = ZEN_B(i);
            BufferType btype = (BufferType)ZEN_C(i);
            Value arg = R[b];
            if (is_int(arg)) {
                int32_t count = arg.as.integer;
                if (count < 0) { runtime_error("buffer size must be non-negative"); return; }
                ObjBuffer *buf = new_buffer(&gc_, btype, count);
                R[a] = val_obj((Obj *)buf);
            } else if (is_array(arg)) {
                ObjArray *src = as_array(arg);
                int32_t count = arr_count(src);
                ObjBuffer *buf = new_buffer(&gc_, btype, count);
                for (int32_t idx = 0; idx < count; idx++) {
                    double v = 0;
                    if (is_int(src->data[idx])) v = (double)src->data[idx].as.integer;
                    else if (is_float(src->data[idx])) v = src->data[idx].as.number;
                    buffer_set(buf, idx, v);
                }
                R[a] = val_obj((Obj *)buf);
            } else {
                runtime_error("buffer constructor expects int or array");
                return;
            }
            NEXT();
        }

        CASE(OP_APPEND)
        {
            uint32_t i = *ip;
            ObjArray *arr = as_array(R[ZEN_A(i)]);
            array_push(&gc_, arr, R[ZEN_B(i)]);
            NEXT();
        }

        CASE(OP_SETADD)
        {
            uint32_t i = *ip;
            ObjSet *set = as_set(R[ZEN_A(i)]);
            set_add(&gc_, set, R[ZEN_B(i)]);
            NEXT();
        }

        /* --- Field/Index access --- */
        CASE(OP_GETFIELD)
        {
            /* R[A] = R[B].field_by_name(constants[C]) — pointer compare fallback */
            uint32_t i = *ip;
            Value receiver = R[ZEN_B(i)];
            ObjString *name = as_string(K[ZEN_C(i)]);
            if (is_struct(receiver))
            {
                ObjStruct *s = as_struct(receiver);
                ObjStructDef *def = s->def;
                for (int32_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        R[ZEN_A(i)] = s->fields[fi];
                        goto getfield_done;
                    }
                }
                runtime_error("struct '%s' has no field '%s'", def->name->chars, name->chars);
                return;
            }
            if (is_native_struct(receiver))
            {
                ObjNativeStruct *ns = as_native_struct(receiver);
                NativeStructDef *def = ns->def;
                for (int16_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->fields[fi].name == name) /* pointer compare (interned) */
                    {
                        uint8_t *ptr = (uint8_t *)ns->data + def->fields[fi].offset;
                        switch (def->fields[fi].type)
                        {
                        case FIELD_BYTE:    R[ZEN_A(i)] = val_int(*(uint8_t *)ptr); break;
                        case FIELD_INT:     R[ZEN_A(i)] = val_int(*(int32_t *)ptr); break;
                        case FIELD_UINT:    R[ZEN_A(i)] = val_int((int64_t)*(uint32_t *)ptr); break;
                        case FIELD_FLOAT:   R[ZEN_A(i)] = val_float(*(float *)ptr); break;
                        case FIELD_DOUBLE:  R[ZEN_A(i)] = val_float(*(double *)ptr); break;
                        case FIELD_BOOL:    R[ZEN_A(i)] = val_bool(*(bool *)ptr); break;
                        case FIELD_POINTER: R[ZEN_A(i)] = val_ptr(*(void **)ptr); break;
                        }
                        goto getfield_done;
                    }
                }
                runtime_error("native struct '%s' has no field '%s'", def->name->chars, name->chars);
                return;
            }
            if (is_instance(receiver))
            {
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;
                for (int32_t fi = 0; fi < klass->num_fields; fi++)
                {
                    if (klass->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        R[ZEN_A(i)] = inst->fields[fi];
                        goto getfield_done;
                    }
                }
                runtime_error("instance of '%s' has no field '%s'", klass->name->chars, name->chars);
                return;
            }
            runtime_error("cannot access field '%s' on this type", name->chars);
            return;
            getfield_done:
            NEXT();
        }
        CASE(OP_SETFIELD)
        {
            /* R[A].field_by_name(constants[B]) = R[C] — pointer compare fallback */
            uint32_t i = *ip;
            Value receiver = R[ZEN_A(i)];
            ObjString *name = as_string(K[ZEN_B(i)]);
            Value val = R[ZEN_C(i)];
            if (is_struct(receiver))
            {
                ObjStruct *s = as_struct(receiver);
                ObjStructDef *def = s->def;
                for (int32_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        s->fields[fi] = val;
                        goto setfield_done;
                    }
                }
                runtime_error("struct '%s' has no field '%s'", def->name->chars, name->chars);
                return;
            }
            if (is_native_struct(receiver))
            {
                ObjNativeStruct *ns = as_native_struct(receiver);
                NativeStructDef *def = ns->def;
                for (int16_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->fields[fi].name == name) /* pointer compare (interned) */
                    {
                        if (def->fields[fi].read_only)
                        {
                            runtime_error("field '%s' is read-only", name->chars);
                            return;
                        }
                        uint8_t *ptr = (uint8_t *)ns->data + def->fields[fi].offset;
                        switch (def->fields[fi].type)
                        {
                        case FIELD_BYTE:    *(uint8_t *)ptr = (uint8_t)to_integer(val); break;
                        case FIELD_INT:     *(int32_t *)ptr = (int32_t)to_integer(val); break;
                        case FIELD_UINT:    *(uint32_t *)ptr = (uint32_t)to_integer(val); break;
                        case FIELD_FLOAT:   *(float *)ptr = (float)to_number(val); break;
                        case FIELD_DOUBLE:  *(double *)ptr = to_number(val); break;
                        case FIELD_BOOL:    *(bool *)ptr = is_truthy(val); break;
                        case FIELD_POINTER: *(void **)ptr = val.as.pointer; break;
                        }
                        goto setfield_done;
                    }
                }
                runtime_error("native struct '%s' has no field '%s'", def->name->chars, name->chars);
                return;
            }
            if (is_instance(receiver))
            {
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;
                for (int32_t fi = 0; fi < klass->num_fields; fi++)
                {
                    if (klass->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        inst->fields[fi] = val;
                        goto setfield_done;
                    }
                }
                runtime_error("instance of '%s' has no field '%s'", klass->name->chars, name->chars);
                return;
            }
            runtime_error("cannot set field '%s' on this type", name->chars);
            return;
            setfield_done:
            NEXT();
        }
        CASE(OP_GETFIELD_IDX)
        {
            /* R[A] = R[B].fields[C] — O(1) direct index */
            uint32_t i = *ip;
            Value obj = R[ZEN_B(i)];
            if (is_instance(obj))
                R[ZEN_A(i)] = as_instance(obj)->fields[ZEN_C(i)];
            else
                R[ZEN_A(i)] = as_struct(obj)->fields[ZEN_C(i)];
            NEXT();
        }
        CASE(OP_SETFIELD_IDX)
        {
            /* R[A].fields[B] = R[C] — O(1) direct index */
            uint32_t i = *ip;
            Value obj = R[ZEN_A(i)];
            if (is_instance(obj))
                as_instance(obj)->fields[ZEN_B(i)] = R[ZEN_C(i)];
            else
                as_struct(obj)->fields[ZEN_B(i)] = R[ZEN_C(i)];
            NEXT();
        }
        CASE(OP_GETINDEX)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_B(i)];
            Value key = R[ZEN_C(i)];
            if (is_array(container)) {
                if (!is_int(key)) { runtime_error("array index must be integer"); return; }
                R[ZEN_A(i)] = array_get(as_array(container), key.as.integer);
            } else if (is_map(container)) {
                bool found;
                R[ZEN_A(i)] = map_get(as_map(container), key, &found);
                if (!found) R[ZEN_A(i)] = val_nil();
            } else if (is_buffer(container)) {
                if (!is_int(key)) { runtime_error("buffer index must be integer"); return; }
                ObjBuffer *buf = as_buffer(container);
                int32_t idx = (int32_t)key.as.integer;
                if ((uint32_t)idx >= (uint32_t)buf->count) { runtime_error("buffer index out of bounds"); return; }
                double v = buffer_get(buf, idx);
                /* Float types return float, integer types return int64 */
                if (buf->btype >= BUF_FLOAT32)
                    R[ZEN_A(i)] = val_float(v);
                else
                    R[ZEN_A(i)] = val_int((int64_t)v);
            } else {
                runtime_error("cannot index value");
                return;
            }
            NEXT();
        }
        CASE(OP_SETINDEX)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_A(i)];
            Value key = R[ZEN_B(i)];
            Value val = R[ZEN_C(i)];
            if (is_array(container)) {
                if (!is_int(key)) { runtime_error("array index must be integer"); return; }
                int32_t idx = key.as.integer;
                ObjArray *arr = as_array(container);
                if ((uint32_t)idx < (uint32_t)arr_count(arr)) {
                    arr->data[idx] = val;
                } else {
                    runtime_error("array index out of bounds");
                    return;
                }
            } else if (is_map(container)) {
                map_set(&gc_, as_map(container), key, val);
            } else if (is_buffer(container)) {
                if (!is_int(key)) { runtime_error("buffer index must be integer"); return; }
                ObjBuffer *buf = as_buffer(container);
                int32_t idx = key.as.integer;
                if ((uint32_t)idx >= (uint32_t)buf->count) { runtime_error("buffer index out of bounds"); return; }
                double v = 0;
                if (is_int(val)) v = (double)val.as.integer;
                else if (is_float(val)) v = val.as.number;
                else { runtime_error("buffer only accepts numbers"); return; }
                buffer_set(buf, idx, v);
            } else {
                runtime_error("cannot index value");
                return;
            }
            NEXT();
        }

        /* --- OP_INVOKE: method dispatch by receiver type --- */
        /* 2-word instruction: word1=[OP_INVOKE|A|B|C], word2=name_ki */
        /* A=base (receiver at R[A], args at R[A+1]..R[A+B]), result → R[A] */
        CASE(OP_INVOKE)
        {
            uint32_t i = *ip;
            uint8_t base = ZEN_A(i);
            uint8_t arg_count = ZEN_B(i);
            uint32_t name_ki = *(++ip); /* second word: method name constant index */
            Value receiver = R[base];
            ObjString *method = as_string(K[name_ki]);
            const char *mname = method->chars;
            int mlen = method->length;
            Value *args = &R[base + 1];

            if (is_array(receiver))
            {
                #include "invoke_array.inl"
            }
            else if (is_string(receiver))
            {
                #include "invoke_string.inl"
            }
            else if (is_map(receiver))
            {
                #include "invoke_map.inl"
            }
            else if (is_set(receiver))
            {
                #include "invoke_set.inl"
            }
            else if (is_buffer(receiver))
            {
                #include "invoke_buffer.inl"
            }
            else if (is_instance(receiver))
            {
                /* Method call on class instance */
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;

                /* Look up method in class hierarchy */
                bool mfound = false;
                Value mval;
                ObjClass *search = klass;
                while (search)
                {
                    mval = map_get(search->methods, val_obj((Obj *)method), &mfound);
                    if (mfound) break;
                    search = search->parent;
                }

                if (!mfound)
                {
                    runtime_error("'%s' has no method '%s'", klass->name->chars, mname);
                    return;
                }

                if (is_closure(mval))
                {
                    ObjClosure *cl = as_closure(mval);
                    ObjFunc *fn = cl->func;
                    /* self goes at R[base], args at R[base+1..] */
                    /* R[base] already has receiver (self) */
                    if (fn->arity >= 0 && arg_count != fn->arity)
                    {
                        runtime_error("%s.%s() expects %d args but got %d",
                                      klass->name->chars, mname, fn->arity, arg_count);
                        return;
                    }
                    if (fiber->frame_count >= kMaxFrames)
                    {
                        runtime_error("stack overflow");
                        return;
                    }
                    ++ip;
                    SAVE_IP();
                    CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                    new_frame->closure = cl;
                    new_frame->func = fn;
                    new_frame->ip = fn->code;
                    new_frame->base = &R[base]; /* base[0]=self, base[1..]=args */
                    new_frame->ret_reg = base;
                    new_frame->ret_count = 1;
                    fiber->stack_top = new_frame->base + fn->num_regs;
                    LOAD_STATE();
                    DISPATCH();
                }
                else if (is_native(mval))
                {
                    ObjNative *nat = as_native(mval);
                    int nret = nat->fn(this, &R[base], arg_count + 1); /* +1 for self */
                    if (nret > 0)
                        R[base] = R[base];
                    else
                        R[base] = val_nil();
                }
                else
                {
                    runtime_error("'%s.%s' is not callable", klass->name->chars, mname);
                    return;
                }
            }
            else
            {
                runtime_error("cannot invoke method '%s' on this type", mname);
                return;
            }
            NEXT();
        }

        CASE(OP_INVOKE_VT)
        {
            /* Single-word vtable dispatch: A=base, B=arg_count, C=slot_idx */
            uint32_t i = *ip;
            uint8_t base = ZEN_A(i);
            uint8_t arg_count = ZEN_B(i);
            uint8_t slot = ZEN_C(i);

            ObjInstance *inst = as_instance(R[base]);
            ObjClass *klass = inst->klass;
            Value mval = klass->vtable[slot];

            if (is_closure(mval))
            {
                ObjClosure *cl = as_closure(mval);
                ObjFunc *fn = cl->func;
                if (fiber->frame_count >= kMaxFrames)
                {
                    runtime_error("stack overflow");
                    return;
                }
                ++ip;
                SAVE_IP();
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                new_frame->closure = cl;
                new_frame->func = fn;
                new_frame->ip = fn->code;
                new_frame->base = &R[base]; /* base[0]=self, base[1..]=args */
                new_frame->ret_reg = base;
                new_frame->ret_count = 1;
                fiber->stack_top = new_frame->base + fn->num_regs;
                LOAD_STATE();
                DISPATCH();
            }
            else if (is_native(mval))
            {
                ObjNative *nat = as_native(mval);
                int nret = nat->fn(this, &R[base], arg_count + 1);
                if (nret > 0)
                    R[base] = R[base];
                else
                    R[base] = val_nil();
            }
            else
            {
                runtime_error("vtable slot %d is nil (method not found)", slot);
                return;
            }
            NEXT();
        }

        /* --- Classes (stubs) --- */
        CASE(OP_NEWCLASS)
        {
            runtime_error("classes not yet implemented");
            return;
        }
        CASE(OP_NEWINSTANCE)
        {
            runtime_error("classes not yet implemented");
            return;
        }
        CASE(OP_GETMETHOD)
        {
            runtime_error("method access not yet implemented");
            return;
        }

        /* --- Misc --- */
        CASE(OP_CONCAT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];

            /* Fast path: both are strings already */
            if (is_string(vb) && is_string(vc))
            {
                R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, as_string(vb), as_string(vc)));
                NEXT();
            }

            /* Slow path: convert to strings then concat */
            char buf_b[64], buf_c[64];
            const char *sb; int lb;
            const char *sc; int lc;

            if (is_string(vb)) { sb = as_cstring(vb); lb = as_string(vb)->length; }
            else if (is_int(vb)) { lb = int_to_cstr(vb.as.integer, buf_b); sb = buf_b; }
            else if (is_float(vb)) { lb = snprintf(buf_b, sizeof(buf_b), "%g", vb.as.number); sb = buf_b; }
            else if (is_bool(vb)) { sb = vb.as.boolean ? "true" : "false"; lb = vb.as.boolean ? 4 : 5; }
            else if (is_nil(vb)) { sb = "nil"; lb = 3; }
            else { sb = "<obj>"; lb = 5; }

            if (is_string(vc)) { sc = as_cstring(vc); lc = as_string(vc)->length; }
            else if (is_int(vc)) { lc = int_to_cstr(vc.as.integer, buf_c); sc = buf_c; }
            else if (is_float(vc)) { lc = snprintf(buf_c, sizeof(buf_c), "%g", vc.as.number); sc = buf_c; }
            else if (is_bool(vc)) { sc = vc.as.boolean ? "true" : "false"; lc = vc.as.boolean ? 4 : 5; }
            else if (is_nil(vc)) { sc = "nil"; lc = 3; }
            else { sc = "<obj>"; lc = 5; }

            /* Stack buffer — no malloc for results ≤ 256 */
            int len = lb + lc;
            char buf[256];
            char *tmp = (len <= 256) ? buf : (char *)malloc(len);
            memcpy(tmp, sb, lb);
            memcpy(tmp + lb, sc, lc);
            R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, tmp, len));
            if (tmp != buf) free(tmp);
            NEXT();
        }

        CASE(OP_STRADD)
        {
            uint32_t i = *ip;
            uint8_t a = ZEN_A(i);
            Value va = R[a], vb = R[ZEN_B(i)];
            if (is_string(va) && is_string(vb))
            {
                ObjString *result = string_append_inplace(&gc_, as_string(va), as_string(vb));
                R[a] = val_obj((Obj *)result);
            }
            else if (is_string(va) || is_string(vb))
            {
                /* At least one is string — coerce the other and concat */
                char buf[64], buf2[64];
                const char *sa; int la;
                const char *sb; int lb;
                if (is_string(va)) { sa = as_cstring(va); la = as_string(va)->length; }
                else if (is_int(va)) { la = int_to_cstr(va.as.integer, buf); sa = buf; }
                else if (is_float(va)) { la = snprintf(buf, sizeof(buf), "%g", va.as.number); sa = buf; }
                else { sa = "nil"; la = 3; }

                if (is_string(vb)) { sb = as_cstring(vb); lb = as_string(vb)->length; }
                else if (is_int(vb)) { lb = int_to_cstr(vb.as.integer, buf2); sb = buf2; }
                else if (is_float(vb)) { lb = snprintf(buf2, sizeof(buf2), "%g", vb.as.number); sb = buf2; }
                else { sb = "nil"; lb = 3; }

                int len = la + lb;
                char tmp[256];
                char *p = (len <= 256) ? tmp : (char *)malloc(len);
                memcpy(p, sa, la);
                memcpy(p + la, sb, lb);
                R[a] = val_obj((Obj *)new_string(&gc_, p, len));
                if (p != tmp) free(p);
            }
            else
            {
                /* Both numeric — regular addition */
                if (is_int(va) && is_int(vb))
                    R[a] = val_int(va.as.integer + vb.as.integer);
                else
                {
                    double da = is_int(va) ? (double)va.as.integer : va.as.number;
                    double db = is_int(vb) ? (double)vb.as.integer : vb.as.number;
                    R[a] = val_float(da + db);
                }
            }
            NEXT();
        }

        CASE(OP_TOSTRING)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (is_string(v))
            {
                R[ZEN_A(i)] = v;
            }
            else
            {
                /* Convert value to string representation */
                char buf[64];
                int len = 0;
                if (is_nil(v))
                {
                    len = snprintf(buf, sizeof(buf), "nil");
                }
                else if (is_bool(v))
                {
                    len = snprintf(buf, sizeof(buf), "%s", v.as.boolean ? "true" : "false");
                }
                else if (is_int(v))
                {
                    len = int_to_cstr(v.as.integer, buf);
                }
                else if (is_float(v))
                {
                    len = snprintf(buf, sizeof(buf), "%g", v.as.number);
                }
                else
                {
                    len = snprintf(buf, sizeof(buf), "<object>");
                }
                R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, buf, len));
            }
            NEXT();
        }

        CASE(OP_LEN)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (is_string(v))
                R[ZEN_A(i)] = val_int(as_string(v)->length);
            else if (is_array(v))
                R[ZEN_A(i)] = val_int(arr_count(as_array(v)));
            else if (is_map(v))
                R[ZEN_A(i)] = val_int(as_map(v)->count);
            else if (is_set(v))
                R[ZEN_A(i)] = val_int(as_set(v)->count);
            else if (is_buffer(v))
                R[ZEN_A(i)] = val_int(as_buffer(v)->count);
            else
                R[ZEN_A(i)] = val_int(0);
            NEXT();
        }

        CASE(OP_PRINT)
        {
            uint32_t i = *ip;
            if (!ZEN_C(i))
            { /* C=0: normal print value */
                Value v = R[ZEN_A(i)];
                switch (v.type)
                {
                case VAL_NIL:
                    zen_writes("nil");
                    break;
                case VAL_BOOL:
                    zen_writes(v.as.boolean ? "true" : "false");
                    break;
                case VAL_INT:
                {
                    char ibuf[21];
                    int ilen = int_to_cstr(v.as.integer, ibuf);
                    zen_write(ibuf, (size_t)ilen);
                    break;
                }
                case VAL_FLOAT:
                {
                    char fbuf[32];
                    int flen = snprintf(fbuf, sizeof(fbuf), "%g", v.as.number);
                    zen_write(fbuf, (size_t)flen);
                    break;
                }
                case VAL_OBJ:
                    if (v.as.obj->type == OBJ_STRING)
                        zen_write(((ObjString *)v.as.obj)->chars, (size_t)((ObjString *)v.as.obj)->length);
                    else
                        print_value(v);
                    break;
                case VAL_PTR:
                {
                    char pbuf[32];
                    int plen = snprintf(pbuf, sizeof(pbuf), "<ptr %p>", v.as.pointer);
                    zen_write(pbuf, (size_t)plen);
                    break;
                }
                }
            }
            if (ZEN_B(i))
                zen_writeln();
            else
                zen_writes(" ");
            NEXT();
        }

        /* --- Math builtins --- */
        CASE(OP_SIN)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(sin(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_COS)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(cos(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_TAN)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(tan(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_ASIN)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(asin(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_ACOS)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(acos(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_ATAN)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(atan(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_ATAN2)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(atan2(to_number(R[ZEN_B(i)]), to_number(R[ZEN_C(i)])));
            NEXT();
        }
        CASE(OP_SQRT)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(sqrt(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_POW)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(pow(to_number(R[ZEN_B(i)]), to_number(R[ZEN_C(i)])));
            NEXT();
        }
        CASE(OP_LOG)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(log(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_ABS)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (v.type == VAL_INT)
                R[ZEN_A(i)] = val_int(v.as.integer < 0
                    ? (int32_t)(-(uint32_t)v.as.integer)
                    : v.as.integer);
            else
                R[ZEN_A(i)] = val_float(fabs(to_number(v)));
            NEXT();
        }
        CASE(OP_FLOOR)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(floor(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_CEIL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(ceil(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_DEG)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(to_number(R[ZEN_B(i)]) * (180.0 / 3.14159265358979323846));
            NEXT();
        }
        CASE(OP_RAD)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(to_number(R[ZEN_B(i)]) * (3.14159265358979323846 / 180.0));
            NEXT();
        }
        CASE(OP_EXP)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float(exp(to_number(R[ZEN_B(i)])));
            NEXT();
        }
        CASE(OP_CLOCK)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_float((double)clock() / CLOCKS_PER_SEC);
            NEXT();
        }

        /* --- Fused comparison + jump superinstructions (2-word) --- */
        CASE(OP_LTJMPIFNOT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            bool less;
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                less = vb.as.integer < vc.as.integer;
            else
                less = to_number(vb) < to_number(vc);
            ++ip; /* advance to the sBx word */
            if (!less)
                ip += ZEN_SBX(*ip);
            NEXT();
        }

        CASE(OP_LEJMPIFNOT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            bool le;
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                le = vb.as.integer <= vc.as.integer;
            else
                le = to_number(vb) <= to_number(vc);
            ++ip; /* advance to the sBx word */
            if (!le)
                ip += ZEN_SBX(*ip);
            NEXT();
        }

        CASE(OP_FORPREP)
        {
            /* R[A]=counter, R[A+1]=limit, R[A+2]=step
               Subtract step so first FORLOOP increments to start value.
               Then jump to FORLOOP for initial test. */
            uint32_t i = *ip;
            int a = ZEN_A(i);
            R[a].as.integer -= R[a + 2].as.integer;
            ip += ZEN_SBX(i); /* jump to FORLOOP */
            NEXT();
        }

        CASE(OP_FORLOOP)
        {
            /* R[A] += R[A+2]; if still in range: pc += sBx (back to body) */
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int32_t counter = R[a].as.integer + R[a + 2].as.integer;
            int32_t limit = R[a + 1].as.integer;
            R[a].as.integer = counter;
            if (counter < limit)
                ip += ZEN_SBX(i); /* loop back */
            NEXT();
        }

        CASE(OP_HALT)
        {
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
