/*
** test_visual_intern - raylib loop owned by the Zen script.
**
** C++ only registers native bindings and runs the script once. The script
** opens/closes raylib, owns the while loop, and calls drawing/input directly.
*/

#include "raylib.h"
#include "zen/compiler.h"
#include "zen/module.h"
#include "zen/vm.h"
#include <cstdio>
#include <cstdlib>

using namespace zen;

static Texture2D g_sprite_texture = Texture2D{0, 0, 0, 0, 0};
static bool g_window_open = false;
static bool g_texture_loaded = false;

static unsigned char byte_arg(Value v)
{
    int n = (int)to_integer(v);
    if (n < 0) n = 0;
    if (n > 255) n = 255;
    return (unsigned char)n;
}

static Color color_arg(Value *args, int base)
{
    return Color{byte_arg(args[base + 0]), byte_arg(args[base + 1]),
                 byte_arg(args[base + 2]), byte_arg(args[base + 3])};
}

static void cleanup_raylib()
{
    if (g_texture_loaded)
    {
        UnloadTexture(g_sprite_texture);
        g_sprite_texture = Texture2D{0, 0, 0, 0, 0};
        g_texture_loaded = false;
    }
    if (g_window_open)
    {
        CloseWindow();
        g_window_open = false;
    }
}

static int nat_init_raylib(VM *, Value *args, int)
{
    int width = (int)to_integer(args[0]);
    int height = (int)to_integer(args[1]);
    const char *title = as_cstring(args[2]);
    const char *texture_path = as_cstring(args[3]);

    if (!g_window_open)
    {
        InitWindow(width, height, title);
        SetTargetFPS(0);
        g_window_open = true;
    }

    if (!g_texture_loaded)
    {
        g_sprite_texture = LoadTexture(texture_path);
        if (g_sprite_texture.id == 0)
        {
            fprintf(stderr, "Cannot load texture: %s\n", texture_path);
            args[0] = val_bool(false);
            return 1;
        }
        SetTextureFilter(g_sprite_texture, TEXTURE_FILTER_POINT);
        g_texture_loaded = true;
    }

    args[0] = val_bool(true);
    return 1;
}

static int nat_close_raylib(VM *, Value *, int)
{
    cleanup_raylib();
    return 0;
}

static int nat_window_should_close(VM *, Value *args, int)
{
    args[0] = val_bool(!g_window_open || WindowShouldClose());
    return 1;
}

static int nat_begin_drawing(VM *, Value *, int)
{
    BeginDrawing();
    return 0;
}

static int nat_end_drawing(VM *, Value *, int)
{
    EndDrawing();
    return 0;
}

static int nat_get_frame_time(VM *, Value *args, int)
{
    args[0] = val_float(GetFrameTime());
    return 1;
}

static int nat_screen_width(VM *, Value *args, int)
{
    args[0] = val_int(GetScreenWidth());
    return 1;
}

static int nat_screen_height(VM *, Value *args, int)
{
    args[0] = val_int(GetScreenHeight());
    return 1;
}

static int nat_mouse_x(VM *, Value *args, int)
{
    args[0] = val_int(GetMouseX());
    return 1;
}

static int nat_mouse_y(VM *, Value *args, int)
{
    args[0] = val_int(GetMouseY());
    return 1;
}

static int nat_mouse_down(VM *, Value *args, int)
{
    args[0] = val_bool(IsMouseButtonDown((int)to_integer(args[0])));
    return 1;
}

static int nat_mouse_pressed(VM *, Value *args, int)
{
    args[0] = val_bool(IsMouseButtonPressed((int)to_integer(args[0])));
    return 1;
}

static int nat_mouse_released(VM *, Value *args, int)
{
    args[0] = val_bool(IsMouseButtonReleased((int)to_integer(args[0])));
    return 1;
}

static int nat_key_down(VM *, Value *args, int)
{
    args[0] = val_bool(IsKeyDown((int)to_integer(args[0])));
    return 1;
}

static int nat_get_fps(VM *, Value *args, int)
{
    args[0] = val_int(GetFPS());
    return 1;
}

static int nat_clear(VM *, Value *args, int)
{
    ClearBackground(color_arg(args, 0));
    return 0;
}

static int nat_sprite_width(VM *, Value *args, int)
{
    args[0] = val_int(g_sprite_texture.width);
    return 1;
}

static int nat_sprite_height(VM *, Value *args, int)
{
    args[0] = val_int(g_sprite_texture.height);
    return 1;
}

static int nat_draw_sprite(VM *, Value *args, int)
{
    float x = (float)to_number(args[0]);
    float y = (float)to_number(args[1]);
    float scale = (float)to_number(args[2]);
    DrawTextureEx(g_sprite_texture, Vector2{x, y}, 0.0f, scale, color_arg(args, 3));
    return 0;
}

static int nat_draw_line(VM *, Value *args, int)
{
    DrawLineEx(Vector2{(float)to_number(args[0]), (float)to_number(args[1])},
               Vector2{(float)to_number(args[2]), (float)to_number(args[3])},
               (float)to_number(args[4]), color_arg(args, 5));
    return 0;
}

static int nat_gui_rect(VM *, Value *args, int)
{
    Rectangle rect{(float)to_number(args[0]), (float)to_number(args[1]),
                   (float)to_number(args[2]), (float)to_number(args[3])};
    bool fill = is_truthy(args[4]);
    Color color = color_arg(args, 5);
    if (fill)
        DrawRectangleRec(rect, color);
    else
        DrawRectangleLinesEx(rect, 1.0f, color);
    return 0;
}

static int nat_gui_circle(VM *, Value *args, int)
{
    int x = (int)to_number(args[0]);
    int y = (int)to_number(args[1]);
    float radius = (float)to_number(args[2]);
    bool fill = is_truthy(args[3]);
    Color color = color_arg(args, 4);
    if (fill)
        DrawCircle(x, y, radius, color);
    else
        DrawCircleLines(x, y, radius, color);
    return 0;
}

static int nat_draw_rect(VM *, Value *args, int)
{
    DrawRectangle((int)to_number(args[0]), (int)to_number(args[1]),
                  (int)to_number(args[2]), (int)to_number(args[3]),
                  color_arg(args, 4));
    return 0;
}

static int nat_draw_text(VM *, Value *args, int)
{
    DrawText(as_cstring(args[0]), (int)to_number(args[1]), (int)to_number(args[2]),
             (int)to_integer(args[3]), color_arg(args, 4));
    return 0;
}

static int nat_text_width(VM *, Value *args, int)
{
    args[0] = val_int(MeasureText(as_cstring(args[0]), (int)to_integer(args[1])));
    return 1;
}

static void register_raylib_bindings(VM &vm)
{
    vm.def_native("init_raylib", nat_init_raylib, 4);
    vm.def_native("close_raylib", nat_close_raylib, 0);
    vm.def_native("window_should_close", nat_window_should_close, 0);
    vm.def_native("begin_drawing", nat_begin_drawing, 0);
    vm.def_native("end_drawing", nat_end_drawing, 0);
    vm.def_native("get_frame_time", nat_get_frame_time, 0);
    vm.def_native("screen_width", nat_screen_width, 0);
    vm.def_native("screen_height", nat_screen_height, 0);
    vm.def_native("mouse_x", nat_mouse_x, 0);
    vm.def_native("mouse_y", nat_mouse_y, 0);
    vm.def_native("mouse_down", nat_mouse_down, 1);
    vm.def_native("mouse_pressed", nat_mouse_pressed, 1);
    vm.def_native("mouse_released", nat_mouse_released, 1);
    vm.def_native("key_down", nat_key_down, 1);
    vm.def_native("get_fps", nat_get_fps, 0);
    vm.def_native("clear", nat_clear, 4);
    vm.def_native("sprite_width", nat_sprite_width, 0);
    vm.def_native("sprite_height", nat_sprite_height, 0);
    vm.def_native("draw_sprite", nat_draw_sprite, 7);
    vm.def_native("draw_line", nat_draw_line, 9);
    vm.def_native("gui_rect", nat_gui_rect, 9);
    vm.def_native("gui_circle", nat_gui_circle, 8);
    vm.def_native("draw_rect", nat_draw_rect, 8);
    vm.def_native("draw_text", nat_draw_text, 8);
    vm.def_native("text_width", nat_text_width, 2);
}

static bool load_and_run(VM &vm, const char *script)
{
    long size = 0;
    char resolved_script[1024] = {0};
    char *source = vm.read_file(script, nullptr, &size, resolved_script, sizeof(resolved_script));
    if (!source)
    {
        fprintf(stderr, "Cannot open: %s\n", script);
        return false;
    }

    Compiler compiler;
    const char *compile_script = resolved_script[0] ? resolved_script : script;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, compile_script);
    free(source);
    if (!fn)
    {
        fprintf(stderr, "Compilation failed: %s\n", compile_script);
        return false;
    }

    vm.run(fn);
    return !vm.had_error();
}

int main(int argc, char *argv[])
{
    const char *script = "scripts/mini_engine_intern.zen";
    if (argc > 1) script = argv[1];

    printf("=== Zen Internal Raylib Loop Test ===\n");
    printf("Script: %s\n", script);

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_time);
    vm.add_search_path(".");
    vm.add_search_path("scripts");
#ifdef ZENVM_SOURCE_DIR
    vm.add_search_path(ZENVM_SOURCE_DIR);
    vm.add_search_path(ZENVM_SOURCE_DIR "/scripts");
#endif
    register_raylib_bindings(vm);

    bool ok = load_and_run(vm, script);
    cleanup_raylib();
    return ok ? 0 : 1;
}
