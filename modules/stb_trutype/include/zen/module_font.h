#ifndef ZEN_MODULE_FONT_H
#define ZEN_MODULE_FONT_H

#include "module.h"

namespace zen
{
#ifdef ZEN_ENABLE_STB_FONT
    extern const NativeLib zen_lib_font;
#endif
}

#endif /* ZEN_MODULE_FONT_H */
