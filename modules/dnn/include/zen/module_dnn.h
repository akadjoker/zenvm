#ifndef ZEN_MODULE_DNN_H
#define ZEN_MODULE_DNN_H

#include "module.h"

namespace zen
{
#ifdef ZEN_ENABLE_NN
    extern const NativeLib zen_lib_nn;
#endif
}

#endif /* ZEN_MODULE_DNN_H */
