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
        {"info", nat_image_info, 1},
        {"load", nat_image_load, -1},
        {"resize", nat_image_resize, -1},
        {"write_png", nat_image_write_png, 5},
        {"write_bmp", nat_image_write_bmp, 5},
        {"write_tga", nat_image_write_tga, 5},
        {"write_jpg", nat_image_write_jpg, -1},
    };

    const NativeLib zen_lib_image = {
        "image", image_funcs, 7, nullptr, 0
    };
}

#endif /* ZEN_ENABLE_STB_IMAGE */
