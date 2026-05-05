/* sdl_funcs_window.inl — SDL2 window + init management
** Included by builtin_sdl2.cpp */

static int nat_SDL_Init(VM *vm, Value *args, int nargs)
{
    uint32_t flags = SDL_INIT_VIDEO;
    if (nargs >= 1) {
        if (!is_int(args[0])) { vm->runtime_error("SDL_Init expects (flags?)."); return 0; }
        flags = (uint32_t)args[0].as.integer;
    }
    args[0] = val_int(SDL_Init(flags));
    return 1;
}

static int nat_SDL_Quit(VM *, Value *args, int)
{
    SDL_Quit(); args[0] = val_nil(); return 1;
}

static int nat_SDL_GetError(VM *vm, Value *args, int)
{
    args[0] = val_obj((Obj *)vm->make_string(SDL_GetError())); return 1;
}

static int nat_SDL_ClearError(VM *, Value *args, int)
{
    SDL_ClearError(); args[0] = val_nil(); return 1;
}

static int nat_SDL_GetRevision(VM *vm, Value *args, int)
{
    args[0] = val_obj((Obj *)vm->make_string(SDL_GetRevision())); return 1;
}

static int nat_SDL_WasInit(VM *vm, Value *args, int nargs)
{
    uint32_t flags = 0;
    if (nargs >= 1) {
        if (!is_int(args[0])) { vm->runtime_error("SDL_WasInit expects (flags?)."); return 0; }
        flags = (uint32_t)args[0].as.integer;
    }
    args[0] = val_int((int64_t)SDL_WasInit(flags));
    return 1;
}

/* SDL_CreateWindow(title, x, y, w, h, flags) → window_ptr */
static int nat_SDL_CreateWindow(VM *vm, Value *args, int nargs)
{
    if (nargs < 6 || !is_string(args[0]) || !is_int(args[1]) || !is_int(args[2])
            || !is_int(args[3]) || !is_int(args[4]) || !is_int(args[5]))
    {
        vm->runtime_error("SDL_CreateWindow expects (title, x, y, w, h, flags).");
        return 0;
    }
    SDL_Window *w = SDL_CreateWindow(
        as_cstring(args[0]),
        (int)args[1].as.integer, (int)args[2].as.integer,
        (int)args[3].as.integer, (int)args[4].as.integer,
        (uint32_t)args[5].as.integer);
    args[0] = w ? val_ptr(w) : val_nil();
    return 1;
}

static int nat_SDL_DestroyWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_DestroyWindow", "window")) return 0;
    SDL_DestroyWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_SetWindowTitle(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 2 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetWindowTitle", "window")
            || !is_string(args[1]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetWindowTitle expects (window, title).");
        return 0;
    }
    SDL_SetWindowTitle((SDL_Window *)p, as_cstring(args[1]));
    args[0] = val_nil(); return 1;
}

/* SDL_GetWindowSize(window) → w, h */
static int nat_SDL_GetWindowSize(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GetWindowSize", "window")) return 0;
    int w = 0, h = 0;
    SDL_GetWindowSize((SDL_Window *)p, &w, &h);
    args[0] = val_int(w); args[1] = val_int(h); return 2;
}

static int nat_SDL_SetWindowSize(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 3 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetWindowSize", "window")
            || !is_int(args[1]) || !is_int(args[2]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetWindowSize expects (window, w, h).");
        return 0;
    }
    SDL_SetWindowSize((SDL_Window *)p, (int)args[1].as.integer, (int)args[2].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_GetWindowPosition(window) → x, y */
static int nat_SDL_GetWindowPosition(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GetWindowPosition", "window")) return 0;
    int x = 0, y = 0;
    SDL_GetWindowPosition((SDL_Window *)p, &x, &y);
    args[0] = val_int(x); args[1] = val_int(y); return 2;
}

static int nat_SDL_SetWindowPosition(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 3 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetWindowPosition", "window")
            || !is_int(args[1]) || !is_int(args[2]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetWindowPosition expects (window, x, y).");
        return 0;
    }
    SDL_SetWindowPosition((SDL_Window *)p, (int)args[1].as.integer, (int)args[2].as.integer);
    args[0] = val_nil(); return 1;
}

static int nat_SDL_ShowWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_ShowWindow", "window")) return 0;
    SDL_ShowWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_HideWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_HideWindow", "window")) return 0;
    SDL_HideWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_RaiseWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_RaiseWindow", "window")) return 0;
    SDL_RaiseWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_MaximizeWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_MaximizeWindow", "window")) return 0;
    SDL_MaximizeWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_MinimizeWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_MinimizeWindow", "window")) return 0;
    SDL_MinimizeWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

static int nat_SDL_RestoreWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_RestoreWindow", "window")) return 0;
    SDL_RestoreWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

/* SDL_SetWindowFullscreen(window, flags) → 0 on success */
static int nat_SDL_SetWindowFullscreen(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 2 || !expect_ptr_arg(vm, args[0], &p, "SDL_SetWindowFullscreen", "window")
            || !is_int(args[1]))
    {
        if (!vm->had_error()) vm->runtime_error("SDL_SetWindowFullscreen expects (window, flags).");
        return 0;
    }
    args[0] = val_int(SDL_SetWindowFullscreen((SDL_Window *)p, (uint32_t)args[1].as.integer));
    return 1;
}

static int nat_SDL_GetWindowID(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GetWindowID", "window")) return 0;
    args[0] = val_int((int64_t)SDL_GetWindowID((SDL_Window *)p)); return 1;
}

static int nat_SDL_GetWindowFlags(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GetWindowFlags", "window")) return 0;
    args[0] = val_int((int64_t)SDL_GetWindowFlags((SDL_Window *)p)); return 1;
}
