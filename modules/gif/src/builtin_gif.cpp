/* =========================================================
** builtin_gif.cpp — "gif" module for Zen
**
** Records animated GIFs from raw RGBA pixel buffers using
** msf_gif (single-header library, zero external deps).
**
** Two usage modes:
**
**   -- MODE 1: write directly to file (memory-efficient) --
**   import gif;
**   var w = gif.create("out.gif", 320, 240);
**   while (recording) {
**       var buf = capture_rgba_pixels();   // Uint8Array, 320*240*4 bytes
**       w.frame(buf, 4);                   // 4 = 40ms = 25 fps
**   }
**   w.close();
**
**   -- MODE 2: collect in memory, then save --
**   import gif;
**   var w = gif.begin(320, 240);
**   w.frame(buf, 4);
**   w.save("out.gif");   // encodes + writes atomically
**
** GifWriter methods:
**   .frame(buffer, centiseconds[, bitDepth=16])
**       buffer  — Uint8Array (or any buffer) with width*height*4 RGBA bytes
**       centiseconds — frame delay (2 = 50fps, 4 = 25fps, 10 = 10fps)
**       bitDepth     — colour depth 1..16 (16 = best quality)
**   .close()   — finalize file-mode GIF (mode 1 only)
**   .save(path)— finalize and save memory-mode GIF (mode 2 only)
**   .done      — true if already finalized
**
** gif module functions:
**   gif.begin(width, height)         → GifWriter (memory mode)
**   gif.create(path, width, height)  → GifWriter (file mode)
**   gif.bgra(bool)                   — set BGRA vs RGBA flag globally
**   gif.alpha(threshold)             — enable transparency (1..255)
** ========================================================= */

#define MSF_GIF_IMPL
#include "../vendor/msf_gif.h"

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace zen
{

/* =========================================================
** GifWriter native class
** ========================================================= */

struct GifCtx
{
    MsfGifState state;
    int         width;
    int         height;
    bool        file_mode; /* true = writing direct to file */
    bool        done;
    FILE       *fp;        /* file_mode only */
};

static ObjClass *g_gif_class = nullptr;

static void gif_dtor(VM * /*vm*/, void *ptr)
{
    GifCtx *ctx = (GifCtx *)ptr;
    if (!ctx->done)
    {
        if (ctx->file_mode)
        {
            msf_gif_end_to_file(&ctx->state);
            if (ctx->fp) { fclose(ctx->fp); ctx->fp = nullptr; }
        }
        else
        {
            MsfGifResult r = msf_gif_end(&ctx->state);
            msf_gif_free(r);
        }
        ctx->done = true;
    }
    free(ctx);
}

/* helper — get raw pixel pointer + byte count from a value */
static bool get_pixel_data(Value v, const uint8_t **out_data, int *out_bytes)
{
    if (is_buffer(v))
    {
        ObjBuffer *buf = as_buffer(v);
        int esz = buffer_elem_size[buf->btype];
        *out_data  = buf->data;
        *out_bytes = buf->count * esz;
        return true;
    }
    return false;
}

/* --- GifWriter.frame(buffer, centiseconds [, bitDepth]) --- */
static int nat_gif_frame(VM *vm, Value *args, int nargs)
{
    if (nargs < 3) { vm->runtime_error("gif.frame: expected (buffer, centiseconds[, bitDepth])"); return 0; }

    if (!is_instance(args[0])) { vm->runtime_error("gif.frame: invalid receiver"); return 0; }
    ObjInstance *inst = as_instance(args[0]);

    GifCtx *ctx = (GifCtx *)inst->native_data;
    if (!ctx || ctx->done) { vm->runtime_error("gif.frame: GifWriter already closed"); return 0; }

    const uint8_t *pixels = nullptr;
    int nbytes = 0;
    if (!get_pixel_data(args[1], &pixels, &nbytes))
    { vm->runtime_error("gif.frame: buffer must be a typed buffer (Uint8Array)"); return 0; }

    int expected = ctx->width * ctx->height * 4;
    if (nbytes < expected)
    {
        vm->runtime_error("gif.frame: buffer too small (%d bytes, need %d = %dx%dx4)",
                          nbytes, expected, ctx->width, ctx->height);
        return 0;
    }

    int centis   = is_int(args[2]) ? (int)args[2].as.integer
                                   : (int)args[2].as.number;
    int bitDepth = (nargs >= 4 && is_int(args[3])) ? (int)args[3].as.integer : 16;
    if (bitDepth < 1)  bitDepth = 1;
    if (bitDepth > 16) bitDepth = 16;

    int pitch = ctx->width * 4;

    int ok;
    if (ctx->file_mode)
        ok = msf_gif_frame_to_file(&ctx->state, (uint8_t *)pixels, centis, bitDepth, pitch);
    else
        ok = msf_gif_frame(&ctx->state, (uint8_t *)pixels, centis, bitDepth, pitch);

    if (!ok) { vm->runtime_error("gif.frame: encoding failed"); return 0; }
    args[0] = val_bool(true);
    return 1;
}

/* --- GifWriter.close() — finalize file-mode --- */
static int nat_gif_close(VM *vm, Value *args, int /*nargs*/)
{
    if (!is_instance(args[0])) { vm->runtime_error("gif.close: invalid receiver"); return 0; }
    ObjInstance *inst = as_instance(args[0]);
    GifCtx *ctx = (GifCtx *)inst->native_data;
    if (!ctx) { vm->runtime_error("gif.close: invalid GifWriter"); return 0; }
    if (ctx->done) { args[0] = val_bool(true); return 1; }

    int ok = 1;
    if (ctx->file_mode)
    {
        ok = msf_gif_end_to_file(&ctx->state);
        if (ctx->fp) { fclose(ctx->fp); ctx->fp = nullptr; }
    }
    else
    {
        /* memory mode: discard */
        MsfGifResult r = msf_gif_end(&ctx->state);
        msf_gif_free(r);
    }
    ctx->done = true;
    args[0] = val_bool(ok != 0);
    return 1;
}

/* --- GifWriter.save(path) — finalize memory-mode, write file --- */
static int nat_gif_save(VM *vm, Value *args, int nargs)
{
    if (nargs < 2 || !is_string(args[1]))
    { vm->runtime_error("gif.save: expected (path)"); return 0; }

    if (!is_instance(args[0])) { vm->runtime_error("gif.save: invalid receiver"); return 0; }
    ObjInstance *inst = as_instance(args[0]);
    GifCtx *ctx = (GifCtx *)inst->native_data;
    if (!ctx || ctx->done) { vm->runtime_error("gif.save: GifWriter already closed"); return 0; }

    MsfGifResult result = msf_gif_end(&ctx->state);
    ctx->done = true;

    if (!result.data)
    { vm->runtime_error("gif.save: encoding produced no data"); return 0; }

    const char *path = as_string(args[1])->chars;
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        msf_gif_free(result);
        vm->runtime_error("gif.save: cannot open '%s' for writing", path);
        return 0;
    }
    size_t written = fwrite(result.data, 1, result.dataSize, f);
    fclose(f);
    msf_gif_free(result);

    args[0] = val_bool(written == result.dataSize);
    return 1;
}

/* --- GifWriter.width / height / done (read-only fields set at construction) --- */

/* =========================================================
** Module functions
** ========================================================= */

static GifCtx *make_ctx(VM *vm, int width, int height,
                         bool file_mode, FILE *fp,
                         Value *out)
{
    GifCtx *ctx = (GifCtx *)calloc(1, sizeof(GifCtx));
    if (!ctx) { vm->runtime_error("gif: out of memory"); return nullptr; }
    ctx->width     = width;
    ctx->height    = height;
    ctx->file_mode = file_mode;
    ctx->done      = false;
    ctx->fp        = fp;

    Value inst_val = vm->make_instance(g_gif_class);
    if (!is_instance(inst_val)) { free(ctx); vm->runtime_error("gif: failed to create instance"); return nullptr; }
    ObjInstance *inst = as_instance(inst_val);
    inst->native_data = ctx;

    *out = inst_val;
    return ctx;
}

/* gif.begin(width, height) → GifWriter (memory mode) */
static int nat_gif_begin(VM *vm, Value *args, int nargs)
{
    if (nargs < 2) { vm->runtime_error("gif.begin: expected (width, height)"); return 0; }
    int w = (int)(is_int(args[0]) ? args[0].as.integer : (int64_t)args[0].as.number);
    int h = (int)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    if (w <= 0 || h <= 0) { vm->runtime_error("gif.begin: invalid dimensions"); return 0; }

    GifCtx *ctx = make_ctx(vm, w, h, false, nullptr, &args[0]);
    if (!ctx) return 0;

    if (!msf_gif_begin(&ctx->state, w, h))
    {
        ctx->done = true;
        vm->runtime_error("gif.begin: msf_gif_begin failed");
        return 0;
    }
    return 1;
}

/* gif.create(path, width, height) → GifWriter (file mode) */
static int nat_gif_open(VM *vm, Value *args, int nargs)
{
    if (nargs < 3 || !is_string(args[0]))
    { vm->runtime_error("gif.create: expected (path, width, height)"); return 0; }

    const char *path = as_string(args[0])->chars;
    int w = (int)(is_int(args[1]) ? args[1].as.integer : (int64_t)args[1].as.number);
    int h = (int)(is_int(args[2]) ? args[2].as.integer : (int64_t)args[2].as.number);
    if (w <= 0 || h <= 0) { vm->runtime_error("gif.open: invalid dimensions"); return 0; }

    FILE *fp = fopen(path, "wb");
    if (!fp) { vm->runtime_error("gif.open: cannot create '%s'", path); return 0; }

    GifCtx *ctx = make_ctx(vm, w, h, true, fp, &args[0]);
    if (!ctx) { fclose(fp); return 0; }

    if (!msf_gif_begin_to_file(&ctx->state, w, h, (MsfGifFileWriteFunc)fwrite, fp))
    {
        fclose(fp); ctx->done = true;
        vm->runtime_error("gif.create: failed to initialize encoder");
        return 0;
    }
    return 1;
}

/* gif.bgra(bool) — set BGRA vs RGBA pixel order */
static int nat_gif_bgra(VM * /*vm*/, Value *args, int nargs)
{
    bool flag = (nargs >= 1 && is_bool(args[0])) ? args[0].as.boolean : false;
    msf_gif_bgra_flag = flag ? 1 : 0;
    return 0;
}

/* gif.alpha(threshold) — enable/disable transparency (0 = off, 1-255 = on) */
static int nat_gif_alpha(VM * /*vm*/, Value *args, int nargs)
{
    int t = (nargs >= 1 && is_int(args[0])) ? (int)args[0].as.integer : 0;
    msf_gif_alpha_threshold = t;
    return 0;
}

/* =========================================================
** Class registration
** ========================================================= */

static void gif_init(VM *vm)
{
    g_gif_class = vm->def_class("GifWriter")
        .dtor(gif_dtor)
        .method("frame",  nat_gif_frame, -1)
        .method("close",  nat_gif_close,  0)
        .method("save",   nat_gif_save,   1)
        .end();
}

/* =========================================================
** Module table
** ========================================================= */

static const NativeReg gif_functions[] = {
    {"begin",  nat_gif_begin, 2},
    {"create", nat_gif_open,  3},
    {"bgra",   nat_gif_bgra,  1},
    {"alpha",  nat_gif_alpha, 1},
};

extern const NativeLib zen_lib_gif = {
    "gif",
    gif_functions,
    4,
    nullptr,
    0,
    gif_init,
};

} /* namespace zen */
