#include "zen/module_rectpack.h"

#ifdef ZEN_ENABLE_STB_RECTPACK

#include "object.h"
#include "memory.h"
#include "vm.h"

#include <cstdlib>
#include <cstring>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

namespace zen
{
    /* pack(atlas_w, atlas_h, sizes) → Float32Array or nil
    **
    ** sizes    : Float32Array with [w0, h0, w1, h1, ...] — N*2 elements.
    ** returns  : Float32Array with [x0, y0, ok0, x1, y1, ok1, ...] — N*3 elements.
    **            x,y are top-left in the atlas.  ok is 1.0 if packed, 0.0 otherwise.
    **            Returns nil if atlas_w/h <= 0 or sizes is empty/invalid.
    **
    ** Example:
    **   var sizes = Float32Array(6)        // 3 rects
    **   sizes[0] = 64;  sizes[1] = 64
    **   sizes[2] = 128; sizes[3] = 32
    **   sizes[4] = 256; sizes[5] = 128
    **   var r = rectpack.pack(1024, 1024, sizes)
    **   for var i = 0; i < 3; i = i + 1 {
    **       print(str(r[i*3]) + "," + str(r[i*3+1]) + " ok=" + str(r[i*3+2]))
    **   }
    */
    static int nat_rectpack_pack(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3 || !is_int(args[0]) || !is_int(args[1]) || !is_buffer(args[2]))
        {
            vm->runtime_error("rectpack.pack() expects (atlas_w, atlas_h, sizes_Float32Array).");
            return 0;
        }

        int atlas_w = (int)args[0].as.integer;
        int atlas_h = (int)args[1].as.integer;
        ObjBuffer *sizes_buf = as_buffer(args[2]);

        if (atlas_w <= 0 || atlas_h <= 0 || sizes_buf->btype != BUF_FLOAT32 || sizes_buf->count < 2)
        {
            args[0] = val_nil();
            return 1;
        }

        int n = (int)(sizes_buf->count / 2);
        float *sizes = (float *)sizes_buf->data;

        /* Build stbrp_rect array */
        stbrp_rect *rects = (stbrp_rect *)malloc(sizeof(stbrp_rect) * n);
        if (!rects) { args[0] = val_nil(); return 1; }

        for (int i = 0; i < n; i++)
        {
            rects[i].id = i;
            rects[i].w  = (stbrp_coord)(int)sizes[i * 2 + 0];
            rects[i].h  = (stbrp_coord)(int)sizes[i * 2 + 1];
            rects[i].x  = 0; rects[i].y = 0; rects[i].was_packed = 0;
        }

        /* stb_rectpack needs a node array — one node per pixel column is typical */
        stbrp_node *nodes = (stbrp_node *)malloc(sizeof(stbrp_node) * atlas_w);
        if (!nodes) { free(rects); args[0] = val_nil(); return 1; }

        stbrp_context ctx;
        stbrp_init_target(&ctx, atlas_w, atlas_h, nodes, atlas_w);
        stbrp_pack_rects(&ctx, rects, n);

        /* Write results into a Float32Array: [x, y, ok] per rect */
        ObjBuffer *out = new_buffer(&vm->get_gc(), BUF_FLOAT32, n * 3);
        float *dst = (float *)out->data;
        for (int i = 0; i < n; i++)
        {
            dst[i * 3 + 0] = (float)rects[i].x;
            dst[i * 3 + 1] = (float)rects[i].y;
            dst[i * 3 + 2] = rects[i].was_packed ? 1.0f : 0.0f;
        }

        free(nodes);
        free(rects);

        args[0] = val_obj((Obj *)out);
        return 1;
    }

    static const NativeReg rectpack_funcs[] = {
        {"pack", nat_rectpack_pack, 3},
    };

    const NativeLib zen_lib_rectpack = {
        "rectpack", rectpack_funcs, 1, nullptr, 0
    };
}

#endif /* ZEN_ENABLE_STB_RECTPACK */
