#include "zen/module_glfw.h"

#ifdef ZEN_ENABLE_GLFW

#include "object.h"
#include "vm.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    static bool expect_window(VM *vm, Value v, GLFWwindow **window, const char *fn)
    {
        if (!is_ptr(v) || !as_ptr(v))
        {
            vm->runtime_error("%s expects a window handle.", fn);
            return false;
        }
        *window = (GLFWwindow *)as_ptr(v);
        return true;
    }

    static double get_number(Value v) {
        if (is_float(v)) return v.as.number;
        if (is_int(v)) return (double)v.as.integer;
        return 0.0;
    }
    static bool is_number(Value v) {
        return is_float(v) || is_int(v);
    }

    static int nat_glfw_init(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        args[0] = val_bool(glfwInit() == GLFW_TRUE);
        return 1;
    }

    static int nat_glfw_terminate(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        glfwTerminate();
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_version(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        const char *version = glfwGetVersionString();
        args[0] = val_obj((Obj *)vm->make_string(version));
        return 1;
    }

    static int nat_glfw_default_window_hints(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        glfwDefaultWindowHints();
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_window_hint(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_int(args[1]))
        {
            vm->runtime_error("glfwWindowHint() expects (hint, value).");
            return 0;
        }
        glfwWindowHint((int)args[0].as.integer, (int)args[1].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_create_window(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3 || !is_int(args[0]) || !is_int(args[1]) || !is_string(args[2]))
        {
            vm->runtime_error("glfwCreateWindow() expects (width, height, title).");
            return 0;
        }

        int width = (int)args[0].as.integer;
        int height = (int)args[1].as.integer;
        const char *title = as_cstring(args[2]);
        GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        args[0] = window ? val_ptr(window) : val_nil();
        return 1;
    }

    static int nat_glfw_destroy_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwDestroyWindow()"))
            return 0;
        glfwDestroyWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_make_context_current(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwMakeContextCurrent()"))
            return 0;
        glfwMakeContextCurrent(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_swap_buffers(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwSwapBuffers()"))
            return 0;
        glfwSwapBuffers(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_poll_events(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        glfwPollEvents();
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_window_should_close(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwWindowShouldClose()"))
            return 0;
        args[0] = val_bool(glfwWindowShouldClose(window) == GLFW_TRUE);
        return 1;
    }

    static int nat_glfw_set_window_should_close(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwSetWindowShouldClose()") || !is_bool(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetWindowShouldClose() expects (window, bool).");
            return 0;
        }
        glfwSetWindowShouldClose(window, args[1].as.boolean ? GLFW_TRUE : GLFW_FALSE);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_framebuffer_size(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwGetFramebufferSize()"))
            return 0;
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        args[0] = val_int(width);
        args[1] = val_int(height);
        return 2;
    }

    static int nat_glfw_get_window_size(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwGetWindowSize()"))
            return 0;
        int width = 0;
        int height = 0;
        glfwGetWindowSize(window, &width, &height);
        args[0] = val_int(width);
        args[1] = val_int(height);
        return 2;
    }

    static int nat_glfw_set_window_title(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwSetWindowTitle()") || !is_string(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetWindowTitle() expects (window, title).");
            return 0;
        }
        glfwSetWindowTitle(window, as_cstring(args[1]));
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_key(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwGetKey()") || !is_int(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwGetKey() expects (window, key).");
            return 0;
        }
        args[0] = val_int(glfwGetKey(window, (int)args[1].as.integer));
        return 1;
    }

    static int nat_glfw_get_time(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_float(glfwGetTime());
        return 1;
    }

    static int nat_glfw_set_time(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_number(args[0]))
        {
            vm->runtime_error("glfwSetTime() expects (time).");
            return 0;
        }
        glfwSetTime(get_number(args[0]));
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_swap_interval(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            vm->runtime_error("glfwSwapInterval() expects (interval).");
            return 0;
        }
        glfwSwapInterval((int)args[0].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_mouse_button(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwGetMouseButton()") || !is_int(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwGetMouseButton() expects (window, button).");
            return 0;
        }
        args[0] = val_int(glfwGetMouseButton(window, (int)args[1].as.integer));
        return 1;
    }

    static int nat_glfw_get_cursor_pos(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwGetCursorPos()"))
            return 0;
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        args[0] = val_float(xpos);
        args[1] = val_float(ypos);
        return 2;
    }

    static int nat_glfw_wait_events(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        glfwWaitEvents();
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_wait_events_timeout(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_number(args[0]))
        {
            vm->runtime_error("glfwWaitEventsTimeout() expects (timeout).");
            return 0;
        }
        glfwWaitEventsTimeout(get_number(args[0]));
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_post_empty_event(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)args; (void)nargs;
        glfwPostEmptyEvent();
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_set_cursor_pos(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 3 || !expect_window(vm, args[0], &window, "glfwSetCursorPos()") || !is_number(args[1]) || !is_number(args[2]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetCursorPos() expects (window, xpos, ypos).");
            return 0;
        }
        glfwSetCursorPos(window, get_number(args[1]), get_number(args[2]));
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_set_input_mode(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 3 || !expect_window(vm, args[0], &window, "glfwSetInputMode()") || !is_int(args[1]) || !is_int(args[2]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetInputMode() expects (window, mode, value).");
            return 0;
        }
        glfwSetInputMode(window, (int)args[1].as.integer, (int)args[2].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_input_mode(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwGetInputMode()") || !is_int(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwGetInputMode() expects (window, mode).");
            return 0;
        }
        args[0] = val_int(glfwGetInputMode(window, (int)args[1].as.integer));
        return 1;
    }

    static int nat_glfw_set_window_pos(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 3 || !expect_window(vm, args[0], &window, "glfwSetWindowPos()") || !is_int(args[1]) || !is_int(args[2]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetWindowPos() expects (window, xpos, ypos).");
            return 0;
        }
        glfwSetWindowPos(window, (int)args[1].as.integer, (int)args[2].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_window_pos(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwGetWindowPos()"))
            return 0;
        int xpos, ypos;
        glfwGetWindowPos(window, &xpos, &ypos);
        args[0] = val_int(xpos);
        args[1] = val_int(ypos);
        return 2;
    }
    
    static int nat_glfw_set_window_size(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 3 || !expect_window(vm, args[0], &window, "glfwSetWindowSize()") || !is_int(args[1]) || !is_int(args[2]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetWindowSize() expects (window, width, height).");
            return 0;
        }
        glfwSetWindowSize(window, (int)args[1].as.integer, (int)args[2].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_maximize_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwMaximizeWindow()"))
            return 0;
        glfwMaximizeWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_iconify_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwIconifyWindow()"))
            return 0;
        glfwIconifyWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_restore_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwRestoreWindow()"))
            return 0;
        glfwRestoreWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_show_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwShowWindow()"))
            return 0;
        glfwShowWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_hide_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwHideWindow()"))
            return 0;
        glfwHideWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_focus_window(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 1 || !expect_window(vm, args[0], &window, "glfwFocusWindow()"))
            return 0;
        glfwFocusWindow(window);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_get_current_context(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        GLFWwindow *window = glfwGetCurrentContext();
        args[0] = window ? val_ptr(window) : val_nil();
        return 1;
    }

    static int nat_glfw_get_window_attrib(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 2 || !expect_window(vm, args[0], &window, "glfwGetWindowAttrib()") || !is_int(args[1]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwGetWindowAttrib() expects (window, attrib).");
            return 0;
        }
        args[0] = val_int(glfwGetWindowAttrib(window, (int)args[1].as.integer));
        return 1;
    }

    static int nat_glfw_set_window_attrib(VM *vm, Value *args, int nargs)
    {
        GLFWwindow *window = nullptr;
        if (nargs < 3 || !expect_window(vm, args[0], &window, "glfwSetWindowAttrib()") || !is_int(args[1]) || !is_int(args[2]))
        {
            if (!vm->had_error())
                vm->runtime_error("glfwSetWindowAttrib() expects (window, attrib, value).");
            return 0;
        }
        glfwSetWindowAttrib(window, (int)args[1].as.integer, (int)args[2].as.integer);
        args[0] = val_nil();
        return 1;
    }

    static int nat_glfw_raw_mouse_motion_supported(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_bool(glfwRawMouseMotionSupported() == GLFW_TRUE);
        return 1;
    }

    static const NativeReg glfw_funcs[] = {
        {"glfwInit", nat_glfw_init, 0},
        {"glfwTerminate", nat_glfw_terminate, 0},
        {"glfwGetVersionString", nat_glfw_version, 0},
        {"glfwDefaultWindowHints", nat_glfw_default_window_hints, 0},
        {"glfwWindowHint", nat_glfw_window_hint, 2},
        {"glfwCreateWindow", nat_glfw_create_window, 3},
        {"glfwDestroyWindow", nat_glfw_destroy_window, 1},
        {"glfwMakeContextCurrent", nat_glfw_make_context_current, 1},
        {"glfwSwapBuffers", nat_glfw_swap_buffers, 1},
        {"glfwPollEvents", nat_glfw_poll_events, 0},
        {"glfwWindowShouldClose", nat_glfw_window_should_close, 1},
        {"glfwSetWindowShouldClose", nat_glfw_set_window_should_close, 2},
        {"glfwGetFramebufferSize", nat_glfw_get_framebuffer_size, 1},
        {"glfwGetWindowSize", nat_glfw_get_window_size, 1},
        {"glfwSetWindowTitle", nat_glfw_set_window_title, 2},
        {"glfwGetKey", nat_glfw_get_key, 2},
        {"glfwGetTime", nat_glfw_get_time, 0},
        {"glfwSetTime", nat_glfw_set_time, 1},
        {"glfwSwapInterval", nat_glfw_swap_interval, 1},
        {"glfwGetMouseButton", nat_glfw_get_mouse_button, 2},
        {"glfwGetCursorPos", nat_glfw_get_cursor_pos, 1},
        {"glfwSetCursorPos", nat_glfw_set_cursor_pos, 3},
        {"glfwWaitEvents", nat_glfw_wait_events, 0},
        {"glfwWaitEventsTimeout", nat_glfw_wait_events_timeout, 1},
        {"glfwPostEmptyEvent", nat_glfw_post_empty_event, 0},
        {"glfwSetInputMode", nat_glfw_set_input_mode, 3},
        {"glfwGetInputMode", nat_glfw_get_input_mode, 2},
        {"glfwSetWindowPos", nat_glfw_set_window_pos, 3},
        {"glfwGetWindowPos", nat_glfw_get_window_pos, 1},
        {"glfwSetWindowSize", nat_glfw_set_window_size, 3},
        {"glfwMaximizeWindow", nat_glfw_maximize_window, 1},
        {"glfwIconifyWindow", nat_glfw_iconify_window, 1},
        {"glfwRestoreWindow", nat_glfw_restore_window, 1},
        {"glfwShowWindow", nat_glfw_show_window, 1},
        {"glfwHideWindow", nat_glfw_hide_window, 1},
        {"glfwFocusWindow", nat_glfw_focus_window, 1},
        {"glfwGetCurrentContext", nat_glfw_get_current_context, 0},
        {"glfwGetWindowAttrib", nat_glfw_get_window_attrib, 2},
        {"glfwSetWindowAttrib", nat_glfw_set_window_attrib, 3},
        {"glfwRawMouseMotionSupported", nat_glfw_raw_mouse_motion_supported, 0},
    };

    static const NativeConst glfw_constants[] = {
        {"GLFW_TRUE", val_int(GLFW_TRUE)},
        {"GLFW_FALSE", val_int(GLFW_FALSE)},
        {"GLFW_RELEASE", val_int(GLFW_RELEASE)},
        {"GLFW_PRESS", val_int(GLFW_PRESS)},
        {"GLFW_REPEAT", val_int(GLFW_REPEAT)},
{"GLFW_KEY_0", val_int(GLFW_KEY_0)},
        {"GLFW_KEY_1", val_int(GLFW_KEY_1)},
        {"GLFW_KEY_2", val_int(GLFW_KEY_2)},
        {"GLFW_KEY_3", val_int(GLFW_KEY_3)},
        {"GLFW_KEY_4", val_int(GLFW_KEY_4)},
        {"GLFW_KEY_5", val_int(GLFW_KEY_5)},
        {"GLFW_KEY_6", val_int(GLFW_KEY_6)},
        {"GLFW_KEY_7", val_int(GLFW_KEY_7)},
        {"GLFW_KEY_8", val_int(GLFW_KEY_8)},
        {"GLFW_KEY_9", val_int(GLFW_KEY_9)},
        {"GLFW_KEY_A", val_int(GLFW_KEY_A)},
        {"GLFW_KEY_APOSTROPHE", val_int(GLFW_KEY_APOSTROPHE)},
        {"GLFW_KEY_B", val_int(GLFW_KEY_B)},
        {"GLFW_KEY_BACKSLASH", val_int(GLFW_KEY_BACKSLASH)},
        {"GLFW_KEY_BACKSPACE", val_int(GLFW_KEY_BACKSPACE)},
        {"GLFW_KEY_C", val_int(GLFW_KEY_C)},
        {"GLFW_KEY_CAPS_LOCK", val_int(GLFW_KEY_CAPS_LOCK)},
        {"GLFW_KEY_COMMA", val_int(GLFW_KEY_COMMA)},
        {"GLFW_KEY_D", val_int(GLFW_KEY_D)},
        {"GLFW_KEY_DELETE", val_int(GLFW_KEY_DELETE)},
        {"GLFW_KEY_DOWN", val_int(GLFW_KEY_DOWN)},
        {"GLFW_KEY_E", val_int(GLFW_KEY_E)},
        {"GLFW_KEY_END", val_int(GLFW_KEY_END)},
        {"GLFW_KEY_ENTER", val_int(GLFW_KEY_ENTER)},
        {"GLFW_KEY_EQUAL", val_int(GLFW_KEY_EQUAL)},
        {"GLFW_KEY_ESCAPE", val_int(GLFW_KEY_ESCAPE)},
        {"GLFW_KEY_F", val_int(GLFW_KEY_F)},
        {"GLFW_KEY_F1", val_int(GLFW_KEY_F1)},
        {"GLFW_KEY_F10", val_int(GLFW_KEY_F10)},
        {"GLFW_KEY_F11", val_int(GLFW_KEY_F11)},
        {"GLFW_KEY_F12", val_int(GLFW_KEY_F12)},
        {"GLFW_KEY_F13", val_int(GLFW_KEY_F13)},
        {"GLFW_KEY_F14", val_int(GLFW_KEY_F14)},
        {"GLFW_KEY_F15", val_int(GLFW_KEY_F15)},
        {"GLFW_KEY_F16", val_int(GLFW_KEY_F16)},
        {"GLFW_KEY_F17", val_int(GLFW_KEY_F17)},
        {"GLFW_KEY_F18", val_int(GLFW_KEY_F18)},
        {"GLFW_KEY_F19", val_int(GLFW_KEY_F19)},
        {"GLFW_KEY_F2", val_int(GLFW_KEY_F2)},
        {"GLFW_KEY_F20", val_int(GLFW_KEY_F20)},
        {"GLFW_KEY_F21", val_int(GLFW_KEY_F21)},
        {"GLFW_KEY_F22", val_int(GLFW_KEY_F22)},
        {"GLFW_KEY_F23", val_int(GLFW_KEY_F23)},
        {"GLFW_KEY_F24", val_int(GLFW_KEY_F24)},
        {"GLFW_KEY_F25", val_int(GLFW_KEY_F25)},
        {"GLFW_KEY_F3", val_int(GLFW_KEY_F3)},
        {"GLFW_KEY_F4", val_int(GLFW_KEY_F4)},
        {"GLFW_KEY_F5", val_int(GLFW_KEY_F5)},
        {"GLFW_KEY_F6", val_int(GLFW_KEY_F6)},
        {"GLFW_KEY_F7", val_int(GLFW_KEY_F7)},
        {"GLFW_KEY_F8", val_int(GLFW_KEY_F8)},
        {"GLFW_KEY_F9", val_int(GLFW_KEY_F9)},
        {"GLFW_KEY_G", val_int(GLFW_KEY_G)},
        {"GLFW_KEY_GRAVE_ACCENT", val_int(GLFW_KEY_GRAVE_ACCENT)},
        {"GLFW_KEY_H", val_int(GLFW_KEY_H)},
        {"GLFW_KEY_HOME", val_int(GLFW_KEY_HOME)},
        {"GLFW_KEY_I", val_int(GLFW_KEY_I)},
        {"GLFW_KEY_INSERT", val_int(GLFW_KEY_INSERT)},
        {"GLFW_KEY_J", val_int(GLFW_KEY_J)},
        {"GLFW_KEY_K", val_int(GLFW_KEY_K)},
        {"GLFW_KEY_KP_0", val_int(GLFW_KEY_KP_0)},
        {"GLFW_KEY_KP_1", val_int(GLFW_KEY_KP_1)},
        {"GLFW_KEY_KP_2", val_int(GLFW_KEY_KP_2)},
        {"GLFW_KEY_KP_3", val_int(GLFW_KEY_KP_3)},
        {"GLFW_KEY_KP_4", val_int(GLFW_KEY_KP_4)},
        {"GLFW_KEY_KP_5", val_int(GLFW_KEY_KP_5)},
        {"GLFW_KEY_KP_6", val_int(GLFW_KEY_KP_6)},
        {"GLFW_KEY_KP_7", val_int(GLFW_KEY_KP_7)},
        {"GLFW_KEY_KP_8", val_int(GLFW_KEY_KP_8)},
        {"GLFW_KEY_KP_9", val_int(GLFW_KEY_KP_9)},
        {"GLFW_KEY_KP_ADD", val_int(GLFW_KEY_KP_ADD)},
        {"GLFW_KEY_KP_DECIMAL", val_int(GLFW_KEY_KP_DECIMAL)},
        {"GLFW_KEY_KP_DIVIDE", val_int(GLFW_KEY_KP_DIVIDE)},
        {"GLFW_KEY_KP_ENTER", val_int(GLFW_KEY_KP_ENTER)},
        {"GLFW_KEY_KP_EQUAL", val_int(GLFW_KEY_KP_EQUAL)},
        {"GLFW_KEY_KP_MULTIPLY", val_int(GLFW_KEY_KP_MULTIPLY)},
        {"GLFW_KEY_KP_SUBTRACT", val_int(GLFW_KEY_KP_SUBTRACT)},
        {"GLFW_KEY_L", val_int(GLFW_KEY_L)},
        {"GLFW_KEY_LAST", val_int(GLFW_KEY_LAST)},
        {"GLFW_KEY_LEFT", val_int(GLFW_KEY_LEFT)},
        {"GLFW_KEY_LEFT_ALT", val_int(GLFW_KEY_LEFT_ALT)},
        {"GLFW_KEY_LEFT_BRACKET", val_int(GLFW_KEY_LEFT_BRACKET)},
        {"GLFW_KEY_LEFT_CONTROL", val_int(GLFW_KEY_LEFT_CONTROL)},
        {"GLFW_KEY_LEFT_SHIFT", val_int(GLFW_KEY_LEFT_SHIFT)},
        {"GLFW_KEY_LEFT_SUPER", val_int(GLFW_KEY_LEFT_SUPER)},
        {"GLFW_KEY_M", val_int(GLFW_KEY_M)},
        {"GLFW_KEY_MENU", val_int(GLFW_KEY_MENU)},
        {"GLFW_KEY_MINUS", val_int(GLFW_KEY_MINUS)},
        {"GLFW_KEY_N", val_int(GLFW_KEY_N)},
        {"GLFW_KEY_NUM_LOCK", val_int(GLFW_KEY_NUM_LOCK)},
        {"GLFW_KEY_O", val_int(GLFW_KEY_O)},
        {"GLFW_KEY_P", val_int(GLFW_KEY_P)},
        {"GLFW_KEY_PAGE_DOWN", val_int(GLFW_KEY_PAGE_DOWN)},
        {"GLFW_KEY_PAGE_UP", val_int(GLFW_KEY_PAGE_UP)},
        {"GLFW_KEY_PAUSE", val_int(GLFW_KEY_PAUSE)},
        {"GLFW_KEY_PERIOD", val_int(GLFW_KEY_PERIOD)},
        {"GLFW_KEY_PRINT_SCREEN", val_int(GLFW_KEY_PRINT_SCREEN)},
        {"GLFW_KEY_Q", val_int(GLFW_KEY_Q)},
        {"GLFW_KEY_R", val_int(GLFW_KEY_R)},
        {"GLFW_KEY_RIGHT", val_int(GLFW_KEY_RIGHT)},
        {"GLFW_KEY_RIGHT_ALT", val_int(GLFW_KEY_RIGHT_ALT)},
        {"GLFW_KEY_RIGHT_BRACKET", val_int(GLFW_KEY_RIGHT_BRACKET)},
        {"GLFW_KEY_RIGHT_CONTROL", val_int(GLFW_KEY_RIGHT_CONTROL)},
        {"GLFW_KEY_RIGHT_SHIFT", val_int(GLFW_KEY_RIGHT_SHIFT)},
        {"GLFW_KEY_RIGHT_SUPER", val_int(GLFW_KEY_RIGHT_SUPER)},
        {"GLFW_KEY_S", val_int(GLFW_KEY_S)},
        {"GLFW_KEY_SCROLL_LOCK", val_int(GLFW_KEY_SCROLL_LOCK)},
        {"GLFW_KEY_SEMICOLON", val_int(GLFW_KEY_SEMICOLON)},
        {"GLFW_KEY_SLASH", val_int(GLFW_KEY_SLASH)},
        {"GLFW_KEY_SPACE", val_int(GLFW_KEY_SPACE)},
        {"GLFW_KEY_T", val_int(GLFW_KEY_T)},
        {"GLFW_KEY_TAB", val_int(GLFW_KEY_TAB)},
        {"GLFW_KEY_U", val_int(GLFW_KEY_U)},
        {"GLFW_KEY_UNKNOWN", val_int(GLFW_KEY_UNKNOWN)},
        {"GLFW_KEY_UP", val_int(GLFW_KEY_UP)},
        {"GLFW_KEY_V", val_int(GLFW_KEY_V)},
        {"GLFW_KEY_W", val_int(GLFW_KEY_W)},
        {"GLFW_KEY_WORLD_1", val_int(GLFW_KEY_WORLD_1)},
        {"GLFW_KEY_WORLD_2", val_int(GLFW_KEY_WORLD_2)},
        {"GLFW_KEY_X", val_int(GLFW_KEY_X)},
        {"GLFW_KEY_Y", val_int(GLFW_KEY_Y)},
        {"GLFW_KEY_Z", val_int(GLFW_KEY_Z)},
        {"GLFW_VISIBLE", val_int(GLFW_VISIBLE)},
        {"GLFW_RESIZABLE", val_int(GLFW_RESIZABLE)},
        {"GLFW_DECORATED", val_int(GLFW_DECORATED)},
        {"GLFW_FLOATING", val_int(GLFW_FLOATING)},
        {"GLFW_FOCUSED", val_int(GLFW_FOCUSED)},
        {"GLFW_MAXIMIZED", val_int(GLFW_MAXIMIZED)},
        {"GLFW_CONTEXT_VERSION_MAJOR", val_int(GLFW_CONTEXT_VERSION_MAJOR)},
        {"GLFW_CONTEXT_VERSION_MINOR", val_int(GLFW_CONTEXT_VERSION_MINOR)},
        {"GLFW_OPENGL_PROFILE", val_int(GLFW_OPENGL_PROFILE)},
        {"GLFW_OPENGL_ANY_PROFILE", val_int(GLFW_OPENGL_ANY_PROFILE)},
        {"GLFW_OPENGL_CORE_PROFILE", val_int(GLFW_OPENGL_CORE_PROFILE)},
        {"GLFW_OPENGL_COMPAT_PROFILE", val_int(GLFW_OPENGL_COMPAT_PROFILE)},
        {"GLFW_OPENGL_FORWARD_COMPAT", val_int(GLFW_OPENGL_FORWARD_COMPAT)},
        {"GLFW_CLIENT_API", val_int(GLFW_CLIENT_API)},
        {"GLFW_OPENGL_API", val_int(GLFW_OPENGL_API)},
        {"GLFW_OPENGL_ES_API", val_int(GLFW_OPENGL_ES_API)},
        {"GLFW_NO_API", val_int(GLFW_NO_API)},
        {"GLFW_SAMPLES", val_int(GLFW_SAMPLES)},
        {"GLFW_TRANSPARENT_FRAMEBUFFER", val_int(GLFW_TRANSPARENT_FRAMEBUFFER)},
        {"GLFW_FOCUS_ON_SHOW", val_int(GLFW_FOCUS_ON_SHOW)},
        {"GLFW_SCALE_TO_MONITOR", val_int(GLFW_SCALE_TO_MONITOR)},
        {"GLFW_MOUSE_BUTTON_1", val_int(GLFW_MOUSE_BUTTON_1)},
        {"GLFW_MOUSE_BUTTON_2", val_int(GLFW_MOUSE_BUTTON_2)},
        {"GLFW_MOUSE_BUTTON_3", val_int(GLFW_MOUSE_BUTTON_3)},
        {"GLFW_MOUSE_BUTTON_4", val_int(GLFW_MOUSE_BUTTON_4)},
        {"GLFW_MOUSE_BUTTON_5", val_int(GLFW_MOUSE_BUTTON_5)},
        {"GLFW_MOUSE_BUTTON_6", val_int(GLFW_MOUSE_BUTTON_6)},
        {"GLFW_MOUSE_BUTTON_7", val_int(GLFW_MOUSE_BUTTON_7)},
        {"GLFW_MOUSE_BUTTON_8", val_int(GLFW_MOUSE_BUTTON_8)},
        {"GLFW_MOUSE_BUTTON_LEFT", val_int(GLFW_MOUSE_BUTTON_LEFT)},
        {"GLFW_MOUSE_BUTTON_RIGHT", val_int(GLFW_MOUSE_BUTTON_RIGHT)},
        {"GLFW_MOUSE_BUTTON_MIDDLE", val_int(GLFW_MOUSE_BUTTON_MIDDLE)},
        {"GLFW_CURSOR", val_int(GLFW_CURSOR)},
        {"GLFW_CURSOR_NORMAL", val_int(GLFW_CURSOR_NORMAL)},
        {"GLFW_CURSOR_HIDDEN", val_int(GLFW_CURSOR_HIDDEN)},
        {"GLFW_CURSOR_DISABLED", val_int(GLFW_CURSOR_DISABLED)},
        {"GLFW_RAW_MOUSE_MOTION", val_int(GLFW_RAW_MOUSE_MOTION)},
    };

    const NativeLib zen_lib_glfw = {
        "glfw", glfw_funcs, ZEN_ARRAY_COUNT(glfw_funcs), glfw_constants, ZEN_ARRAY_COUNT(glfw_constants)
    };
}

#endif /* ZEN_ENABLE_GLFW */
