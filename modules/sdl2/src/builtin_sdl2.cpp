#include "zen/module_sdl2.h"

#ifdef ZEN_ENABLE_SDL2

#include "object.h"
#include "vm.h"

#include <SDL2/SDL.h>

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    static bool expect_ptr_arg(VM *vm, Value v, void **out, const char *fn, const char *kind)
    {
        if (!is_ptr(v) || !as_ptr(v))
        {
            vm->runtime_error("%s expects a %s handle.", fn, kind);
            return false;
        }
        *out = as_ptr(v);
        return true;
    }

    static int nat_sdl_init(VM *vm, Value *args, int nargs)
    {
        uint32_t flags = SDL_INIT_VIDEO;
        if (nargs >= 1)
        {
            if (!is_int(args[0]))
            {
                vm->runtime_error("SDL_Init() expects (flags?).");
                return 0;
            }
            flags = (uint32_t)args[0].as.integer;
        }
        args[0] = val_int(SDL_Init(flags));
        return 1;
    }

    static int nat_sdl_quit(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        SDL_Quit();
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_get_error(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        args[0] = val_obj((Obj *)vm->make_string(SDL_GetError()));
        return 1;
    }

    static int nat_sdl_clear_error(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        SDL_ClearError();
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_get_revision(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        args[0] = val_obj((Obj *)vm->make_string(SDL_GetRevision()));
        return 1;
    }

    static int nat_sdl_was_init(VM *vm, Value *args, int nargs)
    {
        uint32_t flags = 0;
        if (nargs >= 1)
        {
            if (!is_int(args[0]))
            {
                vm->runtime_error("SDL_WasInit() expects (flags?).");
                return 0;
            }
            flags = (uint32_t)args[0].as.integer;
        }
        args[0] = val_int((int64_t)SDL_WasInit(flags));
        return 1;
    }

    static int nat_sdl_create_window(VM *vm, Value *args, int nargs)
    {
        if (nargs < 6 || !is_string(args[0]) || !is_int(args[1]) || !is_int(args[2]) ||
            !is_int(args[3]) || !is_int(args[4]) || !is_int(args[5]))
        {
            vm->runtime_error("SDL_CreateWindow() expects (title, x, y, width, height, flags).");
            return 0;
        }

        SDL_Window *window = SDL_CreateWindow(
            as_cstring(args[0]),
            (int)args[1].as.integer,
            (int)args[2].as.integer,
            (int)args[3].as.integer,
            (int)args[4].as.integer,
            (uint32_t)args[5].as.integer
        );
        args[0] = window ? val_ptr(window) : val_nil();
        return 1;
    }

    static int nat_sdl_destroy_window(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 1 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_DestroyWindow()", "window"))
            return 0;
        SDL_DestroyWindow((SDL_Window *)ptr);
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_set_window_title(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 2 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_SetWindowTitle()", "window") || !is_string(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("SDL_SetWindowTitle() expects (window, title).");
            return 0;
        }
        SDL_SetWindowTitle((SDL_Window *)ptr, as_cstring(args[1]));
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_get_window_size(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 1 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_GetWindowSize()", "window"))
            return 0;
        int w = 0;
        int h = 0;
        SDL_GetWindowSize((SDL_Window *)ptr, &w, &h);
        args[0] = val_int(w);
        args[1] = val_int(h);
        return 2;
    }

    static int nat_sdl_gl_set_attribute(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_int(args[1]))
        {
            vm->runtime_error("SDL_GL_SetAttribute() expects (attr, value).");
            return 0;
        }
        args[0] = val_int(SDL_GL_SetAttribute((SDL_GLattr)args[0].as.integer, (int)args[1].as.integer));
        return 1;
    }

    static int nat_sdl_gl_create_context(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 1 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_GL_CreateContext()", "window"))
            return 0;
        SDL_GLContext ctx = SDL_GL_CreateContext((SDL_Window *)ptr);
        args[0] = ctx ? val_ptr(ctx) : val_nil();
        return 1;
    }

    static int nat_sdl_gl_make_current(VM *vm, Value *args, int nargs)
    {
        void *window_ptr = nullptr;
        void *ctx_ptr = nullptr;
        if (nargs < 2 ||
            !expect_ptr_arg(vm, args[0], &window_ptr, "SDL_GL_MakeCurrent()", "window") ||
            !expect_ptr_arg(vm, args[1], &ctx_ptr, "SDL_GL_MakeCurrent()", "context"))
        {
            return 0;
        }
        args[0] = val_int(SDL_GL_MakeCurrent((SDL_Window *)window_ptr, (SDL_GLContext)ctx_ptr));
        return 1;
    }

    static int nat_sdl_gl_delete_context(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 1 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_GL_DeleteContext()", "context"))
            return 0;
        SDL_GL_DeleteContext((SDL_GLContext)ptr);
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_gl_swap_window(VM *vm, Value *args, int nargs)
    {
        void *ptr = nullptr;
        if (nargs < 1 || !expect_ptr_arg(vm, args[0], &ptr, "SDL_GL_SwapWindow()", "window"))
            return 0;
        SDL_GL_SwapWindow((SDL_Window *)ptr);
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_gl_set_swap_interval(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("SDL_GL_SetSwapInterval() expects (interval).");
            return 0;
        }
        args[0] = val_int(SDL_GL_SetSwapInterval((int)args[0].as.integer));
        return 1;
    }

    static int nat_sdl_gl_get_swap_interval(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_int(SDL_GL_GetSwapInterval());
        return 1;
    }

    static int nat_sdl_delay(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("SDL_Delay() expects (ms).");
            return 0;
        }
        SDL_Delay((uint32_t)args[0].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_sdl_get_ticks(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_int((int64_t)SDL_GetTicks());
        return 1;
    }

    static int nat_sdl_get_performance_counter(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_int((int64_t)SDL_GetPerformanceCounter());
        return 1;
    }

    static int nat_sdl_get_performance_frequency(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_int((int64_t)SDL_GetPerformanceFrequency());
        return 1;
    }

    static int nat_sdl_pump_events(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        SDL_PumpEvents();
        args[0] = val_nil();
        return 1;
    }

    static const NativeReg sdl2_funcs[] = {
        {"SDL_Init", nat_sdl_init, -1},
        {"SDL_Quit", nat_sdl_quit, 0},
        {"SDL_GetError", nat_sdl_get_error, 0},
        {"SDL_ClearError", nat_sdl_clear_error, 0},
        {"SDL_GetRevision", nat_sdl_get_revision, 0},
        {"SDL_WasInit", nat_sdl_was_init, -1},
        {"SDL_CreateWindow", nat_sdl_create_window, 6},
        {"SDL_DestroyWindow", nat_sdl_destroy_window, 1},
        {"SDL_SetWindowTitle", nat_sdl_set_window_title, 2},
        {"SDL_GetWindowSize", nat_sdl_get_window_size, 1},
        {"SDL_GL_SetAttribute", nat_sdl_gl_set_attribute, 2},
        {"SDL_GL_CreateContext", nat_sdl_gl_create_context, 1},
        {"SDL_GL_MakeCurrent", nat_sdl_gl_make_current, 2},
        {"SDL_GL_DeleteContext", nat_sdl_gl_delete_context, 1},
        {"SDL_GL_SwapWindow", nat_sdl_gl_swap_window, 1},
        {"SDL_GL_SetSwapInterval", nat_sdl_gl_set_swap_interval, 1},
        {"SDL_GL_GetSwapInterval", nat_sdl_gl_get_swap_interval, 0},
        {"SDL_Delay", nat_sdl_delay, 1},
        {"SDL_GetTicks", nat_sdl_get_ticks, 0},
        {"SDL_GetPerformanceCounter", nat_sdl_get_performance_counter, 0},
        {"SDL_GetPerformanceFrequency", nat_sdl_get_performance_frequency, 0},
        {"SDL_PumpEvents", nat_sdl_pump_events, 0},
    };

    static const NativeConst sdl2_constants[] = {
        {"SDL_INIT_TIMER", val_int(SDL_INIT_TIMER)},
        {"SDL_INIT_AUDIO", val_int(SDL_INIT_AUDIO)},
        {"SDL_INIT_VIDEO", val_int(SDL_INIT_VIDEO)},
        {"SDL_INIT_JOYSTICK", val_int(SDL_INIT_JOYSTICK)},
        {"SDL_INIT_HAPTIC", val_int(SDL_INIT_HAPTIC)},
        {"SDL_INIT_GAMECONTROLLER", val_int(SDL_INIT_GAMECONTROLLER)},
        {"SDL_INIT_EVENTS", val_int(SDL_INIT_EVENTS)},
        {"SDL_INIT_EVERYTHING", val_int(SDL_INIT_EVERYTHING)},
        {"SDL_WINDOWPOS_CENTERED", val_int(SDL_WINDOWPOS_CENTERED)},
        {"SDL_WINDOWPOS_UNDEFINED", val_int(SDL_WINDOWPOS_UNDEFINED)},
        {"SDL_WINDOW_FULLSCREEN", val_int(SDL_WINDOW_FULLSCREEN)},
        {"SDL_WINDOW_OPENGL", val_int(SDL_WINDOW_OPENGL)},
        {"SDL_WINDOW_SHOWN", val_int(SDL_WINDOW_SHOWN)},
        {"SDL_WINDOW_HIDDEN", val_int(SDL_WINDOW_HIDDEN)},
        {"SDL_WINDOW_BORDERLESS", val_int(SDL_WINDOW_BORDERLESS)},
        {"SDL_WINDOW_RESIZABLE", val_int(SDL_WINDOW_RESIZABLE)},
        {"SDL_WINDOW_MINIMIZED", val_int(SDL_WINDOW_MINIMIZED)},
        {"SDL_WINDOW_MAXIMIZED", val_int(SDL_WINDOW_MAXIMIZED)},
        {"SDL_WINDOW_INPUT_GRABBED", val_int(SDL_WINDOW_INPUT_GRABBED)},
        {"SDL_WINDOW_ALLOW_HIGHDPI", val_int(SDL_WINDOW_ALLOW_HIGHDPI)},
        {"SDL_WINDOW_MOUSE_CAPTURE", val_int(SDL_WINDOW_MOUSE_CAPTURE)},
        {"SDL_WINDOW_ALWAYS_ON_TOP", val_int(SDL_WINDOW_ALWAYS_ON_TOP)},
        {"SDL_WINDOW_SKIP_TASKBAR", val_int(SDL_WINDOW_SKIP_TASKBAR)},
        {"SDL_WINDOW_UTILITY", val_int(SDL_WINDOW_UTILITY)},
        {"SDL_WINDOW_TOOLTIP", val_int(SDL_WINDOW_TOOLTIP)},
        {"SDL_WINDOW_POPUP_MENU", val_int(SDL_WINDOW_POPUP_MENU)},
        {"SDL_WINDOW_VULKAN", val_int(SDL_WINDOW_VULKAN)},
        {"SDL_WINDOW_METAL", val_int(SDL_WINDOW_METAL)},
        {"SDL_GL_RED_SIZE", val_int(SDL_GL_RED_SIZE)},
        {"SDL_GL_GREEN_SIZE", val_int(SDL_GL_GREEN_SIZE)},
        {"SDL_GL_BLUE_SIZE", val_int(SDL_GL_BLUE_SIZE)},
        {"SDL_GL_ALPHA_SIZE", val_int(SDL_GL_ALPHA_SIZE)},
        {"SDL_GL_BUFFER_SIZE", val_int(SDL_GL_BUFFER_SIZE)},
        {"SDL_GL_DOUBLEBUFFER", val_int(SDL_GL_DOUBLEBUFFER)},
        {"SDL_GL_DEPTH_SIZE", val_int(SDL_GL_DEPTH_SIZE)},
        {"SDL_GL_STENCIL_SIZE", val_int(SDL_GL_STENCIL_SIZE)},
        {"SDL_GL_CONTEXT_MAJOR_VERSION", val_int(SDL_GL_CONTEXT_MAJOR_VERSION)},
        {"SDL_GL_CONTEXT_MINOR_VERSION", val_int(SDL_GL_CONTEXT_MINOR_VERSION)},
        {"SDL_GL_CONTEXT_PROFILE_MASK", val_int(SDL_GL_CONTEXT_PROFILE_MASK)},
        {"SDL_GL_CONTEXT_PROFILE_CORE", val_int(SDL_GL_CONTEXT_PROFILE_CORE)},
        {"SDL_GL_CONTEXT_PROFILE_COMPATIBILITY", val_int(SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)},
        {"SDL_GL_CONTEXT_PROFILE_ES", val_int(SDL_GL_CONTEXT_PROFILE_ES)},
        {"SDL_GL_CONTEXT_FLAGS", val_int(SDL_GL_CONTEXT_FLAGS)},
        {"SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG", val_int(SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)},
        {"SDL_GL_CONTEXT_DEBUG_FLAG", val_int(SDL_GL_CONTEXT_DEBUG_FLAG)},
        {"SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG", val_int(SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG)},
        {"SDL_GL_MULTISAMPLEBUFFERS", val_int(SDL_GL_MULTISAMPLEBUFFERS)},
        {"SDL_GL_MULTISAMPLESAMPLES", val_int(SDL_GL_MULTISAMPLESAMPLES)},
    };

    const NativeLib zen_lib_sdl2 = {
        "sdl2", sdl2_funcs, ZEN_ARRAY_COUNT(sdl2_funcs), sdl2_constants, ZEN_ARRAY_COUNT(sdl2_constants)
    };
}

#endif /* ZEN_ENABLE_SDL2 */
