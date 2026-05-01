#include "vm.h"
#include "debug.h"
#include <cstdarg>

namespace zen {

/* =========================================================
** Construtor / Destrutor
** ========================================================= */

VM::VM() : num_globals_(0), main_fiber_(nullptr), current_fiber_(nullptr), fiber_depth_(0), had_error_(false) {
    gc_init(&gc_);
    memset(globals_, 0, sizeof(globals_));
    memset(global_names_, 0, sizeof(global_names_));

    /* Criar main fiber */
    main_fiber_ = new_fiber(nullptr, kMaxRegs * 4);
    current_fiber_ = main_fiber_;
}

VM::~VM() {
    /* Free all objects via GC sweep (marca nenhum → tudo WHITE → free) */
    Obj* obj = gc_.objects;
    while (obj) {
        Obj* next = obj->gc_next;
        /* free_obj(&gc_, obj); — implementar em memory.cpp */
        obj = next;
    }
    /* Free gray list */
    if (gc_.gray_list) free(gc_.gray_list);
    /* Free intern table */
    if (gc_.strings) zen_free(&gc_, gc_.strings, gc_.string_capacity * sizeof(ObjString*));
}

/* =========================================================
** Fiber management
** ========================================================= */

ObjFiber* VM::new_fiber(ObjClosure* closure, int stack_size) {
    ObjFiber* fiber = (ObjFiber*)zen_alloc(&gc_, sizeof(ObjFiber));
    fiber->obj.type = OBJ_FIBER;
    fiber->obj.color = GC_WHITE;
    fiber->obj.hash = 0;
    fiber->obj.gc_next = gc_.objects;
    gc_.objects = (Obj*)fiber;

    fiber->state = FIBER_READY;
    fiber->stack_capacity = stack_size;
    fiber->stack = (Value*)zen_alloc(&gc_, stack_size * sizeof(Value));
    fiber->stack_top = fiber->stack;

    fiber->frame_capacity = kMaxFrames;
    fiber->frames = (CallFrame*)zen_alloc(&gc_, kMaxFrames * sizeof(CallFrame));
    fiber->frame_count = 0;

    fiber->open_upvalues = nullptr;
    fiber->caller = nullptr;
    fiber->transfer_value = val_nil();
    fiber->yield_dest = -1;
    fiber->frame_speed = 100;
    fiber->frame_accumulator = 0;
    fiber->error = nullptr;

    /* Se tiver closure, prepara o primeiro frame */
    if (closure) {
        fiber->frame_count = 1;
        CallFrame* frame = &fiber->frames[0];
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

void VM::run(ObjFunc* func) {
    /* Wrap func num closure trivial (0 upvalues) */
    ObjClosure* cl = (ObjClosure*)zen_alloc(&gc_, sizeof(ObjClosure));
    cl->obj.type = OBJ_CLOSURE;
    cl->obj.color = GC_WHITE;
    cl->obj.hash = 0;
    cl->obj.gc_next = gc_.objects;
    gc_.objects = (Obj*)cl;
    cl->func = func;
    cl->upvalues = nullptr;
    cl->upvalue_count = 0;

    run(cl);
}

void VM::run(ObjClosure* closure) {
    /* Setup main fiber com o closure */
    main_fiber_->frame_count = 1;
    CallFrame* frame = &main_fiber_->frames[0];
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
}

Value VM::call_global(int idx, Value* args, int nargs) {
    Value callee = globals_[idx];
    if (is_native(callee)) {
        ObjNative* nat = as_native(callee);
        int nret = nat->fn(this, args, nargs);
        return (nret > 0) ? args[0] : val_nil();
    }
    if (is_closure(callee)) {
        /* Place callee + args on main fiber stack, call, return result */
        ObjFiber* fiber = main_fiber_;
        Value* base = fiber->stack;
        for (int i = 0; i < nargs; i++) base[i] = args[i];

        ObjClosure* cl = as_closure(callee);
        fiber->frame_count = 1;
        CallFrame* frame = &fiber->frames[0];
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

Value VM::call_global(const char* name, Value* args, int nargs) {
    int idx = find_global(name);
    if (idx < 0) {
        runtime_error("undefined global '%s'", name);
        return val_nil();
    }
    return call_global(idx, args, nargs);
}

/* =========================================================
** Globals
** ========================================================= */

int VM::find_global(const char* name) const {
    int len = (int)strlen(name);
    for (int i = 0; i < num_globals_; i++) {
        if (global_names_[i] &&
            global_names_[i]->length == len &&
            memcmp(global_names_[i]->chars, name, len) == 0) {
            return i;
        }
    }
    return -1;
}

int VM::def_global(const char* name, Value val) {
    int existing = find_global(name);
    if (existing >= 0) {
        globals_[existing] = val;
        return existing;
    }
    int idx = num_globals_++;
    global_names_[idx] = intern_string(&gc_, name, (int)strlen(name),
                                       hash_string(name, (int)strlen(name)));
    globals_[idx] = val;
    return idx;
}

Value VM::get_global(const char* name) const {
    int idx = find_global(name);
    return idx >= 0 ? globals_[idx] : val_nil();
}

void VM::set_global(const char* name, Value val) {
    int idx = find_global(name);
    if (idx >= 0) globals_[idx] = val;
}

int VM::def_native(const char* name, NativeFn fn, int arity) {
    ObjString* s = intern_string(&gc_, name, (int)strlen(name),
                                 hash_string(name, (int)strlen(name)));
    ObjNative* nat = new_native(&gc_, fn, arity, s);
    return def_global(name, val_obj((Obj*)nat));
}

/* =========================================================
** Strings
** ========================================================= */

ObjString* VM::make_string(const char* str, int length) {
    if (length < 0) length = (int)strlen(str);
    return new_string(&gc_, str, length);
}

/* =========================================================
** GC
** ========================================================= */

void VM::collect() {
    gc_collect(this);
}

/* =========================================================
** Upvalues
** ========================================================= */

ObjUpvalue* VM::capture_upvalue(ObjFiber* fiber, Value* local) {
    /* Procura upvalue aberto que já aponta para este slot */
    ObjUpvalue* prev = nullptr;
    ObjUpvalue* upval = fiber->open_upvalues;

    while (upval && upval->location > local) {
        prev = upval;
        upval = upval->next;
    }

    if (upval && upval->location == local) {
        return upval;  /* Já existe — reutiliza */
    }

    /* Cria novo upvalue */
    ObjUpvalue* created = (ObjUpvalue*)zen_alloc(&gc_, sizeof(ObjUpvalue));
    created->obj.type = OBJ_UPVALUE;
    created->obj.color = GC_WHITE;
    created->obj.hash = 0;
    created->obj.gc_next = gc_.objects;
    gc_.objects = (Obj*)created;
    created->location = local;
    created->closed = val_nil();
    created->next = upval;

    /* Insere na lista ordenada por location (desc) */
    if (prev) {
        prev->next = created;
    } else {
        fiber->open_upvalues = created;
    }

    return created;
}

void VM::close_upvalues(ObjFiber* fiber, Value* last) {
    while (fiber->open_upvalues && fiber->open_upvalues->location >= last) {
        ObjUpvalue* upval = fiber->open_upvalues;
        upval->closed = *upval->location;
        upval->location = &upval->closed;
        fiber->open_upvalues = upval->next;
    }
}

/* =========================================================
** Call helpers
** ========================================================= */

bool VM::call_closure(ObjFiber* fiber, ObjClosure* closure, int nargs, int nresults) {
    if (fiber->frame_count >= fiber->frame_capacity) {
        runtime_error("stack overflow (too many frames)");
        return false;
    }

    ObjFunc* func = closure->func;
    /* Verificar arity */
    if (func->arity >= 0 && nargs != func->arity) {
        runtime_error("expected %d args but got %d", func->arity, nargs);
        return false;
    }

    CallFrame* frame = &fiber->frames[fiber->frame_count++];
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

bool VM::call_value(ObjFiber* fiber, Value callee, int nargs, int nresults) {
    if (is_obj(callee)) {
        switch (callee.as.obj->type) {
            case OBJ_CLOSURE:
                return call_closure(fiber, as_closure(callee), nargs, nresults);
            case OBJ_NATIVE: {
                ObjNative* nat = as_native(callee);
                Value* args = fiber->stack_top - nargs;
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

Value VM::resume_fiber(ObjFiber* fiber, Value val) {
    if (fiber->state == FIBER_DONE || fiber->state == FIBER_ERROR) {
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

void VM::runtime_error(const char* fmt, ...) {
    had_error_ = true;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[zen runtime error] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    /* Stack trace */
    if (current_fiber_) {
        for (int i = current_fiber_->frame_count - 1; i >= 0; i--) {
            CallFrame* frame = &current_fiber_->frames[i];
            ObjFunc* func = frame->func;
            int offset = (int)(frame->ip - func->code - 1);
            int line = (offset >= 0 && offset < func->code_count) ? func->lines[offset] : 0;
            fprintf(stderr, "  at %s() [%s:%d]\n",
                    func->name ? func->name->chars : "<script>",
                    func->source ? func->source->chars : "?",
                    line);
        }
    }

    if (current_fiber_ && current_fiber_ != main_fiber_) {
        current_fiber_->state = FIBER_ERROR;
    }
}

/* =========================================================
** Builders (stub — implementar quando tiver classes a funcionar)
** ========================================================= */

VM::ClassBuilder VM::def_class(const char* name) {
    return ClassBuilder(this, name);
}

VM::ClassBuilder::ClassBuilder(VM* vm, const char* name) : vm_(vm) {
    ObjString* s = intern_string(&vm->gc_, name, (int)strlen(name),
                                 hash_string(name, (int)strlen(name)));
    klass_ = new_class(&vm->gc_, s, nullptr);
}

VM::ClassBuilder& VM::ClassBuilder::parent(const char* parent_name) {
    int idx = vm_->find_global(parent_name);
    if (idx >= 0 && is_class(vm_->globals_[idx])) {
        klass_->parent = as_class(vm_->globals_[idx]);
    }
    return *this;
}

VM::ClassBuilder& VM::ClassBuilder::field(const char* name) {
    (void)name; /* TODO: adicionar ao field_names array */
    klass_->num_fields++;
    return *this;
}

VM::ClassBuilder& VM::ClassBuilder::method(const char* name, NativeFn fn, int arity) {
    ObjString* s = intern_string(&vm_->gc_, name, (int)strlen(name),
                                 hash_string(name, (int)strlen(name)));
    ObjNative* nat = new_native(&vm_->gc_, fn, arity, s);
    (void)nat; /* TODO: map_set(klass_->methods, s, nat) */
    return *this;
}

ObjClass* VM::ClassBuilder::end() {
    vm_->def_global(klass_->name->chars, val_obj((Obj*)klass_));
    return klass_;
}

VM::StructBuilder VM::def_struct(const char* name) {
    return StructBuilder(this, name);
}

VM::StructBuilder::StructBuilder(VM* vm, const char* name) : vm_(vm) {
    def_ = (ObjStructDef*)zen_alloc(&vm->gc_, sizeof(ObjStructDef));
    def_->obj.type = OBJ_STRUCT;
    def_->obj.color = GC_WHITE;
    def_->obj.hash = 0;
    def_->obj.gc_next = vm->gc_.objects;
    vm->gc_.objects = (Obj*)def_;
    def_->name = intern_string(&vm->gc_, name, (int)strlen(name),
                               hash_string(name, (int)strlen(name)));
    def_->num_fields = 0;
    def_->field_names = nullptr;
}

VM::StructBuilder& VM::StructBuilder::field(const char* name) {
    (void)name; /* TODO: grow field_names array */
    def_->num_fields++;
    return *this;
}

ObjStructDef* VM::StructBuilder::end() {
    vm_->def_global(def_->name->chars, val_obj((Obj*)def_));
    return def_;
}

} /* namespace zen */
