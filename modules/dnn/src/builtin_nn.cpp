/* =========================================================
** builtin_nn.cpp — "nn" module for Zen
**
** Neural network primitives: activation functions,
** derivatives, loss functions, normalization.
** Pure C math, no external deps.
**
** Usage:
**   import nn
**   print(nn.sigmoid(0.5))
**   print(nn.relu(-1.0))
**   print(nn.mse([1.0, 2.0], [1.1, 1.9]))
**
** Optional: compile with -DZEN_ENABLE_MINIDNN=ON for Network class.
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

namespace zen
{

    /* =========================================================
    ** Activation functions
    ** ========================================================= */

    /* sigmoid(x) → float */
    static int nat_nn_sigmoid(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        args[0] = val_float(1.0 / (1.0 + exp(-x)));
        return 1;
    }

    /* relu(x) → float */
    static int nat_nn_relu(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        args[0] = val_float(x > 0.0 ? x : 0.0);
        return 1;
    }

    /* leaky_relu(x, alpha?) → float */
    static int nat_nn_leaky_relu(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        double alpha = (nargs >= 2) ? to_number(args[1]) : 0.01;
        args[0] = val_float(x > 0.0 ? x : alpha * x);
        return 1;
    }

    /* elu(x, alpha?) → float */
    static int nat_nn_elu(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        double alpha = (nargs >= 2) ? to_number(args[1]) : 1.0;
        args[0] = val_float(x > 0.0 ? x : alpha * (exp(x) - 1.0));
        return 1;
    }

    /* swish(x) = x * sigmoid(x) */
    static int nat_nn_swish(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        args[0] = val_float(x / (1.0 + exp(-x)));
        return 1;
    }

    /* silu(x) = x * sigmoid(x) — alias for swish */
    static int nat_nn_silu(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        args[0] = val_float(x / (1.0 + exp(-x)));
        return 1;
    }

    /* gelu(x) ≈ 0.5*x*(1+tanh(sqrt(2/π)*(x+0.044715*x³))) */
    static int nat_nn_gelu(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        double inner = 0.7978845608028654 * (x + 0.044715 * x * x * x);
        args[0] = val_float(0.5 * x * (1.0 + tanh(inner)));
        return 1;
    }

    /* softplus(x) = log(1 + exp(x)) */
    static int nat_nn_softplus(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        /* Numerically stable */
        args[0] = val_float(x > 20.0 ? x : log(1.0 + exp(x)));
        return 1;
    }

    /* mish(x) = x * tanh(softplus(x)) */
    static int nat_nn_mish(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        double sp = x > 20.0 ? x : log(1.0 + exp(x));
        args[0] = val_float(x * tanh(sp));
        return 1;
    }

    /* step(x, threshold=0.5) → 0.0 or 1.0  (Heaviside) */
    static int nat_nn_step(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double x = to_number(args[0]);
        double t = (nargs >= 2) ? to_number(args[1]) : 0.5;
        args[0] = val_float(x >= t ? 1.0 : 0.0);
        return 1;
    }

    /* tanh(x) → float */
    static int nat_nn_tanh(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        args[0] = val_float(tanh(to_number(args[0])));
        return 1;
    }

    /* =========================================================
    ** Derivatives
    ** ========================================================= */

    /* sigmoid_d(x) = sigmoid(x) * (1 - sigmoid(x)) */
    static int nat_nn_sigmoid_d(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double s = 1.0 / (1.0 + exp(-to_number(args[0])));
        args[0] = val_float(s * (1.0 - s));
        return 1;
    }

    /* relu_d(x) = x > 0 ? 1 : 0 */
    static int nat_nn_relu_d(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        args[0] = val_float(to_number(args[0]) > 0.0 ? 1.0 : 0.0);
        return 1;
    }

    /* tanh_d(x) = 1 - tanh(x)² */
    static int nat_nn_tanh_d(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_float(0.0); return 1; }
        double t = tanh(to_number(args[0]));
        args[0] = val_float(1.0 - t * t);
        return 1;
    }

    /* =========================================================
    ** Loss functions (work on arrays)
    ** ========================================================= */

    /* mse(predicted, actual) → float */
    static int nat_nn_mse(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_array(args[0]) || !is_array(args[1]))
        {
            vm->runtime_error("nn.mse() expects (predicted_array, actual_array).");
            return -1;
        }

        ObjArray *pred = as_array(args[0]);
        ObjArray *actual = as_array(args[1]);
        int n = arr_count(pred);
        int m = arr_count(actual);
        if (n != m || n == 0)
        {
            args[0] = val_float(0.0);
            return 1;
        }

        double sum = 0.0;
        for (int i = 0; i < n; i++)
        {
            double diff = to_number(pred->data[i]) - to_number(actual->data[i]);
            sum += diff * diff;
        }
        args[0] = val_float(sum / n);
        return 1;
    }

    /* bce(predicted, actual) → float (binary cross-entropy) */
    static int nat_nn_bce(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_array(args[0]) || !is_array(args[1]))
        {
            vm->runtime_error("nn.bce() expects (predicted_array, actual_array).");
            return -1;
        }

        ObjArray *pred = as_array(args[0]);
        ObjArray *actual = as_array(args[1]);
        int n = arr_count(pred);
        int m = arr_count(actual);
        if (n != m || n == 0)
        {
            args[0] = val_float(0.0);
            return 1;
        }

        double sum = 0.0;
        const double eps = 1e-15;
        for (int i = 0; i < n; i++)
        {
            double p = to_number(pred->data[i]);
            double y = to_number(actual->data[i]);
            /* Clip to avoid log(0) */
            if (p < eps) p = eps;
            if (p > 1.0 - eps) p = 1.0 - eps;
            sum += -(y * log(p) + (1.0 - y) * log(1.0 - p));
        }
        args[0] = val_float(sum / n);
        return 1;
    }

    /* =========================================================
    ** Normalization
    ** ========================================================= */

    /* normalize(value, min, max) → float [0..1] */
    static int nat_nn_normalize(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3)
        {
            args[0] = val_float(0.0);
            return 1;
        }
        double val = to_number(args[0]);
        double min = to_number(args[1]);
        double max = to_number(args[2]);
        double range = max - min;
        args[0] = val_float(range != 0.0 ? (val - min) / range : 0.0);
        return 1;
    }

    /* denormalize(normalized, min, max) → float */
    static int nat_nn_denormalize(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3)
        {
            args[0] = val_float(0.0);
            return 1;
        }
        double val = to_number(args[0]);
        double min = to_number(args[1]);
        double max = to_number(args[2]);
        args[0] = val_float(val * (max - min) + min);
        return 1;
    }

    /* =========================================================
    ** Image I/O  (BMP only, pure C — no external deps)
    ** ========================================================= */

    static uint32_t bmp_ru32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
    static uint16_t bmp_ru16(const uint8_t *p) { return (uint16_t)p[0]|((uint16_t)p[1]<<8); }
    static int32_t  bmp_ri32(const uint8_t *p) { return (int32_t)bmp_ru32(p); }
    static void bmp_wu32(uint8_t *p, uint32_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }
    static void bmp_wu16(uint8_t *p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }
    static void bmp_wi32(uint8_t *p, int32_t  v) { bmp_wu32(p, (uint32_t)v); }

    /* loadImage(path) → width, height, channels, data_array
    ** Supports uncompressed 24-bit and 32-bit BMP.
    ** Pixel data returned as flat int array [R,G,B,...] top-down. */
    static int nat_nn_loadImage(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("nn.loadImage() expects (path)");
            return -1;
        }
        const char *path = as_cstring(args[0]);
        FILE *f = fopen(path, "rb");
        if (!f) { vm->runtime_error("nn.loadImage(): cannot open file"); return -1; }
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *buf = (uint8_t *)malloc((size_t)fsz);
        if (!buf) { fclose(f); vm->runtime_error("nn.loadImage(): out of memory"); return -1; }
        fread(buf, 1, (size_t)fsz, f);
        fclose(f);

        if (fsz < 54 || buf[0] != 'B' || buf[1] != 'M')
        {
            free(buf);
            vm->runtime_error("nn.loadImage(): not a valid BMP file");
            return -1;
        }
        uint32_t poff = bmp_ru32(buf + 10);
        int32_t  w    = bmp_ri32(buf + 18);
        int32_t  hraw = bmp_ri32(buf + 22);
        uint16_t bpp  = bmp_ru16(buf + 28);
        uint32_t comp = bmp_ru32(buf + 30);

        if (comp != 0 || (bpp != 24 && bpp != 32))
        {
            free(buf);
            vm->runtime_error("nn.loadImage(): only uncompressed 24/32-bit BMP supported");
            return -1;
        }
        int h = (int)(hraw < 0 ? -hraw : hraw);
        bool top_down = hraw < 0;
        int ch = (bpp == 32) ? 4 : 3;
        int stride = (w * (bpp / 8) + 3) & ~3;

        ObjArray *arr = new_array(&vm->get_gc());
        for (int y = 0; y < h; y++)
        {
            int sy = top_down ? y : (h - 1 - y);
            const uint8_t *row = buf + poff + (size_t)sy * stride;
            for (int x = 0; x < w; x++)
            {
                const uint8_t *px = row + x * (bpp / 8);
                array_push(&vm->get_gc(), arr, val_int(px[2])); /* R */
                array_push(&vm->get_gc(), arr, val_int(px[1])); /* G */
                array_push(&vm->get_gc(), arr, val_int(px[0])); /* B */
                if (ch == 4)
                    array_push(&vm->get_gc(), arr, val_int(px[3])); /* A */
            }
        }
        free(buf);
        args[0] = val_int(w);
        args[1] = val_int(h);
        args[2] = val_int(ch);
        args[3] = val_obj((Obj *)arr);
        return 4;
    }

    /* saveImage(path, width, height, channels, data_array) → bool
    ** Writes a 24-bit top-down uncompressed BMP. Alpha channel ignored. */
    static int nat_nn_saveImage(VM *vm, Value *args, int nargs)
    {
        if (nargs < 5 || !is_string(args[0]) || !is_array(args[4]))
        {
            vm->runtime_error("nn.saveImage() expects (path, width, height, channels, data_array)");
            return -1;
        }
        const char *path = as_cstring(args[0]);
        int w  = (int)to_number(args[1]);
        int h  = (int)to_number(args[2]);
        int ch = (int)to_number(args[3]);
        ObjArray *src = as_array(args[4]);

        if (w <= 0 || h <= 0 || ch < 1 || ch > 4)
        {
            vm->runtime_error("nn.saveImage(): invalid dimensions or channel count");
            return -1;
        }
        int stride = (w * 3 + 3) & ~3;
        int pxsz   = stride * h;
        int filesz = 54 + pxsz;
        uint8_t *bmp = (uint8_t *)calloc((size_t)filesz, 1);
        if (!bmp) { vm->runtime_error("nn.saveImage(): out of memory"); return -1; }

        bmp[0]='B'; bmp[1]='M';
        bmp_wu32(bmp + 2,  (uint32_t)filesz);
        bmp_wu32(bmp + 10, 54);
        bmp_wu32(bmp + 14, 40);
        bmp_wi32(bmp + 18, (int32_t)w);
        bmp_wi32(bmp + 22, -(int32_t)h); /* negative → top-down */
        bmp_wu16(bmp + 26, 1);
        bmp_wu16(bmp + 28, 24);
        bmp_wu32(bmp + 34, (uint32_t)pxsz);

        int n = arr_count(src);
        for (int y = 0; y < h; y++)
        {
            uint8_t *row = bmp + 54 + y * stride;
            for (int x = 0; x < w; x++)
            {
                int idx = (y * w + x) * ch;
                uint8_t r = (idx   < n) ? (uint8_t)(int)to_number(src->data[idx])   : 0;
                uint8_t g = (idx+1 < n) ? (uint8_t)(int)to_number(src->data[idx+1]) : 0;
                uint8_t b = (idx+2 < n) ? (uint8_t)(int)to_number(src->data[idx+2]) : 0;
                row[x*3+0]=b; row[x*3+1]=g; row[x*3+2]=r;
            }
        }
        FILE *f = fopen(path, "wb");
        if (!f) { free(bmp); vm->runtime_error("nn.saveImage(): cannot write file"); return -1; }
        fwrite(bmp, 1, (size_t)filesz, f);
        fclose(f);
        free(bmp);
        args[0] = val_bool(true);
        return 1;
    }

    /* loadImageData(data_array, width, height, channels) → float_array [0..1]
    ** Normalises raw byte pixel data for NN input. */
    static int nat_nn_loadImageData(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_array(args[0]))
        {
            vm->runtime_error("nn.loadImageData() expects (data_array, width, height, channels)");
            return -1;
        }
        ObjArray *src = as_array(args[0]);
        int n = arr_count(src);
        ObjArray *dst = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++)
            array_push(&vm->get_gc(), dst, val_float(to_number(src->data[i]) / 255.0));
        args[0] = val_obj((Obj *)dst);
        return 1;
    }

    /* getImageData(data_array, width, height, channels) → byte_array [0..255]
    ** Denormalises float [0..1] NN output back to raw pixel bytes. */
    static int nat_nn_getImageData(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_array(args[0]))
        {
            vm->runtime_error("nn.getImageData() expects (data_array, width, height, channels)");
            return -1;
        }
        ObjArray *src = as_array(args[0]);
        int n = arr_count(src);
        ObjArray *dst = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++)
        {
            double v = to_number(src->data[i]) * 255.0;
            v = v < 0.0 ? 0.0 : (v > 255.0 ? 255.0 : v);
            array_push(&vm->get_gc(), dst, val_int((int)v));
        }
        args[0] = val_obj((Obj *)dst);
        return 1;
    }

    /* =========================================================
    ** Registration
    ** ========================================================= */

    static const NativeReg nn_functions[] = {
        /* Activations */
        {"sigmoid",    nat_nn_sigmoid,    1},
        {"relu",       nat_nn_relu,       1},
        {"leaky_relu", nat_nn_leaky_relu, -1},
        {"elu",        nat_nn_elu,        -1},
        {"swish",      nat_nn_swish,      1},
        {"silu",       nat_nn_silu,       1},
        {"gelu",       nat_nn_gelu,       1},
        {"softplus",   nat_nn_softplus,   1},
        {"mish",       nat_nn_mish,       1},
        {"step",       nat_nn_step,       -1},
        {"tanh",       nat_nn_tanh,       1},
        /* Derivatives */
        {"sigmoid_d",  nat_nn_sigmoid_d,  1},
        {"relu_d",     nat_nn_relu_d,     1},
        {"tanh_d",     nat_nn_tanh_d,     1},
        /* Loss */
        {"mse",        nat_nn_mse,        2},
        {"bce",        nat_nn_bce,        2},
        /* Normalisation */
        {"normalize",  nat_nn_normalize,  3},
        {"denormalize",nat_nn_denormalize,3},
        /* Image I/O */
        {"loadImage",     nat_nn_loadImage,     1},
        {"saveImage",     nat_nn_saveImage,     5},
        {"loadImageData", nat_nn_loadImageData, 4},
        {"getImageData",  nat_nn_getImageData,  4},
    };

/* =========================================================
** Network class — MiniDNN wrapper
** ========================================================= */
#ifdef ZEN_ENABLE_MINIDNN

#include "../vendor/eigen/Eigen/Dense"
#include "../vendor/MiniDNN/include/MiniDNN.h"

    enum class ActivationType { RELU, SIGMOID, TANH, SOFTMAX, IDENTITY, MISH };
    enum class LossType       { MSE, BINARY_CROSS_ENTROPY, MULTI_CROSS_ENTROPY };
    enum class OptimizerType  { SGD, ADAM, RMSPROP, ADAGRAD };

    struct NetworkData
    {
        MiniDNN::Network net;
        int input_size  = 0;
        int output_size = 0;
        bool compiled   = false;
        LossType    loss_type = LossType::MSE;
        OptimizerType opt_type = OptimizerType::ADAM;
        double learning_rate = 0.001;
        double init_sigma    = 0.01;
        int    init_seed     = 42;

        MiniDNN::Adam*    adam_opt     = nullptr;
        MiniDNN::SGD*     sgd_opt      = nullptr;
        MiniDNN::RMSProp* rmsprop_opt  = nullptr;
        MiniDNN::AdaGrad* adagrad_opt  = nullptr;

        ~NetworkData()
        {
            delete adam_opt;
            delete sgd_opt;
            delete rmsprop_opt;
            delete adagrad_opt;
        }
    };

    static NetworkData *net_data(Value self)
    {
        return zen_instance_data<NetworkData>(self);
    }

    /* --- Helpers: Zen array ↔ Eigen matrix --- */

    static bool array_to_col_vector(ObjArray *arr, Eigen::MatrixXd &mat, int col)
    {
        int n = arr_count(arr);
        for (int i = 0; i < n; i++)
        {
            if (!is_number(arr->data[i])) return false;
            mat(i, col) = to_number(arr->data[i]);
        }
        return true;
    }

    /* Convert [[s0],[s1],...] or [s0,s1,...] (1D) to Eigen matrix (features × samples) */
    static bool zen_to_matrix(GC *gc, ObjArray *arr, Eigen::MatrixXd &mat, int expected_rows = 0)
    {
        int n = arr_count(arr);
        if (n == 0) return false;

        /* 1D array of numbers → single sample (column vector) */
        if (is_number(arr->data[0]))
        {
            int rows = n;
            if (expected_rows > 0 && rows != expected_rows) return false;
            mat.resize(rows, 1);
            for (int i = 0; i < rows; i++)
            {
                if (!is_number(arr->data[i])) return false;
                mat(i, 0) = to_number(arr->data[i]);
            }
            return true;
        }

        /* 2D array of arrays → features × samples */
        if (is_array(arr->data[0]))
        {
            ObjArray *first = as_array(arr->data[0]);
            int features = arr_count(first);
            if (expected_rows > 0 && features != expected_rows) return false;
            mat.resize(features, n);
            for (int i = 0; i < n; i++)
            {
                if (!is_array(arr->data[i])) return false;
                ObjArray *sample = as_array(arr->data[i]);
                if (arr_count(sample) != features) return false;
                if (!array_to_col_vector(sample, mat, i)) return false;
            }
            return true;
        }

        return false;
    }

    static Value matrix_to_zen(GC *gc, const Eigen::MatrixXd &mat)
    {
        int rows = (int)mat.rows();
        int cols = (int)mat.cols();

        if (cols == 1)
        {
            /* Single sample → 1D array */
            ObjArray *arr = new_array(gc);
            for (int i = 0; i < rows; i++)
                array_push(gc, arr, val_float(mat(i, 0)));
            return val_obj(arr);
        }

        /* Multiple samples → 2D array */
        ObjArray *outer = new_array(gc);
        for (int c = 0; c < cols; c++)
        {
            ObjArray *inner = new_array(gc);
            for (int r = 0; r < rows; r++)
                array_push(gc, inner, val_float(mat(r, c)));
            array_push(gc, outer, val_obj(inner));
        }
        return val_obj(outer);
    }

    static ActivationType parse_activation(const char *s)
    {
        if (!s) return ActivationType::RELU;
        if (strcmp(s, "sigmoid") == 0) return ActivationType::SIGMOID;
        if (strcmp(s, "tanh")    == 0) return ActivationType::TANH;
        if (strcmp(s, "softmax") == 0) return ActivationType::SOFTMAX;
        if (strcmp(s, "identity")== 0 || strcmp(s, "linear") == 0) return ActivationType::IDENTITY;
        if (strcmp(s, "mish")    == 0) return ActivationType::MISH;
        return ActivationType::RELU;
    }

    /* --- Constructor / Destructor --- */

    static void *network_ctor(VM *vm, int argc, Value *args)
    {
        (void)vm; (void)argc; (void)args;
        return new NetworkData();
    }

    static void network_dtor(VM *vm, void *data)
    {
        (void)vm;
        delete (NetworkData *)data;
    }

    /* --- Methods --- */

    /* Network.add(units, activation="relu") */
    static int network_add(VM *vm, Value *args, int nargs)
    {
        NetworkData *nd = net_data(args[0]);
        if (nargs < 2 || !is_int(args[1]))
        {
            vm->runtime_error("Network.add() expects (units, activation='relu')");
            return -1;
        }
        int units = (int)args[1].as.integer;
        const char *act_str = (nargs >= 3 && is_string(args[2])) ? as_cstring(args[2]) : "relu";
        ActivationType act = parse_activation(act_str);

        int in_size = nd->input_size == 0 ? units : nd->output_size;
        if (nd->input_size == 0) nd->input_size = units;

        switch (act)
        {
            case ActivationType::RELU:     nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::ReLU>(in_size, units));     break;
            case ActivationType::SIGMOID:  nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::Sigmoid>(in_size, units));  break;
            case ActivationType::TANH:     nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::Tanh>(in_size, units));     break;
            case ActivationType::SOFTMAX:  nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::Softmax>(in_size, units));  break;
            case ActivationType::IDENTITY: nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::Identity>(in_size, units)); break;
            case ActivationType::MISH:     nd->net.add_layer(new MiniDNN::FullyConnected<MiniDNN::Mish>(in_size, units));     break;
        }
        nd->output_size = units;
        return 0;
    }

    /* Network.compile(loss="mse", optimizer="adam", lr=0.001, seed=42, sigma=0.01) */
    static int network_compile(VM *vm, Value *args, int nargs)
    {
        NetworkData *nd = net_data(args[0]);
        const char *loss_str = (nargs >= 2 && is_string(args[1])) ? as_cstring(args[1]) : "mse";
        const char *opt_str  = (nargs >= 3 && is_string(args[2])) ? as_cstring(args[2]) : "adam";
        double lr            = (nargs >= 4 && is_number(args[3])) ? to_number(args[3])  : 0.001;
        int    seed          = (nargs >= 5 && is_number(args[4])) ? (int)to_number(args[4]) : 42;
        double sigma         = (nargs >= 6 && is_number(args[5])) ? to_number(args[5])  : 0.01;

        /* Loss */
        if      (strcmp(loss_str, "bce") == 0) nd->loss_type = LossType::BINARY_CROSS_ENTROPY;
        else if (strcmp(loss_str, "cce") == 0) nd->loss_type = LossType::MULTI_CROSS_ENTROPY;
        else                                   nd->loss_type = LossType::MSE;

        /* Optimizer */
        nd->learning_rate = lr;
        nd->init_sigma    = sigma;
        nd->init_seed     = seed;
        if (strcmp(opt_str, "sgd") == 0)
        {
            nd->opt_type = OptimizerType::SGD;
            delete nd->sgd_opt; nd->sgd_opt = new MiniDNN::SGD();
            nd->sgd_opt->m_lrate = lr;
        }
        else if (strcmp(opt_str, "rmsprop") == 0)
        {
            nd->opt_type = OptimizerType::RMSPROP;
            delete nd->rmsprop_opt; nd->rmsprop_opt = new MiniDNN::RMSProp();
            nd->rmsprop_opt->m_lrate = lr;
        }
        else if (strcmp(opt_str, "adagrad") == 0)
        {
            nd->opt_type = OptimizerType::ADAGRAD;
            delete nd->adagrad_opt; nd->adagrad_opt = new MiniDNN::AdaGrad();
            nd->adagrad_opt->m_lrate = lr;
        }
        else /* adam (default) */
        {
            nd->opt_type = OptimizerType::ADAM;
            delete nd->adam_opt; nd->adam_opt = new MiniDNN::Adam();
            nd->adam_opt->m_lrate = lr;
        }

        /* Set loss on network */
        switch (nd->loss_type)
        {
            case LossType::BINARY_CROSS_ENTROPY: nd->net.set_output(new MiniDNN::BinaryClassEntropy()); break;
            case LossType::MULTI_CROSS_ENTROPY:  nd->net.set_output(new MiniDNN::MultiClassEntropy());  break;
            default:                             nd->net.set_output(new MiniDNN::RegressionMSE());      break;
        }

        nd->net.init(0, nd->init_sigma, nd->init_seed);
        nd->compiled = true;
        return 0;
    }

    /* Network.fit(x, y, epochs=10, batch_size=32) */
    static int network_fit(VM *vm, Value *args, int nargs)
    {
        NetworkData *nd = net_data(args[0]);
        if (!nd->compiled)
        {
            vm->runtime_error("Network.fit(): call compile() first");
            return -1;
        }
        if (nargs < 3 || !is_array(args[1]) || !is_array(args[2]))
        {
            vm->runtime_error("Network.fit() expects (x_array, y_array, epochs=10, batch_size=32)");
            return -1;
        }

        int epochs     = (nargs >= 4 && is_int(args[3]))    ? (int)args[3].as.integer : 10;
        int batch_size = (nargs >= 5 && is_int(args[4]))    ? (int)args[4].as.integer : 32;

        Eigen::MatrixXd X, Y;
        if (!zen_to_matrix(&vm->get_gc(), as_array(args[1]), X) ||
            !zen_to_matrix(&vm->get_gc(), as_array(args[2]), Y))
        {
            vm->runtime_error("Network.fit(): failed to convert arrays to matrices");
            return -1;
        }

        MiniDNN::Callback *cb = nullptr;
        switch (nd->opt_type)
        {
            case OptimizerType::SGD:      nd->net.fit(*nd->sgd_opt,      X, Y, batch_size, epochs, 42); break;
            case OptimizerType::RMSPROP:  nd->net.fit(*nd->rmsprop_opt,  X, Y, batch_size, epochs, 42); break;
            case OptimizerType::ADAGRAD:  nd->net.fit(*nd->adagrad_opt,  X, Y, batch_size, epochs, 42); break;
            default:                      nd->net.fit(*nd->adam_opt,      X, Y, batch_size, epochs, 42); break;
        }
        return 0;
    }

    /* Network.predict(x) → array */
    static int network_predict(VM *vm, Value *args, int nargs)
    {
        NetworkData *nd = net_data(args[0]);
        if (!nd->compiled)
        {
            vm->runtime_error("Network.predict(): call compile() first");
            return -1;
        }
        if (nargs < 2 || !is_array(args[1]))
        {
            vm->runtime_error("Network.predict() expects (x_array)");
            return -1;
        }

        Eigen::MatrixXd X;
        if (!zen_to_matrix(&vm->get_gc(), as_array(args[1]), X, nd->input_size))
        {
            vm->runtime_error("Network.predict(): failed to convert input array");
            return -1;
        }

        Eigen::MatrixXd result = nd->net.predict(X);
        args[0] = matrix_to_zen(&vm->get_gc(), result);
        return 1;
    }

    /* Network.summary() */
    static int network_summary(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        NetworkData *nd = net_data(args[0]);
        nd->net.export_net("", "");  /* MiniDNN doesn't have a real summary — print basic info */
        args[0] = val_bool(true);
        return 1;
    }

    /* Network.save(path) */
    static int network_save(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[1]))
        {
            vm->runtime_error("Network.save() expects (path)");
            return -1;
        }
        NetworkData *nd = net_data(args[0]);
        const char *path = as_cstring(args[1]);
        nd->net.export_net(path, "params");
        args[0] = val_bool(true);
        return 1;
    }

    /* Network.load(path) */
    static int network_load(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[1]))
        {
            vm->runtime_error("Network.load() expects (path)");
            return -1;
        }
        NetworkData *nd = net_data(args[0]);
        const char *path = as_cstring(args[1]);
        nd->net.read_net(path, "params");
        nd->compiled = true;
        args[0] = val_bool(true);
        return 1;
    }

    static void nn_init(VM *vm)
    {
        vm->def_class("Network")
            .ctor(network_ctor)
            .dtor(network_dtor)
            .method("add",     network_add,     -1)
            .method("compile", network_compile, -1)
            .method("fit",     network_fit,     -1)
            .method("predict", network_predict,  1)
            .method("summary", network_summary,  0)
            .method("save",    network_save,     1)
            .method("load",    network_load,     1)
            .end();
    }

#endif /* ZEN_ENABLE_MINIDNN */

    extern const NativeLib zen_lib_nn = {
        "nn",
        nn_functions,
        22,
        nullptr,
        0,
#ifdef ZEN_ENABLE_MINIDNN
        nn_init,
#else
        nullptr,
#endif
    };

} /* namespace zen */
