#include "zen/module_raylib.h"

#ifdef ZEN_ENABLE_RAYLIB

#include "object.h"
#include "vm.h"
#include <raylib.h>
#include <cstring>

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    /* =========================================================
    ** NativeStructDef pointers — set during init_fn, used by
    ** native functions to allocate / validate struct instances.
    ** No GC dtors: Unload* must be called explicitly before
    ** CloseWindow/CloseAudioDevice (same as C Raylib usage).
    ** ========================================================= */
    static NativeStructDef *g_def_texture   = nullptr;
    static NativeStructDef *g_def_image     = nullptr;
    static NativeStructDef *g_def_sound     = nullptr;
    static NativeStructDef *g_def_rendertex = nullptr;

    /* =========================================================
    ** Type-check helpers — runtime_error if wrong type
    ** ========================================================= */
    static inline Texture2D *tex_ptr(VM *vm, Value v, const char *fn)
    {
        if (!val_is_native_struct(v) || as_native_struct(v)->def != g_def_texture)
        { vm->runtime_error("%s: expected Texture2D.", fn); return nullptr; }
        return zen_struct_ptr(v, Texture2D);
    }
    static inline Image *img_ptr(VM *vm, Value v, const char *fn)
    {
        if (!val_is_native_struct(v) || as_native_struct(v)->def != g_def_image)
        { vm->runtime_error("%s: expected Image.", fn); return nullptr; }
        return zen_struct_ptr(v, Image);
    }
    static inline Sound *snd_ptr(VM *vm, Value v, const char *fn)
    {
        if (!val_is_native_struct(v) || as_native_struct(v)->def != g_def_sound)
        { vm->runtime_error("%s: expected Sound.", fn); return nullptr; }
        return zen_struct_ptr(v, Sound);
    }
    static inline RenderTexture2D *rtex_ptr(VM *vm, Value v, const char *fn)
    {
        if (!val_is_native_struct(v) || as_native_struct(v)->def != g_def_rendertex)
        { vm->runtime_error("%s: expected RenderTexture2D.", fn); return nullptr; }
        return zen_struct_ptr(v, RenderTexture2D);
    }

    /* =========================================================
    ** Inline helpers
    ** ========================================================= */
    static inline float fval(Value v) { return is_int(v) ? (float)v.as.integer : (float)v.as.number; }
    static inline int   ival(Value v) { return is_int(v) ? (int)v.as.integer : (int)v.as.number; }
    static inline Color color4(Value *a, int b)
    {
        return { (unsigned char)ival(a[b]), (unsigned char)ival(a[b+1]),
                 (unsigned char)ival(a[b+2]), (unsigned char)ival(a[b+3]) };
    }
    static inline const char *str_arg(VM *vm, Value v, const char *fn)
    {
        if (!is_obj(v) || v.as.obj->type != OBJ_STRING)
        {
            vm->runtime_error("%s: expected string.", fn);
            return nullptr;
        }
        return ((ObjString *)v.as.obj)->chars;
    }

    /* =========================================================
    ** Window
    ** ========================================================= */
    static int nat_InitWindow(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3) { vm->runtime_error("InitWindow(width, height, title)"); return 0; }
        const char *title = str_arg(vm, args[2], "InitWindow"); if (!title) return 0;
        InitWindow(ival(args[0]), ival(args[1]), title);
        return 0;
    }
    static int nat_CloseWindow(VM *, Value *, int)           { CloseWindow(); return 0; }
    static int nat_WindowShouldClose(VM *, Value *args, int) { args[0] = val_bool(WindowShouldClose()); return 1; }
    static int nat_IsWindowReady(VM *, Value *args, int)     { args[0] = val_bool(IsWindowReady()); return 1; }
    static int nat_IsWindowFullscreen(VM *, Value *args, int){ args[0] = val_bool(IsWindowFullscreen()); return 1; }
    static int nat_ToggleFullscreen(VM *, Value *, int)      { ToggleFullscreen(); return 0; }
    static int nat_SetWindowSize(VM *, Value *args, int)     { SetWindowSize(ival(args[0]), ival(args[1])); return 0; }
    static int nat_SetWindowTitle(VM *vm, Value *args, int)
    {
        const char *t = str_arg(vm, args[0], "SetWindowTitle"); if (!t) return 0;
        SetWindowTitle(t); return 0;
    }
    static int nat_GetScreenWidth(VM *, Value *args, int)  { args[0] = val_int(GetScreenWidth()); return 1; }
    static int nat_GetScreenHeight(VM *, Value *args, int) { args[0] = val_int(GetScreenHeight()); return 1; }
    static int nat_SetTargetFPS(VM *, Value *args, int)    { SetTargetFPS(ival(args[0])); return 0; }
    static int nat_GetFPS(VM *, Value *args, int)          { args[0] = val_int(GetFPS()); return 1; }
    static int nat_GetFrameTime(VM *, Value *args, int)    { args[0] = val_float(GetFrameTime()); return 1; }
    static int nat_GetTime(VM *, Value *args, int)         { args[0] = val_float((float)GetTime()); return 1; }

    /* =========================================================
    ** Drawing
    ** ========================================================= */
    static int nat_BeginDrawing(VM *, Value *, int)          { BeginDrawing(); return 0; }
    static int nat_EndDrawing(VM *, Value *, int)            { EndDrawing(); return 0; }
    static int nat_ClearBackground(VM *, Value *args, int)   { ClearBackground(color4(args, 0)); return 0; }

    static int nat_DrawRectangle(VM *, Value *args, int)
    {
        DrawRectangle(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]), color4(args, 4));
        return 0;
    }
    static int nat_DrawRectangleF(VM *, Value *args, int)
    {
        DrawRectangleV({fval(args[0]), fval(args[1])}, {fval(args[2]), fval(args[3])}, color4(args, 4));
        return 0;
    }
    static int nat_DrawRectangleLines(VM *, Value *args, int)
    {
        DrawRectangleLines(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]), color4(args, 4));
        return 0;
    }
    static int nat_DrawRectangleLinesEx(VM *, Value *args, int)
    {
        Rectangle r = {fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])};
        DrawRectangleLinesEx(r, fval(args[4]), color4(args, 5));
        return 0;
    }
    static int nat_DrawCircle(VM *, Value *args, int)
    {
        DrawCircle(ival(args[0]), ival(args[1]), fval(args[2]), color4(args, 3));
        return 0;
    }
    static int nat_DrawCircleV(VM *, Value *args, int)
    {
        DrawCircleV({fval(args[0]), fval(args[1])}, fval(args[2]), color4(args, 3));
        return 0;
    }
    static int nat_DrawCircleLines(VM *, Value *args, int)
    {
        DrawCircleLines(ival(args[0]), ival(args[1]), fval(args[2]), color4(args, 3));
        return 0;
    }
    static int nat_DrawLine(VM *, Value *args, int)
    {
        DrawLine(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]), color4(args, 4));
        return 0;
    }
    static int nat_DrawLineV(VM *, Value *args, int)
    {
        DrawLineV({fval(args[0]), fval(args[1])}, {fval(args[2]), fval(args[3])}, color4(args, 4));
        return 0;
    }
    static int nat_DrawLineEx(VM *, Value *args, int)
    {
        DrawLineEx({fval(args[0]), fval(args[1])}, {fval(args[2]), fval(args[3])},
                   fval(args[4]), color4(args, 5));
        return 0;
    }
    static int nat_DrawText(VM *vm, Value *args, int)
    {
        const char *t = str_arg(vm, args[0], "DrawText"); if (!t) return 0;
        DrawText(t, ival(args[1]), ival(args[2]), ival(args[3]), color4(args, 4));
        return 0;
    }
    static int nat_DrawFPS(VM *, Value *args, int) { DrawFPS(ival(args[0]), ival(args[1])); return 0; }
    static int nat_DrawPixel(VM *, Value *args, int)
    {
        DrawPixel(ival(args[0]), ival(args[1]), color4(args, 2));
        return 0;
    }
    static int nat_DrawTriangle(VM *, Value *args, int)
    {
        DrawTriangle({fval(args[0]), fval(args[1])},
                     {fval(args[2]), fval(args[3])},
                     {fval(args[4]), fval(args[5])},
                     color4(args, 6));
        return 0;
    }

    /* =========================================================
    ** Texture
    ** ========================================================= */
    static int nat_LoadTexture(VM *vm, Value *args, int)
    {
        const char *path = str_arg(vm, args[0], "LoadTexture"); if (!path) return 0;
        Texture2D t = LoadTexture(path);
        args[0] = vm->make_native_struct(g_def_texture, nullptr, 0);
        *zen_struct_ptr(args[0], Texture2D) = t;
        return 1;
    }
    static int nat_LoadTextureFromImage(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "LoadTextureFromImage"); if (!img) return 0;
        Texture2D t = LoadTextureFromImage(*img);
        args[0] = vm->make_native_struct(g_def_texture, nullptr, 0);
        *zen_struct_ptr(args[0], Texture2D) = t;
        return 1;
    }
    static int nat_UnloadTexture(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "UnloadTexture"); if (!t) return 0;
        UnloadTexture(*t);
        memset(t, 0, sizeof(Texture2D));
        return 0;
    }
    static int nat_DrawTexture(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "DrawTexture"); if (!t) return 0;
        DrawTexture(*t, ival(args[1]), ival(args[2]), color4(args, 3));
        return 0;
    }
    static int nat_DrawTextureV(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "DrawTextureV"); if (!t) return 0;
        DrawTextureV(*t, {fval(args[1]), fval(args[2])}, color4(args, 3));
        return 0;
    }
    static int nat_DrawTextureEx(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "DrawTextureEx"); if (!t) return 0;
        DrawTextureEx(*t, {fval(args[1]), fval(args[2])}, fval(args[3]), fval(args[4]), color4(args, 5));
        return 0;
    }
    static int nat_DrawTextureRec(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "DrawTextureRec"); if (!t) return 0;
        Rectangle src = {fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4])};
        DrawTextureRec(*t, src, {fval(args[5]), fval(args[6])}, color4(args, 7));
        return 0;
    }
    static int nat_DrawTexturePro(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "DrawTexturePro"); if (!t) return 0;
        Rectangle src = {fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4])};
        Rectangle dst = {fval(args[5]), fval(args[6]), fval(args[7]), fval(args[8])};
        Vector2 origin = {fval(args[9]), fval(args[10])};
        DrawTexturePro(*t, src, dst, origin, fval(args[11]), color4(args, 12));
        return 0;
    }
    static int nat_GetTextureWidth(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "GetTextureWidth"); if (!t) return 0;
        args[0] = val_int(t->width); return 1;
    }
    static int nat_GetTextureHeight(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "GetTextureHeight"); if (!t) return 0;
        args[0] = val_int(t->height); return 1;
    }

    /* =========================================================
    ** Image
    ** ========================================================= */
    static int nat_LoadImage(VM *vm, Value *args, int)
    {
        const char *path = str_arg(vm, args[0], "LoadImage"); if (!path) return 0;
        Image img = LoadImage(path);
        args[0] = vm->make_native_struct(g_def_image, nullptr, 0);
        *zen_struct_ptr(args[0], Image) = img;
        return 1;
    }
    static int nat_UnloadImage(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "UnloadImage"); if (!img) return 0;
        UnloadImage(*img);
        memset(img, 0, sizeof(Image));
        return 0;
    }
    static int nat_GetImageWidth(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "GetImageWidth"); if (!img) return 0;
        args[0] = val_int(img->width); return 1;
    }
    static int nat_GetImageHeight(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "GetImageHeight"); if (!img) return 0;
        args[0] = val_int(img->height); return 1;
    }
    static int nat_ImageResize(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageResize"); if (!img) return 0;
        ImageResize(img, ival(args[1]), ival(args[2]));
        return 0;
    }
    static int nat_ImageCrop(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageCrop"); if (!img) return 0;
        Rectangle crop = {fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4])};
        ImageCrop(img, crop);
        return 0;
    }
    /* GenImageColor(w, h, r, g, b, a) → Image */
    static int nat_GenImageColor(VM *vm, Value *args, int)
    {
        Image img = GenImageColor(ival(args[0]), ival(args[1]), color4(args, 2));
        args[0] = vm->make_native_struct(g_def_image, nullptr, 0);
        *zen_struct_ptr(args[0], Image) = img;
        return 1;
    }
    /* ImageDrawPixel(img, x, y, r, g, b, a) */
    static int nat_ImageDrawPixel(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageDrawPixel"); if (!img) return 0;
        ImageDrawPixel(img, ival(args[1]), ival(args[2]), color4(args, 3));
        return 0;
    }
    /* ImageDrawRectangle(img, x, y, w, h, r, g, b, a) */
    static int nat_ImageDrawRectangle(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageDrawRectangle"); if (!img) return 0;
        ImageDrawRectangle(img, ival(args[1]), ival(args[2]), ival(args[3]), ival(args[4]), color4(args, 5));
        return 0;
    }
    /* ImageDrawLine(img, x0, y0, x1, y1, r, g, b, a) */
    static int nat_ImageDrawLine(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageDrawLine"); if (!img) return 0;
        ImageDrawLine(img, ival(args[1]), ival(args[2]), ival(args[3]), ival(args[4]), color4(args, 5));
        return 0;
    }
    /* ImageDrawCircle(img, cx, cy, radius, r, g, b, a) */
    static int nat_ImageDrawCircle(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ImageDrawCircle"); if (!img) return 0;
        ImageDrawCircle(img, ival(args[1]), ival(args[2]), ival(args[3]), color4(args, 4));
        return 0;
    }
    /* UpdateTexture(tex, img) — re-upload image pixels to existing texture */
    static int nat_UpdateTexture(VM *vm, Value *args, int)
    {
        Texture2D *t = tex_ptr(vm, args[0], "UpdateTexture"); if (!t) return 0;
        Image *img   = img_ptr(vm, args[1], "UpdateTexture"); if (!img) return 0;
        UpdateTexture(*t, img->data);
        return 0;
    }
    /* ExportImage(img, path) → bool */
    static int nat_ExportImage(VM *vm, Value *args, int)
    {
        Image *img = img_ptr(vm, args[0], "ExportImage"); if (!img) return 0;
        const char *path = str_arg(vm, args[1], "ExportImage"); if (!path) return 0;
        bool ok = ExportImage(*img, path);
        args[0] = val_bool(ok);
        return 1;
    }

    /* =========================================================
    ** RenderTexture
    ** ========================================================= */
    static int nat_LoadRenderTexture(VM *vm, Value *args, int)
    {
        RenderTexture2D rt = LoadRenderTexture(ival(args[0]), ival(args[1]));
        args[0] = vm->make_native_struct(g_def_rendertex, nullptr, 0);
        *zen_struct_ptr(args[0], RenderTexture2D) = rt;
        return 1;
    }
    static int nat_UnloadRenderTexture(VM *vm, Value *args, int)
    {
        RenderTexture2D *rt = rtex_ptr(vm, args[0], "UnloadRenderTexture"); if (!rt) return 0;
        UnloadRenderTexture(*rt);
        memset(rt, 0, sizeof(RenderTexture2D));
        return 0;
    }
    static int nat_BeginTextureMode(VM *vm, Value *args, int)
    {
        RenderTexture2D *rt = rtex_ptr(vm, args[0], "BeginTextureMode"); if (!rt) return 0;
        BeginTextureMode(*rt); return 0;
    }
    static int nat_EndTextureMode(VM *, Value *, int) { EndTextureMode(); return 0; }
    static int nat_GetRenderTextureTexture(VM *vm, Value *args, int)
    {
        RenderTexture2D *rt = rtex_ptr(vm, args[0], "GetRenderTextureTexture"); if (!rt) return 0;
        args[0] = vm->make_native_struct(g_def_texture, nullptr, 0);
        *zen_struct_ptr(args[0], Texture2D) = rt->texture;
        return 1;
    }

    /* =========================================================
    ** Input — keyboard
    ** ========================================================= */
    static int nat_IsKeyDown(VM *, Value *args, int)     { args[0] = val_bool(IsKeyDown(ival(args[0]))); return 1; }
    static int nat_IsKeyUp(VM *, Value *args, int)       { args[0] = val_bool(IsKeyUp(ival(args[0]))); return 1; }
    static int nat_IsKeyPressed(VM *, Value *args, int)  { args[0] = val_bool(IsKeyPressed(ival(args[0]))); return 1; }
    static int nat_IsKeyReleased(VM *, Value *args, int) { args[0] = val_bool(IsKeyReleased(ival(args[0]))); return 1; }
    static int nat_GetKeyPressed(VM *, Value *args, int) { args[0] = val_int(GetKeyPressed()); return 1; }

    /* =========================================================
    ** Input — mouse
    ** ========================================================= */
    static int nat_GetMouseX(VM *, Value *args, int)              { args[0] = val_int(GetMouseX()); return 1; }
    static int nat_GetMouseY(VM *, Value *args, int)              { args[0] = val_int(GetMouseY()); return 1; }
    static int nat_GetMouseDelta(VM *, Value *args, int)
    {
        Vector2 d = GetMouseDelta();
        args[0] = val_float(d.x); args[1] = val_float(d.y); return 2;
    }
    static int nat_GetMouseWheelMove(VM *, Value *args, int)     { args[0] = val_float(GetMouseWheelMove()); return 1; }
    static int nat_IsMouseButtonDown(VM *, Value *args, int)     { args[0] = val_bool(IsMouseButtonDown(ival(args[0]))); return 1; }
    static int nat_IsMouseButtonUp(VM *, Value *args, int)       { args[0] = val_bool(IsMouseButtonUp(ival(args[0]))); return 1; }
    static int nat_IsMouseButtonPressed(VM *, Value *args, int)  { args[0] = val_bool(IsMouseButtonPressed(ival(args[0]))); return 1; }
    static int nat_IsMouseButtonReleased(VM *, Value *args, int) { args[0] = val_bool(IsMouseButtonReleased(ival(args[0]))); return 1; }

    /* =========================================================
    ** Audio
    ** ========================================================= */
    static int nat_InitAudioDevice(VM *, Value *, int) { InitAudioDevice(); return 0; }
    static int nat_CloseAudioDevice(VM *, Value *, int) { CloseAudioDevice(); return 0; }
    static int nat_LoadSound(VM *vm, Value *args, int)
    {
        const char *p = str_arg(vm, args[0], "LoadSound"); if (!p) return 0;
        Sound s = LoadSound(p);
        args[0] = vm->make_native_struct(g_def_sound, nullptr, 0);
        *zen_struct_ptr(args[0], Sound) = s;
        return 1;
    }
    static int nat_UnloadSound(VM *vm, Value *args, int)
    {
        Sound *s = snd_ptr(vm, args[0], "UnloadSound"); if (!s) return 0;
        UnloadSound(*s);
        memset(s, 0, sizeof(Sound));
        return 0;
    }
    static int nat_PlaySound(VM *vm, Value *args, int)
    {
        Sound *s = snd_ptr(vm, args[0], "PlaySound"); if (!s) return 0;
        PlaySound(*s); return 0;
    }
    static int nat_StopSound(VM *vm, Value *args, int)
    {
        Sound *s = snd_ptr(vm, args[0], "StopSound"); if (!s) return 0;
        StopSound(*s); return 0;
    }
    static int nat_IsSoundPlaying(VM *vm, Value *args, int)
    {
        Sound *s = snd_ptr(vm, args[0], "IsSoundPlaying"); if (!s) return 0;
        args[0] = val_bool(IsSoundPlaying(*s)); return 1;
    }
    static int nat_SetSoundVolume(VM *vm, Value *args, int)
    {
        Sound *s = snd_ptr(vm, args[0], "SetSoundVolume"); if (!s) return 0;
        SetSoundVolume(*s, fval(args[1])); return 0;
    }

    /* =========================================================
    ** Misc
    ** ========================================================= */
    static int nat_SetExitKey(VM *, Value *args, int)     { SetExitKey(ival(args[0])); return 0; }
    static int nat_TakeScreenshot(VM *vm, Value *args, int)
    {
        const char *p = str_arg(vm, args[0], "TakeScreenshot"); if (!p) return 0;
        TakeScreenshot(p); return 0;
    }
    static int nat_SetRandomSeed(VM *, Value *args, int)  { SetRandomSeed((unsigned int)ival(args[0])); return 0; }
    static int nat_GetRandomValue(VM *, Value *args, int) { args[0] = val_int(GetRandomValue(ival(args[0]), ival(args[1]))); return 1; }
    static int nat_Fade(VM *, Value *args, int)
    {
        Color c = Fade(color4(args, 0), fval(args[4]));
        args[0] = val_int(c.r); args[1] = val_int(c.g); args[2] = val_int(c.b); args[3] = val_int(c.a);
        return 4;
    }
    static int nat_ColorAlpha(VM *, Value *args, int)
    {
        Color c = ColorAlpha(color4(args, 0), fval(args[4]));
        args[0] = val_int(c.r); args[1] = val_int(c.g); args[2] = val_int(c.b); args[3] = val_int(c.a);
        return 4;
    }

    /* =========================================================
    ** Function table
    ** ========================================================= */
    static const NativeReg raylib_funcs[] = {
        /* Window */
        {"InitWindow",            nat_InitWindow,            3},
        {"CloseWindow",           nat_CloseWindow,           0},
        {"WindowShouldClose",     nat_WindowShouldClose,     0},
        {"IsWindowReady",         nat_IsWindowReady,         0},
        {"IsWindowFullscreen",    nat_IsWindowFullscreen,    0},
        {"ToggleFullscreen",      nat_ToggleFullscreen,      0},
        {"SetWindowSize",         nat_SetWindowSize,         2},
        {"SetWindowTitle",        nat_SetWindowTitle,        1},
        {"GetScreenWidth",        nat_GetScreenWidth,        0},
        {"GetScreenHeight",       nat_GetScreenHeight,       0},
        {"SetTargetFPS",          nat_SetTargetFPS,          1},
        {"GetFPS",                nat_GetFPS,                0},
        {"GetFrameTime",          nat_GetFrameTime,          0},
        {"GetTime",               nat_GetTime,               0},
        /* Drawing */
        {"BeginDrawing",          nat_BeginDrawing,          0},
        {"EndDrawing",            nat_EndDrawing,            0},
        {"ClearBackground",       nat_ClearBackground,       4},
        {"DrawRectangle",         nat_DrawRectangle,         8},
        {"DrawRectangleF",        nat_DrawRectangleF,        8},
        {"DrawRectangleLines",    nat_DrawRectangleLines,    8},
        {"DrawRectangleLinesEx",  nat_DrawRectangleLinesEx,  9},
        {"DrawCircle",            nat_DrawCircle,            7},
        {"DrawCircleV",           nat_DrawCircleV,           7},
        {"DrawCircleLines",       nat_DrawCircleLines,       7},
        {"DrawLine",              nat_DrawLine,              8},
        {"DrawLineV",             nat_DrawLineV,             8},
        {"DrawLineEx",            nat_DrawLineEx,            9},
        {"DrawText",              nat_DrawText,              8},
        {"DrawFPS",               nat_DrawFPS,               2},
        {"DrawPixel",             nat_DrawPixel,             6},
        {"DrawTriangle",          nat_DrawTriangle,          10},
        /* Texture */
        {"LoadTexture",           nat_LoadTexture,           1},
        {"LoadTextureFromImage",  nat_LoadTextureFromImage,  1},
        {"UnloadTexture",         nat_UnloadTexture,         1},
        {"DrawTexture",           nat_DrawTexture,           7},
        {"DrawTextureV",          nat_DrawTextureV,          7},
        {"DrawTextureEx",         nat_DrawTextureEx,         9},
        {"DrawTextureRec",        nat_DrawTextureRec,        11},
        {"DrawTexturePro",        nat_DrawTexturePro,        16},
        {"GetTextureWidth",       nat_GetTextureWidth,       1},
        {"GetTextureHeight",      nat_GetTextureHeight,      1},
        /* Image */
        {"LoadImage",             nat_LoadImage,             1},
        {"UnloadImage",           nat_UnloadImage,           1},
        {"GetImageWidth",         nat_GetImageWidth,         1},
        {"GetImageHeight",        nat_GetImageHeight,        1},
        {"ImageResize",           nat_ImageResize,           3},
        {"ImageCrop",             nat_ImageCrop,             5},
        {"GenImageColor",         nat_GenImageColor,         6},
        {"ImageDrawPixel",        nat_ImageDrawPixel,        7},
        {"ImageDrawRectangle",    nat_ImageDrawRectangle,    9},
        {"ImageDrawLine",         nat_ImageDrawLine,         9},
        {"ImageDrawCircle",       nat_ImageDrawCircle,       8},
        {"UpdateTexture",         nat_UpdateTexture,         2},
        {"ExportImage",           nat_ExportImage,           2},
        /* RenderTexture */
        {"LoadRenderTexture",     nat_LoadRenderTexture,     2},
        {"UnloadRenderTexture",   nat_UnloadRenderTexture,   1},
        {"BeginTextureMode",      nat_BeginTextureMode,      1},
        {"EndTextureMode",        nat_EndTextureMode,        0},
        {"GetRenderTextureTexture", nat_GetRenderTextureTexture, 1},
        /* Keyboard */
        {"IsKeyDown",             nat_IsKeyDown,             1},
        {"IsKeyUp",               nat_IsKeyUp,               1},
        {"IsKeyPressed",          nat_IsKeyPressed,          1},
        {"IsKeyReleased",         nat_IsKeyReleased,         1},
        {"GetKeyPressed",         nat_GetKeyPressed,         0},
        /* Mouse */
        {"GetMouseX",             nat_GetMouseX,             0},
        {"GetMouseY",             nat_GetMouseY,             0},
        {"GetMouseDelta",         nat_GetMouseDelta,         0},
        {"GetMouseWheelMove",     nat_GetMouseWheelMove,     0},
        {"IsMouseButtonDown",     nat_IsMouseButtonDown,     1},
        {"IsMouseButtonUp",       nat_IsMouseButtonUp,       1},
        {"IsMouseButtonPressed",  nat_IsMouseButtonPressed,  1},
        {"IsMouseButtonReleased", nat_IsMouseButtonReleased, 1},
        /* Audio */
        {"InitAudioDevice",       nat_InitAudioDevice,       0},
        {"CloseAudioDevice",      nat_CloseAudioDevice,      0},
        {"LoadSound",             nat_LoadSound,             1},
        {"UnloadSound",           nat_UnloadSound,           1},
        {"PlaySound",             nat_PlaySound,             1},
        {"StopSound",             nat_StopSound,             1},
        {"IsSoundPlaying",        nat_IsSoundPlaying,        1},
        {"SetSoundVolume",        nat_SetSoundVolume,        2},
        /* Misc */
        {"SetExitKey",            nat_SetExitKey,            1},
        {"TakeScreenshot",        nat_TakeScreenshot,        1},
        {"SetRandomSeed",         nat_SetRandomSeed,         1},
        {"GetRandomValue",        nat_GetRandomValue,        2},
        {"Fade",                  nat_Fade,                  5},
        {"ColorAlpha",            nat_ColorAlpha,            5},
    };

    /* =========================================================
    ** Constants
    ** ========================================================= */
    static const NativeConst raylib_constants[] = {
        /* Alphabet */
        {"KEY_A",val_int(KEY_A)},{"KEY_B",val_int(KEY_B)},{"KEY_C",val_int(KEY_C)},
        {"KEY_D",val_int(KEY_D)},{"KEY_E",val_int(KEY_E)},{"KEY_F",val_int(KEY_F)},
        {"KEY_G",val_int(KEY_G)},{"KEY_H",val_int(KEY_H)},{"KEY_I",val_int(KEY_I)},
        {"KEY_J",val_int(KEY_J)},{"KEY_K",val_int(KEY_K)},{"KEY_L",val_int(KEY_L)},
        {"KEY_M",val_int(KEY_M)},{"KEY_N",val_int(KEY_N)},{"KEY_O",val_int(KEY_O)},
        {"KEY_P",val_int(KEY_P)},{"KEY_Q",val_int(KEY_Q)},{"KEY_R",val_int(KEY_R)},
        {"KEY_S",val_int(KEY_S)},{"KEY_T",val_int(KEY_T)},{"KEY_U",val_int(KEY_U)},
        {"KEY_V",val_int(KEY_V)},{"KEY_W",val_int(KEY_W)},{"KEY_X",val_int(KEY_X)},
        {"KEY_Y",val_int(KEY_Y)},{"KEY_Z",val_int(KEY_Z)},
        /* Numbers */
        {"KEY_ZERO",val_int(KEY_ZERO)},{"KEY_ONE",val_int(KEY_ONE)},
        {"KEY_TWO",val_int(KEY_TWO)},{"KEY_THREE",val_int(KEY_THREE)},
        {"KEY_FOUR",val_int(KEY_FOUR)},{"KEY_FIVE",val_int(KEY_FIVE)},
        {"KEY_SIX",val_int(KEY_SIX)},{"KEY_SEVEN",val_int(KEY_SEVEN)},
        {"KEY_EIGHT",val_int(KEY_EIGHT)},{"KEY_NINE",val_int(KEY_NINE)},
        /* Arrows + common */
        {"KEY_RIGHT",val_int(KEY_RIGHT)},{"KEY_LEFT",val_int(KEY_LEFT)},
        {"KEY_DOWN",val_int(KEY_DOWN)},{"KEY_UP",val_int(KEY_UP)},
        {"KEY_SPACE",val_int(KEY_SPACE)},{"KEY_ENTER",val_int(KEY_ENTER)},
        {"KEY_ESCAPE",val_int(KEY_ESCAPE)},{"KEY_BACKSPACE",val_int(KEY_BACKSPACE)},
        {"KEY_TAB",val_int(KEY_TAB)},{"KEY_DELETE",val_int(KEY_DELETE)},
        {"KEY_LEFT_SHIFT",val_int(KEY_LEFT_SHIFT)},{"KEY_RIGHT_SHIFT",val_int(KEY_RIGHT_SHIFT)},
        {"KEY_LEFT_CONTROL",val_int(KEY_LEFT_CONTROL)},{"KEY_RIGHT_CONTROL",val_int(KEY_RIGHT_CONTROL)},
        {"KEY_LEFT_ALT",val_int(KEY_LEFT_ALT)},{"KEY_RIGHT_ALT",val_int(KEY_RIGHT_ALT)},
        {"KEY_F1",val_int(KEY_F1)},{"KEY_F2",val_int(KEY_F2)},{"KEY_F3",val_int(KEY_F3)},
        {"KEY_F4",val_int(KEY_F4)},{"KEY_F5",val_int(KEY_F5)},{"KEY_F6",val_int(KEY_F6)},
        {"KEY_F7",val_int(KEY_F7)},{"KEY_F8",val_int(KEY_F8)},{"KEY_F9",val_int(KEY_F9)},
        {"KEY_F10",val_int(KEY_F10)},{"KEY_F11",val_int(KEY_F11)},{"KEY_F12",val_int(KEY_F12)},
        /* Mouse buttons */
        {"MOUSE_BUTTON_LEFT",val_int(MOUSE_BUTTON_LEFT)},
        {"MOUSE_BUTTON_RIGHT",val_int(MOUSE_BUTTON_RIGHT)},
        {"MOUSE_BUTTON_MIDDLE",val_int(MOUSE_BUTTON_MIDDLE)},
    };

    /* =========================================================
    ** init_fn — called by open_lib_globals with a live VM*.
    ** Registers Texture2D, Image, Sound, RenderTexture2D as
    ** native struct types. No dtors: Unload* must be explicit
    ** (safe: OpenGL/audio are closed before VM is destroyed).
    ** ========================================================= */

    static void tex_ctor(VM *, void *buf, int argc, Value *args)
    {
        Texture2D *t = (Texture2D *)buf;
        t->id      = argc > 0 ? ival(args[0]) : 0;
        t->width   = argc > 1 ? ival(args[1]) : 0;
        t->height  = argc > 2 ? ival(args[2]) : 0;
        t->mipmaps = argc > 3 ? ival(args[3]) : 1;
        t->format  = argc > 4 ? ival(args[4]) : 0;
    }
    static void img_ctor(VM *, void *buf, int argc, Value *args)
    {
        Image *img = (Image *)buf;
        img->width   = argc > 0 ? ival(args[0]) : 0;
        img->height  = argc > 1 ? ival(args[1]) : 0;
        img->mipmaps = argc > 2 ? ival(args[2]) : 1;
        img->format  = argc > 3 ? ival(args[3]) : 0;
        img->data    = nullptr;
    }
    static void snd_ctor(VM *, void *buf, int argc, Value *args)
    {
        Sound *s = (Sound *)buf;
        s->frameCount = argc > 0 ? (unsigned int)ival(args[0]) : 0;
    }
    static void rtex_ctor(VM *, void *buf, int argc, Value *args)
    {
        RenderTexture2D *rt = (RenderTexture2D *)buf;
        rt->id = argc > 0 ? (unsigned int)ival(args[0]) : 0;
    }

    static void raylib_init(VM *vm)
    {
        g_def_texture = vm->register_native_struct("Texture2D", sizeof(Texture2D), tex_ctor)
            .i32("id",      offsetof(Texture2D, id),      true)
            .i32("width",   offsetof(Texture2D, width),   true)
            .i32("height",  offsetof(Texture2D, height),  true)
            .i32("mipmaps", offsetof(Texture2D, mipmaps), true)
            .i32("format",  offsetof(Texture2D, format),  true)
            .end();

        g_def_image = vm->register_native_struct("Image", sizeof(Image), img_ctor)
            .i32("width",   offsetof(Image, width),   true)
            .i32("height",  offsetof(Image, height),  true)
            .i32("mipmaps", offsetof(Image, mipmaps), true)
            .i32("format",  offsetof(Image, format),  true)
            .end();

        g_def_sound = vm->register_native_struct("Sound", sizeof(Sound), snd_ctor)
            .u32("frameCount", offsetof(Sound, frameCount), true)
            .end();

        g_def_rendertex = vm->register_native_struct("RenderTexture2D", sizeof(RenderTexture2D), rtex_ctor)
            .i32("id", offsetof(RenderTexture2D, id), true)
            .end();
    }

    const NativeLib zen_lib_raylib = {
        "raylib", raylib_funcs, ZEN_ARRAY_COUNT(raylib_funcs),
        raylib_constants, ZEN_ARRAY_COUNT(raylib_constants),
        raylib_init
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_RAYLIB */
