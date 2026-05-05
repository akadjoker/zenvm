/* gl_funcs_draw.inl — Draw commands & instancing (GLES 3.0 compatible)
** Included by zen_gl.cpp */

static int zen_glDrawArrays(VM *, Value *args, int) {
    glDrawArrays((GLenum)ival(args[0]), ival(args[1]), (GLsizei)ival(args[2]));
    return 0;
}

/* glDrawElements(mode, count, type, offset) — offset is byte offset into bound EBO */
static int zen_glDrawElements(VM *, Value *args, int) {
    GLenum mode    = (GLenum)ival(args[0]);
    GLsizei count  = (GLsizei)ival(args[1]);
    GLenum type    = (GLenum)ival(args[2]);
    GLintptr off   = (GLintptr)ival(args[3]);
    glDrawElements(mode, count, type, (const void *)off);
    return 0;
}

/* Instancing (GLES 3.0) */
static int zen_glDrawArraysInstanced(VM *, Value *args, int) {
    glDrawArraysInstanced((GLenum)ival(args[0]), ival(args[1]),
                          (GLsizei)ival(args[2]), (GLsizei)ival(args[3]));
    return 0;
}
static int zen_glDrawElementsInstanced(VM *, Value *args, int) {
    GLenum mode      = (GLenum)ival(args[0]);
    GLsizei count    = (GLsizei)ival(args[1]);
    GLenum type      = (GLenum)ival(args[2]);
    GLintptr off     = (GLintptr)ival(args[3]);
    GLsizei primcnt  = (GLsizei)ival(args[4]);
    glDrawElementsInstanced(mode, count, type, (const void *)off, primcnt);
    return 0;
}

/* Range elements (GLES 3.0) */
static int zen_glDrawRangeElements(VM *, Value *args, int) {
    GLenum mode    = (GLenum)ival(args[0]);
    GLuint start   = (GLuint)ival(args[1]);
    GLuint end     = (GLuint)ival(args[2]);
    GLsizei count  = (GLsizei)ival(args[3]);
    GLenum type    = (GLenum)ival(args[4]);
    GLintptr off   = (GLintptr)ival(args[5]);
    glDrawRangeElements(mode, start, end, count, type, (const void *)off);
    return 0;
}
