/* sdl_funcs_events.inl — SDL2 event handling
** Included by builtin_sdl2.cpp
**
** Pattern: SDL_PollEvent() returns the event type (int) or 0 when queue is empty.
** Query functions (SDL_event_key, SDL_event_mouse_*…) read s_last_event which
** is updated by any of the four poll/wait calls.
**
** Zen usage:
**   var ev = SDL_PollEvent()
**   if ev == SDL_QUIT { break }
**   if ev == SDL_KEYDOWN {
**       var scan, sym, mod, rep = SDL_event_key()
**       if scan == SDL_SCANCODE_ESCAPE { break }
**   }
**   if ev == SDL_MOUSEMOTION {
**       var x, y, dx, dy, state = SDL_event_mouse_motion()
**   }
*/

static SDL_Event s_last_event;

static int nat_SDL_PumpEvents(VM *, Value *args, int)
{
    SDL_PumpEvents(); args[0] = val_nil(); return 1;
}

/* SDL_PollEvent() → event_type (or 0 if queue empty) */
static int nat_SDL_PollEvent(VM *, Value *args, int)
{
    int ok = SDL_PollEvent(&s_last_event);
    args[0] = val_int(ok ? (int64_t)s_last_event.type : 0);
    return 1;
}

/* SDL_WaitEvent() → event_type (blocks until an event arrives) */
static int nat_SDL_WaitEvent(VM *, Value *args, int)
{
    SDL_WaitEvent(&s_last_event);
    args[0] = val_int((int64_t)s_last_event.type); return 1;
}

/* SDL_WaitEventTimeout(ms) → event_type or 0 if timeout */
static int nat_SDL_WaitEventTimeout(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_int(args[0])) {
        vm->runtime_error("SDL_WaitEventTimeout expects (ms)."); return 0;
    }
    int ok = SDL_WaitEventTimeout(&s_last_event, (int)args[0].as.integer);
    args[0] = val_int(ok ? (int64_t)s_last_event.type : 0);
    return 1;
}

/* SDL_event_key() → scancode, sym, mod, repeat
** Valid after SDL_KEYDOWN or SDL_KEYUP */
static int nat_SDL_event_key(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)s_last_event.key.keysym.scancode);
    args[1] = val_int((int64_t)s_last_event.key.keysym.sym);
    args[2] = val_int((int64_t)s_last_event.key.keysym.mod);
    args[3] = val_int((int64_t)s_last_event.key.repeat);
    return 4;
}

/* SDL_event_mouse_motion() → x, y, xrel, yrel, state
** Valid after SDL_MOUSEMOTION */
static int nat_SDL_event_mouse_motion(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)s_last_event.motion.x);
    args[1] = val_int((int64_t)s_last_event.motion.y);
    args[2] = val_int((int64_t)s_last_event.motion.xrel);
    args[3] = val_int((int64_t)s_last_event.motion.yrel);
    args[4] = val_int((int64_t)s_last_event.motion.state);
    return 5;
}

/* SDL_event_mouse_button() → button, x, y, clicks
** Valid after SDL_MOUSEBUTTONDOWN or SDL_MOUSEBUTTONUP */
static int nat_SDL_event_mouse_button(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)s_last_event.button.button);
    args[1] = val_int((int64_t)s_last_event.button.x);
    args[2] = val_int((int64_t)s_last_event.button.y);
    args[3] = val_int((int64_t)s_last_event.button.clicks);
    return 4;
}

/* SDL_event_mouse_wheel() → x, y, direction
** Valid after SDL_MOUSEWHEEL
** direction: SDL_MOUSEWHEEL_NORMAL or SDL_MOUSEWHEEL_FLIPPED */
static int nat_SDL_event_mouse_wheel(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)s_last_event.wheel.x);
    args[1] = val_int((int64_t)s_last_event.wheel.y);
    args[2] = val_int((int64_t)s_last_event.wheel.direction);
    return 3;
}

/* SDL_event_window() → windowID, event_id, data1, data2
** Valid after SDL_WINDOWEVENT */
static int nat_SDL_event_window(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)s_last_event.window.windowID);
    args[1] = val_int((int64_t)s_last_event.window.event);
    args[2] = val_int((int64_t)s_last_event.window.data1);
    args[3] = val_int((int64_t)s_last_event.window.data2);
    return 4;
}

/* SDL_event_text() → string
** Valid after SDL_TEXTINPUT */
static int nat_SDL_event_text(VM *vm, Value *args, int)
{
    args[0] = val_obj((Obj *)vm->make_string(s_last_event.text.text)); return 1;
}

/* SDL_GetMouseState() → x, y, button_mask */
static int nat_SDL_GetMouseState(VM *, Value *args, int)
{
    int x = 0, y = 0;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    args[0] = val_int(x); args[1] = val_int(y);
    args[2] = val_int((int64_t)buttons);
    return 3;
}

/* SDL_GetKeyboardState(scancode) → bool (is key currently held?) */
static int nat_SDL_GetKeyboardState(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_int(args[0])) {
        vm->runtime_error("SDL_GetKeyboardState expects (scancode)."); return 0;
    }
    int numkeys = 0;
    const uint8_t *state = SDL_GetKeyboardState(&numkeys);
    int sc = (int)args[0].as.integer;
    args[0] = val_bool(sc >= 0 && sc < numkeys && state[sc] != 0);
    return 1;
}

/* SDL_GetModState() → modifier_flags */
static int nat_SDL_GetModState(VM *, Value *args, int)
{
    args[0] = val_int((int64_t)SDL_GetModState()); return 1;
}
