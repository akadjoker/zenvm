#ifndef ZEN_VM_H
#define ZEN_VM_H

#include "memory.h"
#include "opcodes.h"
#include "module.h"

namespace zen
{

    /*
    ** CallFrame — estado de uma chamada activa.
    ** Igual ao conceito do toy_vm mas com multi-return.
    */
    struct CallFrame
    {
        ObjClosure *closure; /* closure a executar (nullptr = top-level script) */
        ObjFunc *func;       /* func shortcut (== closure->func) */
        Instruction *ip;     /* instruction pointer (ponteiro directo) */
        Value *base;         /* base dos registos deste frame */
        int ret_reg;         /* registo no caller onde começam os resultados */
        int ret_count;       /* quantos resultados o caller espera (-1=all) */
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
    class VM
    {
    public:
        VM();
        ~VM();

        /* Não copiável */
        VM(const VM &) = delete;
        VM &operator=(const VM &) = delete;

        /* --- Execução --- */
        void run(ObjFunc *func);
        void run(ObjClosure *closure);
        Value call_global(int idx, Value *args, int nargs);
        Value call_global(const char *name, Value *args, int nargs);

        /* --- Globals (by index — O(1), used at runtime) --- */
        int def_global(const char *name, Value val); /* returns index */
        Value get_global(int idx) const { return globals_[idx]; }
        void set_global(int idx, Value val) { globals_[idx] = val; }

        /* --- Globals (by name — O(n), used at compile/embed time only) --- */
        int find_global(const char *name) const;
        Value get_global(const char *name) const;
        void set_global(const char *name, Value val);
        int num_globals() const { return num_globals_; }
        const char *global_name(int idx) const { return global_names_[idx] ? global_names_[idx]->chars : nullptr; }

        /* --- Natives --- */
        int def_native(const char *name, NativeFn fn, int arity);

        /* --- Module registry --- */
        void register_lib(const NativeLib *lib);  /* make available for import */
        void open_lib(const NativeLib *lib);       /* register + put in globals */
        const NativeLib *find_lib(const char *name) const;
        int open_lib_globals(const NativeLib *lib); /* returns base gidx */
        const NativeLib *try_load_plugin(const char *name); /* dlopen fallback */

        /* --- Class builder --- */
        struct ClassBuilder
        {
            ClassBuilder(VM *vm, const char *name);
            ClassBuilder &parent(const char *parent_name);
            ClassBuilder &field(const char *name);
            ClassBuilder &method(const char *name, NativeFn fn, int arity);
            ClassBuilder &ctor(NativeClassCtor fn);       /* native constructor (returns void*) */
            ClassBuilder &dtor(NativeClassDtor fn);       /* native destructor */
            ClassBuilder &persistent(bool p = true);      /* instances NOT managed by GC */
            ClassBuilder &constructable(bool c = true);   /* false = script cannot instantiate */
            ObjClass *end();

        private:
            VM *vm_;
            ObjClass *klass_;
        };
        ClassBuilder def_class(const char *name);

        /* --- Instance API (for C++ embedding) --- */
        Value make_instance(ObjClass *klass);                    /* creates instance, no init */
        Value make_instance(ObjClass *klass, Value *args, int nargs); /* creates + calls init */
        void destroy_instance(Value instance);                   /* destroy persistent instance */
        Value invoke(Value instance, const char *method, Value *args, int nargs);
        Value invoke(Value instance, int slot, Value *args, int nargs); /* vtable slot, O(1) */

        /* --- Struct builder --- */
        struct StructBuilder
        {
            StructBuilder(VM *vm, const char *name);
            StructBuilder &field(const char *name);
            ObjStructDef *end();

        private:
            VM *vm_;
            ObjStructDef *def_;
        };
        StructBuilder def_struct(const char *name);

        /* --- Native Struct binding (zero-copy C++ struct access) --- */
        struct NativeStructBuilder
        {
            NativeStructBuilder(VM *vm, const char *name, uint16_t size,
                                NativeStructCtor ctor, NativeStructDtor dtor);
            NativeStructBuilder &field(const char *name, uint16_t offset,
                                       NativeFieldType type, bool read_only = false);
            /* Convenience shortcuts */
            NativeStructBuilder &i32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_INT, ro); }
            NativeStructBuilder &u32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_UINT, ro); }
            NativeStructBuilder &f32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_FLOAT, ro); }
            NativeStructBuilder &f64(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_DOUBLE, ro); }
            NativeStructBuilder &byte(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BYTE, ro); }
            NativeStructBuilder &boolean(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BOOL, ro); }
            NativeStructBuilder &ptr(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_POINTER, ro); }
            NativeStructDef *end();

        private:
            VM *vm_;
            NativeStructDef *def_;
        };
        NativeStructBuilder register_native_struct(const char *name, uint16_t size,
                                                   NativeStructCtor ctor = nullptr,
                                                   NativeStructDtor dtor = nullptr);

        /* --- Fiber API --- */
        ObjFiber *new_fiber(ObjClosure *closure, int stack_size = 256, int max_frames = kMaxFrames);
        Value resume_fiber(ObjFiber *fiber, Value val);

        /* --- Process system (cooperative multitasking) --- */

        struct ProcessSlot
        {
            ObjFiber *fiber;
            int id;
            int z;          /* z-order for sorting (lower = drawn first) */
        };

        enum ProcessState : uint8_t
        {
            PSTATE_DEAD = 0,
            PSTATE_RUNNING,
            PSTATE_SUSPENDED,
            PSTATE_FROZEN,
        };

        int spawn_process(ObjClosure *closure, Value *args = nullptr, int nargs = 0);
        void kill_process(int id);
        void kill_all_processes();
        int tick_processes(float dt = 0.016f);   /* returns number of alive processes */
        int num_processes() const { return num_alive_; }
        ObjFiber *get_process(int id) const;
        void sort_processes(); /* sort by z-order */

        /* Iterate all alive processes (DIV-style: read privados via fiber->stack[reg]) */
        typedef void (*ProcessIterFn)(VM *vm, ObjFiber *proc, int id, void *userdata);
        void for_each_process(ProcessIterFn fn, void *userdata = nullptr);

        /* Process hooks (C++ callbacks) — all receive fiber so you can read privados */
        typedef void (*ProcessHook)(VM *vm, ObjFiber *proc);
        ProcessHook on_process_start;   /* after first FRAME */
        ProcessHook on_process_update;  /* after each FRAME (every tick) */
        ProcessHook on_process_end;     /* when process dies */

        /* --- File I/O (for include/import) --- */
        void add_search_path(const char *dir);
        char *read_file(const char *path, const char *relative_to, long *out_size,
                        char *resolved_path = nullptr, int resolved_max = 0);

        /* --- Strings (para embedding — internadas) --- */
        ObjString *make_string(const char *str, int length = -1);

        /* --- GC --- */
        GC &get_gc() { return gc_; }
        ObjFiber *current_fiber() const { return current_fiber_; }
        void collect();
        void gc_mark_roots(); /* mark all VM roots for GC */

        /* --- Error reporting (for native functions) --- */
        void runtime_error(const char *fmt, ...);

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
        void execute(ObjFiber *fiber);

        /* --- Helpers internos --- */
        ObjUpvalue *capture_upvalue(ObjFiber *fiber, Value *local);
        void close_upvalues(ObjFiber *fiber, Value *last);
        bool call_value(ObjFiber *fiber, Value callee, int nargs, int nresults);
        bool call_closure(ObjFiber *fiber, ObjClosure *closure, int nargs, int nresults);

        /* find_global moved to public */

        /* --- Estado --- */
        GC gc_;

        /* Globals */
        Value globals_[MAX_GLOBALS];
        ObjString *global_names_[MAX_GLOBALS];
        int num_globals_;

        /* Main fiber (script top-level corre aqui) */
        ObjFiber *main_fiber_;
        ObjFiber *current_fiber_; /* fiber actualmente a executar */
        int fiber_depth_;         /* current nested execute() depth */
        bool had_error_;          /* runtime error occurred */

        /* Search paths for include/import */
        static const int MAX_SEARCH_PATHS = 16;
        char *search_paths_[MAX_SEARCH_PATHS];
        int num_search_paths_;

        /* Module registry */
        static const int MAX_LIBS = 32;
        const NativeLib *libs_[MAX_LIBS];
        int num_libs_;

        /* Loaded plugin handles (for cleanup) */
        static const int MAX_PLUGINS = 16;
        void *plugin_handles_[MAX_PLUGINS];
        int num_plugins_;

        /* Method selector table (for vtable dispatch) */
        static const int MAX_SELECTORS = 256;
        ObjString *selectors_[MAX_SELECTORS]; /* interned method names */
        int num_selectors_;

        /* Process pool (dynamic, compact, swap-remove) */
        ProcessSlot *pool_;         /* dynamic array of alive processes */
        int num_alive_;             /* count of alive entries in pool_ */
        int pool_capacity_;         /* allocated capacity */
        int next_process_id_;

    public:
        bool had_error() const { return had_error_; }

        /* Intern a method name → slot index (0..255). Same name → same slot. */
        int intern_selector(const char *name, int len);
        int find_selector(const char *name, int len) const;
        int num_selectors() const { return num_selectors_; }
    };

} /* namespace zen */

#endif /* ZEN_VM_H */
