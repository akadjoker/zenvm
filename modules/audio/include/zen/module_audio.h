#ifndef ZEN_MODULE_AUDIO_H
#define ZEN_MODULE_AUDIO_H

#include "module.h"

namespace zen
{
#ifdef ZEN_ENABLE_AUDIO
    extern const NativeLib zen_lib_audio;
#endif
}

#endif /* ZEN_MODULE_AUDIO_H */
