#include "zen/module_image.h"

#ifdef ZEN_ENABLE_STB_IMAGE

#include "memory.h"
#include "object.h"
#include "vm.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include <cstdlib>
#include <cstring>

namespace zen
{
    /* =========================================================
    ** Growing buffer used by stbi_write_*_to_func callbacks
    ** ========================================================= */
    struct EncBuf {
        unsigned char *data;
        int            size;
        int            cap;

        EncBuf() : data(nullptr), size(0), cap(0) {}
        ~EncBuf() { free(data); }

        bool append(const void *src, int n) {
            if (size + n > cap) {
                int new_cap = cap ? cap * 2 : 4096;
                while (new_cap < size + n) new_cap *= 2;
                unsigned char *p = (unsigned char *)realloc(data, new_cap);
                if (!p) return false;
                data = p; cap = new_cap;
            }
            memcpy(data + size, src, n);
            size += n;
            return true;
        }
    };

    static void enc_callback(void *ctx, void *data, int size) {
        ((EncBuf *)ctx)->append(data, size);
    }
    static bool read_image_file(VM *vm, ObjString *path, unsigned char **data, long *size)
    {
        *data = nullptr;
        *size = 0;

        char *bytes = vm->read_file(path->chars, nullptr, size);
        if (!bytes || *size <= 0)
        {
            free(bytes);
            return false;
        }

        *data = (unsigned char *)bytes;
        return true;
    }

    static bool get_desired_channels(Value *args, int nargs, int *desired)
    {
        *desired = 0;
        if (nargs < 2 || is_nil(args[1]))
            return true;
        if (!is_int(args[1]))
            return false;
        int v = (int)args[1].as.integer;
        if (v < 0 || v > 4)
            return false;
        *desired = v;
        return true;
    }

    static bool get_int_arg(Value *args, int index, int *out)
    {
        if (!is_int(args[index]))
            return false;
        *out = (int)args[index].as.integer;
        return true;
    }

    static bool valid_components(int components)
    {
        return components >= 1 && components <= 4;
    }

    static bool buffer_has_image_size(ObjBuffer *buffer, int width, int height, int components)
    {
        if (!buffer || buffer->btype != BUF_UINT8 || width <= 0 || height <= 0 || !valid_components(components))
            return false;
        int64_t required = (int64_t)width * (int64_t)height * (int64_t)components;
        return required >= 0 && required <= buffer->count;
    }

    /* info(path) -> width, height, components */
    static int nat_image_info(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("image.info() expects (path).");
            return 0;
        }

        ObjString *path = as_string(args[0]);
        unsigned char *file_data = nullptr;
        long file_size = 0;
        if (!read_image_file(vm, path, &file_data, &file_size))
        {
            args[0] = val_nil();
            return 1;
        }

        int w = 0;
        int h = 0;
        int components = 0;
        if (!stbi_info_from_memory(file_data, (int)file_size, &w, &h, &components))
        {
            free(file_data);
            args[0] = val_nil();
            return 1;
        }
        free(file_data);

        args[0] = val_int(w);
        args[1] = val_int(h);
        args[2] = val_int(components);
        return 3;
    }

    /* load(path, desired_channels?) -> width, height, components, Uint8Array */
    static int nat_image_load(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("image.load() expects (path, desired_channels?).");
            return 0;
        }

        int desired = 0;
        if (!get_desired_channels(args, nargs, &desired))
        {
            vm->runtime_error("image.load(): desired_channels must be 0, 1, 2, 3, or 4.");
            return 0;
        }

        ObjString *path = as_string(args[0]);
        unsigned char *file_data = nullptr;
        long file_size = 0;
        if (!read_image_file(vm, path, &file_data, &file_size))
        {
            args[0] = val_nil();
            return 1;
        }

        int w = 0;
        int h = 0;
        int file_components = 0;
        unsigned char *pixels = stbi_load_from_memory(file_data, (int)file_size, &w, &h, &file_components, desired);
        free(file_data);
        if (!pixels)
        {
            args[0] = val_nil();
            return 1;
        }

        int out_components = desired > 0 ? desired : file_components;
        int64_t count64 = (int64_t)w * (int64_t)h * (int64_t)out_components;
        if (count64 < 0 || count64 > 0x7fffffff)
        {
            stbi_image_free(pixels);
            vm->runtime_error("image.load(): image is too large.");
            return 0;
        }

        ObjBuffer *buffer = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)count64);
        memcpy(buffer->data, pixels, (size_t)count64);
        stbi_image_free(pixels);

        args[0] = val_int(w);
        args[1] = val_int(h);
        args[2] = val_int(out_components);
        args[3] = val_obj((Obj *)buffer);
        return 4;
    }

    /* resize(pixels, width, height, components, new_width, new_height, srgb?) -> Uint8Array */
    static int nat_image_resize(VM *vm, Value *args, int nargs)
    {
        if (nargs < 6 || !is_buffer(args[0]))
        {
            vm->runtime_error("image.resize() expects (pixels, width, height, components, new_width, new_height, srgb?).");
            return 0;
        }

        int width = 0;
        int height = 0;
        int components = 0;
        int new_width = 0;
        int new_height = 0;
        if (!get_int_arg(args, 1, &width) || !get_int_arg(args, 2, &height) ||
            !get_int_arg(args, 3, &components) || !get_int_arg(args, 4, &new_width) ||
            !get_int_arg(args, 5, &new_height))
        {
            vm->runtime_error("image.resize(): dimensions and components must be integers.");
            return 0;
        }

        ObjBuffer *src = as_buffer(args[0]);
        if (!buffer_has_image_size(src, width, height, components) || new_width <= 0 || new_height <= 0)
        {
            vm->runtime_error("image.resize(): invalid image dimensions, components, or source buffer.");
            return 0;
        }

        int64_t out_count = (int64_t)new_width * (int64_t)new_height * (int64_t)components;
        if (out_count < 0 || out_count > 0x7fffffff)
        {
            vm->runtime_error("image.resize(): output image is too large.");
            return 0;
        }

        ObjBuffer *dst = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)out_count);
        bool srgb = nargs >= 7 && is_bool(args[6]) && args[6].as.boolean;
        stbir_pixel_layout layout = (stbir_pixel_layout)components;
        unsigned char *ok = srgb
            ? stbir_resize_uint8_srgb(src->data, width, height, 0, dst->data, new_width, new_height, 0, layout)
            : stbir_resize_uint8_linear(src->data, width, height, 0, dst->data, new_width, new_height, 0, layout);
        if (!ok)
        {
            args[0] = val_nil();
            return 1;
        }

        args[0] = val_obj((Obj *)dst);
        return 1;
    }

    /* load_mem(buffer_or_string, desired_channels?) -> width, height, components, Uint8Array
    ** Decodes an image from a Uint8Array or string already in memory (e.g. file.read_all(),
    ** downloaded data, embedded assets).
    ** Returns nil on failure. */
    static int nat_image_load_mem(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || (!is_buffer(args[0]) && !is_string(args[0])))
        {
            vm->runtime_error("image.load_mem() expects (buffer_or_string, desired_channels?).");
            return 0;
        }

        int desired = 0;
        if (!get_desired_channels(args, nargs, &desired))
        {
            vm->runtime_error("image.load_mem(): desired_channels must be 0, 1, 2, 3, or 4.");
            return 0;
        }

        const stbi_uc *ptr = nullptr;
        int len = 0;
        if (is_buffer(args[0])) {
            ObjBuffer *src = as_buffer(args[0]);
            ptr = (const stbi_uc *)src->data;
            len = (int)src->count;
        } else {
            ObjString *src = as_string(args[0]);
            ptr = (const stbi_uc *)src->chars;
            len = src->length;
        }

        int w = 0, h = 0, file_components = 0;
        unsigned char *pixels = stbi_load_from_memory(ptr, len, &w, &h, &file_components, desired);
        if (!pixels)
        {
            args[0] = val_nil();
            return 1;
        }

        int out_components = desired > 0 ? desired : file_components;
        int64_t count64 = (int64_t)w * (int64_t)h * (int64_t)out_components;
        if (count64 < 0 || count64 > 0x7fffffff)
        {
            stbi_image_free(pixels);
            vm->runtime_error("image.load_mem(): image is too large.");
            return 0;
        }

        ObjBuffer *out = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)count64);
        memcpy(out->data, pixels, (size_t)count64);
        stbi_image_free(pixels);

        args[0] = val_int(w);
        args[1] = val_int(h);
        args[2] = val_int(out_components);
        args[3] = val_obj((Obj *)out);
        return 4;
    }

    /* info_mem(buffer_or_string) -> width, height, components
    ** Reads image header from an in-memory buffer without decoding pixels. */
    static int nat_image_info_mem(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || (!is_buffer(args[0]) && !is_string(args[0])))
        {
            vm->runtime_error("image.info_mem() expects (buffer_or_string).");
            return 0;
        }

        const stbi_uc *ptr = nullptr;
        int len = 0;
        if (is_buffer(args[0])) {
            ObjBuffer *src = as_buffer(args[0]);
            ptr = (const stbi_uc *)src->data;
            len = (int)src->count;
        } else {
            ObjString *src = as_string(args[0]);
            ptr = (const stbi_uc *)src->chars;
            len = src->length;
        }

        int w = 0, h = 0, components = 0;
        if (!stbi_info_from_memory(ptr, len, &w, &h, &components))
        {
            args[0] = val_nil();
            return 1;
        }

        args[0] = val_int(w);
        args[1] = val_int(h);
        args[2] = val_int(components);
        return 3;
    }

    /* encode(format, width, height, components, pixels, quality?) → Uint8Array or nil
    ** Encodes pixels to an in-memory byte buffer.
    ** format: "png", "bmp", "tga", "jpg"
    ** quality: only used for "jpg" (1-100, default 90) */
    static int nat_image_encode(VM *vm, Value *args, int nargs)
    {
        if (nargs < 5 || !is_string(args[0]) || !is_buffer(args[4]))
        {
            vm->runtime_error("image.encode() expects (format, width, height, components, pixels, quality?).");
            return 0;
        }

        int width = 0, height = 0, components = 0;
        if (!get_int_arg(args, 1, &width) || !get_int_arg(args, 2, &height) ||
            !get_int_arg(args, 3, &components))
        {
            vm->runtime_error("image.encode(): width, height and components must be integers.");
            return 0;
        }

        ObjBuffer *pixels = as_buffer(args[4]);
        if (!buffer_has_image_size(pixels, width, height, components))
        {
            vm->runtime_error("image.encode(): invalid image dimensions or pixel buffer.");
            return 0;
        }

        const char *fmt = as_string(args[0])->chars;
        EncBuf eb;
        int ok = 0;

        if (strcmp(fmt, "png") == 0)
            ok = stbi_write_png_to_func(enc_callback, &eb, width, height, components, pixels->data, width * components);
        else if (strcmp(fmt, "bmp") == 0)
            ok = stbi_write_bmp_to_func(enc_callback, &eb, width, height, components, pixels->data);
        else if (strcmp(fmt, "tga") == 0)
            ok = stbi_write_tga_to_func(enc_callback, &eb, width, height, components, pixels->data);
        else if (strcmp(fmt, "jpg") == 0) {
            int quality = 90;
            if (nargs >= 6 && is_int(args[5])) quality = (int)args[5].as.integer;
            if (quality < 1)   quality = 1;
            if (quality > 100) quality = 100;
            ok = stbi_write_jpg_to_func(enc_callback, &eb, width, height, components, pixels->data, quality);
        } else {
            vm->runtime_error("image.encode(): unknown format '%s' (use png, bmp, tga, jpg).", fmt);
            return 0;
        }

        if (!ok || eb.size <= 0) { args[0] = val_nil(); return 1; }

        ObjBuffer *out = new_buffer(&vm->get_gc(), BUF_UINT8, eb.size);
        memcpy(out->data, eb.data, eb.size);
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* set_flip_on_load(bool) — toggle global stbi flip flag (afecta load/load_mem seguintes) */
    static int nat_image_set_flip_on_load(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_bool(args[0]))
        {
            vm->runtime_error("image.set_flip_on_load() expects (bool).");
            return 0;
        }
        stbi_set_flip_vertically_on_load(args[0].as.boolean ? 1 : 0);
        return 0;
    }

    /* flip_v(pixels, w, h, ch) → Uint8Array — inverte linhas top↔bottom */
    static int nat_image_flip_v(VM *vm, Value *args, int nargs)
    {
        if (nargs < 4 || !is_buffer(args[0]))
        {
            vm->runtime_error("image.flip_v() expects (pixels, width, height, components).");
            return 0;
        }
        int w = 0, h = 0, ch = 0;
        if (!get_int_arg(args, 1, &w) || !get_int_arg(args, 2, &h) || !get_int_arg(args, 3, &ch))
        {
            vm->runtime_error("image.flip_v(): width, height, components must be integers.");
            return 0;
        }
        ObjBuffer *src = as_buffer(args[0]);
        if (!buffer_has_image_size(src, w, h, ch))
        {
            vm->runtime_error("image.flip_v(): invalid image dimensions or pixel buffer.");
            return 0;
        }
        int row_bytes = w * ch;
        ObjBuffer *dst = new_buffer(&vm->get_gc(), BUF_UINT8, src->count);
        unsigned char *s = (unsigned char *)src->data;
        unsigned char *d = (unsigned char *)dst->data;
        for (int y = 0; y < h; y++)
            memcpy(d + y * row_bytes, s + (h - 1 - y) * row_bytes, row_bytes);
        args[0] = val_obj((Obj *)dst);
        return 1;
    }

    /* flip_h(pixels, w, h, ch) → Uint8Array — espelha pixels left↔right em cada linha */
    static int nat_image_flip_h(VM *vm, Value *args, int nargs)
    {
        if (nargs < 4 || !is_buffer(args[0]))
        {
            vm->runtime_error("image.flip_h() expects (pixels, width, height, components).");
            return 0;
        }
        int w = 0, h = 0, ch = 0;
        if (!get_int_arg(args, 1, &w) || !get_int_arg(args, 2, &h) || !get_int_arg(args, 3, &ch))
        {
            vm->runtime_error("image.flip_h(): width, height, components must be integers.");
            return 0;
        }
        ObjBuffer *src = as_buffer(args[0]);
        if (!buffer_has_image_size(src, w, h, ch))
        {
            vm->runtime_error("image.flip_h(): invalid image dimensions or pixel buffer.");
            return 0;
        }
        int row_bytes = w * ch;
        ObjBuffer *dst = new_buffer(&vm->get_gc(), BUF_UINT8, src->count);
        unsigned char *s = (unsigned char *)src->data;
        unsigned char *d = (unsigned char *)dst->data;
        for (int y = 0; y < h; y++) {
            unsigned char *srow = s + y * row_bytes;
            unsigned char *drow = d + y * row_bytes;
            for (int x = 0; x < w; x++)
                memcpy(drow + x * ch, srow + (w - 1 - x) * ch, ch);
        }
        args[0] = val_obj((Obj *)dst);
        return 1;
    }

    static int write_image(VM *vm, Value *args, int nargs, const char *kind)
    {
        if (nargs < 5 || !is_string(args[0]) || !is_buffer(args[4]))
        {
            vm->runtime_error("image.write_*() expects (path, width, height, components, pixels, quality?).");
            return 0;
        }

        int width = 0;
        int height = 0;
        int components = 0;
        if (!get_int_arg(args, 1, &width) || !get_int_arg(args, 2, &height) ||
            !get_int_arg(args, 3, &components))
        {
            vm->runtime_error("image.write_*(): width, height and components must be integers.");
            return 0;
        }

        ObjBuffer *pixels = as_buffer(args[4]);
        if (!buffer_has_image_size(pixels, width, height, components))
        {
            vm->runtime_error("image.write_*(): invalid image dimensions, components, or pixel buffer.");
            return 0;
        }

        const char *path = as_cstring(args[0]);
        int ok = 0;
        if (strcmp(kind, "png") == 0)
            ok = stbi_write_png(path, width, height, components, pixels->data, width * components);
        else if (strcmp(kind, "bmp") == 0)
            ok = stbi_write_bmp(path, width, height, components, pixels->data);
        else if (strcmp(kind, "tga") == 0)
            ok = stbi_write_tga(path, width, height, components, pixels->data);
        else if (strcmp(kind, "jpg") == 0)
        {
            int quality = 90;
            if (nargs >= 6 && is_int(args[5]))
                quality = (int)args[5].as.integer;
            if (quality < 1)
                quality = 1;
            if (quality > 100)
                quality = 100;
            ok = stbi_write_jpg(path, width, height, components, pixels->data, quality);
        }

        args[0] = val_bool(ok != 0);
        return 1;
    }

    static int nat_image_write_png(VM *vm, Value *args, int nargs)
    {
        return write_image(vm, args, nargs, "png");
    }

    static int nat_image_write_bmp(VM *vm, Value *args, int nargs)
    {
        return write_image(vm, args, nargs, "bmp");
    }

    static int nat_image_write_tga(VM *vm, Value *args, int nargs)
    {
        return write_image(vm, args, nargs, "tga");
    }

    static int nat_image_write_jpg(VM *vm, Value *args, int nargs)
    {
        return write_image(vm, args, nargs, "jpg");
    }

    static const NativeReg image_funcs[] = {
        {"info",             nat_image_info,            1},
        {"info_mem",         nat_image_info_mem,        1},
        {"load",             nat_image_load,           -1},
        {"load_mem",         nat_image_load_mem,       -1},
        {"resize",           nat_image_resize,         -1},
        {"encode",           nat_image_encode,         -1},
        {"set_flip_on_load", nat_image_set_flip_on_load, 1},
        {"flip_v",           nat_image_flip_v,          4},
        {"flip_h",           nat_image_flip_h,          4},
        {"write_png",        nat_image_write_png,       5},
        {"write_bmp",        nat_image_write_bmp,       5},
        {"write_tga",        nat_image_write_tga,       5},
        {"write_jpg",        nat_image_write_jpg,      -1},
    };

    const NativeLib zen_lib_image = {
        "image", image_funcs, 13, nullptr, 0
    };
}

#endif /* ZEN_ENABLE_STB_IMAGE */
