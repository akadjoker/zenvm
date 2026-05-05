/*
** canvas_draw.h — standalone 2D draw list
**
** Zero external dependencies beyond <cstdint>, <cmath>, <vector>.
** Produces vertex/index/command buffers ready for any GPU backend.
**
** Vertex layout (stride = 20 bytes):
**   float x, y       — position
**   float u, v       — texture coord (white_u/white_v for solid shapes)
**   uint32_t col     — RGBA packed (R in low byte)
**
** Cmd:
**   uint64_t texture — opaque handle (0 = white pixel / solid)
**   float cx,cy,cw,ch — scissor rect
**   uint32_t idx_offset, idx_count
**
** Usage:
**   canvas::DrawList dl;
**   dl.set_white_pixel(atlas_handle, wu, wv);
**   dl.add_rect(10,10,100,50, dl.pack(255,0,0,255));
**   dl.add_circle(200,200,40, dl.pack(0,255,0,200));
**   // upload dl.verts + dl.indices + iterate dl.cmds
*/

#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace canvas {

/* ── types ────────────────────────────────────────────────────────────────── */

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Vertex {
    float    x, y;   /* position */
    float    u, v;   /* uv */
    uint32_t col;    /* RGBA packed */
};

struct Cmd {
    uint64_t texture;               /* opaque handle, 0 = white pixel */
    float    cx, cy, cw, ch;        /* scissor rect */
    uint32_t idx_offset;
    uint32_t idx_count;
};

/* ── helpers ──────────────────────────────────────────────────────────────── */

static inline uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

/* Auto-segment count: ceil(π / acos(1 - maxError/radius))
** maxError = 0.3 → good quality/vertex balance */
static inline int auto_segments(float radius)
{
    if (radius < 0.5f) return 4;
    const float err = 0.3f;
    double v = 1.0 - (double)err / (double)radius;
    if (v < -1.0) v = -1.0;
    if (v >  1.0) v =  1.0;
    int n = (int)std::ceil(3.14159265f / std::acos((float)v));
    if (n <   4) n =   4;
    if (n > 128) n = 128;
    return n;
}

/* ── DrawList ─────────────────────────────────────────────────────────────── */

class DrawList {
public:
    std::vector<Vertex>   verts;
    std::vector<uint32_t> indices;
    std::vector<Cmd>      cmds;

    float    tess_tol  = 1.25f;  /* bezier adaptive tolerance */

    /* pack convenience */
    static uint32_t pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    { return pack_rgba(r,g,b,a); }

    void set_white_pixel(uint64_t tex, float u, float v)
    { white_tex_ = tex; white_u_ = u; white_v_ = v; has_white_pixel_ = true; }

    bool is_ready() const { return has_white_pixel_; }

    void clear()
    {
        verts.clear(); indices.clear(); cmds.clear();
        clip_stack_.clear(); path_.clear();
    }

    /* ── clip ──────────────────────────────────────────────────────────── */
    void push_clip(float x, float y, float w, float h)
    {
        Clip c { x, y, x+w, y+h };
        if (!clip_stack_.empty()) {
            const Clip& cur = clip_stack_.back();
            c.x1 = std::max(c.x1, cur.x1);
            c.y1 = std::max(c.y1, cur.y1);
            c.x2 = std::min(c.x2, cur.x2);
            c.y2 = std::min(c.y2, cur.y2);
        }
        clip_stack_.push_back(c);
    }
    void pop_clip() { if (!clip_stack_.empty()) clip_stack_.pop_back(); }

    /* ── filled rect ───────────────────────────────────────────────────── */
    void add_rect(float x, float y, float w, float h, uint32_t col)
    {
        uint32_t off = idx_off();
        uint32_t b   = vtx(x,   y,   white_u_,white_v_,col);
                       vtx(x+w, y,   white_u_,white_v_,col);
                       vtx(x+w, y+h, white_u_,white_v_,col);
                       vtx(x,   y+h, white_u_,white_v_,col);
        push_idx6(b,off);
        flush_cmd(white_tex_,6,off);
    }

    /* ── rect outline ──────────────────────────────────────────────────── */
    void add_rect_outline(float x, float y, float w, float h, uint32_t col, float t=1.f)
    {
        float t2 = std::max(t, 1.f);
        float ih = std::max(0.f, h - 2*t2);  /* inner height — clamp to avoid negative dims */
        add_rect(x,       y,       w,  t2, col);
        add_rect(x,       y+h-t2,  w,  t2, col);
        add_rect(x,       y+t2,    t2, ih, col);
        add_rect(x+w-t2,  y+t2,    t2, ih, col);
    }

    /* ── gradient rect (4 corner colors) ──────────────────────────────── */
    void add_rect_gradient(float x, float y, float w, float h,
                           uint32_t col_tl, uint32_t col_tr,
                           uint32_t col_br, uint32_t col_bl)
    {
        uint32_t off = idx_off();
        uint32_t b   = vtx(x,   y,   white_u_,white_v_,col_tl);
                       vtx(x+w, y,   white_u_,white_v_,col_tr);
                       vtx(x+w, y+h, white_u_,white_v_,col_br);
                       vtx(x,   y+h, white_u_,white_v_,col_bl);
        push_idx6(b,off);
        flush_cmd(white_tex_,6,off);
    }

    /* ── rounded rect filled ───────────────────────────────────────────── */
    void add_round_rect(float x, float y, float w, float h, float r, uint32_t col, int segs=0)
    {
        if (r < 0.5f) { add_rect(x,y,w,h,col); return; }
        r = std::min(r, std::min(w,h)*0.5f);
        int s = segs > 0 ? segs/4 : std::max(2, auto_segments(r)/4);
        float x0=x,y0=y,x1=x+w,y1=y+h;
        path_arc_n({x1-r,y0+r},r,-1.5708f,  0.f,     s);
        path_arc_n({x1-r,y1-r},r, 0.f,      1.5708f, s);
        path_arc_n({x0+r,y1-r},r, 1.5708f,  3.14159f,s);
        path_arc_n({x0+r,y0+r},r, 3.14159f, 4.71239f,s);
        do_fill(col);
    }

    /* ── rounded rect outline ──────────────────────────────────────────── */
    void add_round_rect_outline(float x, float y, float w, float h, float r, uint32_t col, float t=1.f, int segs=0)
    {
        if (r < 0.5f) { add_rect_outline(x,y,w,h,col,t); return; }
        r = std::min(r, std::min(w,h)*0.5f);
        int s = segs > 0 ? segs/4 : std::max(2, auto_segments(r)/4);
        float x0=x,y0=y,x1=x+w,y1=y+h;
        path_arc_n({x1-r,y0+r},r,-1.5708f,  0.f,     s);
        path_arc_n({x1-r,y1-r},r, 0.f,      1.5708f, s);
        path_arc_n({x0+r,y1-r},r, 1.5708f,  3.14159f,s);
        path_arc_n({x0+r,y0+r},r, 3.14159f, 4.71239f,s);
        do_stroke(col,t,true);
    }

    /* ── circle filled ─────────────────────────────────────────────────── */
    void add_circle(float cx, float cy, float r, uint32_t col, int segs=0)
    {
        int n = segs > 0 ? segs : auto_segments(r);
        float step = 6.28318f / n;
        for (int i=0;i<n;i++) {
            float a = i*step;
            path_.push_back({cx+std::cos(a)*r, cy+std::sin(a)*r});
        }
        do_fill(col);
    }

    /* ── circle outline ────────────────────────────────────────────────── */
    void add_circle_outline(float cx, float cy, float r, uint32_t col, float t=1.f, int segs=0)
    {
        int n = segs > 0 ? segs : auto_segments(r);
        float step = 6.28318f / n;
        for (int i=0;i<n;i++) {
            float a = i*step;
            path_.push_back({cx+std::cos(a)*r, cy+std::sin(a)*r});
        }
        do_stroke(col,t,true);
    }

    /* ── ellipse filled ────────────────────────────────────────────────── */
    void add_ellipse(float cx, float cy, float rx, float ry, uint32_t col, float rot=0.f, int segs=0)
    {
        int n = segs > 0 ? segs : auto_segments(std::max(rx,ry));
        float cr=std::cos(rot), sr=std::sin(rot);
        float step=6.28318f/n;
        for (int i=0;i<n;i++) {
            float a=i*step;
            float px=std::cos(a)*rx, py=std::sin(a)*ry;
            path_.push_back({cx+px*cr-py*sr, cy+px*sr+py*cr});
        }
        do_fill(col);
    }

    /* ── ellipse outline ───────────────────────────────────────────────── */
    void add_ellipse_outline(float cx, float cy, float rx, float ry, uint32_t col, float t=1.f, float rot=0.f, int segs=0)
    {
        int n = segs > 0 ? segs : auto_segments(std::max(rx,ry));
        float cr=std::cos(rot), sr=std::sin(rot);
        float step=6.28318f/n;
        for (int i=0;i<n;i++) {
            float a=i*step;
            float px=std::cos(a)*rx, py=std::sin(a)*ry;
            path_.push_back({cx+px*cr-py*sr, cy+px*sr+py*cr});
        }
        do_stroke(col,t,true);
    }

    /* ── regular polygon filled ────────────────────────────────────────── */
    void add_ngon(float cx, float cy, float r, int n, uint32_t col)
    {
        if (n < 3) return;
        float step=6.28318f/n;
        for (int i=0;i<n;i++) {
            float a=i*step;
            path_.push_back({cx+std::cos(a)*r, cy+std::sin(a)*r});
        }
        do_fill(col);
    }

    /* ── regular polygon outline ───────────────────────────────────────── */
    void add_ngon_outline(float cx, float cy, float r, int n, uint32_t col, float t=1.f)
    {
        if (n < 3) return;
        float step=6.28318f/n;
        for (int i=0;i<n;i++) {
            float a=i*step;
            path_.push_back({cx+std::cos(a)*r, cy+std::sin(a)*r});
        }
        do_stroke(col,t,true);
    }

    /* ── triangle filled ───────────────────────────────────────────────── */
    void add_triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col)
    {
        uint32_t off = idx_off();
        uint32_t b   = vtx(x0,y0,white_u_,white_v_,col);
                       vtx(x1,y1,white_u_,white_v_,col);
                       vtx(x2,y2,white_u_,white_v_,col);
        indices.insert(indices.end(),{b,b+1,b+2});
        flush_cmd(white_tex_,3,off);
    }

    /* ── triangle outline ──────────────────────────────────────────────── */
    void add_triangle_outline(float x0,float y0, float x1,float y1, float x2,float y2, uint32_t col, float t=1.f)
    {
        path_.push_back({x0,y0});
        path_.push_back({x1,y1});
        path_.push_back({x2,y2});
        do_stroke(col,t,true);
    }

    /* ── line ──────────────────────────────────────────────────────────── */
    void add_line(float x1, float y1, float x2, float y2, uint32_t col, float t=1.f)
    {
        path_.push_back({x1,y1});
        path_.push_back({x2,y2});
        do_stroke(col,t,false);
    }

    /* ── polyline ──────────────────────────────────────────────────────── */
    void add_polyline(const Vec2* pts, int n, uint32_t col, float t=1.f, bool closed=false)
    {
        for (int i=0;i<n;i++) path_.push_back(pts[i]);
        do_stroke(col,t,closed);
    }

    /* ── bezier cubic (4 control points) ──────────────────────────────── */
    void add_bezier_cubic(float x1,float y1, float cx1,float cy1,
                          float cx2,float cy2, float x2,float y2,
                          uint32_t col, float t=1.f, int segs=0)
    {
        path_.push_back({x1,y1});
        if (segs > 0) {
            float step=1.f/segs;
            for (int i=1;i<=segs;i++)
                path_.push_back(bcubic(x1,y1,cx1,cy1,cx2,cy2,x2,y2,i*step));
        } else {
            casteljau3(x1,y1,cx1,cy1,cx2,cy2,x2,y2,tess_tol,0);
        }
        do_stroke(col,t,false);
    }

    /* ── bezier quadratic (3 control points) ──────────────────────────── */
    void add_bezier_quad(float x1,float y1, float cxq,float cyq,
                         float x2,float y2,
                         uint32_t col, float t=1.f, int segs=0)
    {
        path_.push_back({x1,y1});
        if (segs > 0) {
            float step=1.f/segs;
            for (int i=1;i<=segs;i++)
                path_.push_back(bquad(x1,y1,cxq,cyq,x2,y2,i*step));
        } else {
            casteljau2(x1,y1,cxq,cyq,x2,y2,tess_tol,0);
        }
        do_stroke(col,t,false);
    }

    /* ── image quad ────────────────────────────────────────────────────── */
    void add_image(uint64_t tex, float x, float y, float w, float h,
                   float u0=0.f,float v0=0.f,float u1=1.f,float v1=1.f,
                   uint32_t col=0xFFFFFFFFu)
    {
        uint32_t off = idx_off();
        uint32_t b   = vtx(x,   y,   u0,v0,col);
                       vtx(x+w, y,   u1,v0,col);
                       vtx(x+w, y+h, u1,v1,col);
                       vtx(x,   y+h, u0,v1,col);
        push_idx6(b,off);
        flush_cmd(tex,6,off);
    }

    /* Draw image with custom vertex data.
    ** verts: array of n vertices, each {float x, y, u, v}  (stride = 4 floats = 16 bytes)
    ** idx:   optional index array (uint32_t[idx_count]); if nullptr, sequential triangles
    **        (idx_count must be a multiple of 3; vertex count must be a multiple of 3 if no idx)
    ** col:   per-vertex tint applied to all vertices */
    void add_image_vertices(uint64_t tex,
                            const float   *verts_data, int vert_count,
                            const uint32_t *idx,       int idx_count,
                            uint32_t col = 0xFFFFFFFFu)
    {
        if (vert_count < 3) return;
        uint32_t off  = idx_off();
        uint32_t base = (uint32_t)verts.size();
        for (int i = 0; i < vert_count; i++) {
            const float *v = verts_data + i * 4;
            vtx(v[0], v[1], v[2], v[3], col);
        }
        uint32_t tri_count = 0;
        if (idx && idx_count >= 3) {
            for (int i = 0; i < idx_count; i++)
                indices.push_back(base + idx[i]);
            tri_count = (uint32_t)idx_count;
        } else {
            int n = (vert_count / 3) * 3;
            for (int i = 0; i < n; i++)
                indices.push_back(base + (uint32_t)i);
            tri_count = (uint32_t)n;
        }
        flush_cmd(tex, tri_count, off);
    }

    /* Like add_image_vertices but with per-quad colors.
    ** verts_data: float[quad_count*16]  (4 verts * 4 floats: x,y,u,v)
    ** idx:        uint32_t[quad_count*6]
    ** colors:     uint32_t[quad_count]  -- one RGBA packed color per quad */
    void add_image_batch_col(uint64_t tex,
                             const float    *verts_data, int quad_count,
                             const uint32_t *idx,
                             const uint32_t *colors)
    {
        if (quad_count <= 0) return;
        uint32_t off  = idx_off();
        uint32_t base = (uint32_t)verts.size();
        for (int q = 0; q < quad_count; q++) {
            uint32_t col     = colors[q];
            const float *v   = verts_data + q * 16;
            vtx(v[0],  v[1],  v[2],  v[3],  col);
            vtx(v[4],  v[5],  v[6],  v[7],  col);
            vtx(v[8],  v[9],  v[10], v[11], col);
            vtx(v[12], v[13], v[14], v[15], col);
        }
        int idx_count = quad_count * 6;
        for (int i = 0; i < idx_count; i++)
            indices.push_back(base + idx[i]);
        flush_cmd(tex, (uint32_t)idx_count, off);
    }

    /* Draw a sub-region of a texture (sprite sheet / font atlas).
    ** dx,dy,dw,dh -- destination rect in screen pixels
    ** sx,sy,sw,sh -- source rect in texture pixels
    ** tex_w,tex_h -- full texture size in pixels (to convert to UV)
    ** col         -- tint (default opaque white) */
    void add_image_region(uint64_t tex,
                          float dx, float dy, float dw, float dh,
                          float sx, float sy, float sw, float sh,
                          float tex_w, float tex_h,
                          uint32_t col=0xFFFFFFFFu)
    {
        float u0 = sx        / tex_w, v0 = sy        / tex_h;
        float u1 = (sx + sw) / tex_w, v1 = (sy + sh) / tex_h;
        add_image(tex, dx, dy, dw, dh, u0, v0, u1, v1, col);
    }

    /* ── path builder API ──────────────────────────────────────────────── */
    void path_clear()                       { path_.clear(); }
    void path_move(float x, float y)        { path_.clear(); path_.push_back({x,y}); }
    void path_line_to(float x, float y)     { path_.push_back({x,y}); }
    void path_arc_to(float cx, float cy, float r, float a_min, float a_max, int segs=0)
    {
        float span = std::abs(a_max-a_min);
        int n = segs > 0 ? segs : (int)std::ceil(auto_segments(r)*span/6.28318f);
        if (n < 1) n = 1;
        /* if path already has a current point, skip the first arc point to avoid
           a duplicate when chaining arc_to calls */
        path_arc_n({cx,cy},r,a_min,a_max,n, !path_.empty());
    }
    void path_bezier_cubic_to(float cx1,float cy1, float cx2,float cy2, float x2,float y2, int segs=0)
    {
        if (path_.empty()) return;
        Vec2 p1=path_.back();
        if (segs > 0) {
            float step=1.f/segs;
            for (int i=1;i<=segs;i++)
                path_.push_back(bcubic(p1.x,p1.y,cx1,cy1,cx2,cy2,x2,y2,i*step));
        } else {
            casteljau3(p1.x,p1.y,cx1,cy1,cx2,cy2,x2,y2,tess_tol,0);
        }
    }
    void path_bezier_quad_to(float cxq,float cyq, float x2,float y2, int segs=0)
    {
        if (path_.empty()) return;
        Vec2 p1=path_.back();
        if (segs > 0) {
            float step=1.f/segs;
            for (int i=1;i<=segs;i++)
                path_.push_back(bquad(p1.x,p1.y,cxq,cyq,x2,y2,i*step));
        } else {
            casteljau2(p1.x,p1.y,cxq,cyq,x2,y2,tess_tol,0);
        }
    }
    void path_fill(uint32_t col)                  { do_fill(col); }
    void path_stroke(uint32_t col, float t, bool closed) { do_stroke(col,t,closed); }

private:
    struct Clip { float x1,y1,x2,y2; };
    std::vector<Clip>     clip_stack_;
    std::vector<Vec2>     path_;
    /* scratch buffers — reused across calls, never shrink, no per-frame alloc */
    std::vector<uint32_t> scratch_vL_;
    std::vector<uint32_t> scratch_vR_;
    uint64_t white_tex_       = 0;
    float    white_u_         = 0.f;
    float    white_v_         = 0.f;
    bool     has_white_pixel_ = false; /* set by set_white_pixel() */

    uint32_t vtx(float x,float y,float u,float v,uint32_t col)
    {
        uint32_t idx=(uint32_t)verts.size();
        verts.push_back({x,y,u,v,col});
        return idx;
    }
    uint32_t idx_off() const { return (uint32_t)indices.size(); }

    void push_idx6(uint32_t b, uint32_t /*off*/)
    {
        indices.insert(indices.end(),{b,b+1,b+2, b,b+2,b+3});
    }

    Clip cur_clip() const
    {
        if (clip_stack_.empty()) return {-32768,-32768,32767,32767};
        return clip_stack_.back();
    }

    /* merge if same texture+clip and contiguous index range */
    void flush_cmd(uint64_t tex, uint32_t icount, uint32_t ioff)
    {
        Clip c = cur_clip();
        float cw = c.x2-c.x1, ch = c.y2-c.y1;
        if (!cmds.empty()) {
            Cmd& last = cmds.back();
            if (last.texture == tex &&
                last.cx == c.x1 && last.cy == c.y1 &&
                last.cw == cw   && last.ch == ch   &&
                last.idx_offset + last.idx_count == ioff) {
                last.idx_count += icount;
                return;
            }
        }
        cmds.push_back({tex, c.x1,c.y1, cw,ch, ioff, icount});
    }

    /* arc helper: appends points from a_min to a_max (n+1 total)
    ** skip_first=true: omit i=0 (avoids duplicate when chaining path_arc_to calls) */
    void path_arc_n(Vec2 c, float r, float a_min, float a_max, int n, bool skip_first=false)
    {
        int start = skip_first ? 1 : 0;
        for (int i=start;i<=n;i++) {
            float a = a_min+(a_max-a_min)*((float)i/n);
            path_.push_back({c.x+std::cos(a)*r, c.y+std::sin(a)*r});
        }
    }

    /* signed area (positive = CCW in screen-space where Y grows down = CW in math) */
    static float poly_area2(const std::vector<Vec2>& pts)
    {
        float a = 0.f;
        int n = (int)pts.size();
        for (int i = 0, j = n-1; i < n; j = i++)
            a += (pts[j].x + pts[i].x) * (pts[j].y - pts[i].y);
        return a; /* positive = CW screen winding */
    }
    static float cross2(Vec2 o, Vec2 a, Vec2 b)
    { return (a.x-o.x)*(b.y-o.y) - (a.y-o.y)*(b.x-o.x); }
    static bool pt_in_tri(Vec2 p, Vec2 a, Vec2 b, Vec2 c)
    {
        float d1 = cross2(a,b,p), d2 = cross2(b,c,p), d3 = cross2(c,a,p);
        bool neg = (d1<0)||(d2<0)||(d3<0);
        bool pos = (d1>0)||(d2>0)||(d3>0);
        return !(neg && pos);
    }

    /* ear-clipping fill — works for any simple (non-self-intersecting) polygon */
    void do_fill(uint32_t col)
    {
        if (!has_white_pixel_) { path_.clear(); return; }
        int n = (int)path_.size();
        if (n < 3) { path_.clear(); return; }

        uint32_t off  = idx_off();
        uint32_t base = (uint32_t)verts.size();
        for (auto& p : path_) vtx(p.x, p.y, white_u_, white_v_, col);

        /* Build index list; ensure CW screen winding (cross > 0 = ear for CW).
        ** poly_area2 > 0 means CCW in screen space (Y-down) → reverse to make CW.
        ** poly_area2 < 0 means CW  in screen space           → keep as-is.    */
        bool need_reverse = poly_area2(path_) > 0.f;
        std::vector<int> idx(n);
        for (int i = 0; i < n; i++) idx[i] = need_reverse ? (n-1-i) : i;

        int rem = n, iter = 0, limit = n*n + n;
        for (int i = 0; rem > 3 && iter < limit; iter++) {
            int a = idx[i % rem];
            int b = idx[(i+1) % rem];
            int c = idx[(i+2) % rem];
            Vec2 pa = path_[a], pb = path_[b], pc = path_[c];

            /* convex vertex in CW winding: cross > 0 */
            if (cross2(pa, pb, pc) <= 0.f) { i++; continue; }

            /* check no other vertex inside ear */
            bool ear = true;
            for (int j = 0; j < rem && ear; j++) {
                int k = idx[j];
                if (k == a || k == b || k == c) continue;
                if (pt_in_tri(path_[k], pa, pb, pc)) ear = false;
            }
            if (!ear) { i++; continue; }

            indices.push_back(base + (uint32_t)a);
            indices.push_back(base + (uint32_t)b);
            indices.push_back(base + (uint32_t)c);
            idx.erase(idx.begin() + ((i+1) % rem));
            rem--;
            /* restart scan from previous vertex */
            if (i > 0) i--;
            iter = 0; limit = rem*rem + rem;
        }
        if (rem == 3) {
            indices.push_back(base + (uint32_t)idx[0]);
            indices.push_back(base + (uint32_t)idx[1]);
            indices.push_back(base + (uint32_t)idx[2]);
        }
        flush_cmd(white_tex_, (uint32_t)indices.size() - off, off);
        path_.clear();
    }

    /* miter-joined stroke */
    void do_stroke(uint32_t col, float t, bool closed)
    {
        if (!has_white_pixel_) { path_.clear(); return; }
        int n=(int)path_.size();
        if (n < 2) { path_.clear(); return; }
        t = std::max(1.f,t);
        int segs = closed ? n : n-1;
        uint32_t off = idx_off();
        scratch_vL_.resize(n); scratch_vR_.resize(n);
        auto& vL = scratch_vL_; auto& vR = scratch_vR_;
        for (int i=0;i<n;i++) {
            const Vec2& p=path_[i];
            int ip=closed?((i-1+n)%n):std::max(0,i-1);
            int in_=closed?((i+1)%n):std::min(n-1,i+1);
            float dx1=p.x-path_[ip].x, dy1=p.y-path_[ip].y;
            float l1=std::sqrt(dx1*dx1+dy1*dy1);
            if(l1>1e-4f){dx1/=l1;dy1/=l1;}
            float nx1=-dy1, ny1=dx1;
            float dx2=path_[in_].x-p.x, dy2=path_[in_].y-p.y;
            float l2=std::sqrt(dx2*dx2+dy2*dy2);
            if(l2>1e-4f){dx2/=l2;dy2/=l2;}
            float nx2=-dy2, ny2=dx2;
            float mx=nx1+nx2, my=ny1+ny2;
            float d2=mx*mx+my*my;
            if(d2<1e-6f){mx=nx1;my=ny1;d2=1.f;}
            float inv=1.f/d2;
            if(inv>100.f) inv=100.f;
            mx*=inv*t; my*=inv*t;
            vL[i]=(uint32_t)verts.size(); vtx(p.x+mx,p.y+my,white_u_,white_v_,col);
            vR[i]=(uint32_t)verts.size(); vtx(p.x-mx,p.y-my,white_u_,white_v_,col);
        }
        for (int i=0;i<segs;i++) {
            int j=(i+1)%n;
            uint32_t q[]={vL[i],vL[j],vR[j],vL[i],vR[j],vR[i]};
            indices.insert(indices.end(),q,q+6);
        }
        flush_cmd(white_tex_,segs*6,off);
        path_.clear();
    }

    /* ── bezier math ──────────────────────────────────────────────────── */
    Vec2 bcubic(float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4,float t)
    {
        float u=1-t;
        return {u*u*u*x1+3*u*u*t*x2+3*u*t*t*x3+t*t*t*x4,
                u*u*u*y1+3*u*u*t*y2+3*u*t*t*y3+t*t*t*y4};
    }
    Vec2 bquad(float x1,float y1,float x2,float y2,float x3,float y3,float t)
    {
        float u=1-t;
        return {u*u*x1+2*u*t*x2+t*t*x3, u*u*y1+2*u*t*y2+t*t*y3};
    }
    void casteljau3(float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4,float tol,int lvl)
    {
        float dx=x4-x1, dy=y4-y1;
        float d2=(x2-x4)*dy-(y2-y4)*dx; if(d2<0)d2=-d2;
        float d3=(x3-x4)*dy-(y3-y4)*dx; if(d3<0)d3=-d3;
        if ((d2+d3)*(d2+d3) < tol*(dx*dx+dy*dy) || lvl>=10) {
            path_.push_back({x4,y4}); return;
        }
        float x12=(x1+x2)*.5f,y12=(y1+y2)*.5f;
        float x23=(x2+x3)*.5f,y23=(y2+y3)*.5f;
        float x34=(x3+x4)*.5f,y34=(y3+y4)*.5f;
        float x123=(x12+x23)*.5f,y123=(y12+y23)*.5f;
        float x234=(x23+x34)*.5f,y234=(y23+y34)*.5f;
        float x1234=(x123+x234)*.5f,y1234=(y123+y234)*.5f;
        casteljau3(x1,y1,x12,y12,x123,y123,x1234,y1234,tol,lvl+1);
        casteljau3(x1234,y1234,x234,y234,x34,y34,x4,y4,tol,lvl+1);
    }
    void casteljau2(float x1,float y1,float x2,float y2,float x3,float y3,float tol,int lvl)
    {
        float dx=x3-x1, dy=y3-y1;
        float det=(x2-x3)*dy-(y2-y3)*dx;
        if (det*det*4.f < tol*(dx*dx+dy*dy) || lvl>=10) {
            path_.push_back({x3,y3}); return;
        }
        float x12=(x1+x2)*.5f,y12=(y1+y2)*.5f;
        float x23=(x2+x3)*.5f,y23=(y2+y3)*.5f;
        float x123=(x12+x23)*.5f,y123=(y12+y23)*.5f;
        casteljau2(x1,y1,x12,y12,x123,y123,tol,lvl+1);
        casteljau2(x123,y123,x23,y23,x3,y3,tol,lvl+1);
    }
};

} // namespace canvas
