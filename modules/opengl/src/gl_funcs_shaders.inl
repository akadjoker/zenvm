/* gl_funcs_shaders.inl — Shader & program functions (GLES 3.0 compatible)
** Included by zen_gl.cpp */

/* --- Shader objects --- */
static int zen_glCreateShader(VM *vm, Value *args, int) {
    GLuint s = glCreateShader((GLenum)ival(args[0]));
    args[0] = val_int((int)s);
    return 1;
}
static int zen_glDeleteShader(VM *, Value *args, int) {
    glDeleteShader((GLuint)ival(args[0]));
    return 0;
}
static int zen_glShaderSource(VM *vm, Value *args, int) {
    GLuint shader = (GLuint)ival(args[0]);
    if (!is_string(args[1])) { vm->runtime_error("glShaderSource: arg 2 must be string"); return 0; }
    ObjString *src = as_string(args[1]);
    const char *c = src->chars;
    glShaderSource(shader, 1, &c, nullptr);
    return 0;
}
static int zen_glCompileShader(VM *, Value *args, int) {
    glCompileShader((GLuint)ival(args[0]));
    return 0;
}
static int zen_glGetShaderiv(VM *vm, Value *args, int) {
    GLint v = 0;
    glGetShaderiv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
static int zen_glGetShaderInfoLog(VM *vm, Value *args, int) {
    GLuint shader = (GLuint)ival(args[0]);
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) { args[0] = val_obj((Obj *)vm->make_string("", 0)); return 1; }
    char buf[4096];
    if (len > (GLint)sizeof(buf)) len = (GLint)sizeof(buf);
    GLsizei written = 0;
    glGetShaderInfoLog(shader, len, &written, buf);
    args[0] = val_obj((Obj *)vm->make_string(buf, written));
    return 1;
}

/* --- Program objects --- */
static int zen_glCreateProgram(VM *, Value *args, int) {
    GLuint p = glCreateProgram();
    args[0] = val_int((int)p);
    return 1;
}
static int zen_glDeleteProgram(VM *, Value *args, int) {
    glDeleteProgram((GLuint)ival(args[0]));
    return 0;
}
static int zen_glAttachShader(VM *, Value *args, int) {
    glAttachShader((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glDetachShader(VM *, Value *args, int) {
    glDetachShader((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glLinkProgram(VM *, Value *args, int) {
    glLinkProgram((GLuint)ival(args[0]));
    return 0;
}
static int zen_glValidateProgram(VM *, Value *args, int) {
    glValidateProgram((GLuint)ival(args[0]));
    return 0;
}
static int zen_glGetProgramiv(VM *vm, Value *args, int) {
    GLint v = 0;
    glGetProgramiv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
static int zen_glGetProgramInfoLog(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) { args[0] = val_obj((Obj *)vm->make_string("", 0)); return 1; }
    char buf[4096];
    if (len > (GLint)sizeof(buf)) len = (GLint)sizeof(buf);
    GLsizei written = 0;
    glGetProgramInfoLog(prog, len, &written, buf);
    args[0] = val_obj((Obj *)vm->make_string(buf, written));
    return 1;
}
static int zen_glUseProgram(VM *, Value *args, int) {
    glUseProgram((GLuint)ival(args[0]));
    return 0;
}

/* --- Attrib locations --- */
static int zen_glBindAttribLocation(VM *vm, Value *args, int) {
    if (!is_string(args[2])) { vm->runtime_error("glBindAttribLocation: arg 3 must be string"); return 0; }
    glBindAttribLocation((GLuint)ival(args[0]), (GLuint)ival(args[1]), as_string(args[2])->chars);
    return 0;
}
static int zen_glGetAttribLocation(VM *vm, Value *args, int) {
    if (!is_string(args[1])) { vm->runtime_error("glGetAttribLocation: arg 2 must be string"); return 0; }
    GLint loc = glGetAttribLocation((GLuint)ival(args[0]), as_string(args[1])->chars);
    args[0] = val_int(loc);
    return 1;
}

/* --- Uniform locations --- */
static int zen_glGetUniformLocation(VM *vm, Value *args, int) {
    if (!is_string(args[1])) { vm->runtime_error("glGetUniformLocation: arg 2 must be string"); return 0; }
    GLint loc = glGetUniformLocation((GLuint)ival(args[0]), as_string(args[1])->chars);
    args[0] = val_int(loc);
    return 1;
}

/* --- Uniform setters --- */
static int zen_glUniform1i(VM *, Value *args, int) {
    glUniform1i(ival(args[0]), ival(args[1]));
    return 0;
}
static int zen_glUniform2i(VM *, Value *args, int) {
    glUniform2i(ival(args[0]), ival(args[1]), ival(args[2]));
    return 0;
}
static int zen_glUniform3i(VM *, Value *args, int) {
    glUniform3i(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]));
    return 0;
}
static int zen_glUniform4i(VM *, Value *args, int) {
    glUniform4i(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3]), ival(args[4]));
    return 0;
}
static int zen_glUniform1f(VM *, Value *args, int) {
    glUniform1f(ival(args[0]), (GLfloat)fval(args[1]));
    return 0;
}
static int zen_glUniform2f(VM *, Value *args, int) {
    glUniform2f(ival(args[0]), (GLfloat)fval(args[1]), (GLfloat)fval(args[2]));
    return 0;
}
static int zen_glUniform3f(VM *, Value *args, int) {
    glUniform3f(ival(args[0]), (GLfloat)fval(args[1]), (GLfloat)fval(args[2]), (GLfloat)fval(args[3]));
    return 0;
}
static int zen_glUniform4f(VM *, Value *args, int) {
    glUniform4f(ival(args[0]), (GLfloat)fval(args[1]), (GLfloat)fval(args[2]),
                (GLfloat)fval(args[3]), (GLfloat)fval(args[4]));
    return 0;
}

/* --- Uniform array/matrix (use ObjBuffer float32) --- */
static int zen_glUniform1fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniform1fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniform1fv(ival(args[0]), ival(args[1]), (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniform2fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniform2fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniform2fv(ival(args[0]), ival(args[1]), (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniform3fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniform3fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniform3fv(ival(args[0]), ival(args[1]), (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniform4fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniform4fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniform4fv(ival(args[0]), ival(args[1]), (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniformMatrix2fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniformMatrix2fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniformMatrix2fv(ival(args[0]), ival(args[1]), GL_FALSE, (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniformMatrix3fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniformMatrix3fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniformMatrix3fv(ival(args[0]), ival(args[1]), GL_FALSE, (const GLfloat *)b->data);
    return 0;
}
static int zen_glUniformMatrix4fv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glUniformMatrix4fv: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    glUniformMatrix4fv(ival(args[0]), ival(args[1]), GL_FALSE, (const GLfloat *)b->data);
    return 0;
}

/* --- Uniform uint (GLES 3.0) --- */
static int zen_glUniform1ui(VM *, Value *args, int) {
    glUniform1ui(ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glUniform2ui(VM *, Value *args, int) {
    glUniform2ui(ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}
static int zen_glUniform3ui(VM *, Value *args, int) {
    glUniform3ui(ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]), (GLuint)ival(args[3]));
    return 0;
}
static int zen_glUniform4ui(VM *, Value *args, int) {
    glUniform4ui(ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]),
                 (GLuint)ival(args[3]), (GLuint)ival(args[4]));
    return 0;
}

/* --- Shader introspection --- */
/* glGetActiveUniform(prog, idx) → (name, size, type)  [multi-return] */
static int zen_glGetActiveUniform(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLuint idx  = (GLuint)ival(args[1]);
    char name[256];
    GLsizei len = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(prog, idx, (GLsizei)sizeof(name), &len, &size, &type, name);
    args[0] = val_obj((Obj *)vm->make_string(name, len));
    args[1] = val_int(size);
    args[2] = val_int((int)type);
    return 3;
}
/* glGetActiveAttrib(prog, idx) → (name, size, type)  [multi-return] */
static int zen_glGetActiveAttrib(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLuint idx  = (GLuint)ival(args[1]);
    char name[256];
    GLsizei len = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveAttrib(prog, idx, (GLsizei)sizeof(name), &len, &size, &type, name);
    args[0] = val_obj((Obj *)vm->make_string(name, len));
    args[1] = val_int(size);
    args[2] = val_int((int)type);
    return 3;
}

/* ===================================================================
** GLES 3.0 — Program Binary
** =================================================================== */
/* glGetProgramBinary(prog) → [ObjBuffer(uint8), binaryFormat] */
static int zen_glGetProgramBinary(VM *vm, Value *args, int) {
    GLuint prog = (GLuint)ival(args[0]);
    GLint len = 0;
    glGetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &len);
    if (len <= 0) { args[0] = val_nil(); return 1; }
    ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_UINT8, (int32_t)len);
    GLenum fmt = 0;
    GLsizei written = 0;
    glGetProgramBinary(prog, (GLsizei)len, &written, &fmt, buf->data);
    ObjArray *out = new_array(&vm->get_gc());
    array_push(&vm->get_gc(), out, val_obj((Obj *)buf));
    array_push(&vm->get_gc(), out, val_int((int)fmt));
    args[0] = val_obj((Obj *)out);
    return 1;
}
/* glProgramBinary(prog, binaryFormat, buffer) */
static int zen_glProgramBinary(VM *vm, Value *args, int) {
    GLuint prog  = (GLuint)ival(args[0]);
    GLenum fmt   = (GLenum)ival(args[1]);
    if (!is_buffer(args[2])) { vm->runtime_error("glProgramBinary: arg 3 must be buffer"); return 0; }
    ObjBuffer *b = as_buffer(args[2]);
    GLsizei sz   = (GLsizei)(b->count * buffer_elem_size[b->btype]);
    glProgramBinary(prog, fmt, b->data, sz);
    return 0;
}
static int zen_glProgramParameteri(VM *, Value *args, int) {
    glProgramParameteri((GLuint)ival(args[0]), (GLenum)ival(args[1]), ival(args[2]));
    return 0;
}

/* ===================================================================
** GLES 3.0 — Integer uniform arrays (glUniform*iv / glUniform*uiv)
** args: (location, count, Int32Buffer/UInt32Buffer)
** =================================================================== */
#define ZEN_UNIFORM_IV(name, gl_fn) \
static int zen_##name(VM *vm, Value *args, int) { \
    GLint loc = ival(args[0]); \
    GLsizei cnt = (GLsizei)ival(args[1]); \
    if (!is_buffer(args[2])) { vm->runtime_error(#name ": arg 3 must be buffer"); return 0; } \
    ObjBuffer *b = as_buffer(args[2]); \
    gl_fn(loc, cnt, (const GLint *)b->data); \
    return 0; \
}
#define ZEN_UNIFORM_UIV(name, gl_fn) \
static int zen_##name(VM *vm, Value *args, int) { \
    GLint loc = ival(args[0]); \
    GLsizei cnt = (GLsizei)ival(args[1]); \
    if (!is_buffer(args[2])) { vm->runtime_error(#name ": arg 3 must be buffer"); return 0; } \
    ObjBuffer *b = as_buffer(args[2]); \
    gl_fn(loc, cnt, (const GLuint *)b->data); \
    return 0; \
}

ZEN_UNIFORM_IV(glUniform1iv, glUniform1iv)
ZEN_UNIFORM_IV(glUniform2iv, glUniform2iv)
ZEN_UNIFORM_IV(glUniform3iv, glUniform3iv)
ZEN_UNIFORM_IV(glUniform4iv, glUniform4iv)
ZEN_UNIFORM_UIV(glUniform1uiv, glUniform1uiv)
ZEN_UNIFORM_UIV(glUniform2uiv, glUniform2uiv)
ZEN_UNIFORM_UIV(glUniform3uiv, glUniform3uiv)
ZEN_UNIFORM_UIV(glUniform4uiv, glUniform4uiv)

#undef ZEN_UNIFORM_IV
#undef ZEN_UNIFORM_UIV

/* ===================================================================
** GLES 3.0 — Non-square matrix uniforms
** args: (location, count, transpose, Float32Buffer)
** =================================================================== */
#define ZEN_UNIFORM_MATRIX_NM(name, gl_fn) \
static int zen_##name(VM *vm, Value *args, int) { \
    GLint loc = ival(args[0]); \
    GLsizei cnt = (GLsizei)ival(args[1]); \
    GLboolean tr = (GLboolean)ival(args[2]); \
    if (!is_buffer(args[3])) { vm->runtime_error(#name ": arg 4 must be Float32Array"); return 0; } \
    ObjBuffer *b = as_buffer(args[3]); \
    gl_fn(loc, cnt, tr, (const GLfloat *)b->data); \
    return 0; \
}

ZEN_UNIFORM_MATRIX_NM(glUniformMatrix2x3fv, glUniformMatrix2x3fv)
ZEN_UNIFORM_MATRIX_NM(glUniformMatrix3x2fv, glUniformMatrix3x2fv)
ZEN_UNIFORM_MATRIX_NM(glUniformMatrix2x4fv, glUniformMatrix2x4fv)
ZEN_UNIFORM_MATRIX_NM(glUniformMatrix4x2fv, glUniformMatrix4x2fv)
ZEN_UNIFORM_MATRIX_NM(glUniformMatrix3x4fv, glUniformMatrix3x4fv)
ZEN_UNIFORM_MATRIX_NM(glUniformMatrix4x3fv, glUniformMatrix4x3fv)

#undef ZEN_UNIFORM_MATRIX_NM
