#ifndef ZEN_MODULE_SQLITE_H
#define ZEN_MODULE_SQLITE_H

#include "module.h"

namespace zen {
#ifdef ZEN_ENABLE_SQLITE
    extern const NativeLib zen_lib_sqlite;
#endif
}

#endif /* ZEN_MODULE_SQLITE_H */
