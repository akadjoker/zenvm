#include "vm.h"
#include "debug.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* Dynamic loading */
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace zen
{

    /* =========================================================
    ** Construtor / Destrutor
    ** ========================================================= */

    VM::VM() : on_process_start(nullptr), on_process_update(nullptr), on_process_end(nullptr), num_globals_(0), main_fiber_(nullptr), current_fiber_(nullptr), fiber_depth_(0), had_error_(false), num_search_paths_(0), num_libs_(0), num_plugins_(0), num_selectors_(0), pool_(nullptr), num_alive_(0), pool_capacity_(0), next_process_id_(1), current_process_id_(-1)
    {
        gc_init(&gc_);
        gc_.vm = this;
        memset(globals_, 0, sizeof(globals_));
        memset(global_names_, 0, sizeof(global_names_));
        memset(search_paths_, 0, sizeof(search_paths_));
        memset(plugin_handles_, 0, sizeof(plugin_handles_));
        memset(selectors_, 0, sizeof(selectors_));

        /* Criar main fiber */
        main_fiber_ = new_fiber(nullptr, kMaxRegs * 4);
        current_fiber_ = main_fiber_;
    }

    VM::~VM()
    {
        /* Close loaded plugins */
#if defined(__linux__) || defined(__APPLE__)
        for (int i = 0; i < num_plugins_; i++)
            if (plugin_handles_[i]) dlclose(plugin_handles_[i]);
#elif defined(_WIN32)
        for (int i = 0; i < num_plugins_; i++)
            if (plugin_handles_[i]) FreeLibrary((HMODULE)plugin_handles_[i]);
#endif

        /* Free search paths */
        for (int i = 0; i < num_search_paths_; i++)
            free(search_paths_[i]);

        /* Free process pool (fibers + array) */
        kill_all_processes();
        free(pool_);

        /* Free all objects via GC sweep */
        gc_sweep_all(&gc_);

        /* Free gray list */
        if (gc_.gray_list)
            free(gc_.gray_list);
        /* Free intern table */
        if (gc_.strings)
            zen_free(&gc_, gc_.strings, gc_.string_capacity * sizeof(ObjString *));
    }

    /* =========================================================
    ** Fiber management
    ** ========================================================= */

    ObjFiber *VM::new_fiber(ObjClosure *closure, int stack_size, int max_frames)
    {
        /* Suppress GC during fiber construction.
        ** Problem: if zen_alloc triggers GC twice, the first cycle repaints
        ** our BLACK fiber to WHITE, and the second cycle sweeps it because
        ** it's not yet reachable from any root (not in pool_ yet).
        ** Fix: bump the GC threshold so no collection happens until we're done. */
        size_t saved_next_gc = gc_.next_gc;
        gc_.next_gc = (size_t)-1; /* disable GC trigger */

        ObjFiber *fiber = (ObjFiber *)zen_alloc(&gc_, sizeof(ObjFiber));
        fiber->obj.type = OBJ_FIBER;
        fiber->obj.color = GC_BLACK;
        fiber->obj.hash = 0;

        /* Initialize ALL fields BEFORE linking to GC list */
        fiber->state = FIBER_READY;
        fiber->stack_capacity = 0;
        fiber->stack = nullptr;
        fiber->stack_top = nullptr;
        fiber->frame_capacity = 0;
        fiber->frames = nullptr;
        fiber->frame_count = 0;
        fiber->open_upvalues = nullptr;
        fiber->caller = nullptr;
        fiber->transfer_value = val_nil();
        fiber->yield_dest = -1;
        fiber->frame_speed = 100;
        fiber->frame_accumulator = 0;
        fiber->process_id = -1;
        fiber->error = nullptr;

        /* Link to GC list */
        fiber->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)fiber;

        /* Allocate stack and frames (GC suppressed — safe) */
        fiber->stack_capacity = stack_size;
        fiber->stack = (Value *)zen_alloc(&gc_, stack_size * sizeof(Value));
        fiber->stack_top = fiber->stack;

        fiber->frame_capacity = max_frames;
        fiber->frames = (CallFrame *)zen_alloc(&gc_, max_frames * sizeof(CallFrame));

        /* Restore GC threshold */
        gc_.next_gc = saved_next_gc;

        /* Se tiver closure, prepara o primeiro frame */
        if (closure)
        {
            fiber->frame_count = 1;
            CallFrame *frame = &fiber->frames[0];
            frame->closure = closure;
            frame->func = closure->func;
            frame->ip = closure->func->code;
            frame->base = fiber->stack;
            frame->ret_reg = 0;
            frame->ret_count = 0;
            fiber->stack_top = fiber->stack + closure->func->num_regs;
        }

        return fiber;
    }

    /* =========================================================
    ** Run
    ** ========================================================= */

    void VM::run(ObjFunc *func)
    {
        /* Wrap func num closure trivial (0 upvalues) */
        ObjClosure *cl = (ObjClosure *)zen_alloc(&gc_, sizeof(ObjClosure));
        cl->obj.type = OBJ_CLOSURE;
        cl->obj.color = GC_BLACK;
        cl->obj.hash = 0;
        cl->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)cl;
        cl->func = func;
        cl->upvalues = nullptr;
        cl->upvalue_count = 0;

        run(cl);
    }

    void VM::run(ObjClosure *closure)
    {
        /* Setup main fiber com o closure */
        main_fiber_->frame_count = 1;
        CallFrame *frame = &main_fiber_->frames[0];
        frame->closure = closure;
        frame->func = closure->func;
        frame->ip = closure->func->code;
        frame->base = main_fiber_->stack;
        frame->ret_reg = 0;
        frame->ret_count = 0;
        main_fiber_->stack_top = main_fiber_->stack + closure->func->num_regs;
        main_fiber_->state = FIBER_RUNNING;
        current_fiber_ = main_fiber_;

        execute(main_fiber_);

        /* Reset fiber state for subsequent C++ API calls */
        main_fiber_->stack_top = main_fiber_->stack;
        main_fiber_->state = FIBER_RUNNING;
    }

    Value VM::call_global(int idx, Value *args, int nargs)
    {
        Value callee = globals_[idx];
        if (is_native(callee))
        {
            ObjNative *nat = as_native(callee);
            int nret = nat->fn(this, args, nargs);
            return (nret > 0) ? args[0] : val_nil();
        }
        if (is_closure(callee))
        {
            /* Place callee + args on main fiber stack, call, return result */
            ObjFiber *fiber = main_fiber_;
            Value *base = fiber->stack;
            for (int i = 0; i < nargs; i++)
                base[i] = args[i];

            ObjClosure *cl = as_closure(callee);
            fiber->frame_count = 1;
            CallFrame *frame = &fiber->frames[0];
            frame->closure = cl;
            frame->func = cl->func;
            frame->ip = cl->func->code;
            frame->base = base;
            frame->ret_reg = 0;
            frame->ret_count = 1;
            fiber->stack_top = base + cl->func->num_regs;
            fiber->state = FIBER_RUNNING;
            current_fiber_ = fiber;

            execute(fiber);
            return base[0];
        }
        runtime_error("global %d is not callable", idx);
        return val_nil();
    }

    Value VM::call_global(const char *name, Value *args, int nargs)
    {
        int idx = find_global(name);
        if (idx < 0)
        {
            runtime_error("undefined global '%s'", name);
            return val_nil();
        }
        return call_global(idx, args, nargs);
    }

    /* =========================================================
    ** Globals
    ** ========================================================= */

    int VM::find_global(const char *name) const
    {
        int len = (int)strlen(name);
        for (int i = 0; i < num_globals_; i++)
        {
            if (global_names_[i] &&
                global_names_[i]->length == len &&
                memcmp(global_names_[i]->chars, name, len) == 0)
            {
                return i;
            }
        }
        return -1;
    }

    int VM::def_global(const char *name, Value val)
    {
        int existing = find_global(name);
        if (existing >= 0)
        {
            globals_[existing] = val;
            return existing;
        }
        int idx = num_globals_++;
        global_names_[idx] = intern_string(&gc_, name, (int)strlen(name),
                                           hash_string(name, (int)strlen(name)));
        globals_[idx] = val;
        return idx;
    }

    Value VM::get_global(const char *name) const
    {
        int idx = find_global(name);
        return idx >= 0 ? globals_[idx] : val_nil();
    }

    void VM::set_global(const char *name, Value val)
    {
        int idx = find_global(name);
        if (idx >= 0)
            globals_[idx] = val;
    }

    /* --- Method selector table (vtable slots) --- */

    int VM::find_selector(const char *name, int len) const
    {
        for (int i = 0; i < num_selectors_; i++)
        {
            if (selectors_[i]->length == len &&
                memcmp(selectors_[i]->chars, name, len) == 0)
                return i;
        }
        return -1;
    }

    int VM::intern_selector(const char *name, int len)
    {
        int idx = find_selector(name, len);
        if (idx >= 0) return idx;
        idx = num_selectors_++;
        selectors_[idx] = intern_string(&gc_, name, len, hash_string(name, len));
        return idx;
    }

    int VM::def_native(const char *name, NativeFn fn, int arity)
    {
        ObjString *s = intern_string(&gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        ObjNative *nat = new_native(&gc_, fn, arity, s);
        return def_global(name, val_obj((Obj *)nat));
    }

    /* =========================================================
    ** Module registry
    ** ========================================================= */

    void VM::register_lib(const NativeLib *lib)
    {
        if (num_libs_ < MAX_LIBS)
            libs_[num_libs_++] = lib;
    }

    const NativeLib *VM::find_lib(const char *name) const
    {
        for (int i = 0; i < num_libs_; i++)
            if (strcmp(libs_[i]->name, name) == 0)
                return libs_[i];
        return nullptr;
    }

    int VM::open_lib_globals(const NativeLib *lib)
    {
        int base = num_globals_;
        for (int i = 0; i < lib->num_functions; i++)
            def_native(lib->functions[i].name, lib->functions[i].fn, lib->functions[i].arity);
        for (int i = 0; i < lib->num_constants; i++)
            def_global(lib->constants[i].name, lib->constants[i].value);
        return base;
    }

    void VM::open_lib(const NativeLib *lib)
    {
        register_lib(lib);
        open_lib_globals(lib);
    }

    /* =========================================================
    ** try_load_plugin — fallback when find_lib fails.
    **
    ** Searches for <name>.so (Linux), <name>.dylib (macOS),
    ** or <name>.dll (Windows) in search paths and CWD.
    **
    ** The shared library must export:
    **   extern "C" const NativeLib* zen_open_<name>(void);
    **
    ** Returns the NativeLib* on success (already registered),
    ** or nullptr on failure (no error raised — caller handles it).
    ** ========================================================= */
    const NativeLib *VM::try_load_plugin(const char *name)
    {
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
        if (num_plugins_ >= MAX_PLUGINS)
            return nullptr;

        /* Determine extension */
#if defined(__APPLE__)
        const char *ext = ".dylib";
#elif defined(_WIN32)
        const char *ext = ".dll";
#else
        const char *ext = ".so";
#endif

        /* Build symbol name: zen_open_<name> */
        char sym_name[80];
        snprintf(sym_name, sizeof(sym_name), "zen_open_%s", name);

        /* Try paths: CWD first, then search_paths_ */
        char path[512];
        void *handle = nullptr;

        /* Try: ./<name>.so */
        snprintf(path, sizeof(path), "./%s%s", name, ext);
#if defined(_WIN32)
        handle = (void *)LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif

        /* Try search paths */
        if (!handle)
        {
            for (int i = 0; i < num_search_paths_ && !handle; i++)
            {
                int dlen = (int)strlen(search_paths_[i]);
                if (dlen > 0 && search_paths_[i][dlen - 1] == '/')
                    snprintf(path, sizeof(path), "%s%s%s", search_paths_[i], name, ext);
                else
                    snprintf(path, sizeof(path), "%s/%s%s", search_paths_[i], name, ext);
#if defined(_WIN32)
                handle = (void *)LoadLibraryA(path);
#else
                handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
            }
        }

        if (!handle)
            return nullptr;

        /* Find the opener symbol */
        typedef const NativeLib *(*OpenFn)(void);
        OpenFn opener;
#if defined(_WIN32)
        opener = (OpenFn)GetProcAddress((HMODULE)handle, sym_name);
#else
        opener = (OpenFn)dlsym(handle, sym_name);
#endif

        if (!opener)
        {
            /* Symbol not found — close handle */
#if defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        /* Call opener to get NativeLib */
        const NativeLib *lib = opener();
        if (!lib)
        {
#if defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        /* Register plugin handle and library */
        plugin_handles_[num_plugins_++] = handle;
        register_lib(lib);
        return lib;

#else
        /* WASM/other — no dynamic loading */
        (void)name;
        return nullptr;
#endif
    }

    /* =========================================================
    ** Strings
    ** ========================================================= */

    ObjString *VM::make_string(const char *str, int length)
    {
        if (length < 0)
            length = (int)strlen(str);
        return new_string(&gc_, str, length);
    }

    /* =========================================================
    ** GC
    ** ========================================================= */

    void VM::collect()
    {
        gc_collect(this);
    }

    void VM::gc_mark_roots()
    {
        GC *gc = &gc_;

        /* 1. Mark globals (values and names) */
        for (int i = 0; i < num_globals_; i++)
        {
            gc_mark_value(gc, globals_[i]);
            if (global_names_[i])
                gc_mark_obj(gc, (Obj *)global_names_[i]);
        }

        /* 2. Mark all fibers (main + current + any others reachable from them) */
        if (main_fiber_)
            gc_mark_obj(gc, (Obj *)main_fiber_);
        if (current_fiber_ && current_fiber_ != main_fiber_)
            gc_mark_obj(gc, (Obj *)current_fiber_);

        /* 3. Mark process fiber contents directly.
        ** Process fibers are malloc'd (not in GC list) and stay BLACK,
        ** so gc_mark_obj would skip them. We traverse their contents manually. */
        for (int i = 0; i < num_alive_; i++)
        {
            ObjFiber *fiber = pool_[i].fiber;
            if (!fiber) continue;

            /* Mark stack values */
            for (Value *v = fiber->stack; v < fiber->stack_top; v++)
                gc_mark_value(gc, *v);

            /* Mark call frame closures */
            for (int f = 0; f < fiber->frame_count; f++)
            {
                if (fiber->frames[f].closure)
                    gc_mark_obj(gc, (Obj *)fiber->frames[f].closure);
            }

            /* Mark open upvalues */
            ObjUpvalue *uv = fiber->open_upvalues;
            while (uv)
            {
                gc_mark_obj(gc, (Obj *)uv);
                uv = uv->next;
            }
        }
    }

    /* =========================================================
    ** Upvalues
    ** ========================================================= */

    ObjUpvalue *VM::capture_upvalue(ObjFiber *fiber, Value *local)
    {
        /* Procura upvalue aberto que já aponta para este slot */
        ObjUpvalue *prev = nullptr;
        ObjUpvalue *upval = fiber->open_upvalues;

        while (upval && upval->location > local)
        {
            prev = upval;
            upval = upval->next;
        }

        if (upval && upval->location == local)
        {
            return upval; /* Já existe — reutiliza */
        }

        /* Cria novo upvalue */
        ObjUpvalue *created = (ObjUpvalue *)zen_alloc(&gc_, sizeof(ObjUpvalue));
        created->obj.type = OBJ_UPVALUE;
        created->obj.color = GC_BLACK;
        created->obj.hash = 0;
        created->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)created;
        created->location = local;
        created->closed = val_nil();
        created->next = upval;

        /* Insere na lista ordenada por location (desc) */
        if (prev)
        {
            prev->next = created;
        }
        else
        {
            fiber->open_upvalues = created;
        }

        return created;
    }

    void VM::close_upvalues(ObjFiber *fiber, Value *last)
    {
        while (fiber->open_upvalues && fiber->open_upvalues->location >= last)
        {
            ObjUpvalue *upval = fiber->open_upvalues;
            upval->closed = *upval->location;
            upval->location = &upval->closed;
            fiber->open_upvalues = upval->next;
        }
    }

    /* =========================================================
    ** Call helpers
    ** ========================================================= */

    bool VM::call_closure(ObjFiber *fiber, ObjClosure *closure, int nargs, int nresults)
    {
        if (fiber->frame_count >= fiber->frame_capacity)
        {
            runtime_error("stack overflow (too many frames)");
            return false;
        }

        ObjFunc *func = closure->func;
        /* Verificar arity */
        if (func->arity >= 0 && nargs != func->arity)
        {
            runtime_error("expected %d args but got %d", func->arity, nargs);
            return false;
        }

        CallFrame *frame = &fiber->frames[fiber->frame_count++];
        frame->closure = closure;
        frame->func = func;
        frame->ip = func->code;
        /* Args já estão no stack — base aponta para o início */
        frame->base = fiber->stack_top - nargs;
        frame->ret_reg = (int)(frame->base - fiber->frames[fiber->frame_count - 2].base);
        frame->ret_count = nresults;

        /* Expandir stack_top para cobrir registos da nova func */
        fiber->stack_top = frame->base + func->num_regs;

        return true;
    }

    bool VM::call_value(ObjFiber *fiber, Value callee, int nargs, int nresults)
    {
        if (is_obj(callee))
        {
            switch (callee.as.obj->type)
            {
            case OBJ_CLOSURE:
                return call_closure(fiber, as_closure(callee), nargs, nresults);
            case OBJ_NATIVE:
            {
                ObjNative *nat = as_native(callee);
                Value *args = fiber->stack_top - nargs;
                int nret = nat->fn(this, args, nargs);
                /* Coloca resultado onde estava o callable */
                fiber->stack_top -= nargs;
                *(fiber->stack_top - 1) = (nret > 0) ? args[0] : val_nil();
                return true;
            }
            default:
                break;
            }
        }
        runtime_error("value is not callable");
        return false;
    }

    /* =========================================================
    ** Fiber resume/yield
    ** ========================================================= */

    Value VM::resume_fiber(ObjFiber *fiber, Value val)
    {
        if (fiber->state == FIBER_DONE || fiber->state == FIBER_ERROR)
        {
            runtime_error("cannot resume finished fiber");
            return val_nil();
        }

        fiber->caller = current_fiber_;
        fiber->transfer_value = val;
        fiber->state = FIBER_RUNNING;
        current_fiber_->state = FIBER_SUSPENDED;
        current_fiber_ = fiber;

        execute(fiber);

        return fiber->transfer_value;
    }

    /* =========================================================
    ** Error
    ** ========================================================= */

    void VM::runtime_error(const char *fmt, ...)
    {
        had_error_ = true;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "[zen runtime error] ");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);

        /* Stack trace */
        if (current_fiber_)
        {
            for (int i = current_fiber_->frame_count - 1; i >= 0; i--)
            {
                CallFrame *frame = &current_fiber_->frames[i];
                ObjFunc *func = frame->func;
                int offset = (int)(frame->ip - func->code - 1);
                int line = (offset >= 0 && offset < func->code_count) ? func->lines[offset] : 0;
                fprintf(stderr, "  at %s() [%s:%d]\n",
                        func->name ? func->name->chars : "<script>",
                        func->source ? func->source->chars : "?",
                        line);
            }
        }

        if (current_fiber_ && current_fiber_ != main_fiber_)
        {
            current_fiber_->state = FIBER_ERROR;
        }
    }

    /* =========================================================
    ** Builders (stub — implementar quando tiver classes a funcionar)
    ** ========================================================= */

    VM::ClassBuilder VM::def_class(const char *name)
    {
        return ClassBuilder(this, name);
    }

    VM::ClassBuilder::ClassBuilder(VM *vm, const char *name) : vm_(vm)
    {
        ObjString *s = intern_string(&vm->gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        klass_ = new_class(&vm->gc_, s, nullptr);
    }

    VM::ClassBuilder &VM::ClassBuilder::parent(const char *parent_name)
    {
        int idx = vm_->find_global(parent_name);
        if (idx >= 0 && is_class(vm_->globals_[idx]))
        {
            klass_->parent = as_class(vm_->globals_[idx]);
        }
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::field(const char *name)
    {
        int idx = klass_->num_fields++;
        /* Grow field_names array */
        klass_->field_names = (ObjString **)zen_realloc(
            &vm_->gc_, klass_->field_names,
            sizeof(ObjString *) * idx,
            sizeof(ObjString *) * (idx + 1));
        klass_->field_names[idx] = intern_string(&vm_->gc_, name, (int)strlen(name),
                                                 hash_string(name, (int)strlen(name)));
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::method(const char *name, NativeFn fn, int arity)
    {
        ObjString *s = intern_string(&vm_->gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        ObjNative *nat = new_native(&vm_->gc_, fn, arity, s);
        map_set(&vm_->gc_, klass_->methods, val_obj((Obj *)s), val_obj((Obj *)nat));

        /* Also register in vtable */
        int slot = vm_->intern_selector(name, (int)strlen(name));
        if (slot >= klass_->vtable_size)
        {
            int new_size = slot + 1;
            Value *new_vt = (Value *)zen_alloc(&vm_->gc_, sizeof(Value) * new_size);
            for (int i = 0; i < klass_->vtable_size; i++)
                new_vt[i] = klass_->vtable[i];
            for (int i = klass_->vtable_size; i < new_size; i++)
                new_vt[i] = val_nil();
            if (klass_->vtable)
                zen_free(&vm_->gc_, klass_->vtable, sizeof(Value) * klass_->vtable_size);
            klass_->vtable = new_vt;
            klass_->vtable_size = new_size;
        }
        klass_->vtable[slot] = val_obj((Obj *)nat);
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::ctor(NativeClassCtor fn)
    {
        klass_->native_ctor = fn;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::dtor(NativeClassDtor fn)
    {
        klass_->native_dtor = fn;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::persistent(bool p)
    {
        klass_->persistent = p;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::constructable(bool c)
    {
        klass_->constructable = c;
        return *this;
    }

    ObjClass *VM::ClassBuilder::end()
    {
        /* Flatten parent vtable if parent exists */
        if (klass_->parent && klass_->parent->vtable_size > 0)
        {
            ObjClass *p = klass_->parent;
            if (klass_->vtable_size < p->vtable_size)
            {
                int new_size = p->vtable_size;
                Value *new_vt = (Value *)zen_alloc(&vm_->gc_, sizeof(Value) * new_size);
                for (int i = 0; i < klass_->vtable_size; i++)
                    new_vt[i] = klass_->vtable[i];
                for (int i = klass_->vtable_size; i < new_size; i++)
                    new_vt[i] = val_nil();
                if (klass_->vtable)
                    zen_free(&vm_->gc_, klass_->vtable, sizeof(Value) * klass_->vtable_size);
                klass_->vtable = new_vt;
                klass_->vtable_size = new_size;
            }
            for (int i = 0; i < p->vtable_size; i++)
            {
                if (is_nil(klass_->vtable[i]))
                    klass_->vtable[i] = p->vtable[i];
            }
        }
        vm_->def_global(klass_->name->chars, val_obj((Obj *)klass_));
        return klass_;
    }

    /* --- Instance API (C++ embedding) --- */

    Value VM::make_instance(ObjClass *klass)
    {
        ObjInstance *inst = new_instance(&gc_, klass);
        /* Call native constructor — walk parent chain */
        ObjClass *ctor_src = klass;
        while (ctor_src && !ctor_src->native_ctor)
            ctor_src = ctor_src->parent;
        if (ctor_src && ctor_src->native_ctor)
        {
            inst->native_data = ctor_src->native_ctor(this, 0, nullptr);
        }
        return val_obj((Obj *)inst);
    }

    Value VM::make_instance(ObjClass *klass, Value *args, int nargs)
    {
        ObjInstance *inst = new_instance(&gc_, klass);
        Value self = val_obj((Obj *)inst);

        /* Call native constructor — walk parent chain */
        ObjClass *ctor_src = klass;
        while (ctor_src && !ctor_src->native_ctor)
            ctor_src = ctor_src->parent;
        if (ctor_src && ctor_src->native_ctor)
        {
            inst->native_data = ctor_src->native_ctor(this, nargs, args);
        }

        /* Call init if exists */
        int init_slot = find_selector("init", 4);
        if (init_slot >= 0 && init_slot < klass->vtable_size && !is_nil(klass->vtable[init_slot]))
        {
            /* Set up args: [self, arg0, arg1, ...] on stack */
            Value call_args[17]; /* self + max 16 args */
            call_args[0] = self;
            for (int i = 0; i < nargs && i < 16; i++)
                call_args[i + 1] = args[i];

            Value method = klass->vtable[init_slot];
            if (is_native(method))
            {
                ObjNative *nat = as_native(method);
                nat->fn(this, call_args, nargs + 1);
            }
            else if (is_closure(method))
            {
                /* Push args to fiber stack and call */
                ObjFiber *fiber = current_fiber_;
                Value *base = fiber->stack_top;
                for (int i = 0; i <= nargs; i++)
                    base[i] = call_args[i];
                fiber->stack_top = base + as_closure(method)->func->num_regs;

                CallFrame *frame = &fiber->frames[fiber->frame_count++];
                ObjClosure *cl = as_closure(method);
                frame->closure = cl;
                frame->func = cl->func;
                frame->ip = cl->func->code;
                frame->base = base;
                frame->ret_reg = 0;
                frame->ret_count = 0;
                execute(fiber);
                fiber->stack_top = base; /* restore stack */
                fiber->state = FIBER_RUNNING; /* reset after FIBER_DONE */
            }
        }
        return self;
    }

    void VM::destroy_instance(Value instance)
    {
        if (!is_instance(instance)) return;
        ObjInstance *inst = as_instance(instance);
        zen::destroy_instance(&gc_, inst);
    }

    Value VM::invoke(Value instance, const char *method_name, Value *args, int nargs)
    {
        int slot = find_selector(method_name, (int)strlen(method_name));
        if (slot < 0)
        {
            runtime_error("method '%s' not found", method_name);
            return val_nil();
        }
        return invoke(instance, slot, args, nargs);
    }

    Value VM::invoke(Value instance, int slot, Value *args, int nargs)
    {
        ObjInstance *inst = as_instance(instance);
        ObjClass *klass = inst->klass;

        if (slot >= klass->vtable_size || is_nil(klass->vtable[slot]))
        {
            runtime_error("vtable slot %d is nil", slot);
            return val_nil();
        }

        Value method = klass->vtable[slot];

        /* Set up call args: [self, arg0, arg1, ...] */
        if (is_native(method))
        {
            Value call_args[17];
            call_args[0] = instance;
            for (int i = 0; i < nargs && i < 16; i++)
                call_args[i + 1] = args[i];
            ObjNative *nat = as_native(method);
            int nret = nat->fn(this, call_args, nargs + 1);
            return nret > 0 ? call_args[0] : val_nil();
        }
        else if (is_closure(method))
        {
            ObjFiber *fiber = current_fiber_;
            Value *base = fiber->stack_top;
            base[0] = instance; /* self */
            for (int i = 0; i < nargs; i++)
                base[i + 1] = args[i];

            ObjClosure *cl = as_closure(method);
            fiber->stack_top = base + cl->func->num_regs;

            CallFrame *frame = &fiber->frames[fiber->frame_count++];
            frame->closure = cl;
            frame->func = cl->func;
            frame->ip = cl->func->code;
            frame->base = base;
            frame->ret_reg = 0;
            frame->ret_count = 1;
            execute(fiber);

            Value result = base[0];
            fiber->stack_top = base; /* restore stack */
            fiber->state = FIBER_RUNNING; /* reset after FIBER_DONE */
            return result;
        }
        return val_nil();
    }

    VM::StructBuilder VM::def_struct(const char *name)
    {
        return StructBuilder(this, name);
    }

    VM::StructBuilder::StructBuilder(VM *vm, const char *name) : vm_(vm)
    {
        def_ = (ObjStructDef *)zen_alloc(&vm->gc_, sizeof(ObjStructDef));
        def_->obj.type = OBJ_STRUCT_DEF;
        def_->obj.color = GC_BLACK;
        def_->obj.hash = 0;
        def_->obj.gc_next = vm->gc_.objects;
        vm->gc_.objects = (Obj *)def_;
        def_->name = intern_string(&vm->gc_, name, (int)strlen(name),
                                   hash_string(name, (int)strlen(name)));
        def_->num_fields = 0;
        def_->field_names = nullptr;
    }

    VM::StructBuilder &VM::StructBuilder::field(const char *name)
    {
        int idx = def_->num_fields++;
        /* Grow field_names array */
        def_->field_names = (ObjString **)zen_realloc(
            &vm_->gc_, def_->field_names,
            sizeof(ObjString *) * (idx),
            sizeof(ObjString *) * (idx + 1));
        def_->field_names[idx] = intern_string(&vm_->gc_, name, (int)strlen(name),
                                               hash_string(name, (int)strlen(name)));
        return *this;
    }

    ObjStructDef *VM::StructBuilder::end()
    {
        vm_->def_global(def_->name->chars, val_obj((Obj *)def_));
        return def_;
    }

    /* =========================================================
    ** NativeStructBuilder — zero-copy C++ struct binding
    ** ========================================================= */

    VM::NativeStructBuilder VM::register_native_struct(const char *name, uint16_t size,
                                                       NativeStructCtor ctor, NativeStructDtor dtor)
    {
        return NativeStructBuilder(this, name, size, ctor, dtor);
    }

    VM::NativeStructBuilder::NativeStructBuilder(VM *vm, const char *name, uint16_t size,
                                                  NativeStructCtor ctor, NativeStructDtor dtor)
        : vm_(vm)
    {
        def_ = (NativeStructDef *)zen_alloc(&vm->gc_, sizeof(NativeStructDef));
        def_->obj.type = OBJ_NATIVE_STRUCT_DEF;
        def_->obj.color = GC_BLACK;
        def_->obj.hash = 0;
        def_->obj.gc_next = vm->gc_.objects;
        vm->gc_.objects = (Obj *)def_;
        def_->name = intern_string(&vm->gc_, name, (int)strlen(name),
                                   hash_string(name, (int)strlen(name)));
        def_->struct_size = size;
        def_->num_fields = 0;
        def_->fields = nullptr;
        def_->ctor = ctor;
        def_->dtor = dtor;
    }

    VM::NativeStructBuilder &VM::NativeStructBuilder::field(const char *name, uint16_t offset,
                                                             NativeFieldType type, bool read_only)
    {
        int idx = def_->num_fields++;
        def_->fields = (NativeFieldDef *)zen_realloc(
            &vm_->gc_, def_->fields,
            sizeof(NativeFieldDef) * idx,
            sizeof(NativeFieldDef) * (idx + 1));
        def_->fields[idx].name = intern_string(&vm_->gc_, name, (int)strlen(name),
                                               hash_string(name, (int)strlen(name)));
        def_->fields[idx].offset = offset;
        def_->fields[idx].type = type;
        def_->fields[idx].read_only = read_only;
        return *this;
    }

    NativeStructDef *VM::NativeStructBuilder::end()
    {
        vm_->def_global(def_->name->chars, val_obj((Obj *)def_));
        return def_;
    }

    /* =========================================================
    ** File I/O — used by compiler for include/import
    ** ========================================================= */

    void VM::add_search_path(const char *dir)
    {
        if (num_search_paths_ >= MAX_SEARCH_PATHS)
            return;
        int len = 0;
        while (dir[len])
            len++;
        /* Strip trailing slash */
        while (len > 1 && dir[len - 1] == '/')
            len--;
        char *copy = (char *)malloc((size_t)len + 1);
        memcpy(copy, dir, (size_t)len);
        copy[len] = '\0';
        search_paths_[num_search_paths_++] = copy;
    }

    /* Try to open and read a file. Returns malloc'd buffer or nullptr. */
    static char *try_read(const char *path, long *out_size)
    {
        FILE *f = fopen(path, "rb");
        if (!f)
            return nullptr;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char *)malloc((size_t)size + 1);
        if (!buf)
        {
            fclose(f);
            return nullptr;
        }
        fread(buf, 1, (size_t)size, f);
        buf[size] = '\0';
        fclose(f);
        if (out_size)
            *out_size = size;
        return buf;
    }

    char *VM::read_file(const char *path, const char *relative_to, long *out_size,
                        char *resolved_path, int resolved_max)
    {
        char fullpath[1024];

        /* Helper: copy resolved path if requested */
        auto store_resolved = [&](const char *p)
        {
            if (resolved_path && resolved_max > 0)
            {
                int len = 0;
                while (p[len])
                    len++;
                if (len >= resolved_max)
                    len = resolved_max - 1;
                memcpy(resolved_path, p, (size_t)len);
                resolved_path[len] = '\0';
            }
        };

        /* 1. If absolute path, try directly */
        if (path[0] == '/')
        {
            char *result = try_read(path, out_size);
            if (result)
                store_resolved(path);
            return result;
        }

        /* 2. Try relative to the requesting file's directory */
        if (relative_to)
        {
            int dir_len = 0;
            for (int i = 0; relative_to[i]; i++)
                if (relative_to[i] == '/')
                    dir_len = i + 1;
            int path_len = 0;
            while (path[path_len])
                path_len++;
            if (dir_len + path_len < (int)sizeof(fullpath))
            {
                memcpy(fullpath, relative_to, (size_t)dir_len);
                memcpy(fullpath + dir_len, path, (size_t)path_len);
                fullpath[dir_len + path_len] = '\0';
                char *result = try_read(fullpath, out_size);
                if (result)
                {
                    store_resolved(fullpath);
                    return result;
                }
            }
        }

        /* 3. Try each search path */
        for (int i = 0; i < num_search_paths_; i++)
        {
            int dlen = 0;
            while (search_paths_[i][dlen])
                dlen++;
            int plen = 0;
            while (path[plen])
                plen++;
            if (dlen + 1 + plen < (int)sizeof(fullpath))
            {
                memcpy(fullpath, search_paths_[i], (size_t)dlen);
                fullpath[dlen] = '/';
                memcpy(fullpath + dlen + 1, path, (size_t)plen);
                fullpath[dlen + 1 + plen] = '\0';
                char *result = try_read(fullpath, out_size);
                if (result)
                {
                    store_resolved(fullpath);
                    return result;
                }
            }
        }

        return nullptr;
    }

    /* =========================================================
    ** Process system — DIV-style cooperative multitasking
    **
    ** Processes are ObjFiber instances in a dynamic compact pool.
    ** They yield via OP_FRAME (sets state=SUSPENDED).
    ** tick_processes() resumes all SUSPENDED fibers once per game frame.
    ** Removal: swap-with-last O(1).
    ** Pool grows dynamically (no fixed limit).
    ** ========================================================= */

    static void pool_grow(VM::ProcessSlot *&pool, int &capacity)
    {
        int new_cap = capacity < 64 ? 64 : capacity * 2;
        pool = (VM::ProcessSlot *)realloc(pool, new_cap * sizeof(VM::ProcessSlot));
        capacity = new_cap;
    }

    int VM::spawn_process(ObjClosure *closure, Value *args, int nargs)
    {
        /* Grow pool if needed */
        if (num_alive_ >= pool_capacity_)
            pool_grow(pool_, pool_capacity_);

        /* Create process fiber — NOT managed by GC.
        ** Allocated with malloc, freed manually when process dies.
        ** GC marks contents via gc_mark_roots → pool_ traversal. */
        int stack_size = closure->func->num_regs * 2;
        if (stack_size < 128) stack_size = 128;

        ObjFiber *fiber = (ObjFiber *)malloc(sizeof(ObjFiber));
        fiber->obj.type = OBJ_FIBER;
        fiber->obj.color = GC_BLACK;
        fiber->obj.hash = 0;
        fiber->obj.gc_next = nullptr; /* NOT in GC list */

        fiber->state = FIBER_READY;
        fiber->stack_capacity = stack_size;
        fiber->stack = (Value *)malloc(stack_size * sizeof(Value));
        memset(fiber->stack, 0, stack_size * sizeof(Value));
        fiber->stack_top = fiber->stack;

        fiber->frame_capacity = 4;
        fiber->frames = (CallFrame *)malloc(4 * sizeof(CallFrame));
        fiber->frame_count = 0;

        fiber->open_upvalues = nullptr;
        fiber->caller = nullptr;
        fiber->transfer_value = val_nil();
        fiber->yield_dest = -1;
        fiber->frame_speed = 100;
        fiber->frame_accumulator = 0;
        fiber->process_id = -1;
        fiber->error = nullptr;

        /* Set up first frame */
        fiber->frame_count = 1;
        CallFrame *frame = &fiber->frames[0];
        frame->closure = closure;
        frame->func = closure->func;
        frame->ip = closure->func->code;
        frame->base = fiber->stack;
        frame->ret_reg = 0;
        frame->ret_count = 0;
        fiber->stack_top = fiber->stack + closure->func->num_regs;

        /* Place args in fiber registers */
        if (args && nargs > 0)
        {
            int max_args = nargs < closure->func->arity ? nargs : closure->func->arity;
            for (int i = 0; i < max_args; i++)
                fiber->stack[i] = args[i];
        }

        int id = next_process_id_++;
        fiber->process_id = id;
        int parent = current_process_id_; /* who is spawning me */
        pool_[num_alive_] = { fiber, id, 0, parent, -1 };
        num_alive_++;

        /* Update parent's last_child_id */
        if (parent >= 0)
        {
            for (int i = 0; i < num_alive_ - 1; i++)
            {
                if (pool_[i].id == parent)
                {
                    pool_[i].last_child_id = id;
                    break;
                }
            }
        }

        return id;
    }

    /* Free a process fiber (not GC-managed) */
    static void free_process_fiber(ObjFiber *fiber)
    {
        if (fiber->stack) free(fiber->stack);
        if (fiber->frames) free(fiber->frames);
        free(fiber);
    }

    void VM::kill_process(int id)
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].id == id)
            {
                pool_[i].fiber->state = FIBER_DONE;
                break;
            }
        }
    }

    void VM::kill_all_processes()
    {
        for (int i = 0; i < num_alive_; i++)
            free_process_fiber(pool_[i].fiber);
        num_alive_ = 0;
    }

    ObjFiber *VM::get_process(int id) const
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].id == id)
                return pool_[i].fiber;
        }
        return nullptr;
    }

    VM::ProcessSlot *VM::find_slot(int id)
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].id == id)
                return &pool_[i];
        }
        return nullptr;
    }

    void VM::sort_processes()
    {
        /* qsort by z (lower z = first in array = drawn first) */
        qsort(pool_, num_alive_, sizeof(ProcessSlot),
              [](const void *a, const void *b) -> int {
                  int za = ((const ProcessSlot *)a)->z;
                  int zb = ((const ProcessSlot *)b)->z;
                  return za - zb;
              });
    }

    static void apply_signal(ObjFiber *fiber, int sig)
    {
        switch (sig)
        {
        case VM::SIG_KILL:   fiber->state = FIBER_DONE; break;
        case VM::SIG_FREEZE: fiber->state = FIBER_FROZEN; break;
        case VM::SIG_SLEEP:  fiber->state = FIBER_SLEEPING; break;
        case VM::SIG_WAKEUP: fiber->state = FIBER_SUSPENDED; break;
        }
    }

    void VM::signal_process(int id, int sig)
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].id == id)
            {
                apply_signal(pool_[i].fiber, sig);
                break;
            }
        }
    }

    void VM::signal_type(ObjFunc *type, int sig)
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].fiber->frame_count > 0 &&
                pool_[i].fiber->frames[0].func == type)
            {
                apply_signal(pool_[i].fiber, sig);
            }
        }
    }

    void VM::let_me_alone()
    {
        int my_id = current_process_id_;
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].id != my_id)
                pool_[i].fiber->state = FIBER_DONE;
        }
    }

    int VM::get_id_by_type(ObjFunc *type) const
    {
        for (int i = 0; i < num_alive_; i++)
        {
            if (pool_[i].fiber->frame_count > 0 &&
                pool_[i].fiber->frames[0].func == type)
            {
                return pool_[i].id;
            }
        }
        return 0; /* 0 = not found (IDs start at 1) */
    }

    int VM::tick_processes(float dt)
    {
        /* If VM had error, kill all processes and bail */
        if (had_error_)
        {
            kill_all_processes();
            return 0;
        }

        int i = 0;
        while (i < num_alive_)
        {
            ProcessSlot &slot = pool_[i];
            ObjFiber *proc = slot.fiber;

            /* Dead/done? Remove from pool (swap with last) */
            if (proc->state == FIBER_DONE || proc->state == FIBER_ERROR)
            {
                if (on_process_end)
                    on_process_end(this, proc);
                free_process_fiber(proc);
                pool_[i] = pool_[--num_alive_];
                continue;
            }

            /* Frozen/Sleeping? Skip execution */
            if (proc->state == FIBER_FROZEN || proc->state == FIBER_SLEEPING)
            {
                i++;
                continue;
            }

            /* frame_speed gating: accumulate dt, skip if not enough */
            if (proc->state == FIBER_SUSPENDED && proc->frame_speed < 100)
            {
                proc->frame_accumulator += (int)(dt * 1000.0f);
                int interval = (100 - proc->frame_speed) * 2; /* higher speed = shorter interval */
                if (interval <= 0) interval = 1;
                if (proc->frame_accumulator < interval)
                {
                    i++;
                    continue;
                }
                proc->frame_accumulator = 0;
            }

            /* Ready = first run */
            if (proc->state == FIBER_READY)
            {
                proc->state = FIBER_RUNNING;
                current_fiber_ = proc;
                current_process_id_ = slot.id;
                execute(proc);
                current_process_id_ = -1;
                current_fiber_ = main_fiber_;

                /* If process caused error, kill all and stop */
                if (had_error_)
                {
                    kill_all_processes();
                    return 0;
                }

                if (on_process_start && proc->state == FIBER_SUSPENDED)
                    on_process_start(this, proc);
                if (on_process_update && proc->state == FIBER_SUSPENDED)
                    on_process_update(this, proc);
            }
            /* Suspended = resume */
            else if (proc->state == FIBER_SUSPENDED)
            {
                proc->state = FIBER_RUNNING;
                current_fiber_ = proc;
                current_process_id_ = slot.id;
                execute(proc);
                current_process_id_ = -1;
                current_fiber_ = main_fiber_;

                /* If process caused error, kill all and stop */
                if (had_error_)
                {
                    kill_all_processes();
                    return 0;
                }

                if (on_process_update && proc->state == FIBER_SUSPENDED)
                    on_process_update(this, proc);
            }

            /* Check if died during execution */
            if (proc->state == FIBER_DONE || proc->state == FIBER_ERROR)
            {
                if (on_process_end)
                    on_process_end(this, proc);
                free_process_fiber(proc);
                pool_[i] = pool_[--num_alive_];
                continue;
            }

            i++;
        }

        return num_alive_;
    }

    void VM::for_each_process(ProcessIterFn fn, void *userdata)
    {
        for (int i = 0; i < num_alive_; i++)
            fn(this, pool_[i].fiber, pool_[i].id, userdata);
    }

} /* namespace zen */
