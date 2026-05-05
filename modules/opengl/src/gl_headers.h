#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(__EMSCRIPTEN__)
    #include <GLES3/gl3.h>
#elif defined(__ANDROID__)
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
#else
    /* Desktop: use glad (GL 4.3 core, superset of GLES 3.0) */
    #include "../vendor/glad.h"
#endif
