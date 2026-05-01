#ifndef ZEN_OBJECT_H
#define ZEN_OBJECT_H

#include "value.h"

namespace zen
{

    /*
    ** Obj — Base de TODOS os objectos no heap.
    **
    ** Tri-color GC (como Lua 5.x):
    **   WHITE = não visitado (candidato a free)
    **   GRAY  = visitado mas filhos não processados
    **   BLACK = visitado e filhos processados
    **
    ** Todos os Obj vivem numa linked list (gc_next) para varrer.
    ** O tipo (ObjType) permite cast seguro sem virtual/RTTI.
    **
    ** Layout em memória:
    **   [Obj header (24 bytes)] [dados específicos do tipo]
    **   Sem vtable, sem RTTI, sem new/delete — tudo via zen::alloc().
    */

    enum ObjType : uint8_t
    {
        OBJ_STRING,
        OBJ_FUNC,
        OBJ_NATIVE,
        OBJ_UPVALUE,
        OBJ_CLOSURE,
        OBJ_FIBER,
        OBJ_PROCESS,
        OBJ_ARRAY,
        OBJ_MAP,
        OBJ_SET,
        OBJ_STRUCT,
        OBJ_CLASS,
        OBJ_INSTANCE,
    };

    enum GCColor : uint8_t
    {
        GC_WHITE = 0, /* não marcado — vai ser libertado se ficar white */
        GC_GRAY = 1,  /* marcado mas filhos por processar */
        GC_BLACK = 2, /* marcado e filhos processados */
    };

    struct Obj
    {
        ObjType type;
        GCColor color;
        uint8_t _pad[2];
        uint32_t hash; /* hash para strings; 0 para outros */
        Obj *gc_next;  /* linked list de todos os objectos */
    };

    /* Helpers para cast seguro (sem RTTI, sem dynamic_cast) */
    inline bool is_obj_type(Value v, ObjType t)
    {
        return is_obj(v) && v.as.obj->type == t;
    }

    /* =========================================================
    ** ObjString — String interned.
    **
    ** Interning: todas as strings com o mesmo conteúdo partilham
    ** o mesmo ObjString*. Permite comparação por ponteiro (O(1)).
    ** Hash pré-calculado na criação (FNV-1a).
    **
    ** Layout: [Obj header][length][chars... \0]
    ** O buffer de chars está INLINE logo após o struct (flexible member).
    ** Zero allocation extra — um só bloco de memória.
    ** ========================================================= */

    struct ObjString
    {
        Obj obj;
        int32_t length;
        char chars[]; /* flexible array member — C99/C++11 */
    };

    inline bool is_string(Value v) { return is_obj_type(v, OBJ_STRING); }
    inline ObjString *as_string(Value v) { return (ObjString *)v.as.obj; }
    inline const char *as_cstring(Value v) { return ((ObjString *)v.as.obj)->chars; }

    /* FNV-1a hash */
    inline uint32_t hash_string(const char *str, int length)
    {
        uint32_t h = 2166136261u;
        for (int i = 0; i < length; i++)
        {
            h ^= (uint8_t)str[i];
            h *= 16777619u;
        }
        return h;
    }

    /* =========================================================
    ** ObjFunc — Função script compilada.
    **
    ** Contém o bytecode, pool de constantes, e metadados.
    ** Equivalente a Proto* no Lua.
    ** ========================================================= */

    typedef uint32_t Instruction;

    struct ObjFunc
    {
        Obj obj;
        int32_t arity;      /* número de params */
        int32_t num_regs;   /* registos necessários (calculado pelo compiler) */
        int32_t code_count; /* número de instruções */
        int32_t code_capacity;
        int32_t const_count; /* número de constantes */
        int32_t const_capacity;
        Instruction *code; /* bytecode array (fixo após compilação) */
        int32_t *lines;    /* lines[i] = source line da instrução i */
        Value *constants;  /* pool de constantes (ints, floats, strings) */
        ObjString *name;   /* nome da função (debug) */
        ObjString *source; /* ficheiro fonte (debug) */
    };

    inline bool is_func(Value v) { return is_obj_type(v, OBJ_FUNC); }
    inline ObjFunc *as_func(Value v) { return (ObjFunc *)v.as.obj; }

    /* =========================================================
    ** ObjNative — Wrapper de função C++ registada.
    ** ========================================================= */

    typedef int (*NativeFn)(VM *vm, Value *args, int nargs);

    struct ObjNative
    {
        Obj obj;
        NativeFn fn;
        int32_t arity; /* -1 = variadic */
        ObjString *name;
    };

    inline bool is_native(Value v) { return is_obj_type(v, OBJ_NATIVE); }
    inline ObjNative *as_native(Value v) { return (ObjNative *)v.as.obj; }

    /* =========================================================
    ** ObjUpvalue — Célula de upvalue (captura de variável local).
    **
    ** Como Lua 5.x:
    **   - "open": aponta para um slot no stack do fiber (location != &closed)
    **   - "closed": quando o scope fecha, o valor é copiado para 'closed'
    **     e location passa a apontar para &closed
    **
    ** Upvalues abertos do mesmo slot partilham o mesmo ObjUpvalue*
    ** (deduplicados via open_upvalues list no Fiber).
    ** ========================================================= */

    struct ObjUpvalue
    {
        Obj obj;
        Value *location;  /* aponta para stack slot OU para &closed */
        Value closed;     /* valor capturado quando o scope fecha */
        ObjUpvalue *next; /* linked list de upvalues abertos (por fiber) */
    };

    inline bool is_upvalue(Value v) { return is_obj_type(v, OBJ_UPVALUE); }
    inline ObjUpvalue *as_upvalue(Value v) { return (ObjUpvalue *)v.as.obj; }

    /* =========================================================
    ** ObjClosure — Função + upvalues capturados.
    **
    ** Toda função que captura variáveis é wrappada num Closure.
    ** O compilador emite CLOSURE (não LOADK de func) quando há upvalues.
    ** Funções sem captures podem usar ObjFunc directamente (optimização).
    ** ========================================================= */

    struct ObjClosure
    {
        Obj obj;
        ObjFunc *func;         /* a função subjacente */
        ObjUpvalue **upvalues; /* array de ponteiros (tamanho = upvalue_count) */
        int32_t upvalue_count; /* quantos upvalues capturados */
    };

    inline bool is_closure(Value v) { return is_obj_type(v, OBJ_CLOSURE); }
    inline ObjClosure *as_closure(Value v) { return (ObjClosure *)v.as.obj; }

    /* =========================================================
    ** ObjFiber — Coroutine/Fiber (stack próprio, yield/resume).
    **
    ** Cada fiber tem a sua call stack e register stack independentes.
    ** O scheduler é cooperativo (yield explícito, não preemptivo).
    **
    ** Estados:
    **   FIBER_READY    — criado mas nunca executado
    **   FIBER_RUNNING  — activamente a executar
    **   FIBER_SUSPENDED— pausado por yield (pode ser resumido)
    **   FIBER_DONE     — terminou (não pode ser resumido)
    **   FIBER_ERROR    — terminou com erro
    **
    ** Uso no script:
    **   var f = Fiber.new(def() { yield 1; yield 2; })
    **   print(f.resume())  // 1
    **   print(f.resume())  // 2
    **   print(f.state)     // "done"
    ** ========================================================= */

    enum FiberState : uint8_t
    {
        FIBER_READY = 0,
        FIBER_RUNNING,
        FIBER_SUSPENDED,
        FIBER_DONE,
        FIBER_ERROR,
    };

    struct CallFrame; /* forward — definido em vm.h */

    struct ObjFiber
    {
        Obj obj;
        FiberState state;

        /* Stack próprio */
        Value *stack; /* register stack alocado */
        int32_t stack_capacity;
        Value *stack_top;

        /* Call frames próprios */
        CallFrame *frames;
        int32_t frame_count;
        int32_t frame_capacity;

        /* Upvalues abertos neste fiber (para close ao sair de scope) */
        ObjUpvalue *open_upvalues;

        /* Fiber que chamou este (para retornar após finish/yield) */
        ObjFiber *caller;

        /* Valor passado via yield/resume */
        Value transfer_value;

        /* Frame speed (processo DIV-style):
        **   100 = normal (1x por tick)
        **    50 = rápido (2x por tick, ou interpolado)
        **   200 = lento  (1x a cada 2 ticks)
        ** Acumulador: scheduler soma frame_speed por tick,
        ** executa quando accumulator >= 100. */
        int32_t frame_speed;
        int32_t frame_accumulator;

        /* Mensagem de erro (se state == FIBER_ERROR) */
        ObjString *error;
    };

    inline bool is_fiber(Value v) { return is_obj_type(v, OBJ_FIBER); }
    inline ObjFiber *as_fiber(Value v) { return (ObjFiber *)v.as.obj; }

    /* =========================================================
    ** ObjArray — Array dinâmico de Values.
    **
    ** Não usa std::vector — gerido manualmente para controlo do GC.
    ** Quando cresce, aloca novo buffer e o velho é libertado.
    ** ========================================================= */

    struct ObjArray
    {
        Obj obj;
        Value *data;    /* buffer start */
        Value *end;     /* data + count (one past last element) */
        Value *cap_end; /* data + capacity */
    };

/* Convenience macros — zero cost */
#define arr_count(a) ((int32_t)((a)->end - (a)->data))
#define arr_capacity(a) ((int32_t)((a)->cap_end - (a)->data))

    inline bool is_array(Value v) { return is_obj_type(v, OBJ_ARRAY); }
    inline ObjArray *as_array(Value v) { return (ObjArray *)v.as.obj; }

    /* =========================================================
    ** ObjMap — Chained hash table with node pool.
    ** Bucket array (int32_t[]) → compact, cache-friendly.
    ** Node pool (flat array) — O(1) alloc via free-list.
    ** No tombstones, no rehash copies (only relink chains).
    ** Load factor ≤ 1.0 → avg chain length ~1.
    ** ========================================================= */

    struct MapNode
    {
        Value key;     /* 16 */
        Value value;   /* 16 */
        uint32_t hash; /*  4 */
        int32_t next;  /*  4: next in chain or free list (-1 = end) */
    }; /* 40 bytes */

    struct ObjMap
    {
        Obj obj;
        int32_t count;        /* live entries */
        int32_t capacity;     /* node array capacity */
        int32_t bucket_count; /* power of 2 */
        int32_t free_head;    /* head of free list (-1 = none) */
        int32_t *buckets;     /* bucket_count entries, -1 = empty */
        MapNode *nodes;       /* flat node array */
    };

    inline bool is_map(Value v) { return is_obj_type(v, OBJ_MAP); }
    inline ObjMap *as_map(Value v) { return (ObjMap *)v.as.obj; }

    /* =========================================================
    ** ObjSet — Same chaining approach, no value field.
    ** ========================================================= */

    struct SetNode
    {
        Value key;     /* 16 */
        uint32_t hash; /*  4 */
        int32_t next;  /*  4 */
    }; /* 24 bytes */

    struct ObjSet
    {
        Obj obj;
        int32_t count;
        int32_t capacity;
        int32_t bucket_count;
        int32_t free_head;
        int32_t *buckets;
        SetNode *nodes;
    };

    inline bool is_set(Value v) { return is_obj_type(v, OBJ_SET); }
    inline ObjSet *as_set(Value v) { return (ObjSet *)v.as.obj; }

    /* =========================================================
    ** ObjStruct — Value type (sem herança, sem métodos virtuais).
    **
    ** Diferença de class:
    **   - Sem herança
    **   - Sem métodos (ou só métodos estáticos)
    **   - Comparação por valor (fields iguais → structs iguais)
    **   - Mais leve: não precisa de method lookup
    **
    ** Uso no script:
    **   struct Vec3 { x, y, z }
    **   var p = Vec3(1.0, 2.0, 3.0)
    **   print(p.x)  // 1.0
    ** ========================================================= */

    struct ObjStructDef
    {
        Obj obj;
        ObjString *name;
        int32_t num_fields;
        ObjString **field_names; /* nomes dos fields (para init posicional) */
    };

    struct ObjStruct
    {
        Obj obj;
        ObjStructDef *def; /* definição (partilhada por todas as instâncias) */
        Value *fields;     /* array de num_fields Values */
    };

    inline bool is_struct_def(Value v) { return is_obj_type(v, OBJ_STRUCT) && ((ObjStruct *)v.as.obj)->def == nullptr; }
    inline bool is_struct(Value v) { return is_obj_type(v, OBJ_STRUCT); }
    inline ObjStruct *as_struct(Value v) { return (ObjStruct *)v.as.obj; }
    inline ObjStructDef *as_struct_def(Value v) { return (ObjStructDef *)v.as.obj; }

    /* =========================================================
    ** ObjClass + ObjInstance — Classes com herança 1 nível.
    **
    ** class Enemy : NativeBase {
    **     var hp = 100;
    **     def attack(target) { ... }
    ** }
    **
    ** ObjClass guarda métodos (ObjFunc*) e o template de fields.
    ** ObjInstance guarda os valores dos fields.
    ** Herança: se method não existe no script, procura no parent (C++).
    ** ========================================================= */

    struct ObjClass
    {
        Obj obj;
        ObjString *name;
        ObjClass *parent;        /* NULL ou classe pai (1 nível) */
        ObjMap *methods;         /* nome → ObjFunc/ObjNative */
        int32_t num_fields;      /* quantos fields declarados */
        ObjString **field_names; /* nomes dos fields (para init) */
    };

    struct ObjInstance
    {
        Obj obj;
        ObjClass *klass;
        Value *fields; /* array de num_fields Values */
    };

    inline bool is_class(Value v) { return is_obj_type(v, OBJ_CLASS); }
    inline bool is_instance(Value v) { return is_obj_type(v, OBJ_INSTANCE); }
    inline ObjClass *as_class(Value v) { return (ObjClass *)v.as.obj; }
    inline ObjInstance *as_instance(Value v) { return (ObjInstance *)v.as.obj; }

} /* namespace zen */

#endif /* ZEN_OBJECT_H */
