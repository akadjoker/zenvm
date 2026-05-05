/* zen_gl4.cpp — Desktop GL 4.x extension bindings for Zen
**
** import gl4
**
** Extends the base `gl` module with desktop-only GL 4.x functions:
**   compute shaders, tessellation, image load/store, indirect draw,
**   base-vertex draw, vertex attrib binding, buffer storage,
**   timer queries, SSBOs, multisample textures, debug output, etc.
**
** Requires: gl module already loaded (shares the same glad.h / GL context).
** Not available on GLES / Emscripten / Android.
*/

#include "zen/module_gl.h"

#ifdef ZEN_ENABLE_GL4

/* We do NOT re-emit glad here — zen_gl.cpp already did it.
** Just include the header to get the function declarations. */
#include <stdlib.h>

#include "object.h"
#include "memory.h"
#include "vm.h"
#include "gl_headers.h"

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    static inline float fval(Value v) { return is_int(v) ? (float)v.as.integer : (float)v.as.number; }
    static inline int   ival(Value v) { return is_int(v) ? (int)v.as.integer   : (int)v.as.number; }

    /* ========= Function implementations ========= */
    #define ZEN_GL_DESKTOP 1
    #include "gl_funcs_desktop.inl"
    #undef  ZEN_GL_DESKTOP

    /* ========= Function registration table ========= */
    static const NativeReg gl4_functions[] = {
        /* --- Compute Shaders (GL 4.3) --- */
        {"glDispatchCompute",            zen_glDispatchCompute,            3},
        {"glDispatchComputeIndirect",    zen_glDispatchComputeIndirect,    1},

        /* --- Tessellation (GL 4.0) --- */
        {"glPatchParameteri",            zen_glPatchParameteri,            2},
        {"glPatchParameterfv",           zen_glPatchParameterfv,           2},

        /* --- Image Load/Store (GL 4.2) --- */
        {"glBindImageTexture",           zen_glBindImageTexture,           7},
        {"glMemoryBarrier",              zen_glMemoryBarrier,              1},
        {"glMemoryBarrierByRegion",      zen_glMemoryBarrierByRegion,      1},

        /* --- Indirect Draw --- */
        {"glDrawArraysIndirect",         zen_glDrawArraysIndirect,         2},
        {"glDrawElementsIndirect",       zen_glDrawElementsIndirect,       3},
        {"glMultiDrawArraysIndirect",    zen_glMultiDrawArraysIndirect,    4},
        {"glMultiDrawElementsIndirect",  zen_glMultiDrawElementsIndirect,  5},

        /* --- Base-Vertex Draw (GL 3.2) --- */
        {"glDrawElementsBaseVertex",              zen_glDrawElementsBaseVertex,              5},
        {"glDrawRangeElementsBaseVertex",         zen_glDrawRangeElementsBaseVertex,         7},
        {"glDrawElementsInstancedBaseVertex",     zen_glDrawElementsInstancedBaseVertex,     6},

        /* --- Vertex Attrib Binding (GL 4.3) --- */
        {"glVertexAttribFormat",         zen_glVertexAttribFormat,         5},
        {"glVertexAttribIFormat",        zen_glVertexAttribIFormat,        4},
        {"glVertexAttribBinding",        zen_glVertexAttribBinding,        2},
        {"glBindVertexBuffer",           zen_glBindVertexBuffer,           4},
        {"glVertexBindingDivisor",       zen_glVertexBindingDivisor,       2},

        /* --- Desktop-only State --- */
        {"glPolygonMode",               zen_glPolygonMode,               2},
        {"glPointSize",                 zen_glPointSize,                 1},
        {"glBlendColor",                zen_glBlendColor,                4},
        {"glColorMaski",                zen_glColorMaski,                5},
        {"glBlendFunci",                zen_glBlendFunci,                3},
        {"glBlendEquationi",            zen_glBlendEquationi,            2},
        {"glBlendFuncSeparatei",        zen_glBlendFuncSeparatei,        5},
        {"glSampleMaski",               zen_glSampleMaski,               2},
        {"glMinSampleShading",          zen_glMinSampleShading,          1},
        {"glClearDepth",                zen_glClearDepth,                1},
        {"glLogicOp",                   zen_glLogicOp,                   1},
        {"glEnablei",                   zen_glEnablei,                   2},
        {"glDisablei",                  zen_glDisablei,                  2},
        {"glIsEnabledi",                zen_glIsEnabledi,                2},

        /* --- Clear Buffer (GL 3.0) --- */
        {"glClearBufferfv",             zen_glClearBufferfv,             3},
        {"glClearBufferiv",             zen_glClearBufferiv,             3},
        {"glClearBufferuiv",            zen_glClearBufferuiv,            3},
        {"glClearBufferfi",             zen_glClearBufferfi,             4},

        /* --- Copy Image (GL 4.3) --- */
        {"glCopyImageSubData",          zen_glCopyImageSubData,          15},

        /* --- Multi-bind (GL 4.4) --- */
        {"glBindTextures",              zen_glBindTextures,              3},
        {"glBindSamplers",              zen_glBindSamplers,              3},
        {"glBindImageTextures",         zen_glBindImageTextures,         3},

        /* --- Buffer Storage (GL 4.4) --- */
        {"glBufferStorage",             zen_glBufferStorage,             3},

        /* --- Timer Queries (GL 3.3) --- */
        {"glQueryCounter",              zen_glQueryCounter,              2},
        {"glGetQueryObjecti64v",        zen_glGetQueryObjecti64v,        2},
        {"glGetQueryObjectui64v",       zen_glGetQueryObjectui64v,       2},
        {"glGetQueryObjectiv",          zen_glGetQueryObjectiv,          2},

        /* --- SSBO (GL 4.3) --- */
        {"glShaderStorageBlockBinding", zen_glShaderStorageBlockBinding, 3},

        /* --- Framebuffer Parameters (GL 4.3) --- */
        {"glFramebufferParameteri",     zen_glFramebufferParameteri,     3},
        {"glGetFramebufferParameteriv", zen_glGetFramebufferParameteriv, 2},

        /* --- Multisample Textures (GL 3.2) --- */
        {"glTexImage2DMultisample",     zen_glTexImage2DMultisample,     6},
        {"glTexImage3DMultisample",     zen_glTexImage3DMultisample,     7},
        {"glTexStorage2DMultisample",   zen_glTexStorage2DMultisample,   6},
        {"glGetMultisamplefv",          zen_glGetMultisamplefv,          2},

        /* --- Debug Output (GL 4.3) --- */
        {"glObjectLabel",               zen_glObjectLabel,               3},
        {"glPushDebugGroup",            zen_glPushDebugGroup,            3},
        {"glPopDebugGroup",             zen_glPopDebugGroup,             0},
        {"glDebugMessageControl",       zen_glDebugMessageControl,       5},
        {"glDebugMessageInsert",        zen_glDebugMessageInsert,        5},
    };

    /* ========= Constants ========= */
    #include "gl4_constants.inl"

    /* ========= NativeLib definition ========= */
    const NativeLib zen_lib_gl4 = {
        "gl4",
        gl4_functions,   ZEN_ARRAY_COUNT(gl4_functions),
        gl4_constants,   ZEN_ARRAY_COUNT(gl4_constants),
        nullptr
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_GL4 */
