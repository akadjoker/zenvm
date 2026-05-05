/* sdl_funcs_gl.inl — SDL2 OpenGL context management
** Included by builtin_sdl2.cpp
**
** Typical usage:
**   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
**   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
**   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
**   var ctx = SDL_GL_CreateContext(win)
**   SDL_GL_MakeCurrent(win, ctx)
**   LoadOpenGLExtensions()          ← from import gl
**   ...
**   SDL_GL_SwapWindow(win)
*/

/* SDL_GL_SetAttribute(attr, value) → 0 on success */
static int nat_SDL_GL_SetAttribute(VM *vm, Value *args, int nargs)
{
    if (nargs < 2 || !is_int(args[0]) || !is_int(args[1])) {
        vm->runtime_error("SDL_GL_SetAttribute expects (attr, value)."); return 0;
    }
    args[0] = val_int(SDL_GL_SetAttribute(
        (SDL_GLattr)args[0].as.integer, (int)args[1].as.integer));
    return 1;
}

/* SDL_GL_GetAttribute(attr) → value */
static int nat_SDL_GL_GetAttribute(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_int(args[0])) {
        vm->runtime_error("SDL_GL_GetAttribute expects (attr)."); return 0;
    }
    int v = 0;
    SDL_GL_GetAttribute((SDL_GLattr)args[0].as.integer, &v);
    args[0] = val_int(v); return 1;
}

/* SDL_GL_CreateContext(window) → context_ptr or nil */
static int nat_SDL_GL_CreateContext(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GL_CreateContext", "window")) return 0;
    SDL_GLContext ctx = SDL_GL_CreateContext((SDL_Window *)p);
    args[0] = ctx ? val_ptr(ctx) : val_nil(); return 1;
}

/* SDL_GL_MakeCurrent(window, context) → 0 on success */
static int nat_SDL_GL_MakeCurrent(VM *vm, Value *args, int nargs)
{
    void *win = nullptr, *ctx = nullptr;
    if (nargs < 2
            || !expect_ptr_arg(vm, args[0], &win, "SDL_GL_MakeCurrent", "window")
            || !expect_ptr_arg(vm, args[1], &ctx, "SDL_GL_MakeCurrent", "context"))
        return 0;
    args[0] = val_int(SDL_GL_MakeCurrent((SDL_Window *)win, (SDL_GLContext)ctx));
    return 1;
}

/* SDL_GL_DeleteContext(context) */
static int nat_SDL_GL_DeleteContext(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GL_DeleteContext", "context")) return 0;
    SDL_GL_DeleteContext((SDL_GLContext)p); args[0] = val_nil(); return 1;
}

/* SDL_GL_SwapWindow(window) — present frame */
static int nat_SDL_GL_SwapWindow(VM *vm, Value *args, int nargs)
{
    void *p = nullptr;
    if (nargs < 1 || !expect_ptr_arg(vm, args[0], &p, "SDL_GL_SwapWindow", "window")) return 0;
    SDL_GL_SwapWindow((SDL_Window *)p); args[0] = val_nil(); return 1;
}

/* SDL_GL_SetSwapInterval(interval) → 0 on success, -1 if not supported
**   0 = immediate, 1 = vsync, -1 = adaptive vsync */
static int nat_SDL_GL_SetSwapInterval(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_int(args[0])) {
        vm->runtime_error("SDL_GL_SetSwapInterval expects (interval)."); return 0;
    }
    args[0] = val_int(SDL_GL_SetSwapInterval((int)args[0].as.integer)); return 1;
}

/* SDL_GL_GetSwapInterval() → interval */
static int nat_SDL_GL_GetSwapInterval(VM *, Value *args, int)
{
    args[0] = val_int(SDL_GL_GetSwapInterval()); return 1;
}
