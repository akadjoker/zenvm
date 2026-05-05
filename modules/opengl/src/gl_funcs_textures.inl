/* gl_funcs_textures.inl — Texture functions (GLES 3.0 compatible)
** Included by zen_gl.cpp */

static int zen_glGenTextures(VM *vm, Value *args, int) {
    GLuint tex = 0;
    int n = ival(args[0]);
    if (n == 1) {
        glGenTextures(1, &tex);
        args[0] = val_int((int)tex);
    } else {
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++) {
            glGenTextures(1, &tex);
            array_push(&vm->get_gc(), arr, val_int((int)tex));
        }
        args[0] = val_obj((Obj *)arr);
    }
    return 1;
}
static int zen_glDeleteTextures(VM *, Value *args, int) {
    GLuint tex = (GLuint)ival(args[0]);
    glDeleteTextures(1, &tex);
    return 0;
}
static int zen_glBindTexture(VM *, Value *args, int) {
    glBindTexture((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glActiveTexture(VM *, Value *args, int) {
    glActiveTexture((GLenum)ival(args[0]));
    return 0;
}
static int zen_glTexParameteri(VM *, Value *args, int) {
    glTexParameteri((GLenum)ival(args[0]), (GLenum)ival(args[1]), ival(args[2]));
    return 0;
}
static int zen_glTexParameterf(VM *, Value *args, int) {
    glTexParameterf((GLenum)ival(args[0]), (GLenum)ival(args[1]), (GLfloat)fval(args[2]));
    return 0;
}
/* glTexParameteriv(target, pname, array_of_4_ints) — used for GL_TEXTURE_SWIZZLE_RGBA */
static int zen_glTexParameteriv(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    GLenum pname  = (GLenum)ival(args[1]);
    if (!is_array(args[2])) { vm->runtime_error("glTexParameteriv: arg 3 must be an array"); return 0; }
    ObjArray *arr = as_array(args[2]);
    int n = arr_count(arr);
    GLint params[4] = {0, 0, 0, 0};
    if (n > 4) n = 4;
    for (int i = 0; i < n; i++) params[i] = ival(arr->data[i]);
    glTexParameteriv(target, pname, params);
    return 0;
}

/* glTexImage2D(target, level, internalformat, width, height, border, format, type, buffer_or_nil) */
static int zen_glTexImage2D(VM *vm, Value *args, int) {
    GLenum target   = (GLenum)ival(args[0]);
    GLint level     = ival(args[1]);
    GLint ifmt      = ival(args[2]);
    GLsizei w       = (GLsizei)ival(args[3]);
    GLsizei h       = (GLsizei)ival(args[4]);
    GLint border    = ival(args[5]);
    GLenum fmt      = (GLenum)ival(args[6]);
    GLenum type     = (GLenum)ival(args[7]);
    const void *data = nullptr;
    if (is_buffer(args[8])) {
        ObjBuffer *b = as_buffer(args[8]);
        data = b->data;
    }
    glTexImage2D(target, level, ifmt, w, h, border, fmt, type, data);
    return 0;
}

/* glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, buffer) */
static int zen_glTexSubImage2D(VM *vm, Value *args, int) {
    GLenum target   = (GLenum)ival(args[0]);
    GLint level     = ival(args[1]);
    GLint xoff      = ival(args[2]);
    GLint yoff      = ival(args[3]);
    GLsizei w       = (GLsizei)ival(args[4]);
    GLsizei h       = (GLsizei)ival(args[5]);
    GLenum fmt      = (GLenum)ival(args[6]);
    GLenum type     = (GLenum)ival(args[7]);
    const void *data = nullptr;
    if (is_buffer(args[8])) {
        ObjBuffer *b = as_buffer(args[8]);
        data = b->data;
    }
    glTexSubImage2D(target, level, xoff, yoff, w, h, fmt, type, data);
    return 0;
}

static int zen_glGenerateMipmap(VM *, Value *args, int) {
    glGenerateMipmap((GLenum)ival(args[0]));
    return 0;
}

/* glReadPixels(x, y, w, h, format, type, buffer)
** Fills the caller's buffer in-place; returns nothing.
** The buffer must be large enough: w * h * channels * bpc bytes. */
static int zen_glReadPixels(VM *vm, Value *args, int) {
    GLint x       = ival(args[0]);
    GLint y       = ival(args[1]);
    GLsizei w     = (GLsizei)ival(args[2]);
    GLsizei h     = (GLsizei)ival(args[3]);
    GLenum fmt    = (GLenum)ival(args[4]);
    GLenum type   = (GLenum)ival(args[5]);
    if (!is_buffer(args[6])) { vm->runtime_error("glReadPixels: arg 7 must be a Buffer"); return 0; }
    ObjBuffer *buf = as_buffer(args[6]);
    glReadPixels(x, y, w, h, fmt, type, buf->data);
    return 0;
}

/* GLES 3.0: glTexImage3D, glTexStorage2D, glTexStorage3D */
static int zen_glTexImage3D(VM *vm, Value *args, int) {
    GLenum target   = (GLenum)ival(args[0]);
    GLint level     = ival(args[1]);
    GLint ifmt      = ival(args[2]);
    GLsizei w       = (GLsizei)ival(args[3]);
    GLsizei h       = (GLsizei)ival(args[4]);
    GLsizei d       = (GLsizei)ival(args[5]);
    GLint border    = ival(args[6]);
    GLenum fmt      = (GLenum)ival(args[7]);
    GLenum type     = (GLenum)ival(args[8]);
    const void *data = nullptr;
    if (is_buffer(args[9])) {
        ObjBuffer *b = as_buffer(args[9]);
        data = b->data;
    }
    glTexImage3D(target, level, ifmt, w, h, d, border, fmt, type, data);
    return 0;
}
static int zen_glTexStorage2D(VM *, Value *args, int) {
    glTexStorage2D((GLenum)ival(args[0]), (GLsizei)ival(args[1]),
                   (GLenum)ival(args[2]), (GLsizei)ival(args[3]), (GLsizei)ival(args[4]));
    return 0;
}
static int zen_glTexStorage3D(VM *, Value *args, int) {
    glTexStorage3D((GLenum)ival(args[0]), (GLsizei)ival(args[1]),
                   (GLenum)ival(args[2]), (GLsizei)ival(args[3]),
                   (GLsizei)ival(args[4]), (GLsizei)ival(args[5]));
    return 0;
}

/* Samplers (GLES 3.0) */
static int zen_glGenSamplers(VM *, Value *args, int) {
    GLuint s = 0;
    glGenSamplers(1, &s);
    args[0] = val_int((int)s);
    return 1;
}
static int zen_glDeleteSamplers(VM *, Value *args, int) {
    GLuint s = (GLuint)ival(args[0]);
    glDeleteSamplers(1, &s);
    return 0;
}
static int zen_glBindSampler(VM *, Value *args, int) {
    glBindSampler((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glSamplerParameteri(VM *, Value *args, int) {
    glSamplerParameteri((GLuint)ival(args[0]), (GLenum)ival(args[1]), ival(args[2]));
    return 0;
}
static int zen_glSamplerParameterf(VM *, Value *args, int) {
    glSamplerParameterf((GLuint)ival(args[0]), (GLenum)ival(args[1]), (GLfloat)fval(args[2]));
    return 0;
}

/* ===================================================================
** GLES 3.0 — Missing texture functions
** =================================================================== */

/* glTexSubImage3D(target,level,xoff,yoff,zoff,w,h,d,format,type,buffer) */
static int zen_glTexSubImage3D(VM *vm, Value *args, int) {
    GLenum target  = (GLenum)ival(args[0]);
    GLint level    = ival(args[1]);
    GLint xoff     = ival(args[2]);
    GLint yoff     = ival(args[3]);
    GLint zoff     = ival(args[4]);
    GLsizei w      = (GLsizei)ival(args[5]);
    GLsizei h      = (GLsizei)ival(args[6]);
    GLsizei d      = (GLsizei)ival(args[7]);
    GLenum fmt     = (GLenum)ival(args[8]);
    GLenum type    = (GLenum)ival(args[9]);
    const void *data = nullptr;
    if (is_buffer(args[10])) data = as_buffer(args[10])->data;
    glTexSubImage3D(target, level, xoff, yoff, zoff, w, h, d, fmt, type, data);
    return 0;
}

/* glCopyTexImage2D(target, level, internalformat, x, y, w, h, border) */
static int zen_glCopyTexImage2D(VM *, Value *args, int) {
    glCopyTexImage2D((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                     ival(args[3]), ival(args[4]),
                     (GLsizei)ival(args[5]), (GLsizei)ival(args[6]), ival(args[7]));
    return 0;
}
/* glCopyTexSubImage2D(target, level, xoff, yoff, x, y, w, h) */
static int zen_glCopyTexSubImage2D(VM *, Value *args, int) {
    glCopyTexSubImage2D((GLenum)ival(args[0]), ival(args[1]),
                        ival(args[2]), ival(args[3]),
                        ival(args[4]), ival(args[5]),
                        (GLsizei)ival(args[6]), (GLsizei)ival(args[7]));
    return 0;
}
/* glCopyTexSubImage3D(target, level, xoff, yoff, zoff, x, y, w, h) */
static int zen_glCopyTexSubImage3D(VM *, Value *args, int) {
    glCopyTexSubImage3D((GLenum)ival(args[0]), ival(args[1]),
                        ival(args[2]), ival(args[3]), ival(args[4]),
                        ival(args[5]), ival(args[6]),
                        (GLsizei)ival(args[7]), (GLsizei)ival(args[8]));
    return 0;
}

/* glCompressedTexImage2D(target,level,internalformat,w,h,border,buffer) */
static int zen_glCompressedTexImage2D(VM *vm, Value *args, int) {
    const void *data = nullptr;
    GLsizei imageSize = 0;
    if (is_buffer(args[6])) {
        ObjBuffer *b = as_buffer(args[6]);
        data = b->data;
        imageSize = (GLsizei)(b->count * buffer_elem_size[b->btype]);
    }
    glCompressedTexImage2D((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                           (GLsizei)ival(args[3]), (GLsizei)ival(args[4]),
                           ival(args[5]), imageSize, data);
    return 0;
}
/* glCompressedTexSubImage2D(target,level,xoff,yoff,w,h,format,buffer) */
static int zen_glCompressedTexSubImage2D(VM *vm, Value *args, int) {
    const void *data = nullptr;
    GLsizei imageSize = 0;
    if (is_buffer(args[7])) {
        ObjBuffer *b = as_buffer(args[7]);
        data = b->data;
        imageSize = (GLsizei)(b->count * buffer_elem_size[b->btype]);
    }
    glCompressedTexSubImage2D((GLenum)ival(args[0]), ival(args[1]),
                              ival(args[2]), ival(args[3]),
                              (GLsizei)ival(args[4]), (GLsizei)ival(args[5]),
                              (GLenum)ival(args[6]), imageSize, data);
    return 0;
}
/* glCompressedTexImage3D(target,level,internalformat,w,h,d,border,buffer) */
static int zen_glCompressedTexImage3D(VM *vm, Value *args, int) {
    const void *data = nullptr;
    GLsizei imageSize = 0;
    if (is_buffer(args[7])) {
        ObjBuffer *b = as_buffer(args[7]);
        data = b->data;
        imageSize = (GLsizei)(b->count * buffer_elem_size[b->btype]);
    }
    glCompressedTexImage3D((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                           (GLsizei)ival(args[3]), (GLsizei)ival(args[4]),
                           (GLsizei)ival(args[5]), ival(args[6]), imageSize, data);
    return 0;
}
/* glCompressedTexSubImage3D(target,level,xoff,yoff,zoff,w,h,d,format,buffer) */
static int zen_glCompressedTexSubImage3D(VM *vm, Value *args, int) {
    const void *data = nullptr;
    GLsizei imageSize = 0;
    if (is_buffer(args[9])) {
        ObjBuffer *b = as_buffer(args[9]);
        data = b->data;
        imageSize = (GLsizei)(b->count * buffer_elem_size[b->btype]);
    }
    glCompressedTexSubImage3D((GLenum)ival(args[0]), ival(args[1]),
                              ival(args[2]), ival(args[3]), ival(args[4]),
                              (GLsizei)ival(args[5]), (GLsizei)ival(args[6]),
                              (GLsizei)ival(args[7]), (GLenum)ival(args[8]),
                              imageSize, data);
    return 0;
}

/* glGetTexParameteriv(target, pname) → int */
static int zen_glGetTexParameteriv(VM *, Value *args, int) {
    GLint v = 0;
    glGetTexParameteriv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetTexParameterfv(target, pname) → float */
static int zen_glGetTexParameterfv(VM *, Value *args, int) {
    GLfloat v = 0.0f;
    glGetTexParameterfv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_float((double)v);
    return 1;
}

/* glInvalidateFramebuffer(target, array_of_attachments) */
static int zen_glInvalidateFramebuffer(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    if (!is_array(args[1])) { vm->runtime_error("glInvalidateFramebuffer: arg 2 must be array"); return 0; }
    ObjArray *arr = as_array(args[1]);
    int n = arr_count(arr);
    GLenum attachments[16];
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) attachments[i] = (GLenum)ival(arr->data[i]);
    glInvalidateFramebuffer(target, (GLsizei)n, attachments);
    return 0;
}
/* glInvalidateSubFramebuffer(target, array_of_attachments, x, y, w, h) */
static int zen_glInvalidateSubFramebuffer(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    if (!is_array(args[1])) { vm->runtime_error("glInvalidateSubFramebuffer: arg 2 must be array"); return 0; }
    ObjArray *arr = as_array(args[1]);
    int n = arr_count(arr);
    GLenum attachments[16];
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) attachments[i] = (GLenum)ival(arr->data[i]);
    glInvalidateSubFramebuffer(target, (GLsizei)n, attachments,
                               ival(args[2]), ival(args[3]),
                               (GLsizei)ival(args[4]), (GLsizei)ival(args[5]));
    return 0;
}
