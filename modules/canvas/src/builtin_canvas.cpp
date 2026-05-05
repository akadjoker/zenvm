/* =========================================================
** builtin_canvas.cpp — "canvas" module for Zen
**
** Standalone 2D draw list. The GL backend is optional and
** selected at compile time via platform defines:
**
**   ZEN_CANVAS_GL     — desktop OpenGL 3.3 (default on Linux/Win/macOS)
**   ZEN_CANVAS_GLES   — OpenGL ES 3 / WebGL 2 (Android, iOS, Emscripten)
**   (neither)         — pure draw-list, no GPU code (headless / custom backend)
**
** These are set automatically by modules/canvas/CMakeLists.txt based
** on the target platform, or can be forced via -DZEN_CANVAS_BACKEND=GL|GLES|NONE.
**
** Vertex layout (stride = 20 bytes):
**   float x, y, u, v   — position + texture coord
**   uint32_t col        — RGBA packed (R in low byte)
**
** Zen usage (with GL/GLES backend):
**   import canvas;
**   canvas.backend_init(800, 600);
**   var dl = canvas.new();
**   dl.set_color(255, 0, 0);
**   dl.rect(10, 10, 100, 50);
**   dl.flush();                  -- each frame
**
** Zen usage (no backend — headless or custom host):
**   import canvas;
**   var dl = canvas.new();
**   dl.set_white_pixel(tex, u, v);
**   dl.set_color(255, 0, 0);
**   dl.rect(10, 10, 100, 50);
**   -- read dl.vertex_bytes() / dl.index_data() / dl.commands()
** ========================================================= */

/* Auto-select GL/GLES if not forced by CMake */
#if !defined(ZEN_CANVAS_GL) && !defined(ZEN_CANVAS_GLES) && !defined(ZEN_CANVAS_NO_BACKEND)
#  if defined(__EMSCRIPTEN__) || defined(__ANDROID__) \
      || (defined(__APPLE__) && (TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_SIMULATOR))
#    define ZEN_CANVAS_GLES
#  elif defined(__linux__) || defined(_WIN32) \
      || (defined(__APPLE__) && !TARGET_OS_IOS)
#    define ZEN_CANVAS_GL
#  else
#    define ZEN_CANVAS_NO_BACKEND
#  endif
#endif

#if defined(ZEN_CANVAS_GLES)
#  define CANVAS_GL_ES
#endif

#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
#  define CANVAS_GL_BACKEND_IMPL
#  include "canvas_gl_backend.h"   /* pulls in canvas_draw.h + platform GL */
#else
#  include "canvas_draw.h"
#endif

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdlib>

/* stb_truetype types only (no implementation — already in zen_module_stb_font) */
#include "../../stb_trutype/vendor/stb_truetype.h"

namespace zen
{

/* =========================================================
** CanvasCtx — native data wrapped inside a Zen DrawList instance
** ========================================================= */

struct CanvasCtx
{
    canvas::DrawList dl;
    uint32_t         cur_col = 0xFFFFFFFFu; /* default: opaque white */
};

static ObjClass *g_canvas_class = nullptr;
static ObjClass *g_font_class   = nullptr;
#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
static canvas::GLBackend g_backend;
#endif

/* =========================================================
** FontCtx — native data for canvas.Font instances
** ========================================================= */

struct FontCtx {
    stbtt_bakedchar chardata[96]; /* ASCII printable: 32..127 */
    uint64_t        tex        = 0;
    int             atlas_w    = 0;
    int             atlas_h    = 0;
    int             first_char = 32;
    int             num_chars  = 96;
    float           px_height  = 16.f;
};

static void font_ctx_dtor(VM * /*vm*/, void *ptr)
{
    delete (FontCtx *)ptr;
}

static void canvas_dtor(VM * /*vm*/, void *ptr)
{
    delete (CanvasCtx *)ptr;
}

/* ── helpers ──────────────────────────────────────────────────────────────── */

static inline CanvasCtx *get_ctx(VM *vm, Value *args)
{
    if (!is_instance(args[0]))
    {
        vm->runtime_error("canvas: invalid receiver");
        return nullptr;
    }
    CanvasCtx *ctx = (CanvasCtx *)as_instance(args[0])->native_data;
    if (!ctx)
    {
        vm->runtime_error("canvas: null DrawList");
        return nullptr;
    }
    return ctx;
}

static inline float    fval(Value v) { return is_int(v) ? (float)v.as.integer : (float)v.as.number; }
static inline int      ival(Value v) { return is_int(v) ? (int)v.as.integer   : (int)v.as.number;   }
static inline uint32_t cval(Value v) { return (uint32_t)(is_int(v) ? v.as.integer : (int64_t)v.as.number); }
static inline bool     bval(Value v) { return is_bool(v) ? v.as.boolean : (bool)ival(v); }

/* Return color from args[idx] if present, otherwise ctx->cur_col */
static inline uint32_t getcol(CanvasCtx *ctx, Value *args, int nargs, int idx)
{
    return (nargs > idx) ? cval(args[idx]) : ctx->cur_col;
}

/* =========================================================
** Module-level functions
** ========================================================= */

/* canvas.pack(r,g,b,a) → packed uint32 */
static int nat_pack(VM * /*vm*/, Value *args, int /*nargs*/)
{
    uint32_t col = canvas::pack_rgba(
        (uint8_t)ival(args[0]), (uint8_t)ival(args[1]),
        (uint8_t)ival(args[2]), (uint8_t)ival(args[3]));
    args[0] = val_int((int64_t)col);
    return 1;
}

/* canvas.new() → DrawList */
static int nat_new(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = new CanvasCtx();
#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
    if (g_backend.ready)
        ctx->dl.set_white_pixel(g_backend.white_tex_id(),
                                g_backend.white_u(), g_backend.white_v());
#endif
    Value inst_val = vm->make_instance(g_canvas_class);
    as_instance(inst_val)->native_data = ctx;
    args[0] = inst_val;
    return 1;
}

#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
/* canvas.backend_init(w, h) — compile shaders, create GL objects */
static int nat_backend_init(VM *vm, Value *args, int nargs)
{
    if (nargs < 2) { vm->runtime_error("canvas: backend_init(w, h)"); return 0; }
    if (!g_backend.init(ival(args[0]), ival(args[1])))
        { vm->runtime_error("canvas: GL backend init failed"); return 0; }
    args[0] = val_nil();
    return 1;
}
/* canvas.backend_resize(w, h) */
static int nat_backend_resize(VM *vm, Value *args, int nargs)
{
    if (nargs < 2) { vm->runtime_error("canvas: backend_resize(w, h)"); return 0; }
    g_backend.resize(ival(args[0]), ival(args[1]));
    args[0] = val_nil();
    return 1;
}
/* canvas.backend_destroy() */
static int nat_backend_destroy(VM * /*vm*/, Value *args, int /*nargs*/)
{
    g_backend.destroy();
    args[0] = val_nil();
    return 1;
}
#endif

/* =========================================================
** DrawList methods
** ========================================================= */

/* dl.set_white_pixel(tex_id, u, v)
** Must be called once before drawing any solid shapes.
** tex_id is the GPU texture handle (integer) that contains a white pixel. */
static int nat_set_white_pixel(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 4) { vm->runtime_error("canvas: set_white_pixel(tex_id, u, v)"); return 0; }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    ctx->dl.set_white_pixel(tex, fval(args[2]), fval(args[3]));
    args[0] = val_nil();
    return 1;
}

/* dl.clear() — reset buffers for next frame (does NOT reset white_pixel) */
static int nat_clear(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    ctx->dl.clear();
    args[0] = val_nil();
    return 1;
}

/* dl.push_clip(x, y, w, h) */
static int nat_push_clip(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: push_clip(x,y,w,h)"); return 0; }
    ctx->dl.push_clip(fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4]));
    args[0] = val_nil();
    return 1;
}

/* dl.pop_clip() */
static int nat_pop_clip(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    ctx->dl.pop_clip();
    args[0] = val_nil();
    return 1;
}

/* ── color state ────────────────────────────────────────────────────────── */

/* dl.set_color(r, g, b) — set rgb, keep current alpha */
static int nat_set_color(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 4) { vm->runtime_error("canvas: set_color(r,g,b)"); return 0; }
    uint8_t a = (uint8_t)((ctx->cur_col >> 24) & 0xFF);
    ctx->cur_col = canvas::pack_rgba((uint8_t)ival(args[1]),(uint8_t)ival(args[2]),(uint8_t)ival(args[3]),a);
    args[0] = val_nil();
    return 1;
}

/* dl.set_rgba(r, g, b, a) — set full color */
static int nat_set_rgba(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: set_rgba(r,g,b,a)"); return 0; }
    ctx->cur_col = canvas::pack_rgba((uint8_t)ival(args[1]),(uint8_t)ival(args[2]),
                                     (uint8_t)ival(args[3]),(uint8_t)ival(args[4]));
    args[0] = val_nil();
    return 1;
}

/* dl.set_alpha(a) — change only alpha channel */
static int nat_set_alpha(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 2) { vm->runtime_error("canvas: set_alpha(a)"); return 0; }
    ctx->cur_col = (ctx->cur_col & 0x00FFFFFFu) | ((uint32_t)(uint8_t)ival(args[1]) << 24);
    args[0] = val_nil();
    return 1;
}

/* dl.get_color() → packed uint32 */
static int nat_get_color(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    args[0] = val_int((int64_t)ctx->cur_col);
    return 1;
}

/* ── shapes ─────────────────────────────────────────────────────────────── */

/* dl.rect(x, y, w, h [, col]) */
static int nat_rect(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: rect(x,y,w,h[,col])"); return 0; }
    ctx->dl.add_rect(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),getcol(ctx,args,nargs,5));
    args[0] = val_nil();
    return 1;
}

/* dl.rect_outline(x, y, w, h, col [, thickness=1]) */
static int nat_rect_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: rect_outline(x,y,w,h[,col[,t]])"); return 0; }
    float t = nargs >= 7 ? fval(args[6]) : 1.f;
    ctx->dl.add_rect_outline(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),getcol(ctx,args,nargs,5),t);
    args[0] = val_nil();
    return 1;
}

/* dl.rect_gradient(x, y, w, h, col_tl, col_tr, col_br, col_bl) */
static int nat_rect_gradient(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 9) { vm->runtime_error("canvas: rect_gradient(x,y,w,h,tl,tr,br,bl)"); return 0; }
    ctx->dl.add_rect_gradient(
        fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),
        cval(args[5]),cval(args[6]),cval(args[7]),cval(args[8]));
    args[0] = val_nil();
    return 1;
}

/* dl.round_rect(x, y, w, h, radius, col) */
static int nat_round_rect(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 6) { vm->runtime_error("canvas: round_rect(x,y,w,h,radius[,col])"); return 0; }
    ctx->dl.add_round_rect(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),fval(args[5]),getcol(ctx,args,nargs,6));
    args[0] = val_nil();
    return 1;
}

/* dl.round_rect_outline(x, y, w, h, radius, col [, thickness=1]) */
static int nat_round_rect_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 6) { vm->runtime_error("canvas: round_rect_outline(x,y,w,h,radius[,col[,t]])"); return 0; }
    float t = nargs >= 8 ? fval(args[7]) : 1.f;
    ctx->dl.add_round_rect_outline(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),fval(args[5]),getcol(ctx,args,nargs,6),t);
    args[0] = val_nil();
    return 1;
}

/* dl.circle(cx, cy, r, col) */
static int nat_circle(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 4) { vm->runtime_error("canvas: circle(cx,cy,r[,col])"); return 0; }
    ctx->dl.add_circle(fval(args[1]),fval(args[2]),fval(args[3]),getcol(ctx,args,nargs,4));
    args[0] = val_nil();
    return 1;
}

/* dl.circle_outline(cx, cy, r, col [, thickness=1]) */
static int nat_circle_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 4) { vm->runtime_error("canvas: circle_outline(cx,cy,r[,col[,t]])"); return 0; }
    float t = nargs >= 6 ? fval(args[5]) : 1.f;
    ctx->dl.add_circle_outline(fval(args[1]),fval(args[2]),fval(args[3]),getcol(ctx,args,nargs,4),t);
    args[0] = val_nil();
    return 1;
}

/* dl.ellipse(cx, cy, rx, ry, col [, rot=0]) */
static int nat_ellipse(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: ellipse(cx,cy,rx,ry[,col[,rot]])"); return 0; }
    float rot = nargs >= 7 ? fval(args[6]) : 0.f;
    ctx->dl.add_ellipse(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),getcol(ctx,args,nargs,5),rot);
    args[0] = val_nil();
    return 1;
}

/* dl.ellipse_outline(cx, cy, rx, ry, col [, thickness=1 [, rot=0]]) */
static int nat_ellipse_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: ellipse_outline(cx,cy,rx,ry[,col[,t[,rot]]])"); return 0; }
    float t   = nargs >= 7 ? fval(args[6]) : 1.f;
    float rot = nargs >= 8 ? fval(args[7]) : 0.f;
    ctx->dl.add_ellipse_outline(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),getcol(ctx,args,nargs,5),t,rot);
    args[0] = val_nil();
    return 1;
}

/* dl.ngon(cx, cy, r, sides, col) */
static int nat_ngon(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: ngon(cx,cy,r,sides[,col])"); return 0; }
    ctx->dl.add_ngon(fval(args[1]),fval(args[2]),fval(args[3]),ival(args[4]),getcol(ctx,args,nargs,5));
    args[0] = val_nil();
    return 1;
}

/* dl.ngon_outline(cx, cy, r, sides, col [, thickness=1]) */
static int nat_ngon_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: ngon_outline(cx,cy,r,sides[,col[,t]])"); return 0; }
    float t = nargs >= 7 ? fval(args[6]) : 1.f;
    ctx->dl.add_ngon_outline(fval(args[1]),fval(args[2]),fval(args[3]),ival(args[4]),getcol(ctx,args,nargs,5),t);
    args[0] = val_nil();
    return 1;
}

/* dl.triangle(x0,y0, x1,y1, x2,y2, col) */
static int nat_triangle(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 7) { vm->runtime_error("canvas: triangle(x0,y0,x1,y1,x2,y2[,col])"); return 0; }
    ctx->dl.add_triangle(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),
                         fval(args[5]),fval(args[6]),getcol(ctx,args,nargs,7));
    args[0] = val_nil();
    return 1;
}

/* dl.triangle_outline(x0,y0, x1,y1, x2,y2, col [, thickness=1]) */
static int nat_triangle_outline(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 7) { vm->runtime_error("canvas: triangle_outline(x0,y0,x1,y1,x2,y2[,col[,t]])"); return 0; }
    float t = nargs >= 9 ? fval(args[8]) : 1.f;
    ctx->dl.add_triangle_outline(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),
                                 fval(args[5]),fval(args[6]),getcol(ctx,args,nargs,7),t);
    args[0] = val_nil();
    return 1;
}

/* dl.line(x1, y1, x2, y2, col [, thickness=1]) */
static int nat_line(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: line(x1,y1,x2,y2[,col[,t]])"); return 0; }
    float t = nargs >= 7 ? fval(args[6]) : 1.f;
    ctx->dl.add_line(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),getcol(ctx,args,nargs,5),t);
    args[0] = val_nil();
    return 1;
}

/* dl.bezier_cubic(x1,y1, cx1,cy1, cx2,cy2, x2,y2, col [, thickness=1]) */
static int nat_bezier_cubic(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 9) { vm->runtime_error("canvas: bezier_cubic(x1,y1,cx1,cy1,cx2,cy2,x2,y2[,col[,t]])"); return 0; }
    float t = nargs >= 11 ? fval(args[10]) : 1.f;
    ctx->dl.add_bezier_cubic(
        fval(args[1]),fval(args[2]), fval(args[3]),fval(args[4]),
        fval(args[5]),fval(args[6]), fval(args[7]),fval(args[8]),
        getcol(ctx,args,nargs,9), t);
    args[0] = val_nil();
    return 1;
}

/* dl.bezier_quad(x1,y1, cx,cy, x2,y2, col [, thickness=1]) */
static int nat_bezier_quad(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 7) { vm->runtime_error("canvas: bezier_quad(x1,y1,cx,cy,x2,y2[,col[,t]])"); return 0; }
    float t = nargs >= 9 ? fval(args[8]) : 1.f;
    ctx->dl.add_bezier_quad(
        fval(args[1]),fval(args[2]), fval(args[3]),fval(args[4]),
        fval(args[5]),fval(args[6]), getcol(ctx,args,nargs,7), t);
    args[0] = val_nil();
    return 1;
}

/* dl.image(tex_id, x, y, w, h [, u0=0, v0=0, u1=1, v1=1 [, col=0xFFFFFFFF]]) */
static int nat_image(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 6) { vm->runtime_error("canvas: image(tex,x,y,w,h[,u0,v0,u1,v1[,col]])"); return 0; }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    float u0=0.f, v0=0.f, u1=1.f, v1=1.f;
    uint32_t col = 0xFFFFFFFFu;
    if (nargs >= 10) { u0=fval(args[6]); v0=fval(args[7]); u1=fval(args[8]); v1=fval(args[9]); }
    if (nargs >= 11) { col = cval(args[10]); }
    ctx->dl.add_image(tex, fval(args[2]),fval(args[3]),fval(args[4]),fval(args[5]),
                      u0,v0,u1,v1, col);
    args[0] = val_nil();
    return 1;
}

/* dl.image_region(tex, dx,dy,dw,dh, sx,sy,sw,sh, tex_w,tex_h [,col]) */
static int nat_image_region(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 12) { vm->runtime_error("canvas: image_region(tex,dx,dy,dw,dh,sx,sy,sw,sh,tex_w,tex_h[,col])"); return 0; }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    uint32_t col = getcol(ctx, args, nargs, 12);
    ctx->dl.add_image_region(tex,
        fval(args[2]),fval(args[3]),fval(args[4]),fval(args[5]),   /* dst */
        fval(args[6]),fval(args[7]),fval(args[8]),fval(args[9]),   /* src */
        fval(args[10]),fval(args[11]),                             /* tex size */
        col);
    args[0] = val_nil();
    return 1;
}

/* dl.image_vertices(tex, Float32Array_xyzuv, [Uint32Array_indices [, col]])
** Float32Array layout: [x0,y0,u0,v0, x1,y1,u1,v1, ...]  (4 floats per vertex)
** If indices are omitted, vertices are treated as sequential triangles (groups of 3).
** col defaults to cur_col (set with set_color/set_rgba). */
static int nat_image_vertices(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 3 || !is_buffer(args[2])) {
        vm->runtime_error("canvas: image_vertices(tex, Float32Array [, Uint32Array [, col]])");
        return 0;
    }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    ObjBuffer *vb = as_buffer(args[2]);
    if (vb->btype != BUF_FLOAT32) {
        vm->runtime_error("canvas: image_vertices — vertex buffer must be Float32Array");
        return 0;
    }
    int vert_count = vb->count / 4; /* 4 floats per vertex */
    const float    *vdata = (const float *)vb->data;
    const uint32_t *idata = nullptr;
    int             icount = 0;
    int             col_idx = 3;
    /* arg[3] can be:
    **   int/number → explicit vert_count (use first N verts from buffer)
    **   Uint32Array → index buffer (existing behaviour)
    **   anything else → treated as col */
    if (nargs >= 4 && !is_buffer(args[3]) && !is_nil(args[3]) && !is_obj(args[3])) {
        int n = ival(args[3]);
        if (n > 0 && n <= vert_count) vert_count = n;
        col_idx = 4;
    } else if (nargs >= 4 && is_buffer(args[3])) {
        ObjBuffer *ib = as_buffer(args[3]);
        if (ib->btype != BUF_UINT32) {
            vm->runtime_error("canvas: image_vertices — index buffer must be Uint32Array");
            return 0;
        }
        idata  = (const uint32_t *)ib->data;
        icount = ib->count;
        col_idx = 4;
    }
    uint32_t col = getcol(ctx, args, nargs, col_idx);
    ctx->dl.add_image_vertices(tex, vdata, vert_count, idata, icount, col);
    args[0] = val_nil();
    return 1;
}

/* dl.image_batch(tex, Float32Array_verts, Uint32Array_indices, count)
** Draws exactly `count` quads from pre-filled buffers.
** verts:   Float32Array  — 4 floats per vertex (x,y,u,v), capacity >= count*4 verts = count*16 floats
** indices: Uint32Array   — 6 indices per quad,             capacity >= count*6
** count:   number of quads to draw (must not exceed buffer capacity)
** Tint colour from cur_col (call set_rgba before if needed). */
static int nat_image_batch(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5 || !is_buffer(args[2]) || !is_buffer(args[3])) {
        vm->runtime_error("canvas: image_batch(tex, Float32Array, Uint32Array, count)");
        return 0;
    }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    ObjBuffer *vb = as_buffer(args[2]);
    ObjBuffer *ib = as_buffer(args[3]);
    int quads = ival(args[4]);
    if (vb->btype != BUF_FLOAT32) { vm->runtime_error("canvas: image_batch — verts must be Float32Array"); return 0; }
    if (ib->btype != BUF_UINT32)  { vm->runtime_error("canvas: image_batch — indices must be Uint32Array"); return 0; }
    if (quads <= 0) { args[0] = val_nil(); return 1; }
    if (quads * 16 > vb->count) { vm->runtime_error("canvas: image_batch — count exceeds vertex buffer capacity"); return 0; }
    if (quads *  6 > ib->count) { vm->runtime_error("canvas: image_batch — count exceeds index buffer capacity");  return 0; }
    uint32_t col = getcol(ctx, args, nargs, 5);
    ctx->dl.add_image_vertices(tex,
        (const float    *)vb->data, quads * 4,
        (const uint32_t *)ib->data, quads * 6, col);
    args[0] = val_nil();
    return 1;
}

/* dl.image_batch_col(tex, Float32Array, Uint32Array_idx, Uint32Array_colors, count)
** Like image_batch but with per-sprite color (one uint32_t RGBA per quad). */
static int nat_image_batch_col(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 6 || !is_buffer(args[2]) || !is_buffer(args[3]) || !is_buffer(args[4])) {
        vm->runtime_error("canvas: image_batch_col(tex, Float32Array, Uint32Array, Uint32Array_colors, count)");
        return 0;
    }
    uint64_t tex = (uint64_t)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    ObjBuffer *vb = as_buffer(args[2]);
    ObjBuffer *ib = as_buffer(args[3]);
    ObjBuffer *cb = as_buffer(args[4]);
    int quads = ival(args[5]);
    if (vb->btype != BUF_FLOAT32) { vm->runtime_error("canvas: image_batch_col — verts must be Float32Array"); return 0; }
    if (ib->btype != BUF_UINT32)  { vm->runtime_error("canvas: image_batch_col — indices must be Uint32Array"); return 0; }
    if (cb->btype != BUF_UINT32)  { vm->runtime_error("canvas: image_batch_col — colors must be Uint32Array"); return 0; }
    if (quads <= 0) { args[0] = val_nil(); return 1; }
    if (quads * 16 > vb->count) { vm->runtime_error("canvas: image_batch_col — count exceeds vertex buffer"); return 0; }
    if (quads *  6 > ib->count) { vm->runtime_error("canvas: image_batch_col — count exceeds index buffer");  return 0; }
    if (quads      > cb->count) { vm->runtime_error("canvas: image_batch_col — count exceeds color buffer");  return 0; }
    ctx->dl.add_image_batch_col(tex,
        (const float    *)vb->data, quads,
        (const uint32_t *)ib->data,
        (const uint32_t *)cb->data);
    args[0] = val_nil();
    return 1;
}

/* ── path builder ────────────────────────────────────────────────────────── */

/* dl.path_move(x, y) — clear path and set start point */
static int nat_path_move(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 3) { vm->runtime_error("canvas: path_move(x,y)"); return 0; }
    ctx->dl.path_move(fval(args[1]), fval(args[2]));
    args[0] = val_nil();
    return 1;
}

/* dl.path_line_to(x, y) */
static int nat_path_line_to(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 3) { vm->runtime_error("canvas: path_line_to(x,y)"); return 0; }
    ctx->dl.path_line_to(fval(args[1]), fval(args[2]));
    args[0] = val_nil();
    return 1;
}

/* dl.path_arc_to(cx, cy, r, a_min, a_max) — angles in radians */
static int nat_path_arc_to(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 6) { vm->runtime_error("canvas: path_arc_to(cx,cy,r,a_min,a_max)"); return 0; }
    ctx->dl.path_arc_to(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]),fval(args[5]));
    args[0] = val_nil();
    return 1;
}

/* dl.path_bezier_cubic_to(cx1,cy1, cx2,cy2, x2,y2) */
static int nat_path_bezier_cubic_to(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 7) { vm->runtime_error("canvas: path_bezier_cubic_to(cx1,cy1,cx2,cy2,x2,y2)"); return 0; }
    ctx->dl.path_bezier_cubic_to(fval(args[1]),fval(args[2]),fval(args[3]),
                                 fval(args[4]),fval(args[5]),fval(args[6]));
    args[0] = val_nil();
    return 1;
}

/* dl.path_bezier_quad_to(cx, cy, x2, y2) */
static int nat_path_bezier_quad_to(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 5) { vm->runtime_error("canvas: path_bezier_quad_to(cx,cy,x2,y2)"); return 0; }
    ctx->dl.path_bezier_quad_to(fval(args[1]),fval(args[2]),fval(args[3]),fval(args[4]));
    args[0] = val_nil();
    return 1;
}

/* dl.path_fill(col) — tessellate path as filled convex polygon */
static int nat_path_fill(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    ctx->dl.path_fill(getcol(ctx,args,nargs,1));
    args[0] = val_nil();
    return 1;
}

/* dl.path_stroke(col [, thickness=1 [, closed=false]]) */
static int nat_path_stroke(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    float t      = nargs >= 3 ? fval(args[2]) : 1.f;
    bool  closed = nargs >= 4 ? bval(args[3]) : false;
    ctx->dl.path_stroke(getcol(ctx,args,nargs,1), t, closed);
    args[0] = val_nil();
    return 1;
}

/* dl.path_clear() */
static int nat_path_clear(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    ctx->dl.path_clear();
    args[0] = val_nil();
    return 1;
}

#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
/* dl.flush() — upload + draw + clear */
static int nat_flush(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (!g_backend.ready) {
        vm->runtime_error("canvas: backend not initialised — call canvas.backend_init(w,h) first");
        return 0;
    }
    if (!ctx->dl.is_ready())
        ctx->dl.set_white_pixel(g_backend.white_tex_id(),
                                g_backend.white_u(), g_backend.white_v());
    g_backend.flush(ctx->dl);
    args[0] = val_nil();
    return 1;
}
#endif

/* ── data accessors ──────────────────────────────────────────────────────── */

/* dl.vertex_count() → int */
static int nat_vertex_count(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    args[0] = val_int((int64_t)ctx->dl.verts.size());
    return 1;
}

/* dl.index_count() → int */
static int nat_index_count(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    args[0] = val_int((int64_t)ctx->dl.indices.size());
    return 1;
}

/* dl.cmd_count() → int */
static int nat_cmd_count(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    args[0] = val_int((int64_t)ctx->dl.cmds.size());
    return 1;
}

/* dl.vertex_bytes() → Uint8Array
** Raw vertex data: stride = 20 bytes per vertex
**   bytes  0- 3: float x
**   bytes  4- 7: float y
**   bytes  8-11: float u
**   bytes 12-15: float v
**   bytes 16-19: uint32_t col (RGBA, R in low byte) */
static int nat_vertex_bytes(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    GC &gc = vm->get_gc();
    int32_t nbytes = (int32_t)(ctx->dl.verts.size() * sizeof(canvas::Vertex));
    ObjBuffer *buf = new_buffer(&gc, BUF_UINT8, nbytes);
    if (nbytes > 0)
        memcpy(buf->data, ctx->dl.verts.data(), (size_t)nbytes);
    args[0] = val_obj((Obj *)buf);
    return 1;
}

/* dl.index_data() → Uint32Array */
static int nat_index_data(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    GC &gc = vm->get_gc();
    int32_t n = (int32_t)ctx->dl.indices.size();
    ObjBuffer *buf = new_buffer(&gc, BUF_UINT32, n);
    if (n > 0)
        memcpy(buf->data, ctx->dl.indices.data(), (size_t)n * sizeof(uint32_t));
    args[0] = val_obj((Obj *)buf);
    return 1;
}

/* dl.commands() → array of arrays
** Each inner array: [tex, cx, cy, cw, ch, idx_off, idx_count]
**   [0] tex       — GPU texture handle (int)
**   [1] cx        — scissor x
**   [2] cy        — scissor y
**   [3] cw        — scissor width
**   [4] ch        — scissor height
**   [5] idx_off   — first index in index buffer
**   [6] idx_count — number of indices */
static int nat_commands(VM *vm, Value *args, int /*nargs*/)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    GC &gc = vm->get_gc();

    /* Root outer array immediately so GC sees it throughout the loop. */
    ObjArray *arr = new_array(&gc);
    args[0] = val_obj((Obj *)arr);

    for (const auto &cmd : ctx->dl.cmds)
    {
        /* Root inner array in args[1] before any further allocation. */
        ObjArray *row = new_array(&gc);
        args[1] = val_obj((Obj *)row);

        array_push(&gc, row, val_int((int64_t)cmd.texture));
        array_push(&gc, row, val_float((double)cmd.cx));
        array_push(&gc, row, val_float((double)cmd.cy));
        array_push(&gc, row, val_float((double)cmd.cw));
        array_push(&gc, row, val_float((double)cmd.ch));
        array_push(&gc, row, val_int((int64_t)cmd.idx_offset));
        array_push(&gc, row, val_int((int64_t)cmd.idx_count));

        array_push(&gc, arr, val_obj((Obj *)row)); /* arr rooted via args[0] */
    }

    /* args[0] already holds the result array. */
    return 1;
}

/* =========================================================
** Font functions
** ========================================================= */

/* canvas.font_load(path, px_height) → Font
** Loads a TTF, bakes ASCII printable chars, uploads atlas to GL (RGBA8).
** Returns a Font instance or nil on failure. */
static int nat_font_load(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_string(args[0])) {
        vm->runtime_error("canvas: font_load(path [, px_height])");
        return 0;
    }
    float px_h = (nargs >= 2) ? fval(args[1]) : 16.f;
    if (px_h <= 0.f) px_h = 16.f;

    long file_size = 0;
    char *bytes = vm->read_file(as_string(args[0])->chars, nullptr, &file_size);
    if (!bytes || file_size <= 0) {
        free(bytes);
        args[0] = val_nil();
        return 1;
    }

    /* choose atlas size based on px_height */
    int aw = 512, ah = 256;
    if (px_h <= 16.f) { aw = 256; ah = 128; }
    else if (px_h <= 32.f) { aw = 512; ah = 256; }
    else { aw = 1024; ah = 512; }

    FontCtx *fc = new FontCtx();
    fc->first_char = 32;
    fc->num_chars  = 96;
    fc->px_height  = px_h;
    fc->atlas_w    = aw;
    fc->atlas_h    = ah;

    uint8_t *bitmap = (uint8_t *)malloc((size_t)(aw * ah));
    if (!bitmap) { delete fc; args[0] = val_nil(); free(bytes); return 1; }
    memset(bitmap, 0, (size_t)(aw * ah));

    int result = stbtt_BakeFontBitmap(
        (const unsigned char *)bytes, 0, px_h,
        bitmap, aw, ah,
        fc->first_char, fc->num_chars,
        fc->chardata);

    free(bytes);

    if (result < 0) { free(bitmap); delete fc; args[0] = val_nil(); return 1; }

#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
    /* Convert R8 → RGBA8 and upload */
    uint8_t *rgba = (uint8_t *)malloc((size_t)(aw * ah * 4));
    if (!rgba) { free(bitmap); delete fc; args[0] = val_nil(); return 1; }
    for (int p = 0; p < aw * ah; p++) {
        rgba[p*4+0] = 255; rgba[p*4+1] = 255;
        rgba[p*4+2] = 255; rgba[p*4+3] = bitmap[p];
    }
    free(bitmap);

    unsigned int tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aw, ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(rgba);
    fc->tex = (uint64_t)tex_id;
#else
    free(bitmap);
#endif

    Value inst_val = vm->make_instance(g_font_class);
    as_instance(inst_val)->native_data = fc;
    args[0] = inst_val;
    return 1;
}

/* dl.draw_text(font, text, x, y [, col]) → x_end
** Renders text into the DrawList using the font atlas.
** Returns the X position after the last glyph (for cursor/layout). */
static int nat_draw_text(VM *vm, Value *args, int nargs)
{
    CanvasCtx *ctx = get_ctx(vm, args); if (!ctx) return 0;
    if (nargs < 4 || !is_instance(args[1]) || !is_string(args[2])) {
        vm->runtime_error("canvas: draw_text(font, text, x, y [, col])");
        return 0;
    }
    FontCtx *fc = (FontCtx *)as_instance(args[1])->native_data;
    if (!fc) { vm->runtime_error("canvas: draw_text — invalid font"); return 0; }

    ObjString *s = as_string(args[2]);
    float x   = fval(args[3]);
    float fy  = (nargs >= 5) ? fval(args[4]) : 0.f;
    uint32_t col = getcol(ctx, args, nargs, 5);

    float x_end = ctx->dl.add_text(
        fc->tex,
        (const canvas::DrawList::BakedChar *)fc->chardata,
        fc->first_char, fc->num_chars,
        (float)fc->atlas_w, (float)fc->atlas_h,
        s->chars, s->length,
        x, fy, col);

    args[0] = val_float((double)x_end);
    return 1;
}

/* canvas.font_measure(font, text) → (width, height) */
static int nat_font_measure(VM *vm, Value *args, int nargs)
{
    if (nargs < 2 || !is_instance(args[0]) || !is_string(args[1])) {
        vm->runtime_error("canvas: font_measure(font, text)");
        return 0;
    }
    FontCtx *fc = (FontCtx *)as_instance(args[0])->native_data;
    if (!fc) { args[0] = val_float(0.0); args[1] = val_float(0.0); return 2; }
    ObjString *s = as_string(args[1]);
    float w = 0.f;
    for (int i = 0; i < s->length; i++) {
        int idx = (unsigned char)s->chars[i] - fc->first_char;
        if (idx >= 0 && idx < fc->num_chars)
            w += fc->chardata[idx].xadvance;
        else
            w += 4.f;
    }
    args[0] = val_float((double)w);
    args[1] = val_float((double)fc->px_height);
    return 2;
}

/* =========================================================
** Class + module registration
** ========================================================= */

static void canvas_module_init(VM *vm)
{
    g_canvas_class = vm->def_class("DrawList")
        .dtor(canvas_dtor)
        /* setup */
        .method("set_white_pixel",        nat_set_white_pixel,        3)
        .method("clear",                  nat_clear,                  0)
        .method("push_clip",              nat_push_clip,              4)
        .method("pop_clip",               nat_pop_clip,               0)
        /* color state */
        .method("set_color",              nat_set_color,              3)
        .method("set_rgba",               nat_set_rgba,               4)
        .method("set_alpha",              nat_set_alpha,              1)
        .method("get_color",              nat_get_color,              0)
        /* shapes — filled */
        .method("rect",                   nat_rect,                  -1)
        .method("rect_gradient",          nat_rect_gradient,          8)
        .method("round_rect",             nat_round_rect,            -1)
        .method("circle",                 nat_circle,                -1)
        .method("ellipse",                nat_ellipse,               -1)
        .method("ngon",                   nat_ngon,                  -1)
        .method("triangle",               nat_triangle,              -1)
        /* shapes — outline */
        .method("rect_outline",           nat_rect_outline,          -1)
        .method("round_rect_outline",     nat_round_rect_outline,    -1)
        .method("circle_outline",         nat_circle_outline,        -1)
        .method("ellipse_outline",        nat_ellipse_outline,       -1)
        .method("ngon_outline",           nat_ngon_outline,          -1)
        .method("triangle_outline",       nat_triangle_outline,      -1)
        /* lines / curves */
        .method("line",                   nat_line,                  -1)
        .method("bezier_cubic",           nat_bezier_cubic,          -1)
        .method("bezier_quad",            nat_bezier_quad,           -1)
        /* image */
        .method("image",                  nat_image,                 -1)
        .method("image_region",           nat_image_region,          -1)
        .method("image_vertices",         nat_image_vertices,        -1)
        .method("image_batch",            nat_image_batch,           -1)
        .method("image_batch_col",        nat_image_batch_col,       -1)
        /* text */
        .method("draw_text",               nat_draw_text,             -1)
        /* path builder */
        .method("path_move",              nat_path_move,              2)
        .method("path_line_to",           nat_path_line_to,           2)
        .method("path_arc_to",            nat_path_arc_to,            5)
        .method("path_bezier_cubic_to",   nat_path_bezier_cubic_to,   6)
        .method("path_bezier_quad_to",    nat_path_bezier_quad_to,    4)
        .method("path_fill",              nat_path_fill,             -1)
        .method("path_stroke",            nat_path_stroke,           -1)
        .method("path_clear",             nat_path_clear,             0)
#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
        .method("flush",                  nat_flush,                  0)
#endif
        /* data accessors */
        .method("vertex_count",           nat_vertex_count,           0)
        .method("index_count",            nat_index_count,            0)
        .method("cmd_count",              nat_cmd_count,              0)
        .method("vertex_bytes",           nat_vertex_bytes,           0)
        .method("index_data",             nat_index_data,             0)
        .method("commands",               nat_commands,               0)
        .end();

    g_font_class = vm->def_class("Font")
        .dtor(font_ctx_dtor)
        .end();
}

static const NativeReg canvas_functions[] = {
    {"new",         nat_new,          0},
    {"pack",        nat_pack,         4},
    {"font_load",   nat_font_load,   -1},
    {"font_measure",nat_font_measure, 2},
#if defined(ZEN_CANVAS_GL) || defined(ZEN_CANVAS_GLES)
    {"backend_init",    nat_backend_init,    2},
    {"backend_resize",  nat_backend_resize,  2},
    {"backend_destroy", nat_backend_destroy, 0},
#endif
};

extern const NativeLib zen_lib_canvas = {
    "canvas",
    canvas_functions,
    (int)(sizeof(canvas_functions) / sizeof(canvas_functions[0])),
    nullptr,
    0,
    canvas_module_init,
};

} /* namespace zen */
