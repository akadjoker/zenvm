/* zen_gl.cpp — OpenGL bindings module for Zen (GLES 3.0 compatible subset)
**
** import gl
**
** Exposes exact GL C API names: glClear, glEnable, GL_COLOR_BUFFER_BIT, etc.
** Uses ObjBuffer for bulk data upload (vertex data, textures, uniforms).
*/

#include "zen/module_gl.h"

#ifdef ZEN_ENABLE_GL

/* Emit glad function bodies in this translation unit */
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#    define GLAD_GL_IMPLEMENTATION
/* glad 2.0-beta omits these defaults; supply them explicitly */
#    include <stdlib.h>
#    ifndef GLAD_MALLOC
#        define GLAD_MALLOC(sz)  malloc(sz)
#    endif
#    ifndef GLAD_FREE
#        define GLAD_FREE(ptr)   free(ptr)
#    endif
#endif

#include "object.h"
#include "memory.h"
#include "vm.h"
#include "gl_headers.h"

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    /* Helpers — coerce Zen values to int/float regardless of actual type tag */
    static inline float fval(Value v) { return is_int(v) ? (float)v.as.integer : (float)v.as.number; }
    static inline int   ival(Value v) { return is_int(v) ? (int)v.as.integer   : (int)v.as.number; }

    /* ========= Function implementations (via .inl includes) ========= */
    #include "gl_funcs_core.inl"
    #include "gl_funcs_buffers.inl"
    #include "gl_funcs_shaders.inl"
    #include "gl_funcs_textures.inl"
    #include "gl_funcs_framebuffers.inl"
    #include "gl_funcs_draw.inl"
    #include "gl_funcs_sync.inl"

    /* ========= Function registration table ========= */
    static const NativeReg gl_functions[] = {
        /* --- Loader --- */
        {"LoadOpenGLExtensions", zen_LoadOpenGLExtensions, 0},

        /* --- Core state --- */
        {"glEnable",              zen_glEnable,              1},
        {"glDisable",             zen_glDisable,             1},
        {"glClear",               zen_glClear,               1},
        {"glClearColor",          zen_glClearColor,          4},
        {"glClearDepthf",         zen_glClearDepthf,         1},
        {"glClearStencil",        zen_glClearStencil,        1},
        {"glViewport",            zen_glViewport,            4},
        {"glScissor",             zen_glScissor,             4},
        {"glBlendFunc",           zen_glBlendFunc,           2},
        {"glBlendFuncSeparate",   zen_glBlendFuncSeparate,   4},
        {"glBlendEquation",       zen_glBlendEquation,       1},
        {"glDepthFunc",           zen_glDepthFunc,           1},
        {"glDepthMask",           zen_glDepthMask,           1},
        {"glStencilFunc",         zen_glStencilFunc,         3},
        {"glStencilOp",           zen_glStencilOp,           3},
        {"glStencilMask",         zen_glStencilMask,         1},
        {"glColorMask",           zen_glColorMask,           4},
        {"glCullFace",            zen_glCullFace,            1},
        {"glFrontFace",           zen_glFrontFace,           1},
        {"glLineWidth",           zen_glLineWidth,           1},
        {"glPolygonOffset",       zen_glPolygonOffset,       2},
        {"glPixelStorei",         zen_glPixelStorei,         2},
        {"glFlush",               zen_glFlush,               0},
        {"glFinish",              zen_glFinish,              0},
        {"glGetError",            zen_glGetError,            0},
        {"glGetIntegerv",         zen_glGetIntegerv,         1},
        {"glGetString",           zen_glGetString,           1},
        {"glGetFloatv",           zen_glGetFloatv,           1},
        {"glGetBooleanv",         zen_glGetBooleanv,         1},
        {"glGetInteger64v",       zen_glGetInteger64v,       1},
        {"glGetIntegeri_v",       zen_glGetIntegeri_v,       2},
        {"glGetInteger64i_v",     zen_glGetInteger64i_v,     2},
        {"glGetStringi",          zen_glGetStringi,          2},
        {"glGetFramebufferAttachmentParameteriv", zen_glGetFramebufferAttachmentParameteriv, 3},
        {"glGetRenderbufferParameteriv", zen_glGetRenderbufferParameteriv, 2},
        {"glGetVertexAttribfv",   zen_glGetVertexAttribfv,   2},
        {"glGetVertexAttribiv",   zen_glGetVertexAttribiv,   2},
        {"glGetUniformfv",        zen_glGetUniformfv,        2},
        {"glGetUniformiv",        zen_glGetUniformiv,        2},
        {"glGetUniformuiv",       zen_glGetUniformuiv,       2},
        /* Is* predicates */
        {"glIsEnabled",           zen_glIsEnabled,           1},
        {"glIsBuffer",            zen_glIsBuffer,            1},
        {"glIsFramebuffer",       zen_glIsFramebuffer,       1},
        {"glIsProgram",           zen_glIsProgram,           1},
        {"glIsRenderbuffer",      zen_glIsRenderbuffer,      1},
        {"glIsShader",            zen_glIsShader,            1},
        {"glIsSampler",           zen_glIsSampler,           1},
        {"glIsTexture",           zen_glIsTexture,           1},
        {"glIsVertexArray",       zen_glIsVertexArray,       1},
        /* Stencil per-face */
        {"glStencilFuncSeparate", zen_glStencilFuncSeparate, 4},
        {"glStencilOpSeparate",   zen_glStencilOpSeparate,   4},
        {"glStencilMaskSeparate", zen_glStencilMaskSeparate, 2},

        /* --- Buffers & VAO --- */
        {"glGenBuffers",              zen_glGenBuffers,              1},
        {"glDeleteBuffers",           zen_glDeleteBuffers,           1},
        {"glBindBuffer",              zen_glBindBuffer,              2},
        {"glBufferData",              zen_glBufferData,              3},
        {"glBufferSubData",           zen_glBufferSubData,           3},
        {"glGenVertexArrays",         zen_glGenVertexArrays,         1},
        {"glDeleteVertexArrays",      zen_glDeleteVertexArrays,      1},
        {"glBindVertexArray",         zen_glBindVertexArray,         1},
        {"glVertexAttribPointer",     zen_glVertexAttribPointer,     6},
        {"glVertexAttribIPointer",    zen_glVertexAttribIPointer,    5},
        {"glEnableVertexAttribArray", zen_glEnableVertexAttribArray, 1},
        {"glDisableVertexAttribArray",zen_glDisableVertexAttribArray,1},
        {"glVertexAttribDivisor",     zen_glVertexAttribDivisor,     2},
        /* UBOs */
        {"glGetVertexAttribPointerv", zen_glGetVertexAttribPointerv, 2},
        {"glGetBufferPointerv",       zen_glGetBufferPointerv,       2},
        /* UBOs */
        {"glGetUniformBlockIndex",    zen_glGetUniformBlockIndex,    2},
        {"glUniformBlockBinding",     zen_glUniformBlockBinding,     3},
        {"glBindBufferBase",          zen_glBindBufferBase,          3},
        {"glBindBufferRange",         zen_glBindBufferRange,         5},
        {"glGetActiveUniformBlockiv", zen_glGetActiveUniformBlockiv, 3},
        {"glGetActiveUniformBlockName", zen_glGetActiveUniformBlockName, 2},
        {"glGetActiveUniformsiv",     zen_glGetActiveUniformsiv,     3},
        {"glGetUniformIndices",       zen_glGetUniformIndices,       2},
        /* Buffer Mapping */
        {"glMapBufferRange",          zen_glMapBufferRange,          5},
        {"glUnmapBuffer",             zen_glUnmapBuffer,             1},
        {"glFlushMappedBufferRange",  zen_glFlushMappedBufferRange,  3},
        {"glGetBufferParameteriv",    zen_glGetBufferParameteriv,    2},
        {"glGetBufferParameteri64v",  zen_glGetBufferParameteri64v,  2},
        {"glCopyBufferSubData",       zen_glCopyBufferSubData,       5},
        /* Transform Feedback */
        {"glGenTransformFeedbacks",         zen_glGenTransformFeedbacks,         1},
        {"glDeleteTransformFeedbacks",      zen_glDeleteTransformFeedbacks,      1},
        {"glBindTransformFeedback",         zen_glBindTransformFeedback,         2},
        {"glBeginTransformFeedback",        zen_glBeginTransformFeedback,        1},
        {"glEndTransformFeedback",          zen_glEndTransformFeedback,          0},
        {"glPauseTransformFeedback",        zen_glPauseTransformFeedback,        0},
        {"glResumeTransformFeedback",       zen_glResumeTransformFeedback,       0},
        {"glTransformFeedbackVaryings",     zen_glTransformFeedbackVaryings,     3},
        {"glGetTransformFeedbackVarying",   zen_glGetTransformFeedbackVarying,   2},

        /* --- Shaders & programs --- */
        {"glCreateShader",        zen_glCreateShader,        1},
        {"glDeleteShader",        zen_glDeleteShader,        1},
        {"glShaderSource",        zen_glShaderSource,        2},
        {"glCompileShader",       zen_glCompileShader,       1},
        {"glGetShaderiv",         zen_glGetShaderiv,         2},
        {"glGetShaderInfoLog",    zen_glGetShaderInfoLog,    1},
        {"glCreateProgram",       zen_glCreateProgram,       0},
        {"glDeleteProgram",       zen_glDeleteProgram,       1},
        {"glAttachShader",        zen_glAttachShader,        2},
        {"glDetachShader",        zen_glDetachShader,        2},
        {"glLinkProgram",         zen_glLinkProgram,         1},
        {"glValidateProgram",     zen_glValidateProgram,     1},
        {"glGetProgramiv",        zen_glGetProgramiv,        2},
        {"glGetProgramInfoLog",   zen_glGetProgramInfoLog,   1},
        {"glUseProgram",          zen_glUseProgram,          1},
        {"glBindAttribLocation",  zen_glBindAttribLocation,  3},
        {"glGetAttribLocation",   zen_glGetAttribLocation,   2},
        {"glGetUniformLocation",  zen_glGetUniformLocation,  2},

        /* --- Uniforms --- */
        {"glUniform1i",           zen_glUniform1i,           2},
        {"glUniform2i",           zen_glUniform2i,           3},
        {"glUniform3i",           zen_glUniform3i,           4},
        {"glUniform4i",           zen_glUniform4i,           5},
        {"glUniform1f",           zen_glUniform1f,           2},
        {"glUniform2f",           zen_glUniform2f,           3},
        {"glUniform3f",           zen_glUniform3f,           4},
        {"glUniform4f",           zen_glUniform4f,           5},
        {"glUniform1fv",          zen_glUniform1fv,          3},
        {"glUniform2fv",          zen_glUniform2fv,          3},
        {"glUniform3fv",          zen_glUniform3fv,          3},
        {"glUniform4fv",          zen_glUniform4fv,          3},
        {"glUniformMatrix2fv",    zen_glUniformMatrix2fv,    3},
        {"glUniformMatrix3fv",    zen_glUniformMatrix3fv,    3},
        {"glUniformMatrix4fv",    zen_glUniformMatrix4fv,    3},
        {"glUniform1ui",          zen_glUniform1ui,          2},
        {"glUniform2ui",          zen_glUniform2ui,          3},
        {"glUniform3ui",          zen_glUniform3ui,          4},
        {"glUniform4ui",          zen_glUniform4ui,          5},
        {"glGetActiveUniform",    zen_glGetActiveUniform,    2},
        {"glGetActiveAttrib",     zen_glGetActiveAttrib,     2},
        /* Integer uniform arrays */
        {"glUniform1iv",          zen_glUniform1iv,          3},
        {"glUniform2iv",          zen_glUniform2iv,          3},
        {"glUniform3iv",          zen_glUniform3iv,          3},
        {"glUniform4iv",          zen_glUniform4iv,          3},
        {"glUniform1uiv",         zen_glUniform1uiv,         3},
        {"glUniform2uiv",         zen_glUniform2uiv,         3},
        {"glUniform3uiv",         zen_glUniform3uiv,         3},
        {"glUniform4uiv",         zen_glUniform4uiv,         3},
        /* Non-square matrix uniforms */
        {"glUniformMatrix2x3fv",  zen_glUniformMatrix2x3fv,  4},
        {"glUniformMatrix3x2fv",  zen_glUniformMatrix3x2fv,  4},
        {"glUniformMatrix2x4fv",  zen_glUniformMatrix2x4fv,  4},
        {"glUniformMatrix4x2fv",  zen_glUniformMatrix4x2fv,  4},
        {"glUniformMatrix3x4fv",  zen_glUniformMatrix3x4fv,  4},
        {"glUniformMatrix4x3fv",  zen_glUniformMatrix4x3fv,  4},
        /* Program Binary */
        {"glGetProgramBinary",    zen_glGetProgramBinary,    1},
        {"glProgramBinary",       zen_glProgramBinary,       3},
        {"glProgramParameteri",   zen_glProgramParameteri,   3},

        /* --- Textures --- */
        {"glGenTextures",         zen_glGenTextures,         1},
        {"glDeleteTextures",      zen_glDeleteTextures,      1},
        {"glBindTexture",         zen_glBindTexture,         2},
        {"glActiveTexture",       zen_glActiveTexture,       1},
        {"glTexParameteri",       zen_glTexParameteri,       3},
        {"glTexParameterf",       zen_glTexParameterf,       3},
        {"glTexParameteriv",      zen_glTexParameteriv,      3},
        {"glTexImage2D",          zen_glTexImage2D,          9},
        {"glTexSubImage2D",       zen_glTexSubImage2D,       9},
        {"glTexImage3D",          zen_glTexImage3D,         10},
        {"glTexStorage2D",        zen_glTexStorage2D,        5},
        {"glTexStorage3D",        zen_glTexStorage3D,        6},
        {"glGenerateMipmap",      zen_glGenerateMipmap,      1},
        {"glReadPixels",          zen_glReadPixels,          7},
        {"glGenSamplers",         zen_glGenSamplers,         0},
        {"glDeleteSamplers",      zen_glDeleteSamplers,      1},
        {"glBindSampler",         zen_glBindSampler,         2},
        {"glSamplerParameteri",   zen_glSamplerParameteri,   3},
        {"glSamplerParameterf",   zen_glSamplerParameterf,   3},
        /* Missing texture ops */
        {"glTexSubImage3D",              zen_glTexSubImage3D,              11},
        {"glCopyTexImage2D",             zen_glCopyTexImage2D,             8},
        {"glCopyTexSubImage2D",          zen_glCopyTexSubImage2D,          8},
        {"glCopyTexSubImage3D",          zen_glCopyTexSubImage3D,          9},
        {"glCompressedTexImage2D",       zen_glCompressedTexImage2D,       7},
        {"glCompressedTexSubImage2D",    zen_glCompressedTexSubImage2D,    8},
        {"glCompressedTexImage3D",       zen_glCompressedTexImage3D,       8},
        {"glCompressedTexSubImage3D",    zen_glCompressedTexSubImage3D,    10},
        {"glGetTexParameteriv",          zen_glGetTexParameteriv,          2},
        {"glGetTexParameterfv",          zen_glGetTexParameterfv,          2},
        {"glInvalidateFramebuffer",      zen_glInvalidateFramebuffer,      2},
        {"glInvalidateSubFramebuffer",   zen_glInvalidateSubFramebuffer,   6},

        /* --- Framebuffers --- */
        {"glGenFramebuffers",              zen_glGenFramebuffers,              0},
        {"glDeleteFramebuffers",           zen_glDeleteFramebuffers,           1},
        {"glBindFramebuffer",              zen_glBindFramebuffer,              2},
        {"glCheckFramebufferStatus",       zen_glCheckFramebufferStatus,       1},
        {"glFramebufferTexture2D",         zen_glFramebufferTexture2D,         5},
        {"glFramebufferRenderbuffer",      zen_glFramebufferRenderbuffer,      4},
        {"glFramebufferTextureLayer",      zen_glFramebufferTextureLayer,      5},
        {"glDrawBuffers",                  zen_glDrawBuffers,                  1},
        {"glReadBuffer",                   zen_glReadBuffer,                   1},
        {"glGenRenderbuffers",             zen_glGenRenderbuffers,             0},
        {"glDeleteRenderbuffers",          zen_glDeleteRenderbuffers,          1},
        {"glBindRenderbuffer",             zen_glBindRenderbuffer,             2},
        {"glRenderbufferStorage",          zen_glRenderbufferStorage,          4},
        {"glRenderbufferStorageMultisample", zen_glRenderbufferStorageMultisample, 5},
        {"glBlitFramebuffer",              zen_glBlitFramebuffer,             10},

        /* --- Draw commands --- */
        {"glDrawArrays",                zen_glDrawArrays,                3},
        {"glDrawElements",              zen_glDrawElements,              4},
        {"glDrawArraysInstanced",       zen_glDrawArraysInstanced,       4},
        {"glDrawElementsInstanced",     zen_glDrawElementsInstanced,     5},
        {"glDrawRangeElements",         zen_glDrawRangeElements,         6},

        /* --- Sync objects --- */
        {"glFenceSync",           zen_glFenceSync,           2},
        {"glClientWaitSync",      zen_glClientWaitSync,      3},
        {"glWaitSync",            zen_glWaitSync,            3},
        {"glDeleteSync",          zen_glDeleteSync,          1},
        {"glGetSynciv",           zen_glGetSynciv,           2},
        {"glIsSync",              zen_glIsSync,              1},

        /* --- Queries --- */
        {"glGenQueries",          zen_glGenQueries,          1},
        {"glDeleteQueries",       zen_glDeleteQueries,       1},
        {"glBeginQuery",          zen_glBeginQuery,          2},
        {"glEndQuery",            zen_glEndQuery,            1},
        {"glGetQueryiv",          zen_glGetQueryiv,          2},
        {"glGetQueryObjectuiv",   zen_glGetQueryObjectuiv,   2},
        {"glIsQuery",             zen_glIsQuery,             1},
    };

    /* ========= Constants ========= */
    #include "gl_constants.inl"

    /* ========= NativeLib definition ========= */
    const NativeLib zen_lib_gl = {
        "gl",
        gl_functions,   ZEN_ARRAY_COUNT(gl_functions),
        gl_constants,   ZEN_ARRAY_COUNT(gl_constants),
        nullptr         /* no init_fn needed */
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_GL */
