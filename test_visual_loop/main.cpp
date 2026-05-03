/*
** test_visual_loop - script-driven raylib mini loop, without VM processes.
**
** The script owns init/update/draw and regular arrays/structs. C++ only hosts
** the window and exposes small drawing/input bindings.
*/

#include "raylib.h"
#include "zen/compiler.h"
#include "zen/module.h"
#include "zen/vm.h"
#include <cstdio>
#include <cstdlib>

using namespace zen;

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;
static Texture2D g_sprite_texture = Texture2D{0, 0, 0, 0, 0};

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

static int nat_draw_circle(VM *, Value *args, int)
{
    DrawCircleV(Vector2{(float)to_number(args[0]), (float)to_number(args[1])},
                (float)to_number(args[2]), color_arg(args, 3));
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
    Color tint = color_arg(args, 3);
    DrawTextureEx(g_sprite_texture, Vector2{x, y}, 0.0f, scale, tint);
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
    const char *text = as_cstring(args[0]);
    DrawText(text, (int)to_number(args[1]), (int)to_number(args[2]),
             (int)to_integer(args[3]), color_arg(args, 4));
    return 0;
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

static void register_raylib_bindings(VM &vm)
{
    vm.def_native("screen_width", nat_screen_width, 0);
    vm.def_native("screen_height", nat_screen_height, 0);
    vm.def_native("mouse_x", nat_mouse_x, 0);
    vm.def_native("mouse_y", nat_mouse_y, 0);
    vm.def_native("mouse_down", nat_mouse_down, 1);
    vm.def_native("key_down", nat_key_down, 1);
    vm.def_native("get_fps", nat_get_fps, 0);
    vm.def_native("clear", nat_clear, 4);
    vm.def_native("draw_circle", nat_draw_circle, 7);
    vm.def_native("sprite_width", nat_sprite_width, 0);
    vm.def_native("sprite_height", nat_sprite_height, 0);
    vm.def_native("draw_sprite", nat_draw_sprite, 7);
    vm.def_native("draw_rect", nat_draw_rect, 8);
    vm.def_native("draw_text", nat_draw_text, 8);
}

static bool load_sprite_texture(const char *path)
{
    g_sprite_texture = LoadTexture(path);
    if (g_sprite_texture.id == 0)
    {
        fprintf(stderr, "Cannot load texture: %s\n", path);
        return false;
    }
    SetTextureFilter(g_sprite_texture, TEXTURE_FILTER_POINT);
    return true;
}

int main(int argc, char *argv[])
{
    const char *script = "scripts/mini_engine.zen";
    const char *texture_path =
#ifdef ZENVM_SOURCE_DIR
        ZENVM_SOURCE_DIR "/assets/wabbit_alpha.png";
#else
        "assets/wabbit_alpha.png";
#endif
    if (argc > 1) script = argv[1];
    if (argc > 2) texture_path = argv[2];

    printf("=== Zen Visual Loop Test ===\n");
    printf("Script: %s\n", script);
    printf("Texture: %s\n", texture_path);

    VM *vmp = new VM();
    VM &vm = *vmp;
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

    InitWindow(SCREEN_W, SCREEN_H, "Zen Visual Loop - no processes");
    SetTargetFPS(0);

    if (!load_sprite_texture(texture_path))
    {
        CloseWindow();
        delete vmp;
        return 1;
    }

    if (!load_and_run(vm, script))
    {
        UnloadTexture(g_sprite_texture);
        CloseWindow();
        delete vmp;
        return 1;
    }

    int init_idx = vm.find_global("init");
    int update_idx = vm.find_global("update");
    int draw_idx = vm.find_global("draw");
    if (update_idx < 0 || draw_idx < 0)
    {
        fprintf(stderr, "Script must define update(dt) and draw().\n");
        UnloadTexture(g_sprite_texture);
        CloseWindow();
        delete vmp;
        return 1;
    }

    if (init_idx >= 0)
        vm.call_global(init_idx, nullptr, 0);

    while (!WindowShouldClose() && !vm.had_error())
    {
        Value dt = val_float(GetFrameTime());
        vm.call_global(update_idx, &dt, 1);
        if (vm.had_error()) break;

        BeginDrawing();
        vm.call_global(draw_idx, nullptr, 0);
        EndDrawing();
    }

    bool failed = vm.had_error();
    UnloadTexture(g_sprite_texture);
    CloseWindow();
    delete vmp;
    return failed ? 1 : 0;
}
