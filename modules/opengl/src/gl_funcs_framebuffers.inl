/* gl_funcs_framebuffers.inl — FBO & RBO functions (GLES 3.0 compatible)
** Included by zen_gl.cpp */

static int zen_glGenFramebuffers(VM *, Value *args, int) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    args[0] = val_int((int)fbo);
    return 1;
}
static int zen_glDeleteFramebuffers(VM *, Value *args, int) {
    GLuint fbo = (GLuint)ival(args[0]);
    glDeleteFramebuffers(1, &fbo);
    return 0;
}
static int zen_glBindFramebuffer(VM *, Value *args, int) {
    glBindFramebuffer((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glCheckFramebufferStatus(VM *vm, Value *args, int) {
    GLenum st = glCheckFramebufferStatus((GLenum)ival(args[0]));
    args[0] = val_int((int)st);
    return 1;
}
static int zen_glFramebufferTexture2D(VM *, Value *args, int) {
    glFramebufferTexture2D((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                           (GLenum)ival(args[2]), (GLuint)ival(args[3]), ival(args[4]));
    return 0;
}
static int zen_glFramebufferRenderbuffer(VM *, Value *args, int) {
    glFramebufferRenderbuffer((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                              (GLenum)ival(args[2]), (GLuint)ival(args[3]));
    return 0;
}
static int zen_glFramebufferTextureLayer(VM *, Value *args, int) {
    glFramebufferTextureLayer((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                              (GLuint)ival(args[2]), ival(args[3]), ival(args[4]));
    return 0;
}

/* glDrawBuffers(array_of_attachments) */
static int zen_glDrawBuffers(VM *vm, Value *args, int) {
    if (!is_array(args[0])) { vm->runtime_error("glDrawBuffers: arg must be array"); return 0; }
    ObjArray *arr = as_array(args[0]);
    int n = arr_count(arr);
    GLenum bufs[16];
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++)
        bufs[i] = (GLenum)ival(arr->data[i]);
    glDrawBuffers((GLsizei)n, bufs);
    return 0;
}
static int zen_glReadBuffer(VM *, Value *args, int) {
    glReadBuffer((GLenum)ival(args[0]));
    return 0;
}

/* Renderbuffers */
static int zen_glGenRenderbuffers(VM *, Value *args, int) {
    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    args[0] = val_int((int)rbo);
    return 1;
}
static int zen_glDeleteRenderbuffers(VM *, Value *args, int) {
    GLuint rbo = (GLuint)ival(args[0]);
    glDeleteRenderbuffers(1, &rbo);
    return 0;
}
static int zen_glBindRenderbuffer(VM *, Value *args, int) {
    glBindRenderbuffer((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glRenderbufferStorage(VM *, Value *args, int) {
    glRenderbufferStorage((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                          (GLsizei)ival(args[2]), (GLsizei)ival(args[3]));
    return 0;
}
static int zen_glRenderbufferStorageMultisample(VM *, Value *args, int) {
    glRenderbufferStorageMultisample((GLenum)ival(args[0]), (GLsizei)ival(args[1]),
                                    (GLenum)ival(args[2]), (GLsizei)ival(args[3]),
                                    (GLsizei)ival(args[4]));
    return 0;
}

/* Blit (GLES 3.0) */
static int zen_glBlitFramebuffer(VM *, Value *args, int) {
    glBlitFramebuffer(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]),
                      ival(args[4]), ival(args[5]), ival(args[6]), ival(args[7]),
                      (GLbitfield)ival(args[8]), (GLenum)ival(args[9]));
    return 0;
}
