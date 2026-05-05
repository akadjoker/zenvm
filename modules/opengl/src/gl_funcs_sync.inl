/* gl_funcs_sync.inl — Sync objects & occlusion/timer queries (GLES 3.0)
** Included by zen_gl.cpp */

/* ===================================================================
** Sync Objects / Fences
** GLsync is a pointer — we represent it as an integer (uintptr_t).
** =================================================================== */
static int zen_glFenceSync(VM *, Value *args, int) {
    GLsync s = glFenceSync((GLenum)ival(args[0]), (GLbitfield)ival(args[1]));
    args[0] = val_int((int)(uintptr_t)s);
    return 1;
}
static int zen_glClientWaitSync(VM *, Value *args, int) {
    GLsync s   = (GLsync)(uintptr_t)ival(args[0]);
    GLbitfield flags = (GLbitfield)ival(args[1]);
    GLuint64   timeout = (GLuint64)ival(args[2]);
    GLenum result = glClientWaitSync(s, flags, timeout);
    args[0] = val_int((int)result);
    return 1;
}
static int zen_glWaitSync(VM *, Value *args, int) {
    glWaitSync((GLsync)(uintptr_t)ival(args[0]),
               (GLbitfield)ival(args[1]),
               (GLuint64)ival(args[2]));
    return 0;
}
static int zen_glDeleteSync(VM *, Value *args, int) {
    glDeleteSync((GLsync)(uintptr_t)ival(args[0]));
    return 0;
}
/* glGetSynciv(sync, pname) → int */
static int zen_glGetSynciv(VM *, Value *args, int) {
    GLint v = 0;
    GLsizei len = 0;
    glGetSynciv((GLsync)(uintptr_t)ival(args[0]), (GLenum)ival(args[1]),
                1, &len, &v);
    args[0] = val_int(v);
    return 1;
}
static int zen_glIsSync(VM *, Value *args, int) {
    args[0] = val_bool(glIsSync((GLsync)(uintptr_t)ival(args[0])) == GL_TRUE);
    return 1;
}

/* ===================================================================
** Occlusion / Timer Queries
** =================================================================== */
static int zen_glGenQueries(VM *vm, Value *args, int) {
    GLuint q = 0;
    int n = ival(args[0]);
    if (n == 1) {
        glGenQueries(1, &q);
        args[0] = val_int((int)q);
    } else {
        ObjArray *arr = new_array(&vm->get_gc());
        for (int i = 0; i < n; i++) {
            glGenQueries(1, &q);
            array_push(&vm->get_gc(), arr, val_int((int)q));
        }
        args[0] = val_obj((Obj *)arr);
    }
    return 1;
}
static int zen_glDeleteQueries(VM *, Value *args, int) {
    GLuint q = (GLuint)ival(args[0]);
    glDeleteQueries(1, &q);
    return 0;
}
static int zen_glBeginQuery(VM *, Value *args, int) {
    glBeginQuery((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glEndQuery(VM *, Value *args, int) {
    glEndQuery((GLenum)ival(args[0]));
    return 0;
}
/* glGetQueryiv(target, pname) → int */
static int zen_glGetQueryiv(VM *, Value *args, int) {
    GLint v = 0;
    glGetQueryiv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}
/* glGetQueryObjectuiv(query, pname) → int */
static int zen_glGetQueryObjectuiv(VM *, Value *args, int) {
    GLuint v = 0;
    glGetQueryObjectuiv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int((int)v);
    return 1;
}
static int zen_glIsQuery(VM *, Value *args, int) {
    args[0] = val_bool(glIsQuery((GLuint)ival(args[0])) == GL_TRUE);
    return 1;
}
