#include "zen/module_noise.h"

#ifdef ZEN_ENABLE_STB_PERLIN

#include "object.h"
#include "vm.h"

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

namespace zen
{
    /* -------------------------------------------------------
    ** Helper — extract float from Value (accepts int or float)
    ** ------------------------------------------------------ */
    static bool to_float(Value v, float *out)
    {
        if (is_float(v)) { *out = (float)v.as.number; return true; }
        if (is_int(v))   { *out = (float)v.as.integer; return true; }
        return false;
    }

    static bool to_int(Value v, int *out)
    {
        if (is_int(v))   { *out = (int)v.as.integer; return true; }
        if (is_float(v)) { *out = (int)v.as.number;  return true; }
        return false;
    }

    /* noise3(x, y, z, x_wrap?, y_wrap?, z_wrap?) → float  [-1, 1]
    ** Classic 3-D Perlin noise. Wrap params must be 0 or power-of-two. */
    static int nat_noise3(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3)
        {
            vm->runtime_error("noise.noise3() expects (x, y, z, x_wrap?, y_wrap?, z_wrap?).");
            return 0;
        }
        float x, y, z;
        if (!to_float(args[0], &x) || !to_float(args[1], &y) || !to_float(args[2], &z))
        {
            vm->runtime_error("noise.noise3(): x, y, z must be numbers.");
            return 0;
        }
        int xw = 0, yw = 0, zw = 0;
        if (nargs >= 4) to_int(args[3], &xw);
        if (nargs >= 5) to_int(args[4], &yw);
        if (nargs >= 6) to_int(args[5], &zw);
        args[0] = val_float(stb_perlin_noise3(x, y, z, xw, yw, zw));
        return 1;
    }

    /* noise3_seed(x, y, z, x_wrap, y_wrap, z_wrap, seed) → float
    ** Same as noise3 but with a per-seed variation (bottom 8 bits used). */
    static int nat_noise3_seed(VM *vm, Value *args, int nargs)
    {
        if (nargs < 7)
        {
            vm->runtime_error("noise.noise3_seed() expects (x, y, z, x_wrap, y_wrap, z_wrap, seed).");
            return 0;
        }
        float x, y, z;
        if (!to_float(args[0], &x) || !to_float(args[1], &y) || !to_float(args[2], &z))
        {
            vm->runtime_error("noise.noise3_seed(): x, y, z must be numbers.");
            return 0;
        }
        int xw, yw, zw, seed;
        to_int(args[3], &xw); to_int(args[4], &yw); to_int(args[5], &zw); to_int(args[6], &seed);
        args[0] = val_float(stb_perlin_noise3_seed(x, y, z, xw, yw, zw, seed));
        return 1;
    }

    /* fbm(x, y, z, lacunarity, gain, octaves) → float
    ** Fractal Brownian Motion — smooth layered noise. */
    static int nat_fbm(VM *vm, Value *args, int nargs)
    {
        if (nargs < 6)
        {
            vm->runtime_error("noise.fbm() expects (x, y, z, lacunarity, gain, octaves).");
            return 0;
        }
        float x, y, z, lac, gain;
        int octaves;
        if (!to_float(args[0], &x)   || !to_float(args[1], &y)   || !to_float(args[2], &z) ||
            !to_float(args[3], &lac) || !to_float(args[4], &gain) || !to_int(args[5], &octaves))
        {
            vm->runtime_error("noise.fbm(): invalid arguments.");
            return 0;
        }
        args[0] = val_float(stb_perlin_fbm_noise3(x, y, z, lac, gain, octaves));
        return 1;
    }

    /* ridge(x, y, z, lacunarity, gain, offset, octaves) → float
    ** Ridge noise — good for mountain ridges. */
    static int nat_ridge(VM *vm, Value *args, int nargs)
    {
        if (nargs < 7)
        {
            vm->runtime_error("noise.ridge() expects (x, y, z, lacunarity, gain, offset, octaves).");
            return 0;
        }
        float x, y, z, lac, gain, offset;
        int octaves;
        if (!to_float(args[0], &x)      || !to_float(args[1], &y)      || !to_float(args[2], &z)   ||
            !to_float(args[3], &lac)    || !to_float(args[4], &gain)   || !to_float(args[5], &offset) ||
            !to_int(args[6], &octaves))
        {
            vm->runtime_error("noise.ridge(): invalid arguments.");
            return 0;
        }
        args[0] = val_float(stb_perlin_ridge_noise3(x, y, z, lac, gain, offset, octaves));
        return 1;
    }

    /* turbulence(x, y, z, lacunarity, gain, octaves) → float
    ** Turbulence noise — good for clouds, fire. */
    static int nat_turbulence(VM *vm, Value *args, int nargs)
    {
        if (nargs < 6)
        {
            vm->runtime_error("noise.turbulence() expects (x, y, z, lacunarity, gain, octaves).");
            return 0;
        }
        float x, y, z, lac, gain;
        int octaves;
        if (!to_float(args[0], &x)   || !to_float(args[1], &y)   || !to_float(args[2], &z) ||
            !to_float(args[3], &lac) || !to_float(args[4], &gain) || !to_int(args[5], &octaves))
        {
            vm->runtime_error("noise.turbulence(): invalid arguments.");
            return 0;
        }
        args[0] = val_float(stb_perlin_turbulence_noise3(x, y, z, lac, gain, octaves));
        return 1;
    }

    /* noise2(x, y, octaves?, lacunarity?, gain?) → float
    ** Convenience: 2-D noise by calling noise3 with z=0 and optional fBm layering. */
    static int nat_noise2(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2)
        {
            vm->runtime_error("noise.noise2() expects (x, y, octaves?, lacunarity?, gain?).");
            return 0;
        }
        float x, y;
        if (!to_float(args[0], &x) || !to_float(args[1], &y))
        {
            vm->runtime_error("noise.noise2(): x, y must be numbers.");
            return 0;
        }
        int octaves = 1;
        float lac = 2.0f, gain = 0.5f;
        if (nargs >= 3) to_int(args[2], &octaves);
        if (nargs >= 4) to_float(args[3], &lac);
        if (nargs >= 5) to_float(args[4], &gain);

        float v;
        if (octaves <= 1)
            v = stb_perlin_noise3(x, y, 0.0f, 0, 0, 0);
        else
            v = stb_perlin_fbm_noise3(x, y, 0.0f, lac, gain, octaves);
        args[0] = val_float(v);
        return 1;
    }

    static const NativeReg noise_funcs[] = {
        {"noise2",      nat_noise2,      -1},
        {"noise3",      nat_noise3,      -1},
        {"noise3_seed", nat_noise3_seed,  7},
        {"fbm",         nat_fbm,          6},
        {"ridge",       nat_ridge,        7},
        {"turbulence",  nat_turbulence,   6},
    };

    const NativeLib zen_lib_noise = {
        "noise", noise_funcs, 6, nullptr, 0
    };
}

#endif /* ZEN_ENABLE_STB_PERLIN */
