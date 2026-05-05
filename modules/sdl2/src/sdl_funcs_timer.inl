/* sdl_funcs_timer.inl — SDL2 timing & sleep
** Included by builtin_sdl2.cpp */

/* SDL_Delay(ms) — sleep for at least ms milliseconds */
static int nat_SDL_Delay(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_int(args[0])) {
        vm->runtime_error("SDL_Delay expects (ms)."); return 0;
    }
    SDL_Delay((uint32_t)args[0].as.integer);
    args[0] = val_nil(); return 1;
}

/* SDL_GetTicks() → milliseconds since SDL_Init */
static int nat_SDL_GetTicks(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)SDL_GetTicks()); return 1;
}

#if SDL_VERSION_ATLEAST(2, 0, 18)
/* SDL_GetTicks64() → uint64 milliseconds (no 49-day wrap) */
static int nat_SDL_GetTicks64(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)SDL_GetTicks64()); return 1;
}
#endif

/* SDL_GetPerformanceCounter() → high-resolution counter value */
static int nat_SDL_GetPerformanceCounter(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)SDL_GetPerformanceCounter()); return 1;
}

/* SDL_GetPerformanceFrequency() → ticks per second */
static int nat_SDL_GetPerformanceFrequency(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)SDL_GetPerformanceFrequency()); return 1;
}
