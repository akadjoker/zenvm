/*
** toy_vm.c  —  Mini VM register-based, inspirada no Lua
**
** Compile:  gcc -o toy_vm toy_vm.c && ./toy_vm
**
** Esta versão demonstra:
**   1. Globais por ÍNDICE (não hash table) — O(1) como locais
**   2. Funções nativas (C) — ponteiro de função chamado directamente
**   3. Funções script     — novo CallFrame empurrado no frame stack
**
** Comparação de estratégias para globais:
**
**   HASH TABLE (Lua real):
**     globals["x"] → hash lookup, ~O(1) amortizado mas com overhead
**     Vantagem: número ilimitado de globais, nome sempre acessível
**
**   ÍNDICE FIXO (LuaJIT globals, wasm, nossa toy VM):
**     globals[3]   → acesso directo a array, O(1) puro
**     Vantagem: mais rápido; Desvantagem: índice decidido em compile-time
**
** Como funcionam chamadas no Lua real (ldo.c / luaD_precall):
**
**   luaD_precall olha o tipo do valor em R[func]:
**     LUA_VLCF  → função C "leve" (ponteiro directo)     → precallC()
**     LUA_VCCL  → C closure (ponteiro + upvalues)        → precallC()
**     LUA_VLCL  → Lua closure (Proto* + upvalues)        → novo CallInfo
**
**   Aqui fazemos o mesmo mas simplificado:
**     FUNC_NATIVE → chama fn(vm) directamente
**     FUNC_SCRIPT → empurra CallFrame, muda o pc
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* contador global de instruções — reset antes de cada benchmark */
static unsigned long long g_insn_count;

/* =========================================================
** PARTE 1 — Tipos de valor (tagged union, como TValue no Lua)
** ========================================================= */

typedef enum {
    TYPE_NIL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_FUNC_NATIVE,   /* função C: ponteiro directo */
    TYPE_FUNC_SCRIPT,   /* função script: índice em funcs[] */
} ValueType;

/* forward declaration — necessário porque NativeFn e Value dependem um do outro */
typedef struct Value Value;

/*
** Assinatura de função nativa: recebe array de args, devolve resultado.
** Igual a LUA_VLCF no Lua real — só um ponteiro de função, sem upvalues.
** O resultado vai directamente para R[A] no chamador.
*/
typedef Value (*NativeFn)(Value *args, int nargs);

struct Value {
    ValueType type;
    union {
        int        i;       /* TYPE_INT */
        double     f;       /* TYPE_FLOAT */
        int        b;       /* TYPE_BOOL */
        NativeFn   native;  /* TYPE_FUNC_NATIVE */
        int        fidx;    /* TYPE_FUNC_SCRIPT: índice em funcs[] */
    } as;
};

static Value val_nil()              { Value v; v.type = TYPE_NIL;          return v; }
static Value val_int(int n)         { Value v; v.type = TYPE_INT;          v.as.i = n; return v; }
static Value val_float(double f)    { Value v; v.type = TYPE_FLOAT;        v.as.f = f; return v; }
static Value val_bool(int b)        { Value v; v.type = TYPE_BOOL;         v.as.b = b; return v; }
static Value val_native(NativeFn fn){ Value v; v.type = TYPE_FUNC_NATIVE;  v.as.native = fn; return v; }
static Value val_script(int idx)    { Value v; v.type = TYPE_FUNC_SCRIPT;  v.as.fidx = idx; return v; }

static void print_value(Value v) {
    switch (v.type) {
        case TYPE_NIL:         printf("nil"); break;
        case TYPE_INT:         printf("%d", v.as.i); break;
        case TYPE_FLOAT:       printf("%g", v.as.f); break;
        case TYPE_BOOL:        printf("%s", v.as.b ? "true" : "false"); break;
        case TYPE_FUNC_NATIVE: printf("<native fn>"); break;
        case TYPE_FUNC_SCRIPT: printf("<script fn #%d>", v.as.fidx); break;
    }
}

/* =========================================================
** PARTE 2 — Opcodes e formato da instrução
**
** 32 bits:  [ opcode(8) | A(8) | B(8) | C(8) ]
**
**   A = registo destino
**   B = registo fonte 1  (ou índice)
**   C = registo fonte 2  (ou nargs)
** ========================================================= */

typedef enum {
    OP_LOADI,    /* R[A] = (signed)B                */
    OP_LOADF,    /* R[A] = kfloats[B]               */
    OP_LOADG,    /* R[A] = globals[B]   ← O(1)!     */
    OP_SETG,     /* globals[B] = R[A]               */
    OP_MOVE,     /* R[A] = R[B]                     */
    OP_ADD,      /* R[A] = R[B] + R[C]              */
    OP_SUB,      /* R[A] = R[B] - R[C]              */
    OP_MUL,      /* R[A] = R[B] * R[C]              */
    OP_DIV,      /* R[A] = R[B] / R[C]              */
    OP_EQ,       /* R[A] = (R[B] == R[C])            */
    OP_LT,       /* R[A] = (R[B] <  R[C])            */
    OP_JMP,      /* pc += sB                         */
    OP_JMPIF,    /* se R[A]: pc += sB                */
    OP_CALL,     /* R[A] é a função; B=nargs; C=dest */
    OP_RETURN,   /* retorna R[A] ao chamador          */
    OP_PRINT,    /* print R[A]                       */
    OP_HALT,
} OpCode;

/* uma instrução é só um uint32 */
typedef unsigned int Instruction;

/* macros para codificar/descodificar campos */
#define ENCODE(op, a, b, c)  ((unsigned)((op)<<24) | ((a)<<16) | ((b)<<8) | (c))
#define GET_OP(i)   (((i) >> 24) & 0xFF)
#define GET_A(i)    (((i) >> 16) & 0xFF)
#define GET_B(i)    (((i) >>  8) & 0xFF)
#define GET_C(i)    (((i)      ) & 0xFF)
/* B como signed (para JMP, offset negativo) */
#define GET_sB(i)   ((int)(signed char)GET_B(i))

/* =========================================================
** PARTE 3 — Funções script (como Proto* no Lua)
**
** No Lua: Proto contém code[], k[], upvalues[], locvars[], etc.
** Aqui simplificamos: apenas code[] e nparams.
** ========================================================= */

#define MAX_REGS     64
#define MAX_KFLOATS  64
#define MAX_GLOBALS  64
#define MAX_FUNCS    32
#define MAX_FRAMES   64   /* profundidade máxima de chamadas */

typedef struct {
    Instruction *code;
    int          nparams;
    const char  *name;     /* só para debug */
} ScriptFunc;

/* =========================================================
** PARTE 4 — CallFrame: o estado de uma chamada activa
**
** No Lua: CallInfo com savedpc, func, top.
** Ao fazer CALL:  empurra frame novo com base=argpos, pc=0
** Ao fazer RETURN: poppa frame, repõe pc e base do caller
** ========================================================= */

typedef struct {
    int          base;     /* primeiro registo desta função */
    int          pc;       /* program counter desta função  */
    int          ret_reg;  /* registo no caller onde vai o resultado */
    ScriptFunc  *func;     /* qual função está a correr */
} CallFrame;

/* =========================================================
** PARTE 5 — A VM completa
** ========================================================= */

typedef struct VM {
    /* registos globais — array simples, O(1) por índice */
    Value        globals[MAX_GLOBALS];
    const char  *gnames[MAX_GLOBALS];  /* nomes (só para debug) */
    int          nglobals;

    /* pool de constantes float */
    double       kfloats[MAX_KFLOATS];
    int          nkfloats;

    /* funções script registadas */
    ScriptFunc   funcs[MAX_FUNCS];
    int          nfuncs;

    /* stack de registos — partilhada por todos os frames */
    Value        regs[MAX_REGS * MAX_FRAMES];

    /* call stack */
    CallFrame    frames[MAX_FRAMES];
    int          nframes;      /* número de frames activos */
} VM;

static void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));
    for (int i = 0; i < MAX_REGS * MAX_FRAMES; i++)
        vm->regs[i] = val_nil();
    for (int i = 0; i < MAX_GLOBALS; i++)
        vm->globals[i] = val_nil();
}

/* regista um global por índice; devolve o índice */
static int vm_def_global(VM *vm, const char *name, Value val) {
    int idx = vm->nglobals++;
    vm->globals[idx] = val;
    vm->gnames[idx]  = name;
    return idx;
}

static int vm_add_kfloat(VM *vm, double f) {
    int idx = vm->nkfloats++;
    vm->kfloats[idx] = f;
    return idx;
}

/* regista uma função script; devolve o índice */
static int vm_def_func(VM *vm, const char *name, Instruction *code, int nparams) {
    int idx = vm->nfuncs++;
    vm->funcs[idx].code    = code;
    vm->funcs[idx].nparams = nparams;
    vm->funcs[idx].name    = name;
    return idx;
}

/* acesso rápido aos registos do frame actual */
static inline Value *frame_regs(VM *vm) {
    return &vm->regs[vm->frames[vm->nframes - 1].base];
}

/* =========================================================
** PARTE 6 — Execute
** ========================================================= */

static void vm_run(VM *vm, Instruction *main_code) {
    /* empurra o frame principal */
    vm->nframes = 1;
    vm->frames[0].base    = 0;
    vm->frames[0].pc      = 0;
    vm->frames[0].ret_reg = -1;
    vm->frames[0].func    = NULL;

    /* apontamos sempre para o frame activo */
    for (;;) {
        CallFrame *cf   = &vm->frames[vm->nframes - 1];
        Instruction    *code = cf->func ? cf->func->code : main_code;
        Instruction     i    = code[cf->pc++];
        Value          *R    = &vm->regs[cf->base]; /* registos deste frame */
        g_insn_count++;

        int op = GET_OP(i);
        int A  = GET_A(i);
        int B  = GET_B(i);
        int C  = GET_C(i);

        switch (op) {

            case OP_LOADI:
                R[A] = val_int((signed char)B);
                break;

            case OP_LOADF:
                R[A] = val_float(vm->kfloats[B]);
                break;

            /*
            ** OP_LOADG / OP_SETG — globais por ÍNDICE, O(1)
            ** B é o índice em vm->globals[], calculado em compile-time.
            ** Sem hash, sem string lookup — igual ao acesso a local.
            */
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
                if (vb.type == TYPE_INT && vc.type == TYPE_INT)
                    R[A] = val_int(vb.as.i + vc.as.i);
                else {
                    double nb = vb.type==TYPE_INT ? vb.as.i : vb.as.f;
                    double nc = vc.type==TYPE_INT ? vc.as.i : vc.as.f;
                    R[A] = val_float(nb + nc);
                }
                break;
            }

            case OP_SUB: {
                Value vb = R[B], vc = R[C];
                if (vb.type == TYPE_INT && vc.type == TYPE_INT)
                    R[A] = val_int(vb.as.i - vc.as.i);
                else {
                    double nb = vb.type==TYPE_INT ? vb.as.i : vb.as.f;
                    double nc = vc.type==TYPE_INT ? vc.as.i : vc.as.f;
                    R[A] = val_float(nb - nc);
                }
                break;
            }

            case OP_MUL: {
                Value vb = R[B], vc = R[C];
                if (vb.type == TYPE_INT && vc.type == TYPE_INT)
                    R[A] = val_int(vb.as.i * vc.as.i);
                else {
                    double nb = vb.type==TYPE_INT ? vb.as.i : vb.as.f;
                    double nc = vc.type==TYPE_INT ? vc.as.i : vc.as.f;
                    R[A] = val_float(nb * nc);
                }
                break;
            }

            case OP_DIV: {
                Value vb = R[B], vc = R[C];
                double nb = vb.type==TYPE_INT ? vb.as.i : vb.as.f;
                double nc = vc.type==TYPE_INT ? vc.as.i : vc.as.f;
                R[A] = val_float(nb / nc);
                break;
            }

            case OP_EQ: {
                Value vb = R[B], vc = R[C];
                int eq = (vb.type == vc.type) &&
                         ((vb.type==TYPE_INT   && vb.as.i==vc.as.i) ||
                          (vb.type==TYPE_FLOAT && vb.as.f==vc.as.f) ||
                          (vb.type==TYPE_BOOL  && vb.as.b==vc.as.b) ||
                          (vb.type==TYPE_NIL));
                R[A] = val_bool(eq);
                break;
            }

            case OP_LT: {
                Value vb = R[B], vc = R[C];
                double nb = vb.type==TYPE_INT ? vb.as.i : vb.as.f;
                double nc = vc.type==TYPE_INT ? vc.as.i : vc.as.f;
                R[A] = val_bool(nb < nc);
                break;
            }

            case OP_JMP:
                cf->pc += GET_sB(i) - 1;
                break;

            case OP_JMPIF:
                if (R[A].type == TYPE_BOOL && R[A].as.b)
                    cf->pc += GET_sB(i) - 1;
                break;

            /*
            ** OP_CALL A B C
            **   R[A] = a função (native ou script)
            **   B    = número de argumentos (em R[A+1]..R[A+B])
            **   C    = registo de destino no caller (onde guardar resultado)
            **
            ** No Lua real: luaD_precall() faz switch no tipo do valor:
            **   LUA_VLCF / LUA_VCCL → precallC()  (chama directamente)
            **   LUA_VLCL             → prepCallInfo() + luaV_execute()
            */
            /*
            ** OP_CALL A B  (C ignorado)
            **   R[A] = a função (native ou script)
            **   B    = número de argumentos em R[A+1]..R[A+B]
            **   resultado → R[A]  ← IGUAL AO LUA
            **
            ** Porquê R[A] e não R[C]?
            **   new_base = cf->base + A + 1
            **   Se ret_reg = cf->base + C e C > A, o resultado cai DENTRO
            **   do espaço do callee → corrupção em recursão.
            **   Com ret_reg = cf->base + A (< new_base), é sempre seguro.
            */
            case OP_CALL: {
                Value fn = R[A];

                if (fn.type == TYPE_FUNC_NATIVE) {
                    /* NATIVA: chama directamente, resultado vai para R[A] */
                    R[A] = fn.as.native(&R[A + 1], B);

                } else if (fn.type == TYPE_FUNC_SCRIPT) {
                    /* SCRIPT: empurra novo CallFrame — como CallInfo no Lua */
                    if (vm->nframes >= MAX_FRAMES) {
                        fprintf(stderr, "stack overflow\n"); return;
                    }
                    ScriptFunc *sf = &vm->funcs[fn.as.fidx];

                    /* novo base = logo após o slot da função */
                    int new_base = cf->base + A + 1;

                    /* preenche params em falta com nil */
                    for (int p = B; p < sf->nparams; p++)
                        vm->regs[new_base + p] = val_nil();

                    CallFrame *nf = &vm->frames[vm->nframes++];
                    nf->base    = new_base;
                    nf->pc      = 0;
                    nf->ret_reg = cf->base + A;  /* R[A] no caller — abaixo do new_base */
                    nf->func    = sf;

                } else {
                    fprintf(stderr, "CALL: R[%d] não é função (tipo %d)\n", A, fn.type);
                }
                break;
            }

            /*
            ** OP_RETURN A
            **   Copia R[A] para o registo de destino do caller e poppa o frame.
            **   No Lua: moveresults() + restaura ci->previous
            */
            case OP_RETURN: {
                Value result = R[A];
                int   dest   = cf->ret_reg;

                vm->nframes--;   /* poppa este frame */

                if (vm->nframes == 0) return;   /* era o main, acabou */

                if (dest >= 0)
                    vm->regs[dest] = result;   /* escreve resultado no caller */
                break;
            }

            case OP_PRINT:
                printf("  ");
                print_value(R[A]);
                printf("\n");
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
** vm_run_cgoto — Dispatch com computed goto (extensão GCC)
**
** Em vez de:   switch(op) { case OP_X: ... break; }
** usamos:      goto *disptab[op]
**
** Porquê é mais rápido?
**
**   switch   → branch indirecto ÚNICO partilhado por todos os opcodes.
**              O CPU branch predictor aprende só 1 padrão → muitas mispredict.
**
**   goto     → cada handler tem o SEU goto e o SEU predictor entry.
**              O CPU aprende que depois de do_LT vem do_JMPIF,
**              depois de do_ADD vem do_LOADG, etc.
**
** É exactamente o que o Lua faz em lvm.c via ljumptab.h:
**   #define vmdispatch(x)    goto *disptab[x]
**   #define vmcase(l)        l:
**   #define vmbreak          vmdispatch(GET_OPCODE(*pc))
**
** Optimização chave (igual ao Lua):
**   cf, R, code ficam em variáveis locais — o GCC guarda-as em registos.
**   Só são recarregadas em CALL/RETURN, não em cada instrução.
**   O pc é mantido como ponteiro directo (Instruction *pc) sem índice.
** ========================================================= */
static void vm_run_cgoto(VM *vm, Instruction *main_code) {
#ifdef __GNUC__
    static const void *disptab[] = {
        [OP_LOADI]  = &&do_LOADI,
        [OP_LOADF]  = &&do_LOADF,
        [OP_LOADG]  = &&do_LOADG,
        [OP_SETG]   = &&do_SETG,
        [OP_MOVE]   = &&do_MOVE,
        [OP_ADD]    = &&do_ADD,
        [OP_SUB]    = &&do_SUB,
        [OP_MUL]    = &&do_MUL,
        [OP_DIV]    = &&do_DIV,
        [OP_EQ]     = &&do_EQ,
        [OP_LT]     = &&do_LT,
        [OP_JMP]    = &&do_JMP,
        [OP_JMPIF]  = &&do_JMPIF,
        [OP_CALL]   = &&do_CALL,
        [OP_RETURN] = &&do_RETURN,
        [OP_PRINT]  = &&do_PRINT,
        [OP_HALT]   = &&do_HALT,
    };

    /*
    ** NEXT() — igual ao Lua real:
    **   - pc é um ponteiro directo (Instruction*), não um índice
    **   - R aponta directamente para vm->regs[base]
    **   - Nenhum acesso extra a cf — o GCC mantém pc e R em registos
    **   - cf só é necessário em CALL/RETURN (que actualizam pc e R)
    */
    #define NEXT() \
        g_insn_count++; \
        ins = *pc++; \
        A = GET_A(ins); B = GET_B(ins); C = GET_C(ins); \
        goto *disptab[GET_OP(ins)]

    /* macro para CALL/RETURN salvarem e restaurarem o estado */
    #define SAVE_PC()    (cf->pc = (int)(pc - (cf->func ? cf->func->code : main_code)))
    #define LOAD_STATE() \
        cf   = &vm->frames[vm->nframes - 1]; \
        pc   = (cf->func ? cf->func->code : main_code) + cf->pc; \
        R    = &vm->regs[cf->base]

    CallFrame   *cf;
    Instruction *pc;   /* ponteiro directo — como savedpc no Lua */
    Instruction  ins;
    Value       *R;
    int          A, B, C;

    vm->nframes           = 1;
    vm->frames[0].base    = 0;
    vm->frames[0].pc      = 0;
    vm->frames[0].ret_reg = -1;
    vm->frames[0].func    = NULL;

    LOAD_STATE();
    NEXT();

do_LOADI:  R[A] = val_int((signed char)B);  NEXT();
do_LOADF:  R[A] = val_float(vm->kfloats[B]); NEXT();
do_LOADG:  R[A] = vm->globals[B];  NEXT();
do_SETG:   vm->globals[B] = R[A];  NEXT();
do_MOVE:   R[A] = R[B];  NEXT();
do_ADD: {
    Value vb = R[B], vc = R[C];
    if (vb.type==TYPE_INT && vc.type==TYPE_INT) R[A] = val_int(vb.as.i + vc.as.i);
    else { double nb=vb.type==TYPE_INT?vb.as.i:vb.as.f, nc=vc.type==TYPE_INT?vc.as.i:vc.as.f; R[A]=val_float(nb+nc); }
    NEXT();
}
do_SUB: {
    Value vb = R[B], vc = R[C];
    if (vb.type==TYPE_INT && vc.type==TYPE_INT) R[A] = val_int(vb.as.i - vc.as.i);
    else { double nb=vb.type==TYPE_INT?vb.as.i:vb.as.f, nc=vc.type==TYPE_INT?vc.as.i:vc.as.f; R[A]=val_float(nb-nc); }
    NEXT();
}
do_MUL: {
    Value vb = R[B], vc = R[C];
    if (vb.type==TYPE_INT && vc.type==TYPE_INT) R[A] = val_int(vb.as.i * vc.as.i);
    else { double nb=vb.type==TYPE_INT?vb.as.i:vb.as.f, nc=vc.type==TYPE_INT?vc.as.i:vc.as.f; R[A]=val_float(nb*nc); }
    NEXT();
}
do_DIV: {
    Value vb=R[B], vc=R[C];
    double nb=vb.type==TYPE_INT?vb.as.i:vb.as.f, nc=vc.type==TYPE_INT?vc.as.i:vc.as.f;
    R[A]=val_float(nb/nc);
    NEXT();
}
do_EQ: {
    Value vb=R[B], vc=R[C];
    int eq=(vb.type==vc.type)&&
           ((vb.type==TYPE_INT&&vb.as.i==vc.as.i)||
            (vb.type==TYPE_FLOAT&&vb.as.f==vc.as.f)||
            (vb.type==TYPE_BOOL&&vb.as.b==vc.as.b)||
            (vb.type==TYPE_NIL));
    R[A]=val_bool(eq);
    NEXT();
}
do_LT: {
    Value vb=R[B], vc=R[C];
    double nb=vb.type==TYPE_INT?vb.as.i:vb.as.f, nc=vc.type==TYPE_INT?vc.as.i:vc.as.f;
    R[A]=val_bool(nb<nc);
    NEXT();
}
do_JMP:   pc += GET_sB(ins) - 1; NEXT();
do_JMPIF: if (R[A].type==TYPE_BOOL && R[A].as.b) pc += GET_sB(ins) - 1; NEXT();
do_CALL: {
    Value fn = R[A];
    if (fn.type == TYPE_FUNC_NATIVE) {
        R[A] = fn.as.native(&R[A + 1], B);
    } else if (fn.type == TYPE_FUNC_SCRIPT) {
        if (vm->nframes >= MAX_FRAMES) { fprintf(stderr, "stack overflow\n"); return; }
        ScriptFunc *sf = &vm->funcs[fn.as.fidx];
        int new_base = cf->base + A + 1;
        for (int p = B; p < sf->nparams; p++) vm->regs[new_base + p] = val_nil();
        SAVE_PC();   /* guarda pc no frame actual antes de mudar */
        CallFrame *nf = &vm->frames[vm->nframes++];
        nf->base = new_base; nf->pc = 0;
        nf->ret_reg = cf->base + A; nf->func = sf;
        LOAD_STATE(); /* carrega pc e R do novo frame */
    } else { fprintf(stderr, "CALL: not a function\n"); }
    NEXT();
}
do_RETURN: {
    Value result = R[A];
    int dest = cf->ret_reg;
    vm->nframes--;
    if (vm->nframes == 0) return;
    if (dest >= 0) vm->regs[dest] = result;
    LOAD_STATE();   /* restaura pc e R do frame pai */
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
** PARTE 7 — Stack VM (para comparação com register VM)
**
** Uma stack VM tem uma pilha implícita de valores.
** Cada instrução faz push ou pop — não há campos A/B/C para registos.
**
** fib(n) em stack bytecode:
**   PUSH_ARG 0          ; empurra arg n
**   PUSH_INT 2          ; empurra 2
**   LT_S                ; pop 2 vals, push (n < 2)
**   JMPIF_S  →base_case
**   PUSH_SELF           ; empurra fib (índice 0)
**   PUSH_ARG 0          ; n
**   PUSH_INT 1          ; 1
**   SUB_S               ; n-1
**   CALL_S 1            ; fib(n-1), result no topo
**   PUSH_SELF
**   PUSH_ARG 0          ; n
**   PUSH_INT 2          ; 2
**   SUB_S               ; n-2
**   CALL_S 1            ; fib(n-2)
**   ADD_S               ; fib(n-1)+fib(n-2)
**   RETURN_S
** base_case:
**   PUSH_ARG 0          ; n
**   RETURN_S
**
** Instrução: 16 bits opcode | 16 bits operando
** ========================================================= */

typedef enum {
    SOP_PUSH_INT,   /* empurra constante inteira (operando) */
    SOP_PUSH_ARG,   /* empurra argumento N do frame actual */
    SOP_PUSH_GLOBAL,/* empurra global por índice */
    SOP_ADD,        /* pop 2, push soma */
    SOP_SUB,        /* pop 2, push diferença */
    SOP_LT,         /* pop 2, push bool */
    SOP_JMPIF,      /* pop bool, salta se true (operando = destino absoluto) */
    SOP_CALL,       /* pop fn + N args (operando=N), push resultado */
    SOP_RETURN,     /* pop resultado, retorna ao caller */
    SOP_HALT,
} SOpCode;

typedef unsigned int SInstruction;   /* 16|16 */
#define SENCODE(op, val)  ((unsigned)((op) << 16) | ((unsigned)(val) & 0xFFFF))
#define SGET_OP(i)        (((i) >> 16) & 0xFFFF)
#define SGET_VAL(i)       ((int)(short)((i) & 0xFFFF))  /* signed 16-bit */

#define SSTACK_SIZE  (MAX_FRAMES * MAX_REGS)
#define SMAX_FRAMES  MAX_FRAMES

typedef struct {
    int base;       /* índice no sstack onde começam os argumentos */
    int pc;
    int ret_depth;  /* profundidade do stack antes do CALL */
    int fidx;       /* índice da função (globals[0] = self ref) */
} SFrame;

typedef struct {
    Value       sstack[SSTACK_SIZE];
    int         sp;          /* stack pointer: próxima posição livre */
    SFrame      sframes[SMAX_FRAMES];
    int         snframes;

    /* tabela de funções script (partilhada com a VM principal) */
    SInstruction *sfuncs[MAX_FUNCS];
    int           snfuncs;
    /* globais partilhados */
    Value         sglobals[MAX_GLOBALS];
    int           snglobals;
} SVM;

static void svm_push(SVM *s, Value v) { s->sstack[s->sp++] = v; }
static Value svm_pop(SVM *s)          { return s->sstack[--s->sp]; }

static void svm_run(SVM *s, SInstruction *main_code) {
    s->sp = 0;
    s->snframes = 1;
    s->sframes[0].base = 0; s->sframes[0].pc = 0;
    s->sframes[0].ret_depth = 0; s->sframes[0].fidx = -1;

    for (;;) {
        SFrame      *sf   = &s->sframes[s->snframes - 1];
        SInstruction *code = sf->fidx >= 0 ? s->sfuncs[sf->fidx] : main_code;
        SInstruction  ins  = code[sf->pc++];
        int op  = SGET_OP(ins);
        int val = SGET_VAL(ins);
        g_insn_count++;

        switch (op) {
            case SOP_PUSH_INT:
                svm_push(s, val_int(val));
                break;

            case SOP_PUSH_ARG:
                svm_push(s, s->sstack[sf->base + val]);
                break;

            case SOP_PUSH_GLOBAL:
                svm_push(s, s->sglobals[val]);
                break;

            case SOP_ADD: {
                Value b = svm_pop(s), a = svm_pop(s);
                if (a.type==TYPE_INT && b.type==TYPE_INT)
                    svm_push(s, val_int(a.as.i + b.as.i));
                else
                    svm_push(s, val_float((a.type==TYPE_INT?a.as.i:a.as.f) +
                                          (b.type==TYPE_INT?b.as.i:b.as.f)));
                break;
            }
            case SOP_SUB: {
                Value b = svm_pop(s), a = svm_pop(s);
                if (a.type==TYPE_INT && b.type==TYPE_INT)
                    svm_push(s, val_int(a.as.i - b.as.i));
                else
                    svm_push(s, val_float((a.type==TYPE_INT?a.as.i:a.as.f) -
                                          (b.type==TYPE_INT?b.as.i:b.as.f)));
                break;
            }
            case SOP_LT: {
                Value b = svm_pop(s), a = svm_pop(s);
                double na = a.type==TYPE_INT?a.as.i:a.as.f;
                double nb = b.type==TYPE_INT?b.as.i:b.as.f;
                svm_push(s, val_bool(na < nb));
                break;
            }
            case SOP_JMPIF: {
                Value cond = svm_pop(s);
                if (cond.type == TYPE_BOOL && cond.as.b)
                    sf->pc = val;   /* salto absoluto */
                break;
            }
            case SOP_CALL: {
                /* stack: ... fn arg0..argN-1  (N = val) */
                int nargs = val;
                int fn_pos = s->sp - nargs - 1;   /* posição da fn */
                Value fn = s->sstack[fn_pos];
                /* args ficam em sstack[fn_pos+1 .. fn_pos+nargs] */
                int new_base = fn_pos + 1;

                if (s->snframes >= SMAX_FRAMES) { fprintf(stderr, "svm: stack overflow\n"); return; }

                SFrame *nf = &s->sframes[s->snframes++];
                nf->base      = new_base;
                nf->pc        = 0;
                nf->ret_depth = fn_pos;   /* resultado vai sobrescrever fn_pos */
                nf->fidx      = fn.as.fidx;
                /* sp fica acima dos args */
                s->sp = new_base + nargs;
                break;
            }
            case SOP_RETURN: {
                Value result = svm_pop(s);
                int dest = s->sframes[s->snframes - 1].ret_depth;
                s->snframes--;
                if (s->snframes == 0) { svm_push(s, result); return; }
                s->sp = dest;
                svm_push(s, result);
                break;
            }
            case SOP_HALT:
                return;
        }
    }
}

/* =========================================================
** PARTE 8 — Funções nativas de exemplo
** ========================================================= */

/* native_print: recebe args[0], imprime */
static Value native_print(Value *args, int nargs) {
    (void)nargs;
    printf("  [native print] ");
    print_value(args[0]);
    printf("\n");
    return val_nil();
}

/* native_add: soma args[0] + args[1] */
static Value native_add(Value *args, int nargs) {
    (void)nargs;
    double na = args[0].type==TYPE_INT ? args[0].as.i : args[0].as.f;
    double nb = args[1].type==TYPE_INT ? args[1].as.i : args[1].as.f;
    return val_float(na + nb);
}

/*
** native_time_ms: devolve tempo actual em milissegundos (float).
** Não recebe argumentos. Equivale a time.time()*1000 em Python.
** Usa CLOCK_MONOTONIC — não afectado por ajustes de relógio do sistema.
*/
static Value native_time_ms(Value *args, int nargs) {
    (void)args; (void)nargs;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return val_float(ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6);
}

/* =========================================================
** PARTE 8 — Programas de teste
** ========================================================= */

int main(void) {
    VM vm;

    /* --------------------------------------------------
    ** PROGRAMA 1: Globais por índice
    **
    ** Lua equivalente:
    **   gX = 42         (global índice 0)
    **   gY = 10         (global índice 1)
    **   local r = gX + gY
    **   print(r)        → 52
    **   gX = 99
    **   print(gX)       → 99
    ** -------------------------------------------------- */
    printf("=== PROGRAMA 1: globais por indice ===\n");
    {
        vm_init(&vm);
        int gX = vm_def_global(&vm, "gX", val_int(42));
        int gY = vm_def_global(&vm, "gY", val_int(10));

        Instruction prog[] = {
            ENCODE(OP_LOADG, 0, gX,  0),   /* R[0] = globals[gX] = 42 */
            ENCODE(OP_LOADG, 1, gY,  0),   /* R[1] = globals[gY] = 10 */
            ENCODE(OP_ADD,   2,  0,  1),   /* R[2] = 52 */
            ENCODE(OP_PRINT, 2,  0,  0),
            ENCODE(OP_LOADI, 0, 99,  0),   /* R[0] = 99 */
            ENCODE(OP_SETG,  0, gX,  0),   /* globals[gX] = 99 */
            ENCODE(OP_LOADG, 3, gX,  0),   /* R[3] = 99 */
            ENCODE(OP_PRINT, 3,  0,  0),
            ENCODE(OP_HALT,  0,  0,  0),
        };
        vm_run(&vm, prog);
    }

    /* --------------------------------------------------
    ** PROGRAMA 2: Função nativa
    **
    ** Lua equivalente:
    **   local r = native_add(30, 12)   → 42.0
    **   native_print(r)
    ** -------------------------------------------------- */
    printf("=== PROGRAMA 2: funcao nativa ===\n");
    {
        vm_init(&vm);
        int g_nadd   = vm_def_global(&vm, "native_add",  val_native(native_add));
        int g_nprint = vm_def_global(&vm, "native_print", val_native(native_print));

        Instruction prog[] = {
            ENCODE(OP_LOADG, 0, g_nadd,   0),  /* R[0] = native_add */
            ENCODE(OP_LOADI, 1, 30,       0),  /* R[1] = 30 */
            ENCODE(OP_LOADI, 2, 12,       0),  /* R[2] = 12 */
            ENCODE(OP_CALL,  0,  2,       0),  /* native_add(30,12) → R[0] = 42.0 */
            ENCODE(OP_LOADG, 3, g_nprint, 0),  /* R[3] = native_print (acima de R[0]) */
            ENCODE(OP_MOVE,  4,  0,       0),  /* R[4] = resultado (arg para print) */
            ENCODE(OP_CALL,  3,  1,       0),  /* native_print(42.0) → R[3] (nil) */
            ENCODE(OP_HALT,  0,  0,       0),
        };
        vm_run(&vm, prog);
    }

    /* --------------------------------------------------
    ** PROGRAMA 3: Função script com CallFrame
    **
    ** Lua equivalente:
    **   function square(n) return n*n end
    **   print(square(7))   → 49
    ** -------------------------------------------------- */
    printf("=== PROGRAMA 3: funcao script (square) ===\n");
    {
        vm_init(&vm);

        Instruction fn_square[] = {
            ENCODE(OP_MUL,    1,  0,  0),  /* R[1] = R[0]*R[0] */
            ENCODE(OP_RETURN, 1,  0,  0),
        };
        int fidx = vm_def_func(&vm, "square", fn_square, 1);
        int g_sq = vm_def_global(&vm, "square", val_script(fidx));

        Instruction prog[] = {
            ENCODE(OP_LOADG, 0, g_sq, 0),  /* R[0] = square */
            ENCODE(OP_LOADI, 1,  7,   0),  /* R[1] = 7 */
            ENCODE(OP_CALL,  0,  1,   0),  /* square(7) → R[0] = 49 */
            ENCODE(OP_PRINT, 0,  0,   0),  /* print 49 */
            ENCODE(OP_HALT,  0,  0,   0),
        };
        vm_run(&vm, prog);
    }

    /* --------------------------------------------------
    ** PROGRAMA 4: Recursão (fibonacci)
    **
    ** function fib(n)
    **   if n < 2 then return n end
    **   return fib(n-1) + fib(n-2)
    ** end
    ** print(fib(10))   → 55
    ** -------------------------------------------------- */
    printf("=== PROGRAMA 4: recursao fib(10) ===\n");
    {
        vm_init(&vm);

        /*
        ** Bytecode de fib — resultado vai sempre para R[A] (o slot da função)
        **
        ** Layout de registos em cada frame (base = b):
        **   R[0] = n (argumento)
        **   R[1] = temporário / guarda fib(n-1) depois da 1ª chamada
        **   R[2] = resultado da comparação
        **   R[3] = slot para sub-chamadas (fn vai aqui → new_base = b+4)
        **   R[4] = argumento da sub-chamada (n-1 ou n-2)
        **   R[5] = constante 1 ou 2
        **
        ** Segurança do overlap:
        **   new_base  = b+4   (sub-frame começa aqui)
        **   ret_reg   = b+3   (R[A] = R[3]) → abaixo do new_base ✓
        **   R[1]=b+1  <  b+4  → seguro para guardar resultado ✓
        */
        Instruction fn_fib[] = {
         /* 0 */ ENCODE(OP_LOADI,  1,   2,    0),  /* R[1] = 2                       */
         /* 1 */ ENCODE(OP_LT,     2,   0,    1),  /* R[2] = (n < 2)                 */
         /* 2 */ ENCODE(OP_JMPIF,  2,  12,    0),  /* se true: salta → instr 14      */
                                                    /* (pc=3 após fetch, +12-1=14) ✓  */
         /* 3 */ ENCODE(OP_LOADG,  3,   0,    0),  /* R[3] = fib                     */
         /* 4 */ ENCODE(OP_LOADI,  5,   1,    0),  /* R[5] = 1                       */
         /* 5 */ ENCODE(OP_SUB,    4,   0,    5),  /* R[4] = n-1                     */
         /* 6 */ ENCODE(OP_CALL,   3,   1,    0),  /* fib(n-1) → R[3]; base=b+4 ✓   */
         /* 7 */ ENCODE(OP_MOVE,   1,   3,    0),  /* R[1] = fib(n-1)  (b+1 < b+4)  */
         /* 8 */ ENCODE(OP_LOADG,  3,   0,    0),  /* R[3] = fib (recarrega)         */
         /* 9 */ ENCODE(OP_LOADI,  5,   2,    0),  /* R[5] = 2                       */
         /*10 */ ENCODE(OP_SUB,    4,   0,    5),  /* R[4] = n-2                     */
         /*11 */ ENCODE(OP_CALL,   3,   1,    0),  /* fib(n-2) → R[3]; base=b+4 ✓   */
         /*12 */ ENCODE(OP_ADD,    0,   1,    3),  /* R[0] = fib(n-1) + fib(n-2)    */
         /*13 */ ENCODE(OP_RETURN, 0,   0,    0),  /* return soma                    */
         /*14 */ ENCODE(OP_RETURN, 0,   0,    0),  /* return n  (caso base: n < 2)   */
        };
        int fidx = vm_def_func(&vm, "fib", fn_fib, 1);
        vm_def_global(&vm, "fib", val_script(fidx));   /* fica em globals[0] */

        Instruction prog[] = {
            ENCODE(OP_LOADG, 0,  0,  0),  /* R[0] = fib */
            ENCODE(OP_LOADI, 1, 10,  0),  /* R[1] = 10 */
            ENCODE(OP_CALL,  0,  1,  0),  /* fib(10) → R[0] = 55 */
            ENCODE(OP_PRINT, 0,  0,  0),  /* print 55 */
            ENCODE(OP_HALT,  0,  0,  0),
        };
        vm_run(&vm, prog);
    }

    /* ------------------------------------------------------------------
    ** PROGRAMA 5: switch vs computed-goto — fib(35) benchmark
    **
    ** Medição feita em C com clock_gettime (sem overhead de bytecode).
    ** Compila com -O2 para ver o benefício real do computed goto.
    **
    ** Por que o switch é mais lento?
    **   CPU modern branch predictor: cada branch indirecto tem uma entrada
    **   própria na BTB (Branch Target Buffer). Com switch, TODOS os opcodes
    **   partilham UM branch → o predictor tenta prever o próximo opcode
    **   mas só tem 1 slot → muito falha.
    **   Com computed goto, do_LT tem o SEU slot, do_ADD tem o SEU, etc.
    **   O CPU aprende os padrões específicos de cada handler → menos mispredict.
    ** ------------------------------------------------------------------ */
    printf("=== PROGRAMA 5: switch vs computed-goto  fib(35) ===\n");
    {
        static Instruction fn_fib[] = {
         /* 0 */ ENCODE(OP_LOADI,  1,  2, 0),
         /* 1 */ ENCODE(OP_LT,     2,  0, 1),
         /* 2 */ ENCODE(OP_JMPIF,  2, 12, 0),
         /* 3 */ ENCODE(OP_LOADG,  3,  0, 0),
         /* 4 */ ENCODE(OP_LOADI,  5,  1, 0),
         /* 5 */ ENCODE(OP_SUB,    4,  0, 5),
         /* 6 */ ENCODE(OP_CALL,   3,  1, 0),
         /* 7 */ ENCODE(OP_MOVE,   1,  3, 0),
         /* 8 */ ENCODE(OP_LOADG,  3,  0, 0),
         /* 9 */ ENCODE(OP_LOADI,  5,  2, 0),
         /*10 */ ENCODE(OP_SUB,    4,  0, 5),
         /*11 */ ENCODE(OP_CALL,   3,  1, 0),
         /*12 */ ENCODE(OP_ADD,    0,  1, 3),
         /*13 */ ENCODE(OP_RETURN, 0,  0, 0),
         /*14 */ ENCODE(OP_RETURN, 0,  0, 0),
        };
        static Instruction prog[] = {
            ENCODE(OP_LOADG, 0,  0, 0),  /* R[0] = fib */
            ENCODE(OP_LOADI, 1, 35, 0),  /* R[1] = 35  */
            ENCODE(OP_CALL,  0,  1, 0),  /* fib(35) → R[0] */
            ENCODE(OP_HALT,  0,  0, 0),
        };

        struct timespec t0, t1;

        /* --- switch dispatch --- */
        vm_init(&vm);
        { int fi = vm_def_func(&vm, "fib", fn_fib, 1); vm_def_global(&vm, "fib", val_script(fi)); }
        g_insn_count = 0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        vm_run(&vm, prog);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms_sw = (t1.tv_sec - t0.tv_sec)*1000.0 + (t1.tv_nsec - t0.tv_nsec)/1e6;
        unsigned long long cnt_sw = g_insn_count;

        /* --- computed goto dispatch --- */
        vm_init(&vm);
        { int fi = vm_def_func(&vm, "fib", fn_fib, 1); vm_def_global(&vm, "fib", val_script(fi)); }
        g_insn_count = 0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        vm_run_cgoto(&vm, prog);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms_cg = (t1.tv_sec - t0.tv_sec)*1000.0 + (t1.tv_nsec - t0.tv_nsec)/1e6;
        unsigned long long cnt_cg = g_insn_count;

        printf("  fib(35)        = %d\n",  vm.regs[0].as.i);
        printf("  switch:        %7.1f ms  %lluM instrs\n", ms_sw, cnt_sw/1000000);
        printf("  computed goto: %7.1f ms  %lluM instrs\n", ms_cg, cnt_cg/1000000);
        printf("  speedup cgoto: %7.2fx  (mesmo nr de instrs, só dispatch melhor)\n", ms_sw / ms_cg);

        /* --- stack VM --- */
        /*
        ** fib em stack bytecode (18 instruções por frame vs 15 register):
        **   cada n < 2 precisa PUSH_ARG + PUSH_INT + LT + JMPIF
        **   cada fib(n-1): PUSH_GLOBAL + PUSH_ARG + PUSH_INT + SUB + CALL
        **   vs register: LOADG + LOADI + SUB + CALL (+ MOVE extra)
        ** Diferença pequena aqui, mas em loops com muitas variáveis fica grande.
        ** Vantagem stack VM: bytecode mais simples de gerar (sem alloc de registos).
        */
        static SInstruction sfn_fib2[] = {
         /* 0 */ SENCODE(SOP_PUSH_ARG,    0),   /* n                       */
         /* 1 */ SENCODE(SOP_PUSH_INT,    2),   /* 2                       */
         /* 2 */ SENCODE(SOP_LT,          0),   /* n < 2 → bool            */
         /* 3 */ SENCODE(SOP_JMPIF,      16),   /* se true → 16 (base)     */
         /* 4 */ SENCODE(SOP_PUSH_GLOBAL, 0),   /* fib                     */
         /* 5 */ SENCODE(SOP_PUSH_ARG,    0),   /* n                       */
         /* 6 */ SENCODE(SOP_PUSH_INT,    1),   /* 1                       */
         /* 7 */ SENCODE(SOP_SUB,         0),   /* n-1                     */
         /* 8 */ SENCODE(SOP_CALL,        1),   /* fib(n-1) → topo         */
         /* 9 */ SENCODE(SOP_PUSH_GLOBAL, 0),   /* fib                     */
         /*10 */ SENCODE(SOP_PUSH_ARG,    0),   /* n                       */
         /*11 */ SENCODE(SOP_PUSH_INT,    2),   /* 2                       */
         /*12 */ SENCODE(SOP_SUB,         0),   /* n-2                     */
         /*13 */ SENCODE(SOP_CALL,        1),   /* fib(n-2) → topo         */
         /*14 */ SENCODE(SOP_ADD,         0),   /* fib(n-1)+fib(n-2)       */
         /*15 */ SENCODE(SOP_RETURN,      0),   /* return soma              */
         /*16 */ SENCODE(SOP_PUSH_ARG,    0),   /* n   (caso base)         */
         /*17 */ SENCODE(SOP_RETURN,      0),   /* return n                */
        };
        SVM svm;
        memset(&svm, 0, sizeof(svm));
        svm.sfuncs[svm.snfuncs++] = sfn_fib2;
        svm.sglobals[svm.snglobals++] = val_script(0);   /* globals[0] = fib */

        static SInstruction sprog[] = {
            SENCODE(SOP_PUSH_GLOBAL, 0),   /* fib */
            SENCODE(SOP_PUSH_INT,   35),   /* 35 */
            SENCODE(SOP_CALL,        1),   /* fib(35) */
            SENCODE(SOP_HALT,        0),
        };
        g_insn_count = 0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        svm_run(&svm, sprog);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms_stack = (t1.tv_sec - t0.tv_sec)*1000.0 + (t1.tv_nsec - t0.tv_nsec)/1e6;
        unsigned long long cnt_stack = g_insn_count;

        printf("  stack VM:      %7.1f ms  %lluM instrs\n", ms_stack, cnt_stack/1000000);
        printf("  Python (ref):  medido ao mesmo tempo:\n");
    }

    return 0;
}
