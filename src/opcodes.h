#ifndef ZEN_OPCODES_H
#define ZEN_OPCODES_H

/*
** opcodes.h — Todas as instruções da VM.
**
** Formato: 32 bits  [ opcode(8) | A(8) | B(8) | C(8) ]
** ou:      32 bits  [ opcode(8) | A(8) | Bx(16) ]       para constantes
** ou:      32 bits  [ opcode(8) | A(8) | sBx(16) ]      para jumps
**
** A  = registo destino
** B  = registo fonte ou índice curto
** C  = registo fonte 2
** Bx = índice 16-bit (constantes, globais)
** sBx = signed 16-bit offset (jumps)
*/

namespace zen {

enum OpCode : uint8_t {
    /* --- Load/Store --- */
    OP_LOADNIL,     /* R[A] = nil                              */
    OP_LOADBOOL,    /* R[A] = (bool)B; se C: pc++              */
    OP_LOADK,       /* R[A] = constants[Bx]                    */
    OP_LOADI,       /* R[A] = (int)sBx                         */
    OP_MOVE,        /* R[A] = R[B]                             */

    /* --- Globals (indexados) --- */
    OP_GETGLOBAL,   /* R[A] = globals[Bx]                      */
    OP_SETGLOBAL,   /* globals[Bx] = R[A]                      */

    /* --- Aritmética --- */
    OP_ADD,         /* R[A] = R[B] + R[C]                      */
    OP_SUB,         /* R[A] = R[B] - R[C]                      */
    OP_MUL,         /* R[A] = R[B] * R[C]                      */
    OP_DIV,         /* R[A] = R[B] / R[C]                      */
    OP_MOD,         /* R[A] = R[B] % R[C]                      */
    OP_NEG,         /* R[A] = -R[B]                            */

    /* --- Aritmética imediata (superinstruções) --- */
    OP_ADDI,        /* R[A] = R[B] + (signed)C                 */
    OP_SUBI,        /* R[A] = R[B] - (signed)C                 */

    /* --- Bitwise --- */
    OP_BAND,        /* R[A] = R[B] & R[C]                      */
    OP_BOR,         /* R[A] = R[B] | R[C]                      */
    OP_BXOR,        /* R[A] = R[B] ^ R[C]                      */
    OP_BNOT,        /* R[A] = ~R[B]                            */
    OP_SHL,         /* R[A] = R[B] << R[C]                     */
    OP_SHR,         /* R[A] = R[B] >> R[C]                     */

    /* --- Comparação (result em R[A]) --- */
    OP_EQ,          /* R[A] = (R[B] == R[C])                   */
    OP_LT,          /* R[A] = (R[B] <  R[C])                   */
    OP_LE,          /* R[A] = (R[B] <= R[C])                   */
    OP_NOT,         /* R[A] = !R[B]                            */

    /* --- Jumps --- */
    OP_JMP,         /* pc += sBx                               */
    OP_JMPIF,       /* se truthy(R[A]): pc += sBx              */
    OP_JMPIFNOT,    /* se !truthy(R[A]): pc += sBx             */

    /* --- Funções --- */
    OP_CALL,        /* R[A](R[A+1]..R[A+B]) → R[A]..R[A+C-1]  */
    OP_CALLGLOBAL,  /* globals[Bx(word2)](R[A+1]..+B) → R[A]..+C — 2 words */
    OP_RETURN,      /* return R[A]..R[A+B-1] (B=nresults)       */

    /* --- Closures / Upvalues --- */
    OP_CLOSURE,     /* R[A] = closure(constants[Bx])            */
    OP_GETUPVAL,    /* R[A] = upvalues[B]                       */
    OP_SETUPVAL,    /* upvalues[B] = R[A]                       */
    OP_CLOSE,       /* close upvalues >= R[A] (ao sair de scope)*/

    /* --- Fibers --- */
    OP_NEWFIBER,    /* R[A] = Fiber.new(R[B])  (B=closure)     */
    OP_RESUME,      /* R[A] = R[B].resume(R[C])                 */
    OP_YIELD,       /* yield R[A] → suspende fiber actual       */
    OP_FRAME,       /* frame; → yield com speed=100 (default)   */
    OP_FRAME_N,     /* frame(R[A]); → yield com speed=R[A]      */

    /* --- Objects --- */
    OP_NEWARRAY,    /* R[A] = []                               */
    OP_NEWMAP,      /* R[A] = {}                               */
    OP_GETFIELD,    /* R[A] = R[B].constants[C]  (field name)  */
    OP_SETFIELD,    /* R[A].constants[B] = R[C]                */
    OP_GETINDEX,    /* R[A] = R[B][R[C]]                       */
    OP_SETINDEX,    /* R[A][R[B]] = R[C]                       */

    /* --- Classes --- */
    OP_NEWCLASS,    /* R[A] = new class (nome em constants[B]) */
    OP_NEWINSTANCE, /* R[A] = klass.new()                      */
    OP_GETMETHOD,   /* R[A] = R[B]:method(constants[C])        */

    /* --- Misc --- */
    OP_CONCAT,      /* R[A] = R[B] .. R[C]  (string concat)   */
    OP_LEN,         /* R[A] = #R[B]                            */
    OP_PRINT,       /* print R[A] (debug/dev, remove em prod)  */

    /* --- Superinstruções fused (comparação + salto) --- */
    OP_LTJMPIFNOT,  /* if !(R[B] < R[C]): pc += sBx(next_word)  — 2 words */
    OP_LEJMPIFNOT,  /* if !(R[B] <= R[C]): pc += sBx(next_word) — 2 words */

    OP_HALT,
};

/* Encode/Decode — ABC format */
#define ZEN_ENCODE(op,a,b,c)  ((uint32_t)((op)<<24)|((a)<<16)|((b)<<8)|(c))
#define ZEN_OP(i)    ((uint8_t)(((i)>>24)&0xFF))
#define ZEN_A(i)     ((uint8_t)(((i)>>16)&0xFF))
#define ZEN_B(i)     ((uint8_t)(((i)>>8)&0xFF))
#define ZEN_C(i)     ((uint8_t)((i)&0xFF))

/* Encode/Decode — ABx format (16-bit unsigned operand) */
#define ZEN_ENCODE_BX(op,a,bx) ((uint32_t)((op)<<24)|((a)<<16)|((bx)&0xFFFF))
#define ZEN_BX(i)    ((uint16_t)((i)&0xFFFF))

/* Encode/Decode — AsBx format (16-bit signed offset) */
#define ZEN_ENCODE_SBX(op,a,sbx) ((uint32_t)((op)<<24)|((a)<<16)|(((sbx)+32768)&0xFFFF))
#define ZEN_SBX(i)   ((int)((int)((i)&0xFFFF) - 32768))

} /* namespace zen */

#endif /* ZEN_OPCODES_H */
