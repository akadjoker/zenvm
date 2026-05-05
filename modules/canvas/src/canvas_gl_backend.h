#pragma once
/* =========================================================
** canvas_gl_backend.h — OpenGL 3.3 backend for canvas::DrawList
**
** Single-header library. In exactly ONE .cpp define the impl:
**
**   #define CANVAS_GL_BACKEND_IMPL
**   #include "canvas_gl_backend.h"
**
** GL include is auto-selected; override with:
**   #define CANVAS_GL_INCLUDE <your/gl.h>
** ========================================================= */

#ifndef CANVAS_GL_INCLUDE
#  if defined(__EMSCRIPTEN__)
     /* WebGL 2 — GLES3 header, define CANVAS_GL_ES automatically */
#    include <GLES3/gl3.h>
#    ifndef CANVAS_GL_ES
#      define CANVAS_GL_ES
#    endif
#  elif defined(__ANDROID__)
     /* Android OpenGL ES 3 */
#    include <GLES3/gl3.h>
#    ifndef CANVAS_GL_ES
#      define CANVAS_GL_ES
#    endif
#  elif defined(__APPLE__)
#    include <TargetConditionals.h>
#    if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_SIMULATOR
       /* iOS / tvOS — OpenGL ES 3 */
#      include <OpenGLES/ES3/gl.h>
#      ifndef CANVAS_GL_ES
#        define CANVAS_GL_ES
#      endif
#    else
       /* macOS — desktop GL 3.3 */
#      include <OpenGL/gl3.h>
#    endif
#  elif defined(_WIN32)
     /* Windows — glad from opengl module vendor dir (added to include path by CMake) */
#    include <glad.h>
#  else
     /* Linux — same */
#    include <glad.h>
#  endif
#endif

#include "canvas_draw.h"
#include <cstdio>   /* fprintf */
#include <cstdint>

namespace canvas {

/* =========================================================
** GLBackend — owns VAO/VBO/IBO/shader/white-texture
** ========================================================= */
struct GLBackend
{
    GLuint vao       = 0;
    GLuint vbo       = 0;
    GLuint ibo       = 0;
    GLuint prog      = 0;
    GLuint white_tex = 0;
    GLint  u_proj    = -1;
    GLint  u_tex     = -1;
    int    screen_w  = 0;
    int    screen_h  = 0;
    bool   ready     = false;

    /* Call once after the GL context is current. */
    bool init(int w, int h);

    /* Call on window resize to update the projection. */
    void resize(int w, int h);

    /* Upload dl's vertex/index buffers, iterate cmds, issue
    ** glDrawElements for each, then call dl.clear().
    ** Must be called with a valid GL context current. */
    void flush(DrawList &dl);

    /* Release all GL objects. */
    void destroy();

    /* White pixel info — used to auto-configure a DrawList. */
    uint64_t white_tex_id() const { return (uint64_t)white_tex; }
    float    white_u()      const { return 0.5f; }
    float    white_v()      const { return 0.5f; }

private:
    void upload_proj(int w, int h);
};

/* =========================================================
** Implementation (compiled in exactly one TU)
** ========================================================= */
#ifdef CANVAS_GL_BACKEND_IMPL

static GLuint canvas__compile_shader(GLenum type, const char *version, const char *body)
{
    GLuint s = glCreateShader(type);
    const char *srcs[2] = { version, body };
    glShaderSource(s, 2, srcs, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        fprintf(stderr, "[canvas] shader compile error: %s\n", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/* GLSL version line — selected at compile time.
** Define CANVAS_GL_ES before including this header to target OpenGL ES 3.0 / WebGL 2. */
#ifdef CANVAS_GL_ES
static const char *kGLSLVersion =
    "#version 300 es\n"
    "precision mediump float;\n";
#else
static const char *kGLSLVersion = "#version 330 core\n";
#endif

static const char *kCanvasVS = R"glsl(
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_col;
uniform mat4 u_proj;
out vec2 v_uv;
out vec4 v_col;
void main() {
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
    v_uv  = a_uv;
    v_col = a_col;
}
)glsl";

static const char *kCanvasFS = R"glsl(
in  vec2 v_uv;
in  vec4 v_col;
uniform sampler2D u_tex;
out vec4 out_col;
void main() {
    out_col = texture(u_tex, v_uv) * v_col;
}
)glsl";

void GLBackend::upload_proj(int w, int h)
{
    screen_w = w;
    screen_h = h;
    /* Orthographic, Y-down, column-major */
    float L = 0.f, R = (float)w, T = 0.f, B = (float)h;
    float p[16] = {
         2.f/(R-L),      0.f,          0.f,  0.f,
         0.f,            2.f/(T-B),    0.f,  0.f,
         0.f,            0.f,         -1.f,  0.f,
        -(R+L)/(R-L),  -(T+B)/(T-B),  0.f,  1.f
    };
    glUseProgram(prog);
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, p);
}

bool GLBackend::init(int w, int h)
{
    /* Shaders */
    GLuint vs = canvas__compile_shader(GL_VERTEX_SHADER,   kGLSLVersion, kCanvasVS);
    GLuint fs = canvas__compile_shader(GL_FRAGMENT_SHADER, kGLSLVersion, kCanvasFS);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return false; }

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        fprintf(stderr, "[canvas] shader link error: %s\n", buf);
        glDeleteProgram(prog); prog = 0;
        return false;
    }
    u_proj = glGetUniformLocation(prog, "u_proj");
    u_tex  = glGetUniformLocation(prog, "u_tex");

    /* VAO + VBO + IBO
    ** Vertex layout (stride = 20):
    **   offset  0: vec2  pos   (float)
    **   offset  8: vec2  uv    (float)
    **   offset 16: vec4  col   (GL_UNSIGNED_BYTE, normalised) */
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, 20, (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, 20, (void*) 8);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  20, (void*)16);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBindVertexArray(0);

    /* 1×1 white RGBA texture (used for solid shapes) */
    uint32_t white = 0xFFFFFFFFu;
    glGenTextures(1, &white_tex);
    glBindTexture(GL_TEXTURE_2D, white_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Projection + sampler unit */
    upload_proj(w, h);
    glUseProgram(prog);
    glUniform1i(u_tex, 0);

    ready = true;
    return true;
}

void GLBackend::resize(int w, int h)
{
    if (ready) upload_proj(w, h);
}

void GLBackend::flush(DrawList &dl)
{
    if (!ready || dl.verts.empty()) { dl.clear(); return; }

    /* Upload geometry */
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(dl.verts.size() * sizeof(Vertex)),
                 dl.verts.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(dl.indices.size() * sizeof(uint32_t)),
                 dl.indices.data(), GL_STREAM_DRAW);

    /* Set GL state for 2D alpha-blended drawing */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);

    glActiveTexture(GL_TEXTURE0);
    glUseProgram(prog);
    glBindVertexArray(vao);

    /* One draw call per Cmd */
    for (const auto &cmd : dl.cmds)
    {
        /* Scissor: GL origin is bottom-left; canvas origin is top-left */
        glScissor((GLint) cmd.cx,
                  (GLint)(screen_h - cmd.cy - cmd.ch),
                  (GLsizei)cmd.cw,
                  (GLsizei)cmd.ch);

        GLuint tex = cmd.texture ? (GLuint)cmd.texture : white_tex;
        glBindTexture(GL_TEXTURE_2D, tex);

        glDrawElements(GL_TRIANGLES,
                       (GLsizei)cmd.idx_count,
                       GL_UNSIGNED_INT,
                       (void*)(uintptr_t)(cmd.idx_offset * sizeof(uint32_t)));
    }

    glBindVertexArray(0);
    glDisable(GL_SCISSOR_TEST);

    dl.clear();
}

void GLBackend::destroy()
{
    if (vao)       { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo)       { glDeleteBuffers(1, &vbo);      vbo = 0; }
    if (ibo)       { glDeleteBuffers(1, &ibo);      ibo = 0; }
    if (prog)      { glDeleteProgram(prog);         prog = 0; }
    if (white_tex) { glDeleteTextures(1, &white_tex); white_tex = 0; }
    ready = false;
}

#endif /* CANVAS_GL_BACKEND_IMPL */

} /* namespace canvas */
