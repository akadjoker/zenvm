/* process.h - Process field offsets in mem[] (from src/div32run/inter.h) */
#ifndef PROCESS_H
#define PROCESS_H

/*
 * Every process is a block of integers inside mem[].
 * The base address of the block IS the process id (_Id).
 * Fields are accessed as: mem[id + FIELD_OFFSET]
 */

#define PROC_BLOCK_SIZE 64   /* reserved words per process block */

/* offsets from process base address */
#define _Id         0   /* self-id (= base address of this block) */
#define _Status     4   /* 0=dead, 1=killed, 2=alive, 3=sleeping, 4=frozen */
#define _IP         7   /* saved instruction pointer (resume here after frame) */
#define _NumPar     5   /* number of parameters */
#define _Param      6   /* index in pila[] where params start */
#define _SP         8   /* saved stack pointer */
#define _Executed   9   /* already executed this frame? */
#define _Frame      13  /* frame counter (frame(n) does partial frames) */
#define _Caller     19  /* id of calling process */
#define _Father     20  /* id of parent process */
#define _Son        21  /* id of first child */
#define _SmallBro   22  /* younger sibling id */
#define _BigBro     23  /* older sibling id */
#define _Priority   24  /* scheduling priority (higher = runs first) */

/* _Status values */
#define ST_DEAD     0
#define ST_KILLED   1
#define ST_ALIVE    2
#define ST_SLEEPING 3
#define ST_FROZEN   4

#endif
