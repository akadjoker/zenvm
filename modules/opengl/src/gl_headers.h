#pragma once


#ifdef _WIN32
#include <windows.h>
#endif

#if defined(GRAPHICS_API_OPENGL_ES2)
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#elif defined(GRAPHICS_API_OPENGL_ES3)
    #include <GLES3/gl3.h>
    #include <GLES2/gl2ext.h>
#elif defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
    #include "vendor/glad.h"
#else
    // Fallback: assume desktop GL 3.3
    #include "vendor/glad.h"
#endif
