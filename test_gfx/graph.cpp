#include "graph.h"
#include "rlgl.h"
#include <cmath>

/* Half-texel inset: avoids texture bleeding on atlas clips */
#define GFX_FIX_TEXEL_BLEED 1

namespace gfx {

int GraphLibrary::load(const char *path, float px, float py)
{
    return load_clip(path, 0, 0, 0, 0, px, py);
}

int GraphLibrary::load_clip(const char *path,
                             int cx, int cy, int cw, int ch,
                             float px, float py)
{
    if (count >= MAX_GRAPHS) {
        fprintf(stderr, "GraphLibrary: max graphs reached\n");
        return -1;
    }
    int tex_idx = cache.get_or_load(path);
    if (tex_idx < 0) return -1;

    int id = count++;
    GraphDef &g = graphs[id];
    g.tex_idx = tex_idx;
    g.pivot   = { px, py };
    g.valid   = true;

    Texture2D *tex = cache.get(tex_idx);
    g.clip = (cw == 0 || ch == 0)
        ? Rectangle{ 0, 0, (float)tex->width, (float)tex->height }
        : Rectangle{ (float)cx, (float)cy, (float)cw, (float)ch };

    return id;
}

int GraphLibrary::load_anim(const char *path,
                             int frame_w, int frame_h,
                             int first_frame, int num_frames,
                             float px, float py)
{
    if (count + num_frames > MAX_GRAPHS) {
        fprintf(stderr, "GraphLibrary: not enough graph slots for animation\n");
        return -1;
    }
    int tex_idx = cache.get_or_load(path);
    if (tex_idx < 0) return -1;

    Texture2D *tex = cache.get(tex_idx);
    int cols = (frame_w > 0) ? (tex->width / frame_w) : 1;
    int first_id = count;

    for (int i = 0; i < num_frames; i++) {
        int frame = first_frame + i;
        int fx = (frame % cols) * frame_w;
        int fy = (frame / cols) * frame_h;
        int id = count++;
        GraphDef &g = graphs[id];
        g.tex_idx = tex_idx;
        g.clip    = { (float)fx, (float)fy, (float)frame_w, (float)frame_h };
        g.pivot   = { px, py };
        g.valid   = true;
    }
    return first_id;
}

void GraphLibrary::draw(int id, float x, float y,
                         float angle_deg, float size_pct, Color tint,
                         bool flip_h, bool flip_v)
{
    GraphDef *g = get(id);
    if (!g) return;
    Texture2D *tex = cache.get(g->tex_idx);
    if (!tex || tex->id == 0) return;

    float scale = size_pct / 100.0f;
    float w  = g->clip.width  * scale;
    float h  = g->clip.height * scale;
    float ox = w * g->pivot.x;   /* pivot offset in local space */
    float oy = h * g->pivot.y;

    /* UV coords with optional half-texel inset to avoid atlas bleeding */
    float tw = (float)tex->width;
    float th = (float)tex->height;
#if GFX_FIX_TEXEL_BLEED
    float left   = (2.0f * g->clip.x     + 1.0f) / (2.0f * tw);
    float right  = left + (g->clip.width  * 2.0f - 2.0f) / (2.0f * tw);
    float top    = (2.0f * g->clip.y     + 1.0f) / (2.0f * th);
    float bottom = top  + (g->clip.height * 2.0f - 2.0f) / (2.0f * th);
#else
    float left   = g->clip.x / tw;
    float right  = (g->clip.x + g->clip.width)  / tw;
    float top    = g->clip.y / th;
    float bottom = (g->clip.y + g->clip.height) / th;
#endif

    /* UV flip: just swap the coordinates */
    if (flip_h) { float t = left;  left   = right;  right  = t; }
    if (flip_v) { float t = top;   top    = bottom; bottom = t; }

    /* Local quad corners (pivot at origin) */
    float lx0 = -ox,     ly0 = -oy;       /* top-left     */
    float lx1 = -ox,     ly1 = -oy + h;   /* bottom-left  */
    float lx2 = -ox + w, ly2 = -oy + h;   /* bottom-right */
    float lx3 = -ox + w, ly3 = -oy;       /* top-right    */

    /* Rotate in local space, then translate to world */
    float rad = angle_deg * DEG2RAD;
    float ca  = cosf(rad);
    float sa  = sinf(rad);
    auto rot = [&](float lx, float ly) -> Vector2 {
        return { x + lx * ca - ly * sa,
                 y + lx * sa + ly * ca };
    };
    Vector2 r0 = rot(lx0, ly0);
    Vector2 r1 = rot(lx1, ly1);
    Vector2 r2 = rot(lx2, ly2);
    Vector2 r3 = rot(lx3, ly3);

    /* Submit quad via rlgl (same vertex order as RenderClipFlip) */
    rlCheckRenderBatchLimit(4);
    rlSetTexture(tex->id);
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        /* top-left */
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(left,  top);    rlVertex3f(r0.x, r0.y, 0.0f);
        /* bottom-left */
        rlTexCoord2f(left,  bottom); rlVertex3f(r1.x, r1.y, 0.0f);
        /* bottom-right */
        rlTexCoord2f(right, bottom); rlVertex3f(r2.x, r2.y, 0.0f);
        /* top-right */
        rlTexCoord2f(right, top);    rlVertex3f(r3.x, r3.y, 0.0f);
    rlEnd();
    rlSetTexture(0);
}

} /* namespace gfx */
