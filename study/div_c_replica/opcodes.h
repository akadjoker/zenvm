/* opcodes.h - DIV opcode defines (from src/div32run/inter.h) */
#ifndef OPCODES_H
#define OPCODES_H

#define lnop  0   /* no operation */
#define lcar  1   /* push literal: pila[++sp] = mem[ip++] */
#define lasi  2   /* assign: mem[pila[sp-1]] = pila[sp]; sp -= 2 (leaves value) */
#define lptr  3   /* dereference pointer: pila[sp] = mem[pila[sp]] */
#define ladd  12  /* add: pila[sp-1] += pila[sp]; sp-- */
#define lsub  13  /* sub: pila[sp-1] -= pila[sp]; sp-- */
#define lmul  14  /* mul: pila[sp-1] *= pila[sp]; sp-- */
#define ldiv  15  /* div: pila[sp-1] /= pila[sp]; sp-- */
#define lneg  17  /* negate: pila[sp] = -pila[sp] */
#define ljmp  23  /* jump: ip = mem[ip] */
#define ljpf  24  /* jump-if-false: if !(pila[sp--]&1) ip = mem[ip]; else ip++ */
#define lfun  25  /* call built-in function */
#define lcal  26  /* call process: create new process block in mem[] */
#define lret  27  /* return from process: restore caller, kill self */
#define lasp  28  /* adjust sp: sp += mem[ip++] */
#define lfrm  29  /* frame: yield execution, save ip */
#define lcbp  30  /* copy bp: bp = sp */
#define lcpa  31  /* copy param: mem[process+_Param] = sp (parameter slice) */

#endif
