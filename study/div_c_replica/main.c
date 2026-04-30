/* main.c - Test program loaded directly into mem[]
 *
 * Simulates what the DIV compiler would generate for:
 *
 *   process main()
 *   begin
 *     x = 3 + 4;
 *     write(x);
 *     son_proc();    // spawn child
 *     frame;         // yield one frame
 *     write(99);
 *   end
 *
 *   process son_proc()
 *   begin
 *     write(77);
 *     return;
 *   end
 */
#include <stdio.h>
#include <string.h>
#include "opcodes.h"
#include "process.h"

/* vm.c exports */
#define MEM_SIZE 4096
extern int mem[MEM_SIZE];
extern void vm_spawn_main(int entry);
extern void vm_run(void);

/* -----------------------------------------------------------------------
 * mem[] layout:
 *   0x00 .. : main process code
 *   0x20 .. : son_proc code
 *   0x200.  : process blocks (PROC_BASE = 512)
 * ----------------------------------------------------------------------- */

/* helper: write opcodes/data into mem[] sequentially */
static int cursor = 0;
static void E(int v) { mem[cursor++] = v; }

int main(void)
{
    memset(mem, 0, sizeof(mem));

    /* ---- main process code at address 0x00 ---- */
    int main_entry = cursor;   /* = 0 */

    /* x = 3 + 4  (x lives at some global address, say mem[400]) */
    E(lcar); E(3);        /* push 3 */
    E(lcar); E(4);        /* push 4 */
    E(ladd);              /* 3+4=7 on top of stack */
    E(lcar); E(400);      /* push address of x */
    E(lasi);              /* mem[400]=7; pila[sp]=7 */

    /* write(x) - push x then call builtin 0 */
    E(lcar); E(400);      /* push address of x */
    E(lptr);              /* dereference: pila[sp] = mem[400] = 7 */
    E(lfun); E(0);        /* write: prints pila[sp] */

    /* son_proc() - spawn child (entry at 0x20=32) */
    E(lcal); E(0x20); E(0);  /* spawn son_proc, 0 params */

    /* frame - yield back to scheduler */
    E(lfrm);

    /* after resume next frame: write(99) */
    E(lcar); E(99);
    E(lfun); E(0);

    /* end of main */
    E(lret);

    /* ---- son_proc code at address 0x20 = 32 ---- */
    /* pad to 0x20 */
    while (cursor < 0x20) E(lnop);
    int son_entry = cursor;   /* = 32 */

    E(lcar); E(77);
    E(lfun); E(0);   /* write(77) */
    E(lret);

    /* ---- run ---- */
    printf("=== DIV C replica ===\n");
    printf("main_entry=0x%x  son_entry=0x%x\n", main_entry, son_entry);
    vm_spawn_main(main_entry);
    vm_run();
    printf("=== done ===\n");
    return 0;
}
