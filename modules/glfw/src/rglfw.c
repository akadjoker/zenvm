/**********************************************************************************************
*
*   rglfw - raylib GLFW single file compilation
*
*   This file includes latest GLFW sources (https://github.com/glfw/glfw) to be compiled together
*   with raylib for all supported platforms, this way, no vendor dependencies are required.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2017-2023 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

//#define _GLFW_BUILD_DLL           // To build shared version
// Ref: http://www.glfw.org/docs/latest/compile.html#compile_manual

// Platform options:
// _GLFW_WIN32      to use the Win32 API
// _GLFW_X11        to use the X Window System
// _GLFW_WAYLAND    to use the Wayland API (experimental and incomplete)
// _GLFW_COCOA      to use the Cocoa frameworks
// _GLFW_OSMESA     to use the OSMesa API (headless and non-interactive)
// _GLFW_MIR        experimental, not supported at this moment

#if defined(_WIN32) || defined(__CYGWIN__)
    #define _GLFW_WIN32
#endif
#if defined(__linux__)
    #if !defined(_GLFW_WAYLAND)     // Required for Wayland windowing
        #define _GLFW_X11
    #endif
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define _GLFW_X11
#endif
#if defined(__APPLE__)
    #define _GLFW_COCOA
    #define _GLFW_USE_MENUBAR       // To create and populate the menu bar when the first window is created
    #define _GLFW_USE_RETINA        // To have windows use the full resolution of Retina displays
#endif
#if defined(__TINYC__)
    #define _WIN32_WINNT_WINXP      0x0501
#endif

// Common modules to all platforms
#include "vendor/glfw/src/init.c"
#include "vendor/glfw/src/platform.c"
#include "vendor/glfw/src/context.c"
#include "vendor/glfw/src/monitor.c"
#include "vendor/glfw/src/window.c"
#include "vendor/glfw/src/input.c"
#include "vendor/glfw/src/vulkan.c"

#if defined(_WIN32) || defined(__CYGWIN__)
    #include "vendor/glfw/src/win32_init.c"
    #include "vendor/glfw/src/win32_module.c"
    #include "vendor/glfw/src/win32_monitor.c"
    #include "vendor/glfw/src/win32_window.c"
    #include "vendor/glfw/src/win32_joystick.c"
    #include "vendor/glfw/src/win32_time.c"
    #include "vendor/glfw/src/win32_thread.c"
    #include "vendor/glfw/src/wgl_context.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif

#if defined(__linux__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/posix_time.c"
    #include "vendor/glfw/src/posix_poll.c"
    #include "vendor/glfw/src/linux_joystick.c"
    #include "vendor/glfw/src/xkb_unicode.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"

    #if defined(_GLFW_WAYLAND)
        #include "vendor/glfw/src/wl_init.c"
        #include "vendor/glfw/src/wl_monitor.c"
        #include "vendor/glfw/src/wl_window.c"
    #endif
    #if defined(_GLFW_X11)
        #include "vendor/glfw/src/x11_init.c"
        #include "vendor/glfw/src/x11_monitor.c"
        #include "vendor/glfw/src/x11_window.c"
        #include "vendor/glfw/src/glx_context.c"
    #endif
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined( __NetBSD__) || defined(__DragonFly__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/posix_time.c"
    #include "vendor/glfw/src/posix_poll.c"
    #include "vendor/glfw/src/null_joystick.c"
    #include "vendor/glfw/src/xkb_unicode.c"

    #include "vendor/glfw/src/x11_init.c"
    #include "vendor/glfw/src/x11_monitor.c"
    #include "vendor/glfw/src/x11_window.c"
    #include "vendor/glfw/src/glx_context.c"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif

#if defined(__APPLE__)
    #include "vendor/glfw/src/posix_module.c"
    #include "vendor/glfw/src/posix_thread.c"
    #include "vendor/glfw/src/cocoa_init.m"
    #include "vendor/glfw/src/cocoa_joystick.m"
    #include "vendor/glfw/src/cocoa_monitor.m"
    #include "vendor/glfw/src/cocoa_window.m"
    #include "vendor/glfw/src/cocoa_time.c"
    #include "vendor/glfw/src/nsgl_context.m"

    #include "vendor/glfw/src/egl_context.c"
    #include "vendor/glfw/src/osmesa_context.c"
#endif
