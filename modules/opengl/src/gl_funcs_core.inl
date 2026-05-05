/* gl_funcs_core.inl — Core state management functions (GLES 3.0 compatible)
** Included by zen_gl.cpp */

/* ===================================================================
** LoadOpenGLExtensions — platform-native glad loader.
** Call once from Zen after creating the GL context.
** No dependency on GLFW/SDL: uses the platform GL ABI directly.
** =================================================================== */
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#  if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
static GLADapiproc zen_gl_get_proc(const char *name)
{
    GLADapiproc p = (GLADapiproc)wglGetProcAddress(name);
    if (!p) {
        static HMODULE lib = LoadLibraryA("opengl32.dll");
        p = lib ? (GLADapiproc)GetProcAddress(lib, name) : nullptr;
    }
    return p;
}
#  elif defined(__APPLE__)
#    include <dlfcn.h>
static GLADapiproc zen_gl_get_proc(const char *name)
{
    static void *lib = dlopen(
        "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    return lib ? (GLADapiproc)dlsym(lib, name) : nullptr;
}
#  else /* Linux / other Unix */
#    include <dlfcn.h>
static GLADapiproc zen_gl_get_proc(const char *name)
{
    /* libGL is already loaded by the host (GLFW/SDL/EGL opened it).
       RTLD_DEFAULT resolves from the already-loaded symbol tables. */
    return (GLADapiproc)dlsym(RTLD_DEFAULT, name);
}
#  endif
#endif /* !EMSCRIPTEN && !ANDROID */

static int zen_LoadOpenGLExtensions(VM *, Value *args, int)
{
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    args[0] = val_bool(true); /* GLES — symbols always available */
    return 1;
#else
    int ok = gladLoadGL(zen_gl_get_proc);
    args[0] = val_bool(ok != 0);
    return 1;
#endif
}

static int zen_glEnable(VM *, Value *args, int) {
    glEnable((GLenum)ival(args[0]));
    return 0;
}
static int zen_glDisable(VM *, Value *args, int) {
    glDisable((GLenum)ival(args[0]));
    return 0;
}
static int zen_glClear(VM *, Value *args, int) {
    glClear((GLbitfield)ival(args[0]));
    return 0;
}
static int zen_glClearColor(VM *vm, Value *args, int) {
    glClearColor((GLfloat)fval(args[0]), (GLfloat)fval(args[1]),
                 (GLfloat)fval(args[2]), (GLfloat)fval(args[3]));
    return 0;
}
static int zen_glClearDepthf(VM *, Value *args, int) {
    glClearDepthf((GLfloat)fval(args[0]));
    return 0;
}
static int zen_glClearStencil(VM *, Value *args, int) {
    glClearStencil(ival(args[0]));
    return 0;
}
static int zen_glViewport(VM *, Value *args, int) {
    glViewport(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]));
    return 0;
}
static int zen_glScissor(VM *, Value *args, int) {
    glScissor(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]));
    return 0;
}
static int zen_glBlendFunc(VM *, Value *args, int) {
    glBlendFunc((GLenum)ival(args[0]), (GLenum)ival(args[1]));
    return 0;
}
static int zen_glBlendFuncSeparate(VM *, Value *args, int) {
    glBlendFuncSeparate((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                        (GLenum)ival(args[2]), (GLenum)ival(args[3]));
    return 0;
}
static int zen_glBlendEquation(VM *, Value *args, int) {
    glBlendEquation((GLenum)ival(args[0]));
    return 0;
}
static int zen_glDepthFunc(VM *, Value *args, int) {
    glDepthFunc((GLenum)ival(args[0]));
    return 0;
}
static int zen_glDepthMask(VM *, Value *args, int) {
    glDepthMask((GLboolean)ival(args[0]));
    return 0;
}
static int zen_glStencilFunc(VM *, Value *args, int) {
    glStencilFunc((GLenum)ival(args[0]), ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}
static int zen_glStencilOp(VM *, Value *args, int) {
    glStencilOp((GLenum)ival(args[0]), (GLenum)ival(args[1]), (GLenum)ival(args[2]));
    return 0;
}
static int zen_glStencilMask(VM *, Value *args, int) {
    glStencilMask((GLuint)ival(args[0]));
    return 0;
}
static int zen_glColorMask(VM *, Value *args, int) {
    glColorMask((GLboolean)ival(args[0]), (GLboolean)ival(args[1]),
                (GLboolean)ival(args[2]), (GLboolean)ival(args[3]));
    return 0;
}
static int zen_glCullFace(VM *, Value *args, int) {
    glCullFace((GLenum)ival(args[0]));
    return 0;
}
static int zen_glFrontFace(VM *, Value *args, int) {
    glFrontFace((GLenum)ival(args[0]));
    return 0;
}
static int zen_glLineWidth(VM *, Value *args, int) {
    glLineWidth((GLfloat)fval(args[0]));
    return 0;
}
static int zen_glPolygonOffset(VM *, Value *args, int) {
    glPolygonOffset((GLfloat)fval(args[0]), (GLfloat)fval(args[1]));
    return 0;
}
static int zen_glPixelStorei(VM *, Value *args, int) {
    glPixelStorei((GLenum)ival(args[0]), ival(args[1]));
    return 0;
}
static int zen_glFlush(VM *, Value *, int) {
    glFlush();
    return 0;
}
static int zen_glFinish(VM *, Value *, int) {
    glFinish();
    return 0;
}
static int zen_glGetError(VM *, Value *args, int) {
    args[0] = val_int((int)glGetError());
    return 1;
}
static int zen_glGetIntegerv(VM *vm, Value *args, int) {
    GLint v = 0;
    glGetIntegerv((GLenum)ival(args[0]), &v);
    args[0] = val_int(v);
    return 1;
}
static int zen_glGetString(VM *vm, Value *args, int) {
    const char *s = (const char *)glGetString((GLenum)ival(args[0]));
    if (s)
        args[0] = val_obj((Obj *)vm->make_string(s, -1));
    else
        args[0] = val_nil();
    return 1;
}

/* ===================================================================
** GLES 3.0 — Additional state queries
** =================================================================== */
static int zen_glGetFloatv(VM *, Value *args, int) {
    GLfloat v = 0.0f;
    glGetFloatv((GLenum)ival(args[0]), &v);
    args[0] = val_float((double)v);
    return 1;
}
static int zen_glGetBooleanv(VM *, Value *args, int) {
    GLboolean v = GL_FALSE;
    glGetBooleanv((GLenum)ival(args[0]), &v);
    args[0] = val_bool(v == GL_TRUE);
    return 1;
}
static int zen_glGetInteger64v(VM *, Value *args, int) {
    GLint64 v = 0;
    glGetInteger64v((GLenum)ival(args[0]), &v);
    args[0] = val_int((int)v);
    return 1;
}
/* glGetIntegeri_v(pname, index) → int */
static int zen_glGetIntegeri_v(VM *, Value *args, int) {
    GLint v = 0;
    glGetIntegeri_v((GLenum)ival(args[0]), (GLuint)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetInteger64i_v(pname, index) → int */
static int zen_glGetInteger64i_v(VM *, Value *args, int) {
    GLint64 v = 0;
    glGetInteger64i_v((GLenum)ival(args[0]), (GLuint)ival(args[1]), &v);
    args[0] = val_int((int)v);
    return 1;
}
/* glGetFramebufferAttachmentParameteriv(target, attachment, pname) → int */
static int zen_glGetFramebufferAttachmentParameteriv(VM *, Value *args, int) {
    GLint v = 0;
    glGetFramebufferAttachmentParameteriv((GLenum)ival(args[0]),
                                          (GLenum)ival(args[1]),
                                          (GLenum)ival(args[2]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetRenderbufferParameteriv(target, pname) → int */
static int zen_glGetRenderbufferParameteriv(VM *, Value *args, int) {
    GLint v = 0;
    glGetRenderbufferParameteriv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetVertexAttribfv(index, pname) → float */
static int zen_glGetVertexAttribfv(VM *, Value *args, int) {
    GLfloat v[4] = {0};
    glGetVertexAttribfv((GLuint)ival(args[0]), (GLenum)ival(args[1]), v);
    args[0] = val_float((double)v[0]);
    return 1;
}
/* glGetVertexAttribiv(index, pname) → int */
static int zen_glGetVertexAttribiv(VM *, Value *args, int) {
    GLint v = 0;
    glGetVertexAttribiv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetUniformfv(prog, loc) → float */
static int zen_glGetUniformfv(VM *, Value *args, int) {
    GLfloat v[16] = {0};
    glGetUniformfv((GLuint)ival(args[0]), ival(args[1]), v);
    args[0] = val_float((double)v[0]);
    return 1;
}
/* glGetUniformiv(prog, loc) → int */
static int zen_glGetUniformiv(VM *, Value *args, int) {
    GLint v = 0;
    glGetUniformiv((GLuint)ival(args[0]), ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetUniformuiv(prog, loc) → int */
static int zen_glGetUniformuiv(VM *, Value *args, int) {
    GLuint v = 0;
    glGetUniformuiv((GLuint)ival(args[0]), ival(args[1]), &v);
    args[0] = val_int((int)v);
    return 1;
}
/* glGetStringi(name, index) → string */
static int zen_glGetStringi(VM *vm, Value *args, int) {
    const char *s = (const char *)glGetStringi((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    args[0] = s ? val_obj((Obj *)vm->make_string(s, -1)) : val_nil();
    return 1;
}

/* ===================================================================
** GLES 3.0 — Is* predicates
** =================================================================== */
static int zen_glIsEnabled(VM *, Value *args, int) {
    args[0] = val_bool(glIsEnabled((GLenum)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsBuffer(VM *, Value *args, int) {
    args[0] = val_bool(glIsBuffer((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsFramebuffer(VM *, Value *args, int) {
    args[0] = val_bool(glIsFramebuffer((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsProgram(VM *, Value *args, int) {
    args[0] = val_bool(glIsProgram((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsRenderbuffer(VM *, Value *args, int) {
    args[0] = val_bool(glIsRenderbuffer((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsShader(VM *, Value *args, int) {
    args[0] = val_bool(glIsShader((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsSampler(VM *, Value *args, int) {
    args[0] = val_bool(glIsSampler((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsTexture(VM *, Value *args, int) {
    args[0] = val_bool(glIsTexture((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
static int zen_glIsVertexArray(VM *, Value *args, int) {
    args[0] = val_bool(glIsVertexArray((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}

/* ===================================================================
** GLES 3.0 — Stencil per-face
** =================================================================== */
static int zen_glStencilFuncSeparate(VM *, Value *args, int) {
    glStencilFuncSeparate((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                          ival(args[2]), (GLuint)ival(args[3]));
    return 0;
}
static int zen_glStencilOpSeparate(VM *, Value *args, int) {
    glStencilOpSeparate((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                        (GLenum)ival(args[2]), (GLenum)ival(args[3]));
    return 0;
}
static int zen_glStencilMaskSeparate(VM *, Value *args, int) {
    glStencilMaskSeparate((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
