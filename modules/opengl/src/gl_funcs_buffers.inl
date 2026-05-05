/* gl_funcs_buffers.inl — Buffer & VAO functions (GLES 3.0 compatible)
** Included by zen_gl.cpp */

static int zen_glGenBuffers(VM *vm, Value *args, int) {
    GLuint buf = 0;
    int n = ival(args[0]);
    if (n == 1) {
        glGenBuffers(1, &buf);
        args[0] = val_int((int)buf);
    } else {
        /* Return array of ids */
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++) {
            glGenBuffers(1, &buf);
            array_push(&vm->get_gc(), arr, val_int((int)buf));
        }
        args[0] = val_obj((Obj *)arr);
    }
    return 1;
}
static int zen_glDeleteBuffers(VM *vm, Value *args, int) {
    GLuint buf = (GLuint)ival(args[0]);
    glDeleteBuffers(1, &buf);
    return 0;
}
static int zen_glBindBuffer(VM *, Value *args, int) {
    glBindBuffer((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glBufferData(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    GLenum usage  = (GLenum)ival(args[2]);
    /* args[1] = buffer object (ObjBuffer) with raw data */
    if (is_buffer(args[1])) {
        ObjBuffer *b = as_buffer(args[1]);
        GLsizeiptr bytes = (GLsizeiptr)b->count * buffer_elem_size[b->btype];
        glBufferData(target, bytes, b->data, usage);
    } else {
        /* size only, no data */
        glBufferData(target, (GLsizeiptr)ival(args[1]), nullptr, usage);
    }
    return 0;
}
static int zen_glBufferSubData(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    GLintptr offset = (GLintptr)ival(args[1]);
    if (is_buffer(args[2])) {
        ObjBuffer *b = as_buffer(args[2]);
        GLsizeiptr bytes = (GLsizeiptr)b->count * buffer_elem_size[b->btype];
        glBufferSubData(target, offset, bytes, b->data);
    }
    return 0;
}

/* VAO */
static int zen_glGenVertexArrays(VM *vm, Value *args, int) {
    GLuint vao = 0;
    int n = ival(args[0]);
    if (n == 1) {
        glGenVertexArrays(1, &vao);
        args[0] = val_int((int)vao);
    } else {
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++) {
            glGenVertexArrays(1, &vao);
            array_push(&vm->get_gc(), arr, val_int((int)vao));
        }
        args[0] = val_obj((Obj *)arr);
    }
    return 1;
}
static int zen_glDeleteVertexArrays(VM *vm, Value *args, int) {
    GLuint vao = (GLuint)ival(args[0]);
    glDeleteVertexArrays(1, &vao);
    return 0;
}
static int zen_glBindVertexArray(VM *, Value *args, int) {
    glBindVertexArray((GLuint)ival(args[0]));
    return 0;
}
static int zen_glVertexAttribPointer(VM *, Value *args, int) {
    GLuint index     = (GLuint)ival(args[0]);
    GLint size       = ival(args[1]);
    GLenum type      = (GLenum)ival(args[2]);
    GLboolean norm   = (GLboolean)ival(args[3]);
    GLsizei stride   = (GLsizei)ival(args[4]);
    GLintptr offset  = (GLintptr)ival(args[5]);
    glVertexAttribPointer(index, size, type, norm, stride, (const void *)offset);
    return 0;
}
static int zen_glVertexAttribIPointer(VM *, Value *args, int) {
    GLuint index     = (GLuint)ival(args[0]);
    GLint size       = ival(args[1]);
    GLenum type      = (GLenum)ival(args[2]);
    GLsizei stride   = (GLsizei)ival(args[3]);
    GLintptr offset  = (GLintptr)ival(args[4]);
    glVertexAttribIPointer(index, size, type, stride, (const void *)offset);
    return 0;
}
static int zen_glEnableVertexAttribArray(VM *, Value *args, int) {
    glEnableVertexAttribArray((GLuint)ival(args[0]));
    return 0;
}
static int zen_glDisableVertexAttribArray(VM *, Value *args, int) {
    glDisableVertexAttribArray((GLuint)ival(args[0]));
    return 0;
}
static int zen_glVertexAttribDivisor(VM *, Value *args, int) {
    glVertexAttribDivisor((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}

/* ===================================================================
** GLES 3.0 — Uniform Buffer Objects (UBOs)
** =================================================================== */
static int zen_glGetUniformBlockIndex(VM *vm, Value *args, int) {
    if (!is_string(args[1])) { vm->runtime_error("glGetUniformBlockIndex: arg 2 must be string"); return 0; }
    GLuint idx = glGetUniformBlockIndex((GLuint)ival(args[0]), as_string(args[1])->chars);
    args[0] = val_int((int)idx);
    return 1;
}
static int zen_glUniformBlockBinding(VM *, Value *args, int) {
    glUniformBlockBinding((GLuint)ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}
static int zen_glBindBufferBase(VM *, Value *args, int) {
    glBindBufferBase((GLenum)ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}
static int zen_glBindBufferRange(VM *, Value *args, int) {
    glBindBufferRange((GLenum)ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]),
                      (GLintptr)ival(args[3]), (GLsizeiptr)ival(args[4]));
    return 0;
}
/* glGetActiveUniformBlockiv(prog, blockIndex, pname) → int */
static int zen_glGetActiveUniformBlockiv(VM *, Value *args, int) {
    GLint v = 0;
    glGetActiveUniformBlockiv((GLuint)ival(args[0]), (GLuint)ival(args[1]),
                              (GLenum)ival(args[2]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetActiveUniformBlockName(prog, blockIndex) → string *//* glGetActiveUniformBlockName(prog, idx) → string */static int zen_glGetActiveUniformBlockName(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLuint idx  = (GLuint)ival(args[1]);
    char buf[256] = {0};
    GLsizei len = 0;
    glGetActiveUniformBlockName(prog, idx, (GLsizei)sizeof(buf), &len, buf);
    args[0] = val_obj((Obj *)vm->make_string(buf, len));
    return 1;
}
/* glGetActiveUniformsiv(prog, array_of_indices, pname) → array of ints */
static int zen_glGetActiveUniformsiv(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLenum pname = (GLenum)ival(args[2]);
    if (!is_array(args[1])) { vm->runtime_error("glGetActiveUniformsiv: arg 2 must be array"); return 0; }
    ObjArray *ia = as_array(args[1]);
    int n = arr_count(ia);
    GLuint *indices = new GLuint[n];
    GLint  *params  = new GLint[n];
    for (int i = 0; i < n; i++) indices[i] = (GLuint)ival(ia->data[i]);
    glGetActiveUniformsiv(prog, (GLsizei)n, indices, pname, params);
    ObjArray *out = new_array(&vm->get_gc());
    for (int i = 0; i < n; i++) array_push(&vm->get_gc(), out, val_int(params[i]));
    delete[] indices;
    delete[] params;
    args[0] = val_obj((Obj *)out);
    return 1;
}
/* glGetUniformIndices(prog, array_of_names) → array of indices */
static int zen_glGetUniformIndices(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    if (!is_array(args[1])) { vm->runtime_error("glGetUniformIndices: arg 2 must be array"); return 0; }
    ObjArray *na = as_array(args[1]);
    int n = arr_count(na);
    const char **names = new const char*[n];
    GLuint *indices = new GLuint[n];
    for (int i = 0; i < n; i++) {
        names[i] = is_string(na->data[i]) ? as_string(na->data[i])->chars : "";
    }
    glGetUniformIndices(prog, (GLsizei)n, names, indices);
    ObjArray *out = new_array(&vm->get_gc());
    for (int i = 0; i < n; i++) array_push(&vm->get_gc(), out, val_int((int)indices[i]));
    delete[] names;
    delete[] indices;
    args[0] = val_obj((Obj *)out);
    return 1;
}

/* ===================================================================
** GLES 3.0 — Buffer Mapping
** =================================================================== */
/* glMapBufferRange(target, offset, length, access, buffer)
** Copies mapped GPU data into the caller's buffer in-place; returns true/nil on error.
** The buffer must have at least `length` bytes. */
static int zen_glMapBufferRange(VM *vm, Value *args, int) {
    GLenum target      = (GLenum)ival(args[0]);
    GLintptr offset    = (GLintptr)ival(args[1]);
    GLsizeiptr length  = (GLsizeiptr)ival(args[2]);
    GLbitfield access  = (GLbitfield)ival(args[3]);
    if (!is_buffer(args[4])) { vm->runtime_error("glMapBufferRange: arg 5 must be a Buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[4]);
    void *ptr = glMapBufferRange(target, offset, length, access);
    if (!ptr) { args[0] = val_bool(false); return 1; }
    memcpy(b->data, ptr, (size_t)length);
    args[0] = val_bool(true);
    return 1;
}
static int zen_glUnmapBuffer(VM *, Value *args, int) {
    GLboolean ok = glUnmapBuffer((GLenum)ival(args[0]));
    args[0] = val_bool(ok == GL_TRUE);
    return 1;
}
static int zen_glFlushMappedBufferRange(VM *, Value *args, int) {
    glFlushMappedBufferRange((GLenum)ival(args[0]),
                             (GLintptr)ival(args[1]),
                             (GLsizeiptr)ival(args[2]));
    return 0;
}
/* glGetBufferParameteriv(target, pname) → int */
static int zen_glGetBufferParameteriv(VM *, Value *args, int) {
    GLint v = 0;
    glGetBufferParameteriv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
static int zen_glGetBufferParameteri64v(VM *, Value *args, int) {
    GLint64 v = 0;
    glGetBufferParameteri64v((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int((int)v);
    return 1;
}
static int zen_glCopyBufferSubData(VM *, Value *args, int) {
    glCopyBufferSubData((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                        (GLintptr)ival(args[2]), (GLintptr)ival(args[3]),
                        (GLsizeiptr)ival(args[4]));
    return 0;
}

/* ===================================================================
** GLES 3.0 — Transform Feedback
** =================================================================== */
static int zen_glGenTransformFeedbacks(VM *vm, Value *args, int) {
    GLuint tf = 0;
    int n = ival(args[0]);
    if (n == 1) {
        glGenTransformFeedbacks(1, &tf);
        args[0] = val_int((int)tf);
    } else {
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++) {
            glGenTransformFeedbacks(1, &tf);
            array_push(&vm->get_gc(), arr, val_int((int)tf));
        }
        args[0] = val_obj((Obj *)arr);
    }
    return 1;
}
static int zen_glDeleteTransformFeedbacks(VM *, Value *args, int) {
    GLuint tf = (GLuint)ival(args[0]);
    glDeleteTransformFeedbacks(1, &tf);
    return 0;
}
static int zen_glBindTransformFeedback(VM *, Value *args, int) {
    glBindTransformFeedback((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glBeginTransformFeedback(VM *, Value *args, int) {
    glBeginTransformFeedback((GLenum)ival(args[0]));
    return 0;
}
static int zen_glEndTransformFeedback(VM *, Value *, int) {
    glEndTransformFeedback();
    return 0;
}
static int zen_glPauseTransformFeedback(VM *, Value *, int) {
    glPauseTransformFeedback();
    return 0;
}
static int zen_glResumeTransformFeedback(VM *, Value *, int) {
    glResumeTransformFeedback();
    return 0;
}
/* glTransformFeedbackVaryings(prog, array_of_names, bufferMode) */
static int zen_glTransformFeedbackVaryings(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLenum mode = (GLenum)ival(args[2]);
    if (!is_array(args[1])) { vm->runtime_error("glTransformFeedbackVaryings: arg 2 must be array"); return 0; }
    ObjArray *na = as_array(args[1]);
    int n = arr_count(na);
    const char **names = new const char*[n];
    for (int i = 0; i < n; i++) {
        names[i] = is_string(na->data[i]) ? as_string(na->data[i])->chars : "";
    }
    glTransformFeedbackVaryings(prog, (GLsizei)n, names, mode);
    delete[] names;
    return 0;
}
/* glGetTransformFeedbackVarying(prog, index) → [name, size, type] */
/* glGetTransformFeedbackVarying(prog, idx) → (name, size, type)  [multi-return] */
static int zen_glGetTransformFeedbackVarying(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLuint idx  = (GLuint)ival(args[1]);
    char name[256] = {0};
    GLsizei len = 0, size = 0;
    GLenum type = 0;
    glGetTransformFeedbackVarying(prog, idx, (GLsizei)sizeof(name), &len, &size, &type, name);
    args[0] = val_obj((Obj *)vm->make_string(name, len));
    args[1] = val_int((int)size);
    args[2] = val_int((int)type);
    return 3;
}

/* ===================================================================
** Pointer queries
** =================================================================== */
/* glGetVertexAttribPointerv(index, pname) → byte offset (int) */
static int zen_glGetVertexAttribPointerv(VM *, Value *args, int) {
    void *ptr = nullptr;
    glGetVertexAttribPointerv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &ptr);
    args[0] = val_int((int)(uintptr_t)ptr);
    return 1;
}
/* glGetBufferPointerv(target, pname) → mapped address as int */
static int zen_glGetBufferPointerv(VM *, Value *args, int) {
    void *ptr = nullptr;
    glGetBufferPointerv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &ptr);
    args[0] = val_int((int)(uintptr_t)ptr);
    return 1;
}
