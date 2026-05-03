#include "raylib.h"
#include "rlgl.h"
#include "zen/vm.h"
#include "zen/compiler.h"
#include "zen/module.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace zen;

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;

enum class RenderKind
{
    None,
    Player,
    Bullet,
};

struct EngineComponent
{
    Value instance;
    int update_slot;
    RenderKind render_kind;
};

struct EngineObject
{
    Value go;
    Value transform;
    std::vector<EngineComponent> components;
    bool alive;
};

static VM *g_vm = nullptr;
static ObjClass *g_game_object_class = nullptr;
static ObjClass *g_transform_class = nullptr;
static int g_update_slot = -1;
static std::vector<EngineObject> g_objects;
static Texture2D g_player_texture = {};
static Texture2D g_bullet_texture = {};
static std::vector<Vector2> g_bullet_draw_positions;
static char g_status_text[256] = "F9 hot reload";

static bool is_component_instance(Value v)
{
    return is_instance(v) && as_instance(v) != nullptr;
}

static const char *engine_value_type(Value v)
{
    if (is_nil(v)) { return "nil"; }
    if (is_bool(v)) { return "bool"; }
    if (is_int(v)) { return "int"; }
    if (is_float(v)) { return "float"; }
    if (is_string(v)) { return "string"; }
    if (is_class(v)) { return "class"; }
    if (is_instance(v)) { return as_instance(v)->klass->name->chars; }
    if (is_obj(v)) { return "object"; }
    if (is_ptr(v)) { return "ptr"; }
    return "unknown";
}

static RenderKind render_kind_for_component(Value component)
{
    if (!is_instance(component))
    {
        return RenderKind::None;
    }

    const char *name = as_instance(component)->klass->name->chars;
    if (std::strcmp(name, "Player") == 0)
    {
        return RenderKind::Player;
    }
    if (std::strcmp(name, "Bullet") == 0)
    {
        return RenderKind::Bullet;
    }
    return RenderKind::None;
}

static bool class_is_or_derives_from(ObjClass *klass, ObjClass *wanted)
{
    for (ObjClass *cur = klass; cur; cur = cur->parent)
    {
        if (cur == wanted)
        {
            return true;
        }
    }
    return false;
}

static EngineObject *find_engine_object_by_go(Value go)
{
    if (!is_instance(go))
    {
        return nullptr;
    }

    for (size_t i = 0; i < g_objects.size(); ++i)
    {
        if (g_objects[i].alive && is_instance(g_objects[i].go) &&
            g_objects[i].go.as.obj == go.as.obj)
        {
            return &g_objects[i];
        }
    }
    return nullptr;
}

static Value find_component_on_object(EngineObject *obj, ObjClass *wanted)
{
    if (!obj || !wanted)
    {
        return val_nil();
    }

    if (wanted == g_transform_class)
    {
        return obj->transform;
    }

    for (size_t i = 0; i < obj->components.size(); ++i)
    {
        Value component = obj->components[i].instance;
        if (!is_instance(component))
        {
            continue;
        }

        ObjClass *klass = as_instance(component)->klass;
        if (class_is_or_derives_from(klass, wanted))
        {
            return component;
        }
    }
    return val_nil();
}

static EngineComponent make_engine_component(VM &vm, Value component)
{
    EngineComponent result;
    result.instance = component;
    result.update_slot = -1;
    result.render_kind = render_kind_for_component(component);

    if (is_instance(component))
    {
        ObjClass *klass = as_instance(component)->klass;
        if (g_update_slot >= 0 && g_update_slot < klass->vtable_size &&
            !is_nil(klass->vtable[g_update_slot]))
        {
            result.update_slot = g_update_slot;
        }
        else
        {
            int slot = vm.find_selector("update", 6);
            if (slot >= 0 && slot < klass->vtable_size && !is_nil(klass->vtable[slot]))
            {
                result.update_slot = slot;
            }
        }
    }

    return result;
}

static Texture2D make_circle_texture(int radius, Color color)
{
    if (radius < 1)
        radius = 1;
    const int size = radius * 2 + 1;
    Image image = GenImageColor(size, size, BLANK);
    ImageDrawCircle(&image, radius, radius, radius, color);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static void render_engine_object(const EngineObject &obj)
{
    if (!obj.alive || !is_instance(obj.transform))
    {
        return;
    }

    ObjInstance *transform = as_instance(obj.transform);
    const int x = (int)to_number(transform->fields[0]);
    const int y = (int)to_number(transform->fields[1]);

    for (size_t i = 0; i < obj.components.size(); ++i)
    {
        switch (obj.components[i].render_kind)
        {
        case RenderKind::Player:
            DrawTexture(g_player_texture, x - 14, y - 14, WHITE);
            DrawText("Player", x - 22, y - 30, 16, {255, 255, 255, 255});
            break;
        case RenderKind::Bullet:
            g_bullet_draw_positions.push_back({(float)(x - 6), (float)(y - 6)});
            break;
        case RenderKind::None:
            break;
        }
    }
}

static void flush_bullet_batch()
{
    if (g_bullet_draw_positions.empty() || g_bullet_texture.id == 0)
    {
        return;
    }

    const float w = (float)g_bullet_texture.width;
    const float h = (float)g_bullet_texture.height;

    rlSetTexture(g_bullet_texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    for (size_t i = 0; i < g_bullet_draw_positions.size(); ++i)
    {
        rlCheckRenderBatchLimit(4);

        const float x = g_bullet_draw_positions[i].x;
        const float y = g_bullet_draw_positions[i].y;

        rlTexCoord2f(0.0f, 0.0f); rlVertex2f(x, y);
        rlTexCoord2f(0.0f, 1.0f); rlVertex2f(x, y + h);
        rlTexCoord2f(1.0f, 1.0f); rlVertex2f(x + w, y + h);
        rlTexCoord2f(1.0f, 0.0f); rlVertex2f(x + w, y);
    }
    rlEnd();
    rlSetTexture(0);

    g_bullet_draw_positions.clear();
}

static void destroy_engine_object(VM &vm, EngineObject &obj)
{
    obj.alive = false;

    for (size_t i = 0; i < obj.components.size(); ++i)
    {
        if (is_instance(obj.components[i].instance))
        {
            ObjInstance *comp = as_instance(obj.components[i].instance);
            comp->fields[0] = val_nil(); /* gameObject */
            comp->fields[1] = val_nil(); /* transform */
            vm.destroy_instance(obj.components[i].instance);
        }
    }
    obj.components.clear();

    if (is_instance(obj.transform))
    {
        vm.destroy_instance(obj.transform);
        obj.transform = val_nil();
    }
    if (is_instance(obj.go))
    {
        vm.destroy_instance(obj.go);
        obj.go = val_nil();
    }
}

static void sweep_dead_objects(VM &vm)
{
    for (size_t i = 0; i < g_objects.size();)
    {
        if (g_objects[i].alive)
        {
            ++i;
            continue;
        }

        destroy_engine_object(vm, g_objects[i]);

        const size_t last = g_objects.size() - 1;
        if (i != last)
        {
            g_objects[i] = std::move(g_objects[last]);
        }
        g_objects.pop_back();
    }
}

/* -------------------- Native class methods -------------------- */

static int nat_transform_init(VM *, Value *args, int nargs)
{
    ObjInstance *self = as_instance(args[0]);
    self->fields[0] = (nargs > 1) ? args[1] : val_float(0.0); /* x */
    self->fields[1] = (nargs > 2) ? args[2] : val_float(0.0); /* y */
    args[0] = val_obj((Obj *)self);
    return 1;
}

static int nat_transform_translate(VM *, Value *args, int)
{
    ObjInstance *self = as_instance(args[0]);
    self->fields[0] = val_float(to_number(self->fields[0]) + to_number(args[1]));
    self->fields[1] = val_float(to_number(self->fields[1]) + to_number(args[2]));
    return 0;
}

static int nat_transform_get_x(VM *, Value *args, int)
{
    ObjInstance *self = as_instance(args[0]);
    args[0] = self->fields[0];
    return 1;
}

static int nat_transform_get_y(VM *, Value *args, int)
{
    ObjInstance *self = as_instance(args[0]);
    args[0] = self->fields[1];
    return 1;
}

static int nat_game_object_init(VM *, Value *args, int nargs)
{
    ObjInstance *self = as_instance(args[0]);
    self->fields[0] = (nargs > 1) ? args[1] : val_nil(); /* name */
    self->fields[1] = val_nil();                         /* transform */
    args[0] = val_obj((Obj *)self);
    return 1;
}

static int nat_game_object_get_transform(VM *, Value *args, int)
{
    if (!is_instance(args[0]))
    {
        args[0] = val_nil();
        return 1;
    }
    ObjInstance *self = as_instance(args[0]);
    args[0] = self->fields[1];
    return 1;
}

static int nat_game_object_get_component(VM *vm, Value *args, int nargs)
{
    if (nargs != 2 || !is_instance(args[0]) || !is_class(args[1]))
    {
        vm->runtime_error("GameObject.getComponent<T>() expects a component type");
        args[0] = val_nil();
        return 1;
    }

    ObjInstance *self = as_instance(args[0]);
    if (self->klass != g_game_object_class)
    {
        args[0] = val_nil();
        return 1;
    }

    args[0] = find_component_on_object(find_engine_object_by_go(args[0]), as_class(args[1]));
    return 1;
}

static int nat_script_component_get_transform(VM *vm, Value *args, int)
{
    (void)vm;
    if (!is_instance(args[0]))
    {
        args[0] = val_nil();
        return 1;
    }

    ObjInstance *self = as_instance(args[0]);
    Value cached_transform = self->fields[1]; /* transform */
    if (is_instance(cached_transform))
    {
        args[0] = cached_transform;
        return 1;
    }

    Value go = self->fields[0]; /* gameObject */
    if (!is_instance(go))
    {
        args[0] = val_nil();
        return 1;
    }

    ObjInstance *go_inst = as_instance(go);
    if (go_inst->klass != g_game_object_class || go_inst->klass->num_fields < 2)
    {
        args[0] = val_nil();
        return 1;
    }

    args[0] = go_inst->fields[1];
    return 1;
}

static int nat_script_component_get_component(VM *vm, Value *args, int nargs)
{
    if (nargs != 2 || !is_instance(args[0]) || !is_class(args[1]))
    {
        vm->runtime_error("ScriptComponent.getComponent<T>() expects a component type");
        args[0] = val_nil();
        return 1;
    }

    ObjInstance *self = as_instance(args[0]);
    if (self->klass->num_fields < 1)
    {
        args[0] = val_nil();
        return 1;
    }

    Value go = self->fields[0]; /* gameObject */
    args[0] = find_component_on_object(find_engine_object_by_go(go), as_class(args[1]));
    return 1;
}

/* -------------------- Native functions -------------------- */

static int nat_create_instance(VM *vm, Value *args, int nargs)
{
    if (nargs != 1)
    {
        vm->runtime_error("createInstance(className) does not accept arguments in fake_engine");
        args[0] = val_nil();
        return 1;
    }

    const char *class_name = as_cstring(args[0]);
    int idx = vm->find_global(class_name);
    if (idx < 0)
    {
        vm->runtime_error("createInstance: class '%s' not found", class_name);
        args[0] = val_nil();
        return 1;
    }

    Value cls_val = vm->get_global(idx);
    if (!is_class(cls_val))
    {
        vm->runtime_error("createInstance: '%s' is not a class", class_name);
        args[0] = val_nil();
        return 1;
    }

    /* Engine-owned components are kept in C++ vectors, not VM roots.
       Keep them persistent to avoid GC collecting them mid-frame. */
    ObjClass *klass = as_class(cls_val);
    klass->persistent = true;

    args[0] = vm->make_instance(klass, nullptr, 0);
    return 1;
}

static int nat_create_game_object(VM *vm, Value *args, int nargs)
{
    if (!g_game_object_class || !g_transform_class)
    {
        vm->runtime_error("engine classes are not initialized");
        args[0] = val_nil();
        return 1;
    }

    if (nargs > 3 && !is_instance(args[3]))
    {
        vm->runtime_error("createGameObject expects ScriptComponent instance as argument 4, got %s",
                          engine_value_type(args[3]));
        args[0] = val_nil();
        return 1;
    }

    Value ctor_args[1] = { (nargs > 0) ? args[0] : val_nil() };
    Value go = vm->make_instance(g_game_object_class, ctor_args, 1);

    Value tx_args[2] = { (nargs > 1) ? args[1] : val_float(0.0),
                         (nargs > 2) ? args[2] : val_float(0.0) };
    Value transform = vm->make_instance(g_transform_class, tx_args, 2);

    ObjInstance *go_inst = as_instance(go);
    go_inst->fields[1] = transform;

    if (nargs > 3 && is_instance(args[3]))
    {
        ObjInstance *comp = as_instance(args[3]);
        comp->fields[0] = go; /* ScriptComponent.gameObject */
        comp->fields[1] = transform; /* ScriptComponent.transform */
    }

    EngineObject obj;
    obj.go = go;
    obj.transform = transform;
    obj.alive = true;
    if (nargs > 3 && is_instance(args[3]))
    {
        obj.components.push_back(make_engine_component(*vm, args[3]));
    }
    g_objects.push_back(obj);

    args[0] = go;
    return 1;
}

static int nat_destroy_game_object(VM *, Value *args, int)
{
    Value go = args[0];
    for (size_t i = 0; i < g_objects.size(); ++i)
    {
        if (g_objects[i].alive && is_instance(g_objects[i].go) &&
            g_objects[i].go.as.obj == go.as.obj)
        {
            g_objects[i].alive = false;
            break;
        }
    }
    return 0;
}

static int nat_object_count(VM *, Value *args, int)
{
    args[0] = val_int((int)g_objects.size());
    return 1;
}

static int nat_key_down(VM *, Value *args, int)
{
    args[0] = val_bool(IsKeyDown((int)to_integer(args[0])));
    return 1;
}

static int nat_key_pressed(VM *, Value *args, int)
{
    args[0] = val_bool(IsKeyPressed((int)to_integer(args[0])));
    return 1;
}

static int nat_draw_circle(VM *, Value *args, int)
{
    DrawCircle((int)to_number(args[0]), (int)to_number(args[1]), (float)to_number(args[2]),
               { (unsigned char)to_integer(args[3]),
                 (unsigned char)to_integer(args[4]),
                 (unsigned char)to_integer(args[5]),
                 (unsigned char)to_integer(args[6]) });
    return 0;
}

static int nat_draw_text(VM *, Value *args, int)
{
    DrawText(as_cstring(args[0]), (int)to_number(args[1]), (int)to_number(args[2]), (int)to_number(args[3]),
             { (unsigned char)to_integer(args[4]),
               (unsigned char)to_integer(args[5]),
               (unsigned char)to_integer(args[6]),
               (unsigned char)to_integer(args[7]) });
    return 0;
}

/* -------------------- Engine loop -------------------- */

static void register_engine_api(VM &vm)
{
    g_transform_class = vm.def_class("Transform")
        .field("x")
        .field("y")
        .method("init", nat_transform_init, 2)
        .method("translate", nat_transform_translate, 2)
        .method("get_x", nat_transform_get_x, 0)
        .method("get_y", nat_transform_get_y, 0)
        .persistent(true)
        .constructable(false)
        .end();

    g_game_object_class = vm.def_class("GameObject")
        .field("name")
        .field("transform")
        .method("init", nat_game_object_init, 1)
        .method("get_transform", nat_game_object_get_transform, 0)
        .method("getComponent", nat_game_object_get_component, 1)
        .persistent(true)
        .constructable(false)
        .end();

    vm.def_class("ScriptComponent")
        .field("gameObject")
        .field("transform")
        .method("get_transform", nat_script_component_get_transform, 0)
        .method("getComponent", nat_script_component_get_component, 1)
        .persistent(true)
        .constructable(false)
        .end();

    vm.def_native("createInstance", nat_create_instance, 1);
    vm.def_native("createGameObject", nat_create_game_object, 4);
    vm.def_native("destroyGameObject", nat_destroy_game_object, 1);
    vm.def_native("objectCount", nat_object_count, 0);

    vm.def_native("key_down", nat_key_down, 1);
    vm.def_native("key_pressed", nat_key_pressed, 1);
    vm.def_native("draw_circle", nat_draw_circle, 7);
    vm.def_native("draw_text", nat_draw_text, 8);

    g_update_slot = vm.find_selector("update", 6);
}

static void configure_vm(VM &vm)
{
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);
    vm.add_search_path(".");
    vm.add_search_path("scripts");
#ifdef ZENVM_SOURCE_DIR
    vm.add_search_path(ZENVM_SOURCE_DIR);
    vm.add_search_path(ZENVM_SOURCE_DIR "/scripts");
#endif

    register_engine_api(vm);
}

static ObjFunc *compile_script(VM &vm, const char *script)
{
    long size = 0;
    char resolved_script[1024] = {0};
    char *source = vm.read_file(script, nullptr, &size, resolved_script, sizeof(resolved_script));
    if (!source)
    {
        std::fprintf(stderr, "Cannot open: %s\n", script);
        return nullptr;
    }

    Compiler compiler;
    const char *compile_script_path = resolved_script[0] ? resolved_script : script;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, compile_script_path);
    std::free(source);
    if (!fn)
    {
        std::fprintf(stderr, "Compilation failed.\n");
        return nullptr;
    }

    return fn;
}

static void destroy_all_objects(VM &vm)
{
    for (size_t i = 0; i < g_objects.size(); ++i)
    {
        destroy_engine_object(vm, g_objects[i]);
    }
    g_objects.clear();
    g_bullet_draw_positions.clear();
}

static bool reload_script(VM *&vm, const char *script)
{
    if (vm)
    {
        destroy_all_objects(*vm);
        delete vm;
        vm = nullptr;
    }

    g_game_object_class = nullptr;
    g_transform_class = nullptr;
    g_update_slot = -1;

    vm = new VM();
    g_vm = vm;
    configure_vm(*vm);

    ObjFunc *fn = compile_script(*vm, script);
    if (!fn)
    {
        std::snprintf(g_status_text, sizeof(g_status_text), "F9 reload failed: compile error");
        return false;
    }

    vm->run(fn);

    if (vm->had_error())
    {
        std::fprintf(stderr, "Hot reload failed while running: %s\n", script);
        std::snprintf(g_status_text, sizeof(g_status_text), "F9 reload failed: runtime error");
        return false;
    }

    std::printf("Hot reloaded: %s\n", script);
    std::snprintf(g_status_text, sizeof(g_status_text), "Hot reloaded: %s", script);
    return true;
}

int main(int argc, char *argv[])
{
    const char *script = "../tests/fake_engine.zen";
    if (argc > 1)
    {
        script = argv[1];
    }

    VM *vm = nullptr;
    if (!reload_script(vm, script))
    {
        return 1;
    }

    InitWindow(SCREEN_W, SCREEN_H, "Zen Fake Engine - Script <-> C++");
    SetTargetFPS(60);
    g_player_texture = make_circle_texture(14, {80, 220, 160, 255});
    g_bullet_texture = make_circle_texture(6, {255, 210, 70, 255});

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F9))
        {
            reload_script(vm, script);
        }

        const double frame_start = GetTime();
        const float dt = GetFrameTime();

        double update_ms = 0.0;
        if (!vm->had_error())
        {
            const double update_start = GetTime();
            for (size_t i = 0; i < g_objects.size(); ++i)
            {
                if (!g_objects[i].alive) { continue; }
                for (size_t j = 0; j < g_objects[i].components.size(); ++j)
                {
                    EngineComponent &component = g_objects[i].components[j];
                    if (component.update_slot < 0 || !is_component_instance(component.instance)) { continue; }
                    Value dt_arg[1] = { val_float(dt) };
                    vm->invoke(component.instance, component.update_slot, dt_arg, 1);
                    if (vm->had_error()) { break; }
                }
                if (vm->had_error()) { break; }
            }
            update_ms = (GetTime() - update_start) * 1000.0;
            if (vm->had_error())
            {
                std::snprintf(g_status_text, sizeof(g_status_text), "Runtime error - fix script and press F9");
            }
        }

        const double sweep_start = GetTime();
        if (!vm->had_error())
        {
            sweep_dead_objects(*vm);
        }
        const double sweep_ms = (GetTime() - sweep_start) * 1000.0;

        const double render_start = GetTime();
        BeginDrawing();
        ClearBackground({22, 26, 32, 255});

        for (size_t i = 0; i < g_objects.size(); ++i)
        {
            render_engine_object(g_objects[i]);
        }
        flush_bullet_batch();
        const double render_ms = (GetTime() - render_start) * 1000.0;
        const double frame_ms = (GetTime() - frame_start) * 1000.0;

        DrawFPS(10, 10);
        DrawText("WASD move player, SPACE shoot, F9 hot reload", 10, 34, 20, {220, 220, 220, 255});
        char object_buf[64];
        std::snprintf(object_buf, sizeof(object_buf), "Objects: %zu", g_objects.size());
        DrawText(object_buf, 10, 58, 20, {220, 220, 220, 255});
        char perf_buf[160];
        std::snprintf(perf_buf, sizeof(perf_buf), "script update %.2fms | sweep %.2fms | render %.2fms | frame %.2fms",
                      update_ms, sweep_ms, render_ms, frame_ms);
        DrawText(perf_buf, 10, 82, 18, {255, 210, 120, 255});
        DrawText(g_status_text, 10, 106, 18, vm->had_error() ? RED : Color{140, 220, 160, 255});
        EndDrawing();
    }

    if (vm)
    {
        destroy_all_objects(*vm);
        delete vm;
    }

    UnloadTexture(g_bullet_texture);
    UnloadTexture(g_player_texture);
    CloseWindow();
    return 0;
}
