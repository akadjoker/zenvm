#ifdef ZEN_ENABLE_NN

/* =========================================================
** builtin_nn.cpp — "nn" module for Zen
**
** Neural network primitives: activation functions,
** derivatives, loss functions, normalization.
** Pure C math, no external deps.
**
** Usage:
**   import nn;
**   print(nn.sigmoid(0.5));
**   print(nn.relu(-1.0));
**   print(nn.mse([1.0, 2.0], [1.1, 1.9]));
** ========================================================= */

#include "module.h"
#include "vm.h"
#include <cmath>

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
    ** Registration
    ** ========================================================= */

    static const NativeReg nn_functions[] = {
        {"sigmoid", nat_nn_sigmoid, 1},
        {"relu", nat_nn_relu, 1},
        {"leaky_relu", nat_nn_leaky_relu, -1},
        {"elu", nat_nn_elu, -1},
        {"swish", nat_nn_swish, 1},
        {"gelu", nat_nn_gelu, 1},
        {"softplus", nat_nn_softplus, 1},
        {"mish", nat_nn_mish, 1},
        {"tanh", nat_nn_tanh, 1},
        {"sigmoid_d", nat_nn_sigmoid_d, 1},
        {"relu_d", nat_nn_relu_d, 1},
        {"tanh_d", nat_nn_tanh_d, 1},
        {"mse", nat_nn_mse, 2},
        {"bce", nat_nn_bce, 2},
        {"normalize", nat_nn_normalize, 3},
        {"denormalize", nat_nn_denormalize, 3},
    };

    const NativeLib zen_lib_nn = {
        "nn",
        nn_functions,
        16,
        nullptr,
        0,
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_NN */
