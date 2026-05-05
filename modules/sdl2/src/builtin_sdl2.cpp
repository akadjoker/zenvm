/* builtin_sdl2.cpp — SDL2 module for Zen
**
** import sdl2
**
** Exposes exact SDL2 C API names: SDL_CreateWindow, SDL_PollEvent, etc.
** Organised in .inl files by category:
**   sdl_funcs_window.inl    — SDL_Init/Quit, window management
**   sdl_funcs_gl.inl        — SDL_GL_* context management (create, swap…)
**   sdl_funcs_renderer.inl  — SDL2 2D renderer (SDL_Renderer, textures…)
**   sdl_funcs_events.inl    — SDL_PollEvent + event field accessors
**   sdl_funcs_timer.inl     — SDL_GetTicks, SDL_Delay, performance counter
**   sdl_constants.inl       — NativeConst[] with all SDL_ constants
**
** NOTE: raw OpenGL drawing (glClear, glDraw*, shaders…) comes from
**       import gl, NOT this module. SDL only manages the window and
**       GL context lifetime via SDL_GL_*.
*/

#include "zen/module_sdl2.h"

#ifdef ZEN_ENABLE_SDL2

#include "object.h"
#include "vm.h"

#include <SDL2/SDL.h>

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    /* Shared helper — validate a raw pointer Value */
    static bool expect_ptr_arg(VM *vm, Value v, void **out, const char *fn, const char *kind)
    {
        if (!is_ptr(v) || !as_ptr(v)) {
            vm->runtime_error("%s expects a %s handle.", fn, kind);
            return false;
        }
        *out = as_ptr(v);
        return true;
    }

    /* ===== Function implementations (via .inl) ===== */
    #include "sdl_funcs_window.inl"
    #include "sdl_funcs_gl.inl"
    #include "sdl_funcs_renderer.inl"
    #include "sdl_funcs_events.inl"
    #include "sdl_funcs_timer.inl"

    /* ===== Function registration table ===== */
    static const NativeReg sdl2_funcs[] = {
        /* Init / lifecycle */
        {"SDL_Init",        nat_SDL_Init,        -1},
        {"SDL_Quit",        nat_SDL_Quit,          0},
        {"SDL_GetError",    nat_SDL_GetError,      0},
        {"SDL_ClearError",  nat_SDL_ClearError,    0},
        {"SDL_GetRevision", nat_SDL_GetRevision,   0},
        {"SDL_WasInit",     nat_SDL_WasInit,      -1},

        /* Window */
        {"SDL_CreateWindow",        nat_SDL_CreateWindow,        6},
        {"SDL_DestroyWindow",       nat_SDL_DestroyWindow,       1},
        {"SDL_SetWindowTitle",      nat_SDL_SetWindowTitle,      2},
        {"SDL_GetWindowSize",       nat_SDL_GetWindowSize,       1},
        {"SDL_SetWindowSize",       nat_SDL_SetWindowSize,       3},
        {"SDL_GetWindowPosition",   nat_SDL_GetWindowPosition,   1},
        {"SDL_SetWindowPosition",   nat_SDL_SetWindowPosition,   3},
        {"SDL_ShowWindow",          nat_SDL_ShowWindow,          1},
        {"SDL_HideWindow",          nat_SDL_HideWindow,          1},
        {"SDL_RaiseWindow",         nat_SDL_RaiseWindow,         1},
        {"SDL_MaximizeWindow",      nat_SDL_MaximizeWindow,      1},
        {"SDL_MinimizeWindow",      nat_SDL_MinimizeWindow,      1},
        {"SDL_RestoreWindow",       nat_SDL_RestoreWindow,       1},
        {"SDL_SetWindowFullscreen", nat_SDL_SetWindowFullscreen, 2},
        {"SDL_GetWindowID",         nat_SDL_GetWindowID,         1},
        {"SDL_GetWindowFlags",      nat_SDL_GetWindowFlags,      1},

        /* GL context (use with import gl for actual drawing) */
        {"SDL_GL_SetAttribute",    nat_SDL_GL_SetAttribute,    2},
        {"SDL_GL_GetAttribute",    nat_SDL_GL_GetAttribute,    1},
        {"SDL_GL_CreateContext",   nat_SDL_GL_CreateContext,   1},
        {"SDL_GL_MakeCurrent",     nat_SDL_GL_MakeCurrent,     2},
        {"SDL_GL_DeleteContext",   nat_SDL_GL_DeleteContext,   1},
        {"SDL_GL_SwapWindow",      nat_SDL_GL_SwapWindow,      1},
        {"SDL_GL_SetSwapInterval", nat_SDL_GL_SetSwapInterval, 1},
        {"SDL_GL_GetSwapInterval", nat_SDL_GL_GetSwapInterval, 0},

        /* 2D renderer */
        {"SDL_CreateRenderer",          nat_SDL_CreateRenderer,          3},
        {"SDL_DestroyRenderer",         nat_SDL_DestroyRenderer,         1},
        {"SDL_SetRenderDrawColor",      nat_SDL_SetRenderDrawColor,      5},
        {"SDL_SetRenderDrawBlendMode",  nat_SDL_SetRenderDrawBlendMode,  2},
        {"SDL_RenderClear",             nat_SDL_RenderClear,             1},
        {"SDL_RenderPresent",           nat_SDL_RenderPresent,           1},
        {"SDL_RenderDrawPoint",         nat_SDL_RenderDrawPoint,         3},
        {"SDL_RenderDrawLine",          nat_SDL_RenderDrawLine,          5},
        {"SDL_RenderDrawRect",          nat_SDL_RenderDrawRect,          5},
        {"SDL_RenderFillRect",          nat_SDL_RenderFillRect,          5},
        {"SDL_RenderSetViewport",       nat_SDL_RenderSetViewport,       5},
        {"SDL_RenderSetScale",          nat_SDL_RenderSetScale,          3},
        {"SDL_GetRendererOutputSize",   nat_SDL_GetRendererOutputSize,   1},
        {"SDL_CreateTexture",           nat_SDL_CreateTexture,           5},
        {"SDL_CreateTextureFromSurface",nat_SDL_CreateTextureFromSurface,2},
        {"SDL_DestroyTexture",          nat_SDL_DestroyTexture,          1},
        {"SDL_SetTextureBlendMode",     nat_SDL_SetTextureBlendMode,     2},
        {"SDL_SetTextureAlphaMod",      nat_SDL_SetTextureAlphaMod,      2},
        {"SDL_SetTextureColorMod",      nat_SDL_SetTextureColorMod,      4},
        {"SDL_QueryTexture",            nat_SDL_QueryTexture,            1},
        {"SDL_RenderCopy",              nat_SDL_RenderCopy,             10},
        {"SDL_RenderCopyEx",            nat_SDL_RenderCopyEx,           14},
        {"SDL_UpdateTexture",           nat_SDL_UpdateTexture,           7},
        {"SDL_FreeSurface",             nat_SDL_FreeSurface,             1},

        /* Events */
        {"SDL_PumpEvents",         nat_SDL_PumpEvents,         0},
        {"SDL_PollEvent",          nat_SDL_PollEvent,          0},
        {"SDL_WaitEvent",          nat_SDL_WaitEvent,          0},
        {"SDL_WaitEventTimeout",   nat_SDL_WaitEventTimeout,   1},
        {"SDL_event_key",          nat_SDL_event_key,          0},
        {"SDL_event_mouse_motion", nat_SDL_event_mouse_motion, 0},
        {"SDL_event_mouse_button", nat_SDL_event_mouse_button, 0},
        {"SDL_event_mouse_wheel",  nat_SDL_event_mouse_wheel,  0},
        {"SDL_event_window",       nat_SDL_event_window,       0},
        {"SDL_event_text",         nat_SDL_event_text,         0},
        {"SDL_GetMouseState",      nat_SDL_GetMouseState,      0},
        {"SDL_GetKeyboardState",   nat_SDL_GetKeyboardState,   1},
        {"SDL_GetModState",        nat_SDL_GetModState,        0},

        /* Timer */
        {"SDL_Delay",                   nat_SDL_Delay,                   1},
        {"SDL_GetTicks",                nat_SDL_GetTicks,                0},
#if SDL_VERSION_ATLEAST(2, 0, 18)
        {"SDL_GetTicks64",              nat_SDL_GetTicks64,              0},
#endif
        {"SDL_GetPerformanceCounter",   nat_SDL_GetPerformanceCounter,   0},
        {"SDL_GetPerformanceFrequency", nat_SDL_GetPerformanceFrequency, 0},
    };

    /* ===== Constants ===== */
    #include "sdl_constants.inl"

    /* ===== NativeLib descriptor ===== */
    const NativeLib zen_lib_sdl2 = {
        "sdl2",
        sdl2_funcs,     ZEN_ARRAY_COUNT(sdl2_funcs),
        sdl2_constants, ZEN_ARRAY_COUNT(sdl2_constants),
        nullptr
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_SDL2 */
