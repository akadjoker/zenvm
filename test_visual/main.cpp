/*
** test_visual - process benchmark with raylib + Zen VM
**
** C++ side ONLY:
**   1. Loads .zen script (defines processes + spawns them)
**   2. Raylib window loop
**   3. tick_processes() each frame
**   4. for_each_process callback to render
**
** ALL game logic lives in the .zen script.
*/

#include "raylib.h"
#include "zen/vm.h"
#include "zen/compiler.h"
#include "zen/module.h"
#include <cstdio>
#include <cstdlib>

using namespace zen;

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;

/* --- Raylib input bindings for scripts --- */

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

static int nat_mouse_down(VM *vm, Value *args, int)
{
    int btn = (int)to_integer(args[0]);
    args[0] = val_bool(IsMouseButtonDown(btn));
            //  printf("Processes alive: %d\n", vm->num_processes());
    return 1;
}

static int nat_draw_text(VM *, Value *args, int)
{
    const char *text = as_cstring(args[0]);
    int x = (int)to_integer(args[1]);
    int y = (int)to_integer(args[2]);
    int size = (int)to_integer(args[3]);
    Color color = { (unsigned char)to_integer(args[4]),
                    (unsigned char)to_integer(args[5]),
                    (unsigned char)to_integer(args[6]),
                    (unsigned char)to_integer(args[7]) };
    DrawText(text, x, y, size, color);
    return 0;
} 



/* Process hooks */
static int created_total = 0;
static int destroyed_total = 0;

static void on_create(VM *vm, ObjFiber *proc)
{
    (void)vm; (void)proc;
    created_total++;
}

static void on_destroy(VM *vm, ObjFiber *proc)
{
    (void)vm; (void)proc;
    destroyed_total++;
}

/* Render callback - reads process privados (registers) and draws */
static void render_process(VM *vm, ObjFiber *proc, int id, void *userdata)
{
    (void)vm; (void)id; (void)userdata;
    if (proc->frame_count < 1) return;
    ObjFunc *fn = proc->frames[0].func;
    if (!fn || !fn->is_process) return;
    if (fn->arity < 2) return;
    Value *R = proc->frames[0].base;
    int x = (int)to_number(R[0]);
    int y = (int)to_number(R[1]);
    DrawCircle(x, y, 3, WHITE);
}

int main(int argc, char *argv[])
{
    const char *script = "/media/ctw04578/data/projects/zen/zen/test_visual/bunnymark.zen";
    if (argc > 1) script = argv[1];

    printf("=== Zen Process Benchmark ===\n");
    printf("Script: %s\n", script);

    VM *vmp = new VM();
    VM &vm = *vmp;

    /* Register standard libraries */
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_time);

    /* Register raylib input bindings */
    vm.def_native("mouse_x", nat_mouse_x, 0);
    vm.def_native("mouse_y", nat_mouse_y, 0);
    vm.def_native("mouse_down", nat_mouse_down, 1);
    vm.def_native("draw_text", nat_draw_text, 8);

    /* Init window before script so input functions work */
    InitWindow(SCREEN_W, SCREEN_H, "Zen Process Benchmark");
    SetTargetFPS(0);

    /* Process hooks */
    vm.on_process_start = on_create;
    vm.on_process_end = on_destroy;

    /* Run the script - defines processes and spawns them */
    long size = 0;
    char *source = vm.read_file(script, nullptr, &size);
    if (!source)
    {
        fprintf(stderr, "Cannot open: %s\n", script);
        CloseWindow();
        delete vmp;
        return 1;
    }

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, script);
    free(source);
    if (!fn)
    {
        fprintf(stderr, "Compilation failed.\n");
        CloseWindow();
        delete vmp;
        return 1;
    }
    vm.run(fn);

    printf("Processes alive: %d\n", vm.num_processes());

    while (!WindowShouldClose())
    {
        vm.tick_processes(GetFrameTime());

        BeginDrawing();
        ClearBackground({20, 20, 30, 255});

        vm.for_each_process(render_process);


        DrawFPS(10, 10);
        char buf[128];
        snprintf(buf, sizeof(buf), "Alive: %d", vm.num_processes());
        DrawText(buf, 10, 40, 20, GREEN);
        snprintf(buf, sizeof(buf), "Created: %d  Destroyed: %d", created_total, destroyed_total);
        DrawText(buf, 10, 65, 20, YELLOW);
        EndDrawing();
    }

    CloseWindow();
    delete vmp;
    return 0;
}
