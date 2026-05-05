#ifndef ZEN_MODULE_NOISE_H
#define ZEN_MODULE_NOISE_H

#include "module.h"

namespace zen
{
#ifdef ZEN_ENABLE_STB_PERLIN
    extern const NativeLib zen_lib_noise;
#endif
}

#endif /* ZEN_MODULE_NOISE_H */
