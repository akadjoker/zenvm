/*
** toy_vm_nan.c — VM register-based com NaN boxing
**
** Compile:  gcc -O2 -o toy_vm_nan toy_vm_nan.c && ./toy_vm_nan
**
** NaN boxing: encode ALL values in a single uint64_t (8 bytes vs 16).
**
** IEEE 754 double: quando exponent=0x7FF e mantissa≠0 → é NaN.
** Há 2^52 combinações de NaN — usamos o espaço para guardar tipos:
**
**   Se é float válido  → é um double literal (tipo implícito)
**   Se bits [63:50] == 0x7FFC (quiet NaN + tag bit) → payload = valor codificado
**
** Layout:
**   double válido (float):  bits[63:0] = IEEE 754 double
**   integer:                QNAN | TAG_INT | (int32 nos bits baixos)
**   bool:                   QNAN | TAG_BOOL | 0 ou 1
**   nil:                    QNAN | TAG_NIL
**   script func:            QNAN | TAG_FUNC_SCRIPT | fidx
**   native func:            QNAN | TAG_FUNC_NATIVE | (índice na tabela de natives)
**
** Vantagem: Value = 8 bytes. Cache-friendly. Sem branch no type check de aritmética
** quando ambos são double (caso mais comum em Lua).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* =========================================================
** PARTE 1 — NaN boxing
** ========================================================= */

/*
** Quiet NaN mask: exponent=0x7FF, quiet bit=1, tag bit=1
** bits [63:50] = 0x7FFC
** Anything with these top bits set is a "boxed" non-double value.
*/
#define QNAN        ((uint64_t)0x7FFC000000000000ULL)
#define TAG_INT     ((uint64_t)0x0001000000000000ULL)  /* bit 48 */
#define TAG_BOOL    ((uint64_t)0x0002000000000000ULL)  /* bit 49 */
#define TAG_NIL     ((uint64_t)0x0003000000000000ULL)
#define TAG_FSCRIPT ((uint64_t)0x0004000000000000ULL)
#define TAG_FNATIVE ((uint64_t)0x0005000000000000ULL)
#define TAG_MASK    ((uint64_t)0x000F000000000000ULL)  /* bits [51:48] */
#define PAYLOAD_MASK ((uint64_t)0x00000000FFFFFFFFULL) /* lower 32 bits */

typedef uint64_t Value;

/* Encode */
static inline Value val_double(double d) {
    Value v; memcpy(&v, &d, 8); return v;
}
static inline Value val_int(int32_t i) {
    return QNAN | TAG_INT | ((uint64_t)(uint32_t)i);
}
static inline Value val_bool(int b) {
    return QNAN | TAG_BOOL | (uint64_t)(b != 0);
}
static inline Value val_nil(void) {
    return QNAN | TAG_NIL;
}
static inline Value val_script(int32_t fidx) {
    return QNAN | TAG_FSCRIPT | (uint64_t)(uint32_t)fidx;
}
static inline Value val_native(int32_t nidx) {
    return QNAN | TAG_FNATIVE | (uint64_t)(uint32_t)nidx;
}

/* Decode / type check */
static inline int is_double(Value v) { return (v & QNAN) != QNAN; }
static inline int is_int(Value v)    { return (v & (QNAN|TAG_MASK)) == (QNAN|TAG_INT); }
static inline int is_bool(Value v)   { return (v & (QNAN|TAG_MASK)) == (QNAN|TAG_BOOL); }
static inline int is_nil(Value v)    { return v == (QNAN|TAG_NIL); }
static inline int is_fscript(Value v){ return (v & (QNAN|TAG_MASK)) == (QNAN|TAG_FSCRIPT); }
static inline int is_fnative(Value v){ return (v & (QNAN|TAG_MASK)) == (QNAN|TAG_FNATIVE); }

static inline double  as_double(Value v) { double d; memcpy(&d, &v, 8); return d; }
static inline int32_t as_int(Value v)    { return (int32_t)(v & PAYLOAD_MASK); }
static inline int     as_bool(Value v)   { return (int)(v & 1); }
static inline int32_t as_fidx(Value v)   { return (int32_t)(v & PAYLOAD_MASK); }
static inline int32_t as_nidx(Value v)   { return (int32_t)(v & PAYLOAD_MASK); }

/* Conversão numérica rápida */
static inline double to_number(Value v) {
    if (is_int(v)) return (double)as_int(v);
    return as_double(v);  /* assume que é double se não é int */
}

static void print_value(Value v) {
    if (is_nil(v))         printf("nil");
    else if (is_int(v))    printf("%d", as_int(v));
    else if (is_bool(v))   printf("%s", as_bool(v) ? "true" : "false");
    else if (is_double(v)) printf("%g", as_double(v));
    else if (is_fscript(v))printf("<script fn #%d>", as_fidx(v));
    else if (is_fnative(v))printf("<native fn #%d>", as_nidx(v));
    else                   printf("<unknown>");
}

/* =========================================================
** PARTE 2 — Opcodes e instrução
** ========================================================= */

typedef enum {
    OP_LOADI,    /* R[A] = (signed)B                */
    OP_LOADG,    /* R[A] = globals[B]               */
    OP_SETG,     /* globals[B] = R[A]               */
    OP_MOVE,     /* R[A] = R[B]                     */
    OP_ADD,      /* R[A] = R[B] + R[C]              */
    OP_SUB,      /* R[A] = R[B] - R[C]              */
    OP_MUL,      /* R[A] = R[B] * R[C]              */
    OP_LT,       /* R[A] = (R[B] <  R[C])           */
    OP_JMP,      /* pc += sB                        */
    OP_JMPIF,    /* se R[A]: pc += sB               */
    OP_CALL,     /* R[A](R[A+1]..R[A+B]) → R[A]    */
    OP_RETURN,   /* retorna R[A]                    */
    OP_PRINT,    /* print R[A]                      */
    OP_HALT,
} OpCode;

typedef unsigned int Instruction;

#define ENCODE(op, a, b, c) ((unsigned)((op)<<24)|((a)<<16)|((b)<<8)|(c))
#define GET_OP(i)   (((i)>>24)&0xFF)
#define GET_A(i)    (((i)>>16)&0xFF)
#define GET_B(i)    (((i)>>8)&0xFF)
#define GET_C(i)    (((i))&0xFF)
#define GET_sB(i)   ((int)(signed char)GET_B(i))

/* =========================================================
** PARTE 3 — VM structures
** ========================================================= */

#define MAX_REGS    64
#define MAX_GLOBALS 64
#define MAX_FUNCS   32
#define MAX_FRAMES  64
#define MAX_NATIVES 16

typedef struct {
    Instruction *code;
    int nparams;
    const char *name;
} ScriptFunc;

typedef struct {
    int base;
    int pc;
    int ret_reg;
    ScriptFunc *func;
} CallFrame;

/* forward */
typedef struct VM VM;

/* native function: recebe ponteiro para args (Value*), nargs, retorna Value */
typedef Value (*NativeFn)(Value *args, int nargs);

struct VM {
    Value       globals[MAX_GLOBALS];
    int         nglobals;
    ScriptFunc  funcs[MAX_FUNCS];
    int         nfuncs;
    NativeFn    natives[MAX_NATIVES];
    int         nnatives;
    Value       regs[MAX_REGS * MAX_FRAMES];
    CallFrame   frames[MAX_FRAMES];
    int         nframes;
};

static unsigned long long g_insn_count;

static void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));
    for (int i = 0; i < MAX_REGS * MAX_FRAMES; i++)
        vm->regs[i] = val_nil();
    for (int i = 0; i < MAX_GLOBALS; i++)
        vm->globals[i] = val_nil();
}

static int vm_def_global(VM *vm, const char *name, Value val) {
    (void)name;
    int idx = vm->nglobals++;
    vm->globals[idx] = val;
    return idx;
}

static int vm_def_func(VM *vm, const char *name, Instruction *code, int nparams) {
    int idx = vm->nfuncs++;
    vm->funcs[idx].code = code;
    vm->funcs[idx].nparams = nparams;
    vm->funcs[idx].name = name;
    return idx;
}

static int vm_def_native(VM *vm, NativeFn fn) {
    int idx = vm->nnatives++;
    vm->natives[idx] = fn;
    return idx;
}

/* =========================================================
** PARTE 4 — vm_run (switch dispatch)
** ========================================================= */

static void vm_run(VM *vm, Instruction *main_code) {
    vm->nframes = 1;
    vm->frames[0].base = 0;
    vm->frames[0].pc = 0;
    vm->frames[0].ret_reg = -1;
    vm->frames[0].func = NULL;

    for (;;) {
        CallFrame *cf = &vm->frames[vm->nframes - 1];
        Instruction *code = cf->func ? cf->func->code : main_code;
        Instruction i = code[cf->pc++];
        Value *R = &vm->regs[cf->base];
        g_insn_count++;

        int op = GET_OP(i);
        int A  = GET_A(i);
        int B  = GET_B(i);
        int C  = GET_C(i);

        switch (op) {
            case OP_LOADI:
                R[A] = val_int((signed char)B);
                break;
            case OP_LOADG:
                R[A] = vm->globals[B];
                break;
            case OP_SETG:
                vm->globals[B] = R[A];
                break;
            case OP_MOVE:
                R[A] = R[B];
                break;
            case OP_ADD: {
                Value vb = R[B], vc = R[C];
                if (is_int(vb) && is_int(vc))
                    R[A] = val_int(as_int(vb) + as_int(vc));
                else
                    R[A] = val_double(to_number(vb) + to_number(vc));
                break;
            }
            case OP_SUB: {
                Value vb = R[B], vc = R[C];
                if (is_int(vb) && is_int(vc))
                    R[A] = val_int(as_int(vb) - as_int(vc));
                else
                    R[A] = val_double(to_number(vb) - to_number(vc));
                break;
            }
            case OP_MUL: {
                Value vb = R[B], vc = R[C];
                if (is_int(vb) && is_int(vc))
                    R[A] = val_int(as_int(vb) * as_int(vc));
                else
                    R[A] = val_double(to_number(vb) * to_number(vc));
                break;
            }
            case OP_LT: {
                Value vb = R[B], vc = R[C];
                R[A] = val_bool(to_number(vb) < to_number(vc));
                break;
            }
            case OP_JMP:
                cf->pc += GET_sB(i) - 1;
                break;
            case OP_JMPIF:
                if (is_bool(R[A]) && as_bool(R[A]))
                    cf->pc += GET_sB(i) - 1;
                break;
            case OP_CALL: {
                Value fn = R[A];
                if (is_fnative(fn)) {
                    int nidx = as_nidx(fn);
                    R[A] = vm->natives[nidx](&R[A + 1], B);
                } else if (is_fscript(fn)) {
                    if (vm->nframes >= MAX_FRAMES) { fprintf(stderr, "stack overflow\n"); return; }
                    ScriptFunc *sf = &vm->funcs[as_fidx(fn)];
                    int new_base = cf->base + A + 1;
                    for (int p = B; p < sf->nparams; p++)
                        vm->regs[new_base + p] = val_nil();
                    CallFrame *nf = &vm->frames[vm->nframes++];
                    nf->base = new_base; nf->pc = 0;
                    nf->ret_reg = cf->base + A; nf->func = sf;
                }
                break;
            }
            case OP_RETURN: {
                Value result = R[A];
                int dest = cf->ret_reg;
                vm->nframes--;
                if (vm->nframes == 0) return;
                if (dest >= 0) vm->regs[dest] = result;
                break;
            }
            case OP_PRINT:
                printf("  "); print_value(R[A]); printf("\n");
                break;
            case OP_HALT:
                return;
            default:
                fprintf(stderr, "opcode desconhecido: %d\n", op);
                return;
        }
    }
}

/* =========================================================
** PARTE 5 — vm_run_cgoto (computed goto, optimised NEXT)
** ========================================================= */

static void vm_run_cgoto(VM *vm, Instruction *main_code) {
#ifdef __GNUC__
    static const void *disptab[] = {
        [OP_LOADI]  = &&do_LOADI,
        [OP_LOADG]  = &&do_LOADG,
        [OP_SETG]   = &&do_SETG,
        [OP_MOVE]   = &&do_MOVE,
        [OP_ADD]    = &&do_ADD,
        [OP_SUB]    = &&do_SUB,
        [OP_MUL]    = &&do_MUL,
        [OP_LT]     = &&do_LT,
        [OP_JMP]    = &&do_JMP,
        [OP_JMPIF]  = &&do_JMPIF,
        [OP_CALL]   = &&do_CALL,
        [OP_RETURN] = &&do_RETURN,
        [OP_PRINT]  = &&do_PRINT,
        [OP_HALT]   = &&do_HALT,
    };

    #define NEXT() \
        g_insn_count++; \
        ins = *pc++; \
        A = GET_A(ins); B = GET_B(ins); C = GET_C(ins); \
        goto *disptab[GET_OP(ins)]

    #define SAVE_PC() (cf->pc = (int)(pc - (cf->func ? cf->func->code : main_code)))
    #define LOAD_STATE() \
        cf = &vm->frames[vm->nframes - 1]; \
        pc = (cf->func ? cf->func->code : main_code) + cf->pc; \
        R  = &vm->regs[cf->base]

    CallFrame   *cf;
    Instruction *pc;
    Instruction  ins;
    Value       *R;
    int          A, B, C;

    vm->nframes = 1;
    vm->frames[0].base = 0;
    vm->frames[0].pc = 0;
    vm->frames[0].ret_reg = -1;
    vm->frames[0].func = NULL;

    LOAD_STATE();
    NEXT();

do_LOADI: R[A] = val_int((signed char)B); NEXT();
do_LOADG: R[A] = vm->globals[B]; NEXT();
do_SETG:  vm->globals[B] = R[A]; NEXT();
do_MOVE:  R[A] = R[B]; NEXT();
do_ADD: {
    Value vb = R[B], vc = R[C];
    if (is_int(vb) && is_int(vc)) R[A] = val_int(as_int(vb) + as_int(vc));
    else R[A] = val_double(to_number(vb) + to_number(vc));
    NEXT();
}
do_SUB: {
    Value vb = R[B], vc = R[C];
    if (is_int(vb) && is_int(vc)) R[A] = val_int(as_int(vb) - as_int(vc));
    else R[A] = val_double(to_number(vb) - to_number(vc));
    NEXT();
}
do_MUL: {
    Value vb = R[B], vc = R[C];
    if (is_int(vb) && is_int(vc)) R[A] = val_int(as_int(vb) * as_int(vc));
    else R[A] = val_double(to_number(vb) * to_number(vc));
    NEXT();
}
do_LT: {
    Value vb = R[B], vc = R[C];
    R[A] = val_bool(to_number(vb) < to_number(vc));
    NEXT();
}
do_JMP:   pc += GET_sB(ins) - 1; NEXT();
do_JMPIF: if (is_bool(R[A]) && as_bool(R[A])) pc += GET_sB(ins) - 1; NEXT();
do_CALL: {
    Value fn = R[A];
    if (is_fnative(fn)) {
        R[A] = vm->natives[as_nidx(fn)](&R[A + 1], B);
    } else if (is_fscript(fn)) {
        if (vm->nframes >= MAX_FRAMES) { fprintf(stderr, "stack overflow\n"); return; }
        ScriptFunc *sf = &vm->funcs[as_fidx(fn)];
        int new_base = cf->base + A + 1;
        for (int p = B; p < sf->nparams; p++) vm->regs[new_base + p] = val_nil();
        SAVE_PC();
        CallFrame *nf = &vm->frames[vm->nframes++];
        nf->base = new_base; nf->pc = 0;
        nf->ret_reg = cf->base + A; nf->func = sf;
        LOAD_STATE();
    }
    NEXT();
}
do_RETURN: {
    Value result = R[A];
    int dest = cf->ret_reg;
    vm->nframes--;
    if (vm->nframes == 0) return;
    if (dest >= 0) vm->regs[dest] = result;
    LOAD_STATE();
    NEXT();
}
do_PRINT:
    printf("  "); print_value(R[A]); printf("\n");
    NEXT();
do_HALT:
    return;

    #undef NEXT
    #undef SAVE_PC
    #undef LOAD_STATE
#else
    vm_run(vm, main_code);
#endif
}

/* =========================================================
** PARTE 6 — Natives
** ========================================================= */

static Value native_print(Value *args, int nargs) {
    (void)nargs;
    printf("  [native print] "); print_value(args[0]); printf("\n");
    return val_nil();
}

static Value native_add(Value *args, int nargs) {
    (void)nargs;
    return val_double(to_number(args[0]) + to_number(args[1]));
}

/* =========================================================
** PARTE 7 — Benchmark: fib(35)
** ========================================================= */

int main(void) {
    VM vm;
    struct timespec t0, t1;

    /* --- fib bytecode (register-based, NaN-boxed values) --- */
    static Instruction fn_fib[] = {
     /* 0 */ ENCODE(OP_LOADI,  1,  2, 0),    /* R[1] = 2          */
     /* 1 */ ENCODE(OP_LT,     2,  0, 1),    /* R[2] = n < 2      */
     /* 2 */ ENCODE(OP_JMPIF,  2, 12, 0),    /* se true → 14      */
     /* 3 */ ENCODE(OP_LOADG,  3,  0, 0),    /* R[3] = fib        */
     /* 4 */ ENCODE(OP_LOADI,  5,  1, 0),    /* R[5] = 1          */
     /* 5 */ ENCODE(OP_SUB,    4,  0, 5),    /* R[4] = n-1        */
     /* 6 */ ENCODE(OP_CALL,   3,  1, 0),    /* fib(n-1) → R[3]   */
     /* 7 */ ENCODE(OP_MOVE,   1,  3, 0),    /* R[1] = fib(n-1)   */
     /* 8 */ ENCODE(OP_LOADG,  3,  0, 0),    /* R[3] = fib        */
     /* 9 */ ENCODE(OP_LOADI,  5,  2, 0),    /* R[5] = 2          */
     /*10 */ ENCODE(OP_SUB,    4,  0, 5),    /* R[4] = n-2        */
     /*11 */ ENCODE(OP_CALL,   3,  1, 0),    /* fib(n-2) → R[3]   */
     /*12 */ ENCODE(OP_ADD,    0,  1, 3),    /* R[0] = sum         */
     /*13 */ ENCODE(OP_RETURN, 0,  0, 0),
     /*14 */ ENCODE(OP_RETURN, 0,  0, 0),    /* base case: return n */
    };

    static Instruction prog[] = {
        ENCODE(OP_LOADG, 0, 0, 0),
        ENCODE(OP_LOADI, 1, 35, 0),
        ENCODE(OP_CALL,  0, 1, 0),
        ENCODE(OP_HALT,  0, 0, 0),
    };

    (void)native_print;
    (void)native_add;

    printf("=== NaN-boxed Register VM — fib(35) ===\n");
    printf("  sizeof(Value) = %zu bytes (vs 16 with tagged union)\n\n", sizeof(Value));

    /* --- switch --- */
    vm_init(&vm);
    { int fi = vm_def_func(&vm, "fib", fn_fib, 1); vm_def_global(&vm, "fib", val_script(fi)); }
    g_insn_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    vm_run(&vm, prog);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms_sw = (t1.tv_sec-t0.tv_sec)*1000.0 + (t1.tv_nsec-t0.tv_nsec)/1e6;
    unsigned long long cnt_sw = g_insn_count;

    /* --- computed goto --- */
    vm_init(&vm);
    { int fi = vm_def_func(&vm, "fib", fn_fib, 1); vm_def_global(&vm, "fib", val_script(fi)); }
    g_insn_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    vm_run_cgoto(&vm, prog);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms_cg = (t1.tv_sec-t0.tv_sec)*1000.0 + (t1.tv_nsec-t0.tv_nsec)/1e6;
    unsigned long long cnt_cg = g_insn_count;

    int result = as_int(vm.regs[0]);
    printf("  fib(35)        = %d\n", result);
    printf("  switch:        %7.1f ms  %lluM instrs\n", ms_sw, cnt_sw/1000000);
    printf("  computed goto: %7.1f ms  %lluM instrs\n", ms_cg, cnt_cg/1000000);
    printf("  speedup:       %7.2fx\n", ms_sw / ms_cg);

    return 0;
}
