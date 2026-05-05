#ifndef ZEN_MODULE_GL_H
#define ZEN_MODULE_GL_H

#include "module.h"

namespace zen
{
#ifdef ZEN_ENABLE_GL
    extern const NativeLib zen_lib_gl;
#endif
#ifdef ZEN_ENABLE_GL4
    extern const NativeLib zen_lib_gl4;   /* import gl4  — desktop GL 4.x */
    extern const NativeLib zen_lib_gll;   /* import gll  — legacy immediate mode */
#endif
}

#endif /* ZEN_MODULE_GL_H */
