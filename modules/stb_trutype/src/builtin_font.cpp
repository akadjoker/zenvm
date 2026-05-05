#include "zen/module_font.h"

#ifdef ZEN_ENABLE_STB_FONT

#include "object.h"
#include "memory.h"
#include "vm.h"

#include <cstdlib>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace zen
{
    /* -------------------------------------------------------
    ** Internal helpers
    ** ------------------------------------------------------ */
    static bool font_src(Value v, const unsigned char **ptr, int *len)
    {
        if (is_buffer(v)) {
            ObjBuffer *b = as_buffer(v);
            if (b->btype != BUF_UINT8) return false;
            *ptr = (const unsigned char *)b->data;
            *len = (int)b->count;
            return true;
        }
        if (is_string(v)) {
            ObjString *s = as_string(v);
            *ptr = (const unsigned char *)s->chars;
            *len = s->length;
            return true;
        }
        return false;
    }

    /* -------------------------------------------------------
    ** font.load(path) → Uint8Array
    **
    ** Reads a TTF/OTF file from disk and returns the raw bytes
    ** as a Uint8Array.  Keep this buffer alive for the lifetime
    ** of any baked chardata derived from it.
    ** ------------------------------------------------------ */
    static int nat_font_load(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("font.load() expects (path).");
            return 0;
        }

        long file_size = 0;
        char *bytes = vm->read_file(as_string(args[0])->chars, nullptr, &file_size);
        if (!bytes || file_size <= 0)
        {
            free(bytes);
            args[0] = val_nil();
            return 1;
        }

        ObjBuffer *out = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)file_size);
        memcpy(out->data, bytes, (size_t)file_size);
        free(bytes);

        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* -------------------------------------------------------
    ** font.bake(ttf, pixel_height, atlas_w, atlas_h,
    **           first_char, num_chars) →
    **   bitmap (Uint8Array atlas_w*atlas_h, 1-ch gray),
    **   atlas_w, atlas_h,
    **   chardata (Float32Array  num_chars × 7:
    **             [x0,y0,x1,y1, xoff,yoff, xadvance])
    **
    ** Returns nil on failure (font not found, atlas too small, etc.).
    ** ------------------------------------------------------ */
    static int nat_font_bake(VM *vm, Value *args, int nargs)
    {
        if (nargs < 6)
        {
            vm->runtime_error("font.bake() expects (ttf, pixel_height, atlas_w, atlas_h, first_char, num_chars).");
            return 0;
        }

        const unsigned char *ttf_ptr = nullptr;
        int ttf_len = 0;
        if (!font_src(args[0], &ttf_ptr, &ttf_len))
        {
            vm->runtime_error("font.bake(): ttf must be a Uint8Array or string.");
            return 0;
        }

        float px_height = 0.0f;
        if (is_float(args[1]))      px_height = (float)args[1].as.number;
        else if (is_int(args[1]))   px_height = (float)args[1].as.integer;
        else { vm->runtime_error("font.bake(): pixel_height must be a number."); return 0; }

        int atlas_w = 0, atlas_h = 0;
        if (!is_int(args[2]) || !is_int(args[3]))
        {
            vm->runtime_error("font.bake(): atlas_w and atlas_h must be integers.");
            return 0;
        }
        atlas_w = (int)args[2].as.integer;
        atlas_h = (int)args[3].as.integer;

        if (!is_int(args[4]) || !is_int(args[5]))
        {
            vm->runtime_error("font.bake(): first_char and num_chars must be integers.");
            return 0;
        }
        int first_char = (int)args[4].as.integer;
        int num_chars  = (int)args[5].as.integer;

        if (atlas_w <= 0 || atlas_h <= 0 || num_chars <= 0 || px_height <= 0.0f)
        {
            args[0] = val_nil();
            return 1;
        }

        stbtt_bakedchar *chardata = (stbtt_bakedchar *)malloc(sizeof(stbtt_bakedchar) * num_chars);
        if (!chardata) { args[0] = val_nil(); return 1; }

        /* Allocate atlas bitmap */
        int64_t atlas_pixels = (int64_t)atlas_w * (int64_t)atlas_h;
        if (atlas_pixels <= 0 || atlas_pixels > 0x7fffffff)
        {
            free(chardata);
            vm->runtime_error("font.bake(): atlas too large.");
            return 0;
        }
        ObjBuffer *bitmap = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)atlas_pixels);
        memset(bitmap->data, 0, (size_t)atlas_pixels);

        int result = stbtt_BakeFontBitmap(
            ttf_ptr, 0,
            px_height,
            (unsigned char *)bitmap->data, atlas_w, atlas_h,
            first_char, num_chars,
            chardata);

        if (result <= 0)
        {
            /* result == 0 or negative means atlas too small (some glyphs didn't fit) */
            /* We still return the partial bitmap; caller can check chardata[i].x0 != 0 */
            /* For hard failure, result < 0 means nothing fitted at all */
            if (result < 0)
            {
                free(chardata);
                args[0] = val_nil();
                return 1;
            }
        }

        /* Pack chardata into Float32Array: 7 floats per char */
        ObjBuffer *cd_buf = new_buffer(&vm->get_gc(), BUF_FLOAT32, num_chars * 7);
        float *cd = (float *)cd_buf->data;
        for (int i = 0; i < num_chars; i++)
        {
            cd[i * 7 + 0] = (float)chardata[i].x0;
            cd[i * 7 + 1] = (float)chardata[i].y0;
            cd[i * 7 + 2] = (float)chardata[i].x1;
            cd[i * 7 + 3] = (float)chardata[i].y1;
            cd[i * 7 + 4] = chardata[i].xoff;
            cd[i * 7 + 5] = chardata[i].yoff;
            cd[i * 7 + 6] = chardata[i].xadvance;
        }
        free(chardata);

        args[0] = val_obj((Obj *)bitmap);
        args[1] = val_int(atlas_w);
        args[2] = val_int(atlas_h);
        args[3] = val_obj((Obj *)cd_buf);
        return 4;
    }

    /* -------------------------------------------------------
    ** font.quad(chardata, atlas_w, atlas_h, first_char,
    **           codepoint, xpos, ypos) →
    **   x0, y0, x1, y1,          (screen rect, floats)
    **   s0, t0, s1, t1,          (UV in [0,1])
    **   xadvance                  (how much to advance xpos)
    **
    ** Call this per character when building a text mesh.
    ** xpos and ypos are the pen position (top-left baseline).
    ** Returns nil if codepoint is out of range.
    ** ------------------------------------------------------ */
    static int nat_font_quad(VM *vm, Value *args, int nargs)
    {
        if (nargs < 7)
        {
            vm->runtime_error("font.quad() expects (chardata, atlas_w, atlas_h, first_char, codepoint, xpos, ypos).");
            return 0;
        }

        if (!is_buffer(args[0]) || as_buffer(args[0])->btype != BUF_FLOAT32)
        {
            vm->runtime_error("font.quad(): chardata must be a Float32Array (from font.bake).");
            return 0;
        }

        ObjBuffer *cd_buf = as_buffer(args[0]);
        int atlas_w   = is_int(args[1]) ? (int)args[1].as.integer : 0;
        int atlas_h   = is_int(args[2]) ? (int)args[2].as.integer : 0;
        int first_char = is_int(args[3]) ? (int)args[3].as.integer : 32;
        int codepoint  = is_int(args[4]) ? (int)args[4].as.integer : 0;

        float xpos = 0.0f, ypos = 0.0f;
        if (is_float(args[5])) xpos = (float)args[5].as.number;
        else if (is_int(args[5])) xpos = (float)args[5].as.integer;
        if (is_float(args[6])) ypos = (float)args[6].as.number;
        else if (is_int(args[6])) ypos = (float)args[6].as.integer;

        int num_chars = (int)(cd_buf->count / 7);
        int idx = codepoint - first_char;
        if (idx < 0 || idx >= num_chars || atlas_w <= 0 || atlas_h <= 0)
        {
            args[0] = val_nil();
            return 1;
        }

        /* Reconstruct stbtt_bakedchar from Float32 storage */
        float *cd = (float *)cd_buf->data;
        stbtt_bakedchar bc;
        bc.x0 = (unsigned short)cd[idx * 7 + 0];
        bc.y0 = (unsigned short)cd[idx * 7 + 1];
        bc.x1 = (unsigned short)cd[idx * 7 + 2];
        bc.y1 = (unsigned short)cd[idx * 7 + 3];
        bc.xoff = cd[idx * 7 + 4];
        bc.yoff = cd[idx * 7 + 5];
        bc.xadvance = cd[idx * 7 + 6];

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(&bc, atlas_w, atlas_h, 0 /* char_index 0 = we pre-selected bc */,
                           &xpos, &ypos, &q, 1 /* opengl = top-left origin */);

        args[0] = val_float(q.x0);
        args[1] = val_float(q.y0);
        args[2] = val_float(q.x1);
        args[3] = val_float(q.y1);
        args[4] = val_float(q.s0);
        args[5] = val_float(q.t0);
        args[6] = val_float(q.s1);
        args[7] = val_float(q.t1);
        args[8] = val_float(bc.xadvance);
        return 9;
    }

    /* -------------------------------------------------------
    ** font.metrics(ttf, pixel_height) → ascent, descent, line_gap
    **
    ** Returns the vertical metrics of the font scaled to pixel_height.
    ** ascent  > 0 (pixels above baseline)
    ** descent < 0 (pixels below baseline)
    ** line_gap = additional spacing between lines
    ** ------------------------------------------------------ */
    static int nat_font_metrics(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2)
        {
            vm->runtime_error("font.metrics() expects (ttf, pixel_height).");
            return 0;
        }

        const unsigned char *ttf_ptr = nullptr;
        int ttf_len = 0;
        if (!font_src(args[0], &ttf_ptr, &ttf_len))
        {
            vm->runtime_error("font.metrics(): ttf must be a Uint8Array or string.");
            return 0;
        }

        float px_height = 0.0f;
        if (is_float(args[1]))    px_height = (float)args[1].as.number;
        else if (is_int(args[1])) px_height = (float)args[1].as.integer;
        else { vm->runtime_error("font.metrics(): pixel_height must be a number."); return 0; }

        stbtt_fontinfo fi;
        if (!stbtt_InitFont(&fi, ttf_ptr, stbtt_GetFontOffsetForIndex(ttf_ptr, 0)))
        {
            args[0] = val_nil();
            return 1;
        }

        float scale = stbtt_ScaleForPixelHeight(&fi, px_height);
        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&fi, &ascent, &descent, &line_gap);

        args[0] = val_float(ascent    * scale);
        args[1] = val_float(descent   * scale);
        args[2] = val_float(line_gap  * scale);
        return 3;
    }

    /* -------------------------------------------------------
    ** font.measure(chardata, first_char, text) → width, height
    **
    ** Measures the pixel width and height of a string using
    ** the baked chardata (no re-rendering needed).
    ** height = ascent - descent of the tallest glyph rendered.
    ** ------------------------------------------------------ */
    static int nat_font_measure(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3 || !is_buffer(args[0]) || !is_int(args[1]) || !is_string(args[2]))
        {
            vm->runtime_error("font.measure() expects (chardata, first_char, text).");
            return 0;
        }

        ObjBuffer *cd_buf = as_buffer(args[0]);
        if (cd_buf->btype != BUF_FLOAT32)
        {
            vm->runtime_error("font.measure(): chardata must be a Float32Array.");
            return 0;
        }

        int first_char = (int)args[1].as.integer;
        ObjString *text = as_string(args[2]);
        float *cd = (float *)cd_buf->data;
        int num_chars = (int)(cd_buf->count / 7);

        float width = 0.0f;
        float max_y1 = 0.0f;
        float min_y0 = 0.0f;

        for (int i = 0; i < text->length; i++)
        {
            unsigned char c = (unsigned char)text->chars[i];
            int idx = (int)c - first_char;
            if (idx < 0 || idx >= num_chars) continue;
            float xadv = cd[idx * 7 + 6];
            float yoff = cd[idx * 7 + 5];
            float y0   = cd[idx * 7 + 1];
            float y1   = cd[idx * 7 + 3];
            width += xadv;
            if (yoff + (y1 - y0) > max_y1) max_y1 = yoff + (y1 - y0);
            if (yoff < min_y0) min_y0 = yoff;
        }

        args[0] = val_float(width);
        args[1] = val_float(max_y1 - min_y0);
        return 2;
    }

    static const NativeReg font_funcs[] = {
        {"load",    nat_font_load,    1},
        {"bake",    nat_font_bake,    6},
        {"quad",    nat_font_quad,    7},
        {"metrics", nat_font_metrics, 2},
        {"measure", nat_font_measure, 3},
    };

    const NativeLib zen_lib_font = {
        "font", font_funcs, 5, nullptr, 0
    };
}

#endif /* ZEN_ENABLE_STB_FONT */
