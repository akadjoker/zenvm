/* gl_funcs_desktop.inl — Desktop GL 4.x specific bindings
** NOT available on GLES / Emscripten / Android.
** Compiled only when ZEN_GL_DESKTOP is defined (zen_gl.cpp).
** Included by zen_gl.cpp */

#ifdef ZEN_GL_DESKTOP

/* ===================================================================
** Compute Shaders (GL 4.3)
** =================================================================== */
static int zen_glDispatchCompute(VM *, Value *args, int) {
    glDispatchCompute((GLuint)ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}
static int zen_glDispatchComputeIndirect(VM *, Value *args, int) {
    glDispatchComputeIndirect((GLintptr)ival(args[0]));
    return 0;
}

/* ===================================================================
** Tessellation (GL 4.0)
** =================================================================== */
/* glPatchParameteri(GL_PATCH_VERTICES, count) */
static int zen_glPatchParameteri(VM *, Value *args, int) {
    glPatchParameteri((GLenum)ival(args[0]), ival(args[1]));
    return 0;
}
/* glPatchParameterfv(pname, Float32Buffer) */
static int zen_glPatchParameterfv(VM *vm, Value *args, int) {
    if (!is_buffer(args[1])) { vm->runtime_error("glPatchParameterfv: arg 2 must be Float32Array"); return 0; }
    glPatchParameterfv((GLenum)ival(args[0]), (const GLfloat *)as_buffer(args[1])->data);
    return 0;
}

/* ===================================================================
** Image Load/Store (GL 4.2)
** =================================================================== */
/* glBindImageTexture(unit, tex, level, layered, layer, access, format) */
static int zen_glBindImageTexture(VM *, Value *args, int) {
    glBindImageTexture((GLuint)ival(args[0]), (GLuint)ival(args[1]), ival(args[2]),
                       (GLboolean)ival(args[3]), ival(args[4]),
                       (GLenum)ival(args[5]), (GLenum)ival(args[6]));
    return 0;
}
static int zen_glMemoryBarrier(VM *, Value *args, int) {
    glMemoryBarrier((GLbitfield)ival(args[0]));
    return 0;
}
static int zen_glMemoryBarrierByRegion(VM *, Value *args, int) {
    glMemoryBarrierByRegion((GLbitfield)ival(args[0]));
    return 0;
}

/* ===================================================================
** Indirect Draw
** args: offset = byte offset into GL_DRAW_INDIRECT_BUFFER
** =================================================================== */
static int zen_glDrawArraysIndirect(VM *, Value *args, int) {
    glDrawArraysIndirect((GLenum)ival(args[0]), (const void *)(uintptr_t)ival(args[1]));
    return 0;
}
static int zen_glDrawElementsIndirect(VM *, Value *args, int) {
    glDrawElementsIndirect((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                           (const void *)(uintptr_t)ival(args[2]));
    return 0;
}
static int zen_glMultiDrawArraysIndirect(VM *, Value *args, int) {
    glMultiDrawArraysIndirect((GLenum)ival(args[0]),
                              (const void *)(uintptr_t)ival(args[1]),
                              ival(args[2]), ival(args[3]));
    return 0;
}
static int zen_glMultiDrawElementsIndirect(VM *, Value *args, int) {
    glMultiDrawElementsIndirect((GLenum)ival(args[0]), (GLenum)ival(args[1]),
                                (const void *)(uintptr_t)ival(args[2]),
                                ival(args[3]), ival(args[4]));
    return 0;
}

/* ===================================================================
** Base-Vertex Draw Calls (GL 3.2)
** =================================================================== */
/* glDrawElementsBaseVertex(mode, count, type, byte_offset, basevertex) */
static int zen_glDrawElementsBaseVertex(VM *, Value *args, int) {
    glDrawElementsBaseVertex((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                             (const void *)(uintptr_t)ival(args[3]), ival(args[4]));
    return 0;
}
static int zen_glDrawRangeElementsBaseVertex(VM *, Value *args, int) {
    glDrawRangeElementsBaseVertex((GLenum)ival(args[0]),
                                  (GLuint)ival(args[1]), (GLuint)ival(args[2]),
                                  ival(args[3]), (GLenum)ival(args[4]),
                                  (const void *)(uintptr_t)ival(args[5]), ival(args[6]));
    return 0;
}
static int zen_glDrawElementsInstancedBaseVertex(VM *, Value *args, int) {
    glDrawElementsInstancedBaseVertex((GLenum)ival(args[0]), ival(args[1]),
                                      (GLenum)ival(args[2]),
                                      (const void *)(uintptr_t)ival(args[3]),
                                      ival(args[4]), ival(args[5]));
    return 0;
}

/* ===================================================================
** Vertex Attrib Binding (GL 4.3)
** Separates format description from buffer binding.
** =================================================================== */
/* glVertexAttribFormat(attrib, size, type, normalized, reloffset) */
static int zen_glVertexAttribFormat(VM *, Value *args, int) {
    glVertexAttribFormat((GLuint)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                         (GLboolean)ival(args[3]), (GLuint)ival(args[4]));
    return 0;
}
/* glVertexAttribIFormat(attrib, size, type, reloffset) */
static int zen_glVertexAttribIFormat(VM *, Value *args, int) {
    glVertexAttribIFormat((GLuint)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                          (GLuint)ival(args[3]));
    return 0;
}
/* glVertexAttribBinding(attribindex, bindingindex) */
static int zen_glVertexAttribBinding(VM *, Value *args, int) {
    glVertexAttribBinding((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
/* glBindVertexBuffer(bindingindex, buffer, offset_bytes, stride) */
static int zen_glBindVertexBuffer(VM *, Value *args, int) {
    glBindVertexBuffer((GLuint)ival(args[0]), (GLuint)ival(args[1]),
                       (GLintptr)ival(args[2]), (GLsizei)ival(args[3]));
    return 0;
}
/* glVertexBindingDivisor(bindingindex, divisor) */
static int zen_glVertexBindingDivisor(VM *, Value *args, int) {
    glVertexBindingDivisor((GLuint)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}

/* ===================================================================
** Desktop-only State
** =================================================================== */
static int zen_glPolygonMode(VM *, Value *args, int) {
    glPolygonMode((GLenum)ival(args[0]), (GLenum)ival(args[1]));
    return 0;
}
static int zen_glPointSize(VM *, Value *args, int) {
    glPointSize(fval(args[0]));
    return 0;
}
static int zen_glBlendColor(VM *, Value *args, int) {
    glBlendColor(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3]));
    return 0;
}
/* glColorMaski(draw_buffer, r, g, b, a) */
static int zen_glColorMaski(VM *, Value *args, int) {
    glColorMaski((GLuint)ival(args[0]),
                 (GLboolean)ival(args[1]), (GLboolean)ival(args[2]),
                 (GLboolean)ival(args[3]), (GLboolean)ival(args[4]));
    return 0;
}
static int zen_glBlendFunci(VM *, Value *args, int) {
    glBlendFunci((GLuint)ival(args[0]), (GLenum)ival(args[1]), (GLenum)ival(args[2]));
    return 0;
}
static int zen_glBlendEquationi(VM *, Value *args, int) {
    glBlendEquationi((GLuint)ival(args[0]), (GLenum)ival(args[1]));
    return 0;
}
static int zen_glBlendFuncSeparatei(VM *, Value *args, int) {
    glBlendFuncSeparatei((GLuint)ival(args[0]),
                         (GLenum)ival(args[1]), (GLenum)ival(args[2]),
                         (GLenum)ival(args[3]), (GLenum)ival(args[4]));
    return 0;
}
static int zen_glSampleMaski(VM *, Value *args, int) {
    glSampleMaski((GLuint)ival(args[0]), (GLbitfield)ival(args[1]));
    return 0;
}
static int zen_glMinSampleShading(VM *, Value *args, int) {
    glMinSampleShading(fval(args[0]));
    return 0;
}
/* glClearDepth (double variant; glClearDepthf is ES-compatible and already in core) */
static int zen_glClearDepth(VM *, Value *args, int) {
    glClearDepth((GLdouble)fval(args[0]));
    return 0;
}
static int zen_glLogicOp(VM *, Value *args, int) {
    glLogicOp((GLenum)ival(args[0]));
    return 0;
}
/* glEnablei / glDisablei (indexed caps e.g. GL_BLEND for each draw buffer) */
static int zen_glEnablei(VM *, Value *args, int) {
    glEnablei((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glDisablei(VM *, Value *args, int) {
    glDisablei((GLenum)ival(args[0]), (GLuint)ival(args[1]));
    return 0;
}
static int zen_glIsEnabledi(VM *, Value *args, int) {
    args[0] = val_bool(glIsEnabledi((GLenum)ival(args[0]), (GLuint)ival(args[1])) == GL_TRUE);
    return 1;
}

/* ===================================================================
** Clear Buffer (GL 3.0) — clear a specific draw buffer by value
** =================================================================== */
static int zen_glClearBufferfv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glClearBufferfv: arg 3 must be Float32Array"); return 0; }
    glClearBufferfv((GLenum)ival(args[0]), ival(args[1]),
                    (const GLfloat *)as_buffer(args[2])->data);
    return 0;
}
static int zen_glClearBufferiv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glClearBufferiv: arg 3 must be Int32Array"); return 0; }
    glClearBufferiv((GLenum)ival(args[0]), ival(args[1]),
                    (const GLint *)as_buffer(args[2])->data);
    return 0;
}
static int zen_glClearBufferuiv(VM *vm, Value *args, int) {
    if (!is_buffer(args[2])) { vm->runtime_error("glClearBufferuiv: arg 3 must be UInt32Array"); return 0; }
    glClearBufferuiv((GLenum)ival(args[0]), ival(args[1]),
                     (const GLuint *)as_buffer(args[2])->data);
    return 0;
}
/* glClearBufferfi(GL_DEPTH_STENCIL, drawbuffer, depth, stencil) */
static int zen_glClearBufferfi(VM *, Value *args, int) {
    glClearBufferfi((GLenum)ival(args[0]), ival(args[1]), fval(args[2]), ival(args[3]));
    return 0;
}

/* ===================================================================
** Copy Image (GL 4.3)
** glCopyImageSubData(src, srcTarget, srcLevel, srcX, srcY, srcZ,
**                   dst, dstTarget, dstLevel, dstX, dstY, dstZ, w, h, d)
** =================================================================== */
static int zen_glCopyImageSubData(VM *, Value *args, int) {
    glCopyImageSubData((GLuint)ival(args[0]),  (GLenum)ival(args[1]),  ival(args[2]),
                       ival(args[3]),  ival(args[4]),  ival(args[5]),
                       (GLuint)ival(args[6]),  (GLenum)ival(args[7]),  ival(args[8]),
                       ival(args[9]),  ival(args[10]), ival(args[11]),
                       ival(args[12]), ival(args[13]), ival(args[14]));
    return 0;
}

/* ===================================================================
** Multi-bind (GL 4.4) — bind arrays of textures/samplers/image textures at once
** glBindBuffers is absent from this glad build; use glBindBufferBase/Range instead.
** =================================================================== */
/* glBindTextures(first, count, UInt32Buffer_or_nil) */
static int zen_glBindTextures(VM *vm, Value *args, int) {
    GLuint  first = (GLuint)ival(args[0]);
    GLsizei count = (GLsizei)ival(args[1]);
    const GLuint *ids = nullptr;
    if (!is_nil(args[2])) {
        if (!is_buffer(args[2])) { vm->runtime_error("glBindTextures: arg 3 must be UInt32Array or nil"); return 0; }
        ids = (const GLuint *)as_buffer(args[2])->data;
    }
    glBindTextures(first, count, ids);
    return 0;
}
/* glBindSamplers(first, count, UInt32Buffer_or_nil) */
static int zen_glBindSamplers(VM *vm, Value *args, int) {
    GLuint  first = (GLuint)ival(args[0]);
    GLsizei count = (GLsizei)ival(args[1]);
    const GLuint *ids = nullptr;
    if (!is_nil(args[2])) {
        if (!is_buffer(args[2])) { vm->runtime_error("glBindSamplers: arg 3 must be UInt32Array or nil"); return 0; }
        ids = (const GLuint *)as_buffer(args[2])->data;
    }
    glBindSamplers(first, count, ids);
    return 0;
}
/* glBindImageTextures(first, count, UInt32Buffer_or_nil) */
static int zen_glBindImageTextures(VM *vm, Value *args, int) {
    GLuint  first = (GLuint)ival(args[0]);
    GLsizei count = (GLsizei)ival(args[1]);
    const GLuint *ids = nullptr;
    if (!is_nil(args[2])) {
        if (!is_buffer(args[2])) { vm->runtime_error("glBindImageTextures: arg 3 must be UInt32Array or nil"); return 0; }
        ids = (const GLuint *)as_buffer(args[2])->data;
    }
    glBindImageTextures(first, count, ids);
    return 0;
}

/* ===================================================================
** Buffer Storage (GL 4.4) — immutable buffer with access flags
** glBufferStorage(target, buffer_or_size, flags)
**   buffer_or_size: ObjBuffer → uploads data; integer → just allocates N bytes
** =================================================================== */
static int zen_glBufferStorage(VM *vm, Value *args, int) {
    GLenum target = (GLenum)ival(args[0]);
    GLsizeiptr size;
    const void *data = nullptr;
    if (is_buffer(args[1])) {
        ObjBuffer *b = as_buffer(args[1]);
        size = (GLsizeiptr)(b->count * buffer_elem_size[b->btype]);
        data = b->data;
    } else if (is_int(args[1])) {
        size = (GLsizeiptr)args[1].as.integer;
    } else {
        vm->runtime_error("glBufferStorage: arg 2 must be ObjBuffer or int size");
        return 0;
    }
    glBufferStorage(target, size, data, (GLbitfield)ival(args[2]));
    return 0;
}

/* ===================================================================
** Timer Queries (GL 3.3 — ARB_timer_query)
** Returns as float (double precision) to preserve nanosecond values.
** =================================================================== */
static int zen_glQueryCounter(VM *, Value *args, int) {
    glQueryCounter((GLuint)ival(args[0]), (GLenum)ival(args[1]));
    return 0;
}
/* glGetQueryObjecti64v(query, pname) → number */
static int zen_glGetQueryObjecti64v(VM *, Value *args, int) {
    GLint64 v = 0;
    glGetQueryObjecti64v((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_float((double)v);
    return 1;
}
/* glGetQueryObjectui64v(query, pname) → number */
static int zen_glGetQueryObjectui64v(VM *, Value *args, int) {
    GLuint64 v = 0;
    glGetQueryObjectui64v((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_float((double)v);
    return 1;
}
/* glGetQueryObjectiv (non-64 signed version, GL 1.5) */
static int zen_glGetQueryObjectiv(VM *, Value *args, int) {
    GLint v = 0;
    glGetQueryObjectiv((GLuint)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}

/* ===================================================================
** SSBO (GL 4.3)
** =================================================================== */
static int zen_glShaderStorageBlockBinding(VM *, Value *args, int) {
    glShaderStorageBlockBinding((GLuint)ival(args[0]), (GLuint)ival(args[1]), (GLuint)ival(args[2]));
    return 0;
}

/* ===================================================================
** Framebuffer Parameters (GL 4.3 — framebuffer without attachments)
** =================================================================== */
static int zen_glFramebufferParameteri(VM *, Value *args, int) {
    glFramebufferParameteri((GLenum)ival(args[0]), (GLenum)ival(args[1]), ival(args[2]));
    return 0;
}
static int zen_glGetFramebufferParameteriv(VM *, Value *args, int) {
    GLint v = 0;
    glGetFramebufferParameteriv((GLenum)ival(args[0]), (GLenum)ival(args[1]), &v);
    args[0] = val_int(v);
    return 1;
}

/* ===================================================================
** Multisample Textures (GL 3.2)
** =================================================================== */
static int zen_glTexImage2DMultisample(VM *, Value *args, int) {
    glTexImage2DMultisample((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                            ival(args[3]), ival(args[4]), (GLboolean)ival(args[5]));
    return 0;
}
static int zen_glTexImage3DMultisample(VM *, Value *args, int) {
    glTexImage3DMultisample((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                            ival(args[3]), ival(args[4]), ival(args[5]), (GLboolean)ival(args[6]));
    return 0;
}
/* glGetMultisamplefv(pname, index) → (x, y)  [multi-return] */
static int zen_glGetMultisamplefv(VM *, Value *args, int) {
    GLfloat v[2] = {0.f, 0.f};
    glGetMultisamplefv((GLenum)ival(args[0]), (GLuint)ival(args[1]), v);
    args[0] = val_float(v[0]);
    args[1] = val_float(v[1]);
    return 2;
}

/* ===================================================================
** Texture Storage for Multisample (GL 4.3 / ES 3.1)
** =================================================================== */
static int zen_glTexStorage2DMultisample(VM *, Value *args, int) {
    glTexStorage2DMultisample((GLenum)ival(args[0]), ival(args[1]), (GLenum)ival(args[2]),
                              ival(args[3]), ival(args[4]), (GLboolean)ival(args[5]));
    return 0;
}

/* ===================================================================
** Debug Output (GL 4.3)
** =================================================================== */
/* glObjectLabel(identifier, name, label_string) */
static int zen_glObjectLabel(VM *vm, Value *args, int) {
    if (!is_string(args[2])) { vm->runtime_error("glObjectLabel: arg 3 must be string"); return 0; }
    ObjString *s = as_string(args[2]);
    glObjectLabel((GLenum)ival(args[0]), (GLuint)ival(args[1]), (GLsizei)s->length, s->chars);
    return 0;
}
/* glPushDebugGroup(source, id, message) */
static int zen_glPushDebugGroup(VM *vm, Value *args, int) {
    if (!is_string(args[2])) { vm->runtime_error("glPushDebugGroup: arg 3 must be string"); return 0; }
    ObjString *s = as_string(args[2]);
    glPushDebugGroup((GLenum)ival(args[0]), (GLuint)ival(args[1]), (GLsizei)s->length, s->chars);
    return 0;
}
static int zen_glPopDebugGroup(VM *, Value *, int) {
    glPopDebugGroup();
    return 0;
}
/* glDebugMessageControl(src, type, severity, ids_UInt32Buffer_or_nil, enabled) */
static int zen_glDebugMessageControl(VM *, Value *args, int) {
    const GLuint *ids = nullptr;
    GLsizei count = 0;
    if (is_buffer(args[3])) {
        ObjBuffer *b = as_buffer(args[3]);
        count = (GLsizei)b->count;
        ids   = (const GLuint *)b->data;
    }
    glDebugMessageControl((GLenum)ival(args[0]), (GLenum)ival(args[1]), (GLenum)ival(args[2]),
                          count, ids, (GLboolean)ival(args[4]));
    return 0;
}
/* glDebugMessageInsert(source, type, id, severity, message) */
static int zen_glDebugMessageInsert(VM *vm, Value *args, int) {
    if (!is_string(args[4])) { vm->runtime_error("glDebugMessageInsert: arg 5 must be string"); return 0; }
    ObjString *s = as_string(args[4]);
    glDebugMessageInsert((GLenum)ival(args[0]), (GLenum)ival(args[1]), (GLuint)ival(args[2]),
                         (GLenum)ival(args[3]), (GLsizei)s->length, s->chars);
    return 0;
}

#endif /* ZEN_GL_DESKTOP */
