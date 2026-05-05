/* zen_gl_legacy.cpp — OpenGL 1.x / 2.x Legacy (Immediate Mode) bindings
**
** import gll
**
** Provides the classic fixed-function pipeline:
**   glBegin/glEnd, glVertex*, glColor*, glNormal*, glTexCoord*,
**   matrix stack (glMatrixMode, glPushMatrix, glOrtho, ...),
**   display lists, lighting, fog, alpha test, line/polygon stipple, etc.
**
** Does NOT include glad — uses system <GL/gl.h> + <GL/glext.h> directly.
** Only available on desktop (not GLES / Emscripten / Android).
*/

#include "zen/module_gl.h"

#ifdef ZEN_ENABLE_GL4

/* System GL headers — no glad here */
#ifdef _WIN32
#  include <windows.h>
#  include <GL/gl.h>
#  include <GL/glext.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl3ext.h>
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include "object.h"
#include "memory.h"
#include "vm.h"

namespace zen
{
    #define ZEN_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

    static inline float fval(Value v) { return is_int(v) ? (float)v.as.integer : (float)v.as.number; }
    static inline int   ival(Value v) { return is_int(v) ? (int)v.as.integer   : (int)v.as.number; }

    /* ===================================================================
    ** Immediate Mode — Begin / End
    ** =================================================================== */
    static int zen_glBegin(VM *, Value *args, int) { glBegin((GLenum)ival(args[0])); return 0; }
    static int zen_glEnd  (VM *, Value *,    int) { glEnd();   return 0; }

    /* ===================================================================
    ** Vertex
    ** =================================================================== */
    static int zen_glVertex2f(VM *, Value *args, int) { glVertex2f(fval(args[0]), fval(args[1])); return 0; }
    static int zen_glVertex2i(VM *, Value *args, int) { glVertex2i(ival(args[0]), ival(args[1])); return 0; }
    static int zen_glVertex3f(VM *, Value *args, int) { glVertex3f(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glVertex3i(VM *, Value *args, int) { glVertex3i(ival(args[0]), ival(args[1]), ival(args[2])); return 0; }
    static int zen_glVertex4f(VM *, Value *args, int) { glVertex4f(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }
    /* glVertex2fv / glVertex3fv — accept Float32Buffer */
    static int zen_glVertex2fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glVertex2fv: need Float32Array"); return 0; }
        glVertex2fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }
    static int zen_glVertex3fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glVertex3fv: need Float32Array"); return 0; }
        glVertex3fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }

    /* ===================================================================
    ** Color
    ** =================================================================== */
    static int zen_glColor3f (VM *, Value *args, int) { glColor3f (fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glColor4f (VM *, Value *args, int) { glColor4f (fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }
    static int zen_glColor3ub(VM *, Value *args, int) { glColor3ub((GLubyte)ival(args[0]), (GLubyte)ival(args[1]), (GLubyte)ival(args[2])); return 0; }
    static int zen_glColor4ub(VM *, Value *args, int) { glColor4ub((GLubyte)ival(args[0]), (GLubyte)ival(args[1]), (GLubyte)ival(args[2]), (GLubyte)ival(args[3])); return 0; }
    static int zen_glColor3fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glColor3fv: need Float32Array"); return 0; }
        glColor3fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }
    static int zen_glColor4fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glColor4fv: need Float32Array"); return 0; }
        glColor4fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }

    /* ===================================================================
    ** Normal
    ** =================================================================== */
    static int zen_glNormal3f(VM *, Value *args, int) { glNormal3f(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glNormal3fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glNormal3fv: need Float32Array"); return 0; }
        glNormal3fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }

    /* ===================================================================
    ** TexCoord
    ** =================================================================== */
    static int zen_glTexCoord2f(VM *, Value *args, int) { glTexCoord2f(fval(args[0]), fval(args[1])); return 0; }
    static int zen_glTexCoord3f(VM *, Value *args, int) { glTexCoord3f(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glTexCoord2fv(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glTexCoord2fv: need Float32Array"); return 0; }
        glTexCoord2fv((const GLfloat *)as_buffer(args[0])->data); return 0;
    }

    /* ===================================================================
    ** Matrix Stack
    ** =================================================================== */
    static int zen_glMatrixMode  (VM *, Value *args, int) { glMatrixMode((GLenum)ival(args[0])); return 0; }
    static int zen_glLoadIdentity(VM *, Value *,    int) { glLoadIdentity(); return 0; }
    static int zen_glPushMatrix  (VM *, Value *,    int) { glPushMatrix();   return 0; }
    static int zen_glPopMatrix   (VM *, Value *,    int) { glPopMatrix();    return 0; }

    static int zen_glLoadMatrixf(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glLoadMatrixf: need Float32Array(16)"); return 0; }
        glLoadMatrixf((const GLfloat *)as_buffer(args[0])->data); return 0;
    }
    static int zen_glMultMatrixf(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glMultMatrixf: need Float32Array(16)"); return 0; }
        glMultMatrixf((const GLfloat *)as_buffer(args[0])->data); return 0;
    }
    static int zen_glLoadMatrixd(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glLoadMatrixd: need Float64Array(16)"); return 0; }
        glLoadMatrixd((const GLdouble *)as_buffer(args[0])->data); return 0;
    }
    static int zen_glMultMatrixd(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glMultMatrixd: need Float64Array(16)"); return 0; }
        glMultMatrixd((const GLdouble *)as_buffer(args[0])->data); return 0;
    }

    static int zen_glTranslatef(VM *, Value *args, int) { glTranslatef(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glRotatef   (VM *, Value *args, int) { glRotatef(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }
    static int zen_glScalef    (VM *, Value *args, int) { glScalef(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glTranslated(VM *, Value *args, int) { glTranslated(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    static int zen_glRotated   (VM *, Value *args, int) { glRotated(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }
    static int zen_glScaled    (VM *, Value *args, int) { glScaled(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }

    static int zen_glOrtho   (VM *, Value *args, int) { glOrtho(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4]), fval(args[5])); return 0; }
    static int zen_glFrustum (VM *, Value *args, int) { glFrustum(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3]), fval(args[4]), fval(args[5])); return 0; }

    /* glGetFloatv for matrix retrieval — gllGetMatrixf(pname, Float32Buffer[16])
    ** Fills the caller's buffer in-place; returns nothing.
    ** The buffer must have at least 16 elements (64 bytes). */
    static int zen_gllGetMatrixf(VM *vm, Value *args, int) {
        if (!is_buffer(args[1])) { vm->runtime_error("gllGetMatrixf: arg 2 must be Float32Buffer(16)"); return 0; }
        ObjBuffer *buf = as_buffer(args[1]);
        if (buf->count < 16) { vm->runtime_error("gllGetMatrixf: buffer too small (need 16 floats)"); return 0; }
        glGetFloatv((GLenum)ival(args[0]), (GLfloat *)buf->data);
        return 0;
    }

    /* ===================================================================
    ** Shade Model
    ** =================================================================== */
    static int zen_glShadeModel(VM *, Value *args, int) { glShadeModel((GLenum)ival(args[0])); return 0; }

    /* ===================================================================
    ** Lighting
    ** =================================================================== */
    static int zen_glLightf (VM *, Value *args, int) { glLightf ((GLenum)ival(args[0]), (GLenum)ival(args[1]), fval(args[2])); return 0; }
    static int zen_glLighti (VM *, Value *args, int) { glLighti ((GLenum)ival(args[0]), (GLenum)ival(args[1]), ival(args[2])); return 0; }
    static int zen_glLightfv(VM *vm, Value *args, int) {
        if (!is_buffer(args[2])) { vm->runtime_error("glLightfv: arg 3 must be Float32Array"); return 0; }
        glLightfv((GLenum)ival(args[0]), (GLenum)ival(args[1]), (const GLfloat *)as_buffer(args[2])->data); return 0;
    }
    static int zen_glLightModelf (VM *, Value *args, int) { glLightModelf ((GLenum)ival(args[0]), fval(args[1])); return 0; }
    static int zen_glLightModeli (VM *, Value *args, int) { glLightModeli ((GLenum)ival(args[0]), ival(args[1])); return 0; }
    static int zen_glLightModelfv(VM *vm, Value *args, int) {
        if (!is_buffer(args[1])) { vm->runtime_error("glLightModelfv: arg 2 must be Float32Array"); return 0; }
        glLightModelfv((GLenum)ival(args[0]), (const GLfloat *)as_buffer(args[1])->data); return 0;
    }
    static int zen_glMaterialf (VM *, Value *args, int) { glMaterialf ((GLenum)ival(args[0]), (GLenum)ival(args[1]), fval(args[2])); return 0; }
    static int zen_glMateriali (VM *, Value *args, int) { glMateriali ((GLenum)ival(args[0]), (GLenum)ival(args[1]), ival(args[2])); return 0; }
    static int zen_glMaterialfv(VM *vm, Value *args, int) {
        if (!is_buffer(args[2])) { vm->runtime_error("glMaterialfv: arg 3 must be Float32Array"); return 0; }
        glMaterialfv((GLenum)ival(args[0]), (GLenum)ival(args[1]), (const GLfloat *)as_buffer(args[2])->data); return 0;
    }
    static int zen_glColorMaterial(VM *, Value *args, int) { glColorMaterial((GLenum)ival(args[0]), (GLenum)ival(args[1])); return 0; }

    /* ===================================================================
    ** Fog
    ** =================================================================== */
    static int zen_glFogi (VM *, Value *args, int) { glFogi((GLenum)ival(args[0]), ival(args[1])); return 0; }
    static int zen_glFogf (VM *, Value *args, int) { glFogf((GLenum)ival(args[0]), fval(args[1])); return 0; }
    static int zen_glFogfv(VM *vm, Value *args, int) {
        if (!is_buffer(args[1])) { vm->runtime_error("glFogfv: arg 2 must be Float32Array"); return 0; }
        glFogfv((GLenum)ival(args[0]), (const GLfloat *)as_buffer(args[1])->data); return 0;
    }

    /* ===================================================================
    ** Alpha Test
    ** =================================================================== */
    static int zen_glAlphaFunc(VM *, Value *args, int) { glAlphaFunc((GLenum)ival(args[0]), fval(args[1])); return 0; }

    /* ===================================================================
    ** Stipple
    ** =================================================================== */
    static int zen_glLineStipple(VM *, Value *args, int) { glLineStipple(ival(args[0]), (GLushort)ival(args[1])); return 0; }
    /* glPolygonStipple(UInt8Buffer[128]) */
    static int zen_glPolygonStipple(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glPolygonStipple: need UInt8Array(128)"); return 0; }
        glPolygonStipple((const GLubyte *)as_buffer(args[0])->data); return 0;
    }

    /* ===================================================================
    ** Display Lists
    ** =================================================================== */
    /* glGenLists(range) → first id */
    static int zen_glGenLists   (VM *, Value *args, int) { args[0] = val_int(glGenLists(ival(args[0]))); return 1; }
    static int zen_glNewList    (VM *, Value *args, int) { glNewList((GLuint)ival(args[0]), (GLenum)ival(args[1])); return 0; }
    static int zen_glEndList    (VM *, Value *,    int) { glEndList(); return 0; }
    static int zen_glCallList   (VM *, Value *args, int) { glCallList((GLuint)ival(args[0])); return 0; }
    static int zen_glDeleteLists(VM *, Value *args, int) { glDeleteLists((GLuint)ival(args[0]), ival(args[1])); return 0; }
    static int zen_glListBase   (VM *, Value *args, int) { glListBase((GLuint)ival(args[0])); return 0; }

    /* ===================================================================
    ** Push/Pop Attrib
    ** =================================================================== */
    static int zen_glPushAttrib       (VM *, Value *args, int) { glPushAttrib((GLbitfield)ival(args[0])); return 0; }
    static int zen_glPopAttrib        (VM *, Value *,    int) { glPopAttrib(); return 0; }
    static int zen_glPushClientAttrib (VM *, Value *args, int) { glPushClientAttrib((GLbitfield)ival(args[0])); return 0; }
    static int zen_glPopClientAttrib  (VM *, Value *,    int) { glPopClientAttrib(); return 0; }

    /* ===================================================================
    ** Raster Position
    ** =================================================================== */
    static int zen_glRasterPos2f(VM *, Value *args, int) { glRasterPos2f(fval(args[0]), fval(args[1])); return 0; }
    static int zen_glRasterPos2i(VM *, Value *args, int) { glRasterPos2i(ival(args[0]), ival(args[1])); return 0; }
    static int zen_glRasterPos3f(VM *, Value *args, int) { glRasterPos3f(fval(args[0]), fval(args[1]), fval(args[2])); return 0; }
    /* glWindowPos* requer GL 1.4 / ARB_window_pos — usar RasterPos como fallback simples */
    static int zen_glWindowPos2f(VM *, Value *args, int) { glRasterPos2f(fval(args[0]), fval(args[1])); return 0; }
    static int zen_glWindowPos2i(VM *, Value *args, int) { glRasterPos2i(ival(args[0]), ival(args[1])); return 0; }

    /* ===================================================================
    ** Pixel Zoom & Bitmap
    ** =================================================================== */
    static int zen_glPixelZoom(VM *, Value *args, int) { glPixelZoom(fval(args[0]), fval(args[1])); return 0; }
    /* glBitmap(w, h, xorig, yorig, xmove, ymove, UInt8Buffer_or_nil) */
    static int zen_glBitmap(VM *, Value *args, int) {
        const GLubyte *bits = nullptr;
        if (is_buffer(args[6])) bits = (const GLubyte *)as_buffer(args[6])->data;
        glBitmap((GLsizei)ival(args[0]), (GLsizei)ival(args[1]),
                 fval(args[2]), fval(args[3]), fval(args[4]), fval(args[5]), bits);
        return 0;
    }

    /* ===================================================================
    ** Rect
    ** =================================================================== */
    static int zen_glRectf(VM *, Value *args, int) { glRectf(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }
    static int zen_glRecti(VM *, Value *args, int) { glRecti(ival(args[0]), ival(args[1]), ival(args[2]), ival(args[3])); return 0; }

    /* ===================================================================
    ** Select / Feedback / Render Mode (GL 1.x picking)
    ** =================================================================== */
    static int zen_glRenderMode(VM *, Value *args, int) { args[0] = val_int(glRenderMode((GLenum)ival(args[0]))); return 1; }
    static int zen_glInitNames (VM *, Value *,    int) { glInitNames();  return 0; }
    static int zen_glPushName  (VM *, Value *args, int) { glPushName((GLuint)ival(args[0])); return 0; }
    static int zen_glPopName   (VM *, Value *,    int) { glPopName();   return 0; }
    static int zen_glLoadName  (VM *, Value *args, int) { glLoadName((GLuint)ival(args[0])); return 0; }
    /* glSelectBuffer(UInt32Buffer) */
    static int zen_glSelectBuffer(VM *vm, Value *args, int) {
        if (!is_buffer(args[0])) { vm->runtime_error("glSelectBuffer: need UInt32Array"); return 0; }
        ObjBuffer *b = as_buffer(args[0]);
        glSelectBuffer((GLsizei)b->count, (GLuint *)b->data); return 0;
    }
    /* glPassThrough(token) */
    static int zen_glPassThrough(VM *, Value *args, int) { glPassThrough(fval(args[0])); return 0; }

    /* ===================================================================
    ** Accumulation Buffer
    ** =================================================================== */
    static int zen_glAccum    (VM *, Value *args, int) { glAccum((GLenum)ival(args[0]), fval(args[1])); return 0; }
    static int zen_glClearAccum(VM *, Value *args, int) { glClearAccum(fval(args[0]), fval(args[1]), fval(args[2]), fval(args[3])); return 0; }

    /* ===================================================================
    ** Legacy Vertex Arrays (fixed-function pipeline)
    **   glVertexPointer / glColorPointer etc.
    **   arg "ptr": Buffer → usa buffer->data
    **              int   → interpreta como offset VBO (GLvoid*)(uintptr_t)
    **              nil   → nullptr
    ** =================================================================== */
    static const void *gll_ptr(Value v) {
        if (is_buffer(v))  return as_buffer(v)->data;
        if (is_int(v))     return (const void *)(uintptr_t)(GLintptr)v.as.integer;
        return nullptr;
    }

    static int zen_glEnableClientState (VM *, Value *args, int) { glEnableClientState ((GLenum)ival(args[0])); return 0; }
    static int zen_glDisableClientState(VM *, Value *args, int) { glDisableClientState((GLenum)ival(args[0])); return 0; }

    /* glVertexPointer(size, type, stride, buffer_or_offset) */
    static int zen_glVertexPointer(VM *, Value *args, int) {
        glVertexPointer(ival(args[0]), (GLenum)ival(args[1]), ival(args[2]), gll_ptr(args[3])); return 0;
    }
    /* glColorPointer(size, type, stride, buffer_or_offset) */
    static int zen_glColorPointer(VM *, Value *args, int) {
        glColorPointer(ival(args[0]), (GLenum)ival(args[1]), ival(args[2]), gll_ptr(args[3])); return 0;
    }
    /* glNormalPointer(type, stride, buffer_or_offset) */
    static int zen_glNormalPointer(VM *, Value *args, int) {
        glNormalPointer((GLenum)ival(args[0]), ival(args[1]), gll_ptr(args[2])); return 0;
    }
    /* glTexCoordPointer(size, type, stride, buffer_or_offset) */
    static int zen_glTexCoordPointer(VM *, Value *args, int) {
        glTexCoordPointer(ival(args[0]), (GLenum)ival(args[1]), ival(args[2]), gll_ptr(args[3])); return 0;
    }
    /* glIndexPointer(type, stride, buffer_or_offset) */
    static int zen_glIndexPointer(VM *, Value *args, int) {
        glIndexPointer((GLenum)ival(args[0]), ival(args[1]), gll_ptr(args[2])); return 0;
    }
    /* glEdgeFlagPointer(stride, buffer_or_offset) */
    static int zen_glEdgeFlagPointer(VM *, Value *args, int) {
        glEdgeFlagPointer(ival(args[0]), gll_ptr(args[1])); return 0;
    }

    /* glClientActiveTexture(unit) — multi-texture coord arrays */
    static int zen_glClientActiveTexture(VM *, Value *args, int) {
        glClientActiveTexture((GLenum)ival(args[0])); return 0;
    }

    /* glArrayElement(i) — emit one element from the arrays */
    static int zen_glArrayElement(VM *, Value *args, int) { glArrayElement(ival(args[0])); return 0; }

    /* glInterleavedArrays(format, stride, buffer_or_offset) */
    static int zen_glInterleavedArrays(VM *, Value *args, int) {
        glInterleavedArrays((GLenum)ival(args[0]), ival(args[1]), gll_ptr(args[2])); return 0;
    }

    /* ===================================================================
    ** TexGen (automatic texture coordinate generation)
    ** =================================================================== */
    static int zen_glTexGeni (VM *, Value *args, int) { glTexGeni ((GLenum)ival(args[0]), (GLenum)ival(args[1]), ival(args[2])); return 0; }
    static int zen_glTexGenfv(VM *vm, Value *args, int) {
        if (!is_buffer(args[2])) { vm->runtime_error("glTexGenfv: arg 3 must be Float32Array"); return 0; }
        glTexGenfv((GLenum)ival(args[0]), (GLenum)ival(args[1]), (const GLfloat *)as_buffer(args[2])->data); return 0;
    }

    /* ========= Function registration table ========= */
    static const NativeReg gll_functions[] = {
        /* Immediate mode */
        {"glBegin",        zen_glBegin,        1},
        {"glEnd",          zen_glEnd,          0},
        /* Vertex */
        {"glVertex2f",     zen_glVertex2f,     2},
        {"glVertex2i",     zen_glVertex2i,     2},
        {"glVertex3f",     zen_glVertex3f,     3},
        {"glVertex3i",     zen_glVertex3i,     3},
        {"glVertex4f",     zen_glVertex4f,     4},
        {"glVertex2fv",    zen_glVertex2fv,    1},
        {"glVertex3fv",    zen_glVertex3fv,    1},
        /* Color */
        {"glColor3f",      zen_glColor3f,      3},
        {"glColor4f",      zen_glColor4f,      4},
        {"glColor3ub",     zen_glColor3ub,     3},
        {"glColor4ub",     zen_glColor4ub,     4},
        {"glColor3fv",     zen_glColor3fv,     1},
        {"glColor4fv",     zen_glColor4fv,     1},
        /* Normal */
        {"glNormal3f",     zen_glNormal3f,     3},
        {"glNormal3fv",    zen_glNormal3fv,    1},
        /* TexCoord */
        {"glTexCoord2f",   zen_glTexCoord2f,   2},
        {"glTexCoord3f",   zen_glTexCoord3f,   3},
        {"glTexCoord2fv",  zen_glTexCoord2fv,  1},
        /* Matrix stack */
        {"glMatrixMode",   zen_glMatrixMode,   1},
        {"glLoadIdentity", zen_glLoadIdentity, 0},
        {"glPushMatrix",   zen_glPushMatrix,   0},
        {"glPopMatrix",    zen_glPopMatrix,    0},
        {"glLoadMatrixf",  zen_glLoadMatrixf,  1},
        {"glMultMatrixf",  zen_glMultMatrixf,  1},
        {"glLoadMatrixd",  zen_glLoadMatrixd,  1},
        {"glMultMatrixd",  zen_glMultMatrixd,  1},
        {"glTranslatef",   zen_glTranslatef,   3},
        {"glRotatef",      zen_glRotatef,      4},
        {"glScalef",       zen_glScalef,       3},
        {"glTranslated",   zen_glTranslated,   3},
        {"glRotated",      zen_glRotated,      4},
        {"glScaled",       zen_glScaled,       3},
        {"glOrtho",        zen_glOrtho,        6},
        {"glFrustum",      zen_glFrustum,      6},
        {"gllGetMatrixf",  zen_gllGetMatrixf,  2},
        /* Shade model */
        {"glShadeModel",   zen_glShadeModel,   1},
        /* Lighting */
        {"glLightf",       zen_glLightf,       3},
        {"glLighti",       zen_glLighti,       3},
        {"glLightfv",      zen_glLightfv,      3},
        {"glLightModelf",  zen_glLightModelf,  2},
        {"glLightModeli",  zen_glLightModeli,  2},
        {"glLightModelfv", zen_glLightModelfv, 2},
        {"glMaterialf",    zen_glMaterialf,    3},
        {"glMateriali",    zen_glMateriali,    3},
        {"glMaterialfv",   zen_glMaterialfv,   3},
        {"glColorMaterial",zen_glColorMaterial,2},
        /* Fog */
        {"glFogi",         zen_glFogi,         2},
        {"glFogf",         zen_glFogf,         2},
        {"glFogfv",        zen_glFogfv,        2},
        /* Alpha test */
        {"glAlphaFunc",    zen_glAlphaFunc,    2},
        /* Stipple */
        {"glLineStipple",    zen_glLineStipple,    2},
        {"glPolygonStipple", zen_glPolygonStipple, 1},
        /* Display lists */
        {"glGenLists",    zen_glGenLists,    1},
        {"glNewList",     zen_glNewList,     2},
        {"glEndList",     zen_glEndList,     0},
        {"glCallList",    zen_glCallList,    1},
        {"glDeleteLists", zen_glDeleteLists, 2},
        {"glListBase",    zen_glListBase,    1},
        /* Push/Pop attrib */
        {"glPushAttrib",       zen_glPushAttrib,       1},
        {"glPopAttrib",        zen_glPopAttrib,        0},
        {"glPushClientAttrib", zen_glPushClientAttrib, 1},
        {"glPopClientAttrib",  zen_glPopClientAttrib,  0},
        /* Raster position */
        {"glRasterPos2f",  zen_glRasterPos2f, 2},
        {"glRasterPos2i",  zen_glRasterPos2i, 2},
        {"glRasterPos3f",  zen_glRasterPos3f, 3},
        {"glWindowPos2f",  zen_glWindowPos2f, 2},
        {"glWindowPos2i",  zen_glWindowPos2i, 2},
        /* Pixel zoom & bitmap */
        {"glPixelZoom",   zen_glPixelZoom,   2},
        {"glBitmap",      zen_glBitmap,      7},
        /* Rect */
        {"glRectf", zen_glRectf, 4},
        {"glRecti", zen_glRecti, 4},
        /* Selection / picking */
        {"glRenderMode",   zen_glRenderMode,   1},
        {"glInitNames",    zen_glInitNames,    0},
        {"glPushName",     zen_glPushName,     1},
        {"glPopName",      zen_glPopName,      0},
        {"glLoadName",     zen_glLoadName,     1},
        {"glSelectBuffer", zen_glSelectBuffer, 1},
        {"glPassThrough",  zen_glPassThrough,  1},
        /* Accum */
        {"glAccum",      zen_glAccum,      2},
        {"glClearAccum", zen_glClearAccum, 4},
        /* TexGen */
        {"glTexGeni",  zen_glTexGeni,  3},
        {"glTexGenfv", zen_glTexGenfv, 3},
        /* Legacy vertex arrays */
        {"glEnableClientState",    zen_glEnableClientState,    1},
        {"glDisableClientState",   zen_glDisableClientState,   1},
        {"glVertexPointer",        zen_glVertexPointer,        4},
        {"glColorPointer",         zen_glColorPointer,         4},
        {"glNormalPointer",        zen_glNormalPointer,        3},
        {"glTexCoordPointer",      zen_glTexCoordPointer,      4},
        {"glIndexPointer",         zen_glIndexPointer,         3},
        {"glEdgeFlagPointer",      zen_glEdgeFlagPointer,      2},
        {"glClientActiveTexture",  zen_glClientActiveTexture,  1},
        {"glArrayElement",         zen_glArrayElement,         1},
        {"glInterleavedArrays",    zen_glInterleavedArrays,    3},
    };

    /* ========= Constants ========= */
    #include "gll_constants.inl"

    /* ========= NativeLib definition ========= */
    const NativeLib zen_lib_gll = {
        "gll",
        gll_functions,  ZEN_ARRAY_COUNT(gll_functions),
        gll_constants,  ZEN_ARRAY_COUNT(gll_constants),
        nullptr
    };

} /* namespace zen */

#endif /* ZEN_ENABLE_GL4 */
