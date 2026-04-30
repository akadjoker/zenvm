#ifndef ZEN_VALUE_H
#define ZEN_VALUE_H

#include "common.h"

namespace zen {

/*
** Value — tagged union, 16 bytes.
**
** Tipos imediatos (vivem dentro do Value, sem GC):
**   NIL, BOOL, INT, FLOAT
**
** Tipos heap (apontam para GCObject, geridos pelo GC):
**   OBJ → ObjString, ObjFunc, ObjArray, ObjMap, ObjClass, ObjInstance
**
** Porquê tagged union e não NaN boxing?
**   - Debug: v.type == TYPE_INT é óbvio, 0x7FFC... não é
**   - Extensível: novos tipos = +1 enum, +1 campo
**   - Performance equivalente no nosso uso (L1 quente)
**   - NaN boxing = optimização prematura para <5% de ganho aqui
*/

enum ValueType : uint8_t {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,      /* qualquer objecto no heap (string, func, array...) */
};

struct Value {
    ValueType type;
    union {
        bool    boolean;
        int32_t integer;
        double  number;
        Obj*    obj;
    } as;
};

/* Constructores inline — sem overhead */
inline Value val_nil()             { Value v; v.type = VAL_NIL;   return v; }
inline Value val_bool(bool b)      { Value v; v.type = VAL_BOOL;  v.as.boolean = b; return v; }
inline Value val_int(int32_t i)    { Value v; v.type = VAL_INT;   v.as.integer = i; return v; }
inline Value val_float(double d)   { Value v; v.type = VAL_FLOAT; v.as.number = d;  return v; }
inline Value val_obj(Obj* o)       { Value v; v.type = VAL_OBJ;   v.as.obj = o;     return v; }

/* Type checks */
inline bool is_nil(Value v)   { return v.type == VAL_NIL; }
inline bool is_bool(Value v)  { return v.type == VAL_BOOL; }
inline bool is_int(Value v)   { return v.type == VAL_INT; }
inline bool is_float(Value v) { return v.type == VAL_FLOAT; }
inline bool is_obj(Value v)   { return v.type == VAL_OBJ; }

/* Truthiness — nil e false são falsy, tudo o resto truthy */
inline bool is_truthy(Value v) {
    if (v.type == VAL_NIL)  return false;
    if (v.type == VAL_BOOL) return v.as.boolean;
    return true;
}

/* Conversão numérica */
inline double to_number(Value v) {
    if (v.type == VAL_INT)   return (double)v.as.integer;
    if (v.type == VAL_FLOAT) return v.as.number;
    return 0.0;
}

/* Conversão para inteiro (bitwise ops) */
inline int64_t to_integer(Value v) {
    if (v.type == VAL_INT)   return v.as.integer;
    if (v.type == VAL_FLOAT) return (int64_t)v.as.number;
    return 0;
}

/* Comparação de igualdade — fast path for int (most common in tables) */
inline bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    if (__builtin_expect(a.type == VAL_INT, 1))
        return a.as.integer == b.as.integer;
    switch (a.type) {
        case VAL_NIL:   return true;
        case VAL_BOOL:  return a.as.boolean == b.as.boolean;
        case VAL_INT:   __builtin_unreachable();
        case VAL_FLOAT: return a.as.number == b.as.number;
        case VAL_OBJ:   return a.as.obj == b.as.obj;
    }
    return false;
}

} /* namespace zen */

#endif /* ZEN_VALUE_H */
