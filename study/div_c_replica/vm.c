/* vm.c - DIV VM: flat memory, pila (stack), process scheduling, opcode loop
 * Mirrors the architecture of src/div32run/i.cpp + kernel.cpp
 */
#include <stdio.h>
#include <string.h>
#include "opcodes.h"
#include "process.h"

/* -----------------------------------------------------------------------
 * Global flat memory (code + process blocks + globals live here)
 * In original DIV: extern int *mem;  allocated to ~512KB
 * ----------------------------------------------------------------------- */
#define MEM_SIZE    4096
#define PILA_SIZE   2048   /* pila = stack in Spanish */

int  mem[MEM_SIZE];
int  pila[PILA_SIZE];
int  sp  = -1;    /* stack pointer */
int  ip  = 0;     /* instruction pointer */
int  id  = 0;     /* currently executing process id (= base in mem[]) */
int  bp  = 0;     /* base pointer (start of local vars on stack) */

/* -----------------------------------------------------------------------
 * Process table: ids of all known process blocks
 * In DIV this is tracked differently; we keep it simple.
 * ----------------------------------------------------------------------- */
#define MAX_PROCS 32
int proc_ids[MAX_PROCS];
int proc_count = 0;

/* -----------------------------------------------------------------------
 * Process allocation: carve out a block in mem[] above code area.
 * Returns base address (= process id).
 * ----------------------------------------------------------------------- */
#define PROC_BASE 512   /* mem[0..511] reserved for code; procs start here */
static int next_proc_base = PROC_BASE;

static int alloc_process(void)
{
    int base = next_proc_base;
    next_proc_base += PROC_BLOCK_SIZE;
    memset(&mem[base], 0, PROC_BLOCK_SIZE * sizeof(int));
    return base;
}

/* -----------------------------------------------------------------------
 * Built-in functions (lfun operand selects which)
 * ----------------------------------------------------------------------- */
static void exec_builtin(int fn)
{
    switch (fn) {
        case 0: /* write: print top of stack */
            printf("[write] %d\n", pila[sp]);
            break;
        default:
            printf("[builtin %d] not implemented\n", fn);
            break;
    }
}

/* -----------------------------------------------------------------------
 * nucleo_exec - opcode interpreter for one process
 * Mirrors kernel.cpp included inside nucleo_exec() in i.cpp.
 * Runs until lfrm (frame/yield), lret (return/kill), or lnop end.
 * Returns 1 if process yielded (frame), 0 if process died (ret).
 * ----------------------------------------------------------------------- */
static int nucleo_exec(void)
{
    int running = 1;
    int yielded = 0;

    while (running) {
        int op = (unsigned char)mem[ip++];

        switch (op) {

        case lnop:
            running = 0;
            break;

        /* --- stack / memory --- */
        case lcar:  /* push literal */
            pila[++sp] = mem[ip++];
            break;

        case lasi:  /* assign: mem[addr] = value; leaves value on stack */
            /* pila[sp] = address, pila[sp-1] = value  (DIV: addr pushed last) */
            mem[pila[sp]] = pila[sp-1];
            sp--;           /* discard address; value stays on top */
            break;

        case lptr:  /* dereference */
            pila[sp] = mem[pila[sp]];
            break;

        /* --- arithmetic --- */
        case ladd:  pila[sp-1] += pila[sp]; sp--; break;
        case lsub:  pila[sp-1] -= pila[sp]; sp--; break;
        case lmul:  pila[sp-1] *= pila[sp]; sp--; break;
        case ldiv:  pila[sp-1] /= pila[sp]; sp--; break;
        case lneg:  pila[sp]    = -pila[sp]; break;

        /* --- flow --- */
        case ljmp:  /* unconditional jump */
            ip = mem[ip];
            break;

        case ljpf:  /* jump if false (0) */
            if (pila[sp--] & 1)
                ip++;       /* condition true: skip target, fall through */
            else
                ip = mem[ip];
            break;

        case lfun:  /* call built-in */
            exec_builtin(mem[ip++]);
            break;

        /* --- stack frame helpers --- */
        case lasp:  /* adjust sp (alloc/free locals) */
            sp += mem[ip++];
            break;

        case lcbp:  /* copy bp = sp (mark local frame base) */
            bp = sp;
            break;

        /* --- process call (lcal) ---
         * Operand at mem[ip] = address of process template/code entry.
         * Creates new process block, sets id/status/ip/father/caller.
         * The new process will be scheduled by exec_process next frame.
         */
        case lcal: {
            int entry = mem[ip++];          /* code entry point of new proc */
            int npar  = mem[ip++];          /* number of parameters */
            int child = alloc_process();

            mem[child + _Id]       = child;
            mem[child + _Status]   = ST_ALIVE;
            mem[child + _IP]       = entry;
            mem[child + _SP]       = -1;
            mem[child + _Frame]    = 0;
            mem[child + _Executed] = 0;
            mem[child + _Priority] = mem[id + _Priority]; /* inherit */
            mem[child + _Father]   = id;
            mem[child + _Caller]   = id;
            mem[child + _NumPar]   = npar;
            /* push child id so caller can store it if needed */
            pila[++sp] = child;

            /* register in table */
            if (proc_count < MAX_PROCS)
                proc_ids[proc_count++] = child;

            printf("[lcal] spawned process id=%d entry=0x%x\n", child, entry);
            break;
        }

        /* --- return (lret) ---
         * Process finished. Mark dead, stop execution.
         */
        case lret:
            mem[id + _Status] = ST_DEAD;
            running = 0;
            yielded = 0;
            printf("[lret] process id=%d died\n", id);
            break;

        /* --- frame (lfrm) ---
         * Yield execution back to scheduler. Save ip and sp.
         * Mirrors: save ip, set _Executed=1, return to exec_process.
         */
        case lfrm:
            mem[id + _IP]       = ip;   /* resume here next frame */
            mem[id + _SP]       = sp;
            mem[id + _Executed] = 1;
            running = 0;
            yielded = 1;
            break;

        default:
            printf("[vm] unknown opcode %d at ip=%d\n", op, ip-1);
            running = 0;
            break;
        }
    }
    return yielded;
}

/* -----------------------------------------------------------------------
 * guarda_pila / carga_pila
 * Save/restore the stack slice belonging to a process.
 * In DIV each process owns a slice of pila[]; we replicate that with sp.
 * ----------------------------------------------------------------------- */
static void guarda_pila(int proc)
{
    mem[proc + _SP] = sp;
}

static void carga_pila(int proc)
{
    sp = mem[proc + _SP];
}

/* -----------------------------------------------------------------------
 * exec_process - one-frame scheduler
 * Mirrors i.cpp:interprete() / exec_process().
 * Iterates all alive processes, highest priority first,
 * runs each once per frame (checks _Executed flag).
 * ----------------------------------------------------------------------- */
void exec_process(void)
{
    int i, best, best_pri;

    /* reset executed flags */
    for (i = 0; i < proc_count; i++) {
        int p = proc_ids[i];
        mem[p + _Executed] = 0;
    }

    /* round-robin by priority: keep picking until all executed */
    int remaining = proc_count;
    while (remaining > 0) {
        best = -1;
        best_pri = -1;

        for (i = 0; i < proc_count; i++) {
            int p = proc_ids[i];
            if (mem[p + _Status] != ST_ALIVE)   continue;
            if (mem[p + _Executed])              continue;
            if (mem[p + _Priority] > best_pri) {
                best_pri = mem[p + _Priority];
                best     = p;
            }
        }

        if (best == -1) break;  /* none left */

        /* switch to this process */
        id = best;
        ip = mem[id + _IP];
        carga_pila(id);

        nucleo_exec();

        guarda_pila(id);
        mem[id + _Executed] = 1;
        remaining--;
    }

    /* purge dead processes */
    int new_count = 0;
    for (i = 0; i < proc_count; i++) {
        int p = proc_ids[i];
        if (mem[p + _Status] != ST_DEAD)
            proc_ids[new_count++] = p;
    }
    proc_count = new_count;
}

/* -----------------------------------------------------------------------
 * vm_spawn_main - bootstrap the first (main) process
 * ----------------------------------------------------------------------- */
void vm_spawn_main(int entry)
{
    int base = alloc_process();
    mem[base + _Id]       = base;
    mem[base + _Status]   = ST_ALIVE;
    mem[base + _IP]       = entry;
    mem[base + _SP]       = -1;
    mem[base + _Frame]    = 0;
    mem[base + _Executed] = 0;
    mem[base + _Priority] = 100;
    mem[base + _Father]   = 0;
    mem[base + _Caller]   = 0;

    proc_ids[proc_count++] = base;
    printf("[vm] main process id=%d entry=0x%x\n", base, entry);
}

/* -----------------------------------------------------------------------
 * vm_run - run the scheduler until no processes remain
 * ----------------------------------------------------------------------- */
void vm_run(void)
{
    int frame = 0;
    while (proc_count > 0) {
        printf("--- frame %d  (procs alive: %d) ---\n", frame++, proc_count);
        exec_process();
        if (frame > 1000) { puts("frame limit hit"); break; }
    }
}
