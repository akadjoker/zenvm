#ifndef ZEN_VM_H
#define ZEN_VM_H

#include "memory.h"
#include "opcodes.h"

namespace zen {

/*
** CallFrame — estado de uma chamada activa.
** Igual ao conceito do toy_vm mas com multi-return.
*/
struct CallFrame {
    ObjClosure*  closure;   /* closure a executar (nullptr = top-level script) */
    ObjFunc*     func;      /* func shortcut (== closure->func) */
    Instruction* ip;        /* instruction pointer (ponteiro directo) */
    Value*       base;      /* base dos registos deste frame */
    int          ret_reg;   /* registo no caller onde começam os resultados */
    int          ret_count; /* quantos resultados o caller espera (-1=all) */
};

/*
** VM — estado completo do interpretador.
**
** Classe em vez de struct+funções livres:
**   - Encapsulamento natural (internals privados)
**   - Métodos inline no hot path sem overhead
**   - Permite herdar/mock para testes se necessário
**   - this implícito em vez de passar vm* em tudo
*/
class VM {
public:
    VM();
    ~VM();

    /* Não copiável */
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    /* --- Execução --- */
    void  run(ObjFunc* func);
    void  run(ObjClosure* closure);
    Value call_global(int idx, Value* args, int nargs);
    Value call_global(const char* name, Value* args, int nargs);

    /* --- Globals (by index — O(1), used at runtime) --- */
    int   def_global(const char* name, Value val);   /* returns index */
    Value get_global(int idx) const  { return globals_[idx]; }
    void  set_global(int idx, Value val) { globals_[idx] = val; }

    /* --- Globals (by name — O(n), used at compile/embed time only) --- */
    int   find_global(const char* name) const;
    Value get_global(const char* name) const;
    void  set_global(const char* name, Value val);
    int   num_globals() const { return num_globals_; }
    const char* global_name(int idx) const { return global_names_[idx] ? global_names_[idx]->chars : nullptr; }

    /* --- Natives --- */
    int  def_native(const char* name, NativeFn fn, int arity);

    /* --- Class builder --- */
    struct ClassBuilder {
        ClassBuilder(VM* vm, const char* name);
        ClassBuilder& parent(const char* parent_name);
        ClassBuilder& field(const char* name);
        ClassBuilder& method(const char* name, NativeFn fn, int arity);
        ObjClass*     end();
    private:
        VM*         vm_;
        ObjClass*   klass_;
    };
    ClassBuilder def_class(const char* name);

    /* --- Struct builder --- */
    struct StructBuilder {
        StructBuilder(VM* vm, const char* name);
        StructBuilder& field(const char* name);
        ObjStructDef*  end();
    private:
        VM*          vm_;
        ObjStructDef* def_;
    };
    StructBuilder def_struct(const char* name);

    /* --- Fiber API --- */
    ObjFiber* new_fiber(ObjClosure* closure, int stack_size = 256);
    Value     resume_fiber(ObjFiber* fiber, Value val);

    /* --- Strings (para embedding — internadas) --- */
    ObjString* make_string(const char* str, int length = -1);

    /* --- GC --- */
    GC&  get_gc() { return gc_; }
    void collect() ;

private:
/*
** Dispatch mode — seleccionado em compile-time:
**   -DZEN_COMPUTED_GOTO   → computed goto (GCC/Clang, ~45% mais rápido)
**   default               → switch (portável, MSVC)
**
** Detecção automática se não definido explicitamente:
*/
#ifndef ZEN_DISPATCH_MODE
  #if defined(__GNUC__) || defined(__clang__)
    #define ZEN_COMPUTED_GOTO
  #endif
#endif

    /* --- Dispatch (hot path) — implementado em vm_dispatch.cpp --- */
    void execute(ObjFiber* fiber);

    /* --- Helpers internos --- */
    ObjUpvalue* capture_upvalue(ObjFiber* fiber, Value* local);
    void        close_upvalues(ObjFiber* fiber, Value* last);
    bool        call_value(ObjFiber* fiber, Value callee, int nargs, int nresults);
    bool        call_closure(ObjFiber* fiber, ObjClosure* closure, int nargs, int nresults);
    void        runtime_error(const char* fmt, ...);

    /* find_global moved to public */

    /* --- Estado --- */
    GC          gc_;

    /* Globals */
    Value       globals_[MAX_GLOBALS];
    ObjString*  global_names_[MAX_GLOBALS];
    int         num_globals_;

    /* Main fiber (script top-level corre aqui) */
    ObjFiber*   main_fiber_;
    ObjFiber*   current_fiber_;  /* fiber actualmente a executar */
};

} /* namespace zen */

#endif /* ZEN_VM_H */
