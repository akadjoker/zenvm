#pragma once
/*
** graph.h — DIV-style graph (sprite) system with spritesheet clip support.
**
** Design:
**   - GraphLibrary owns a TextureCache (path → Texture2D, one load per file)
**   - Each graph has: texture ref, source clip rect, normalized pivot
**   - Spritesheet: load_clip() or load_anim() slice N consecutive frames
**   - graph=-1 (or id<=0) = no texture → host falls back to geometric shape
**
** VM integration (future):
**   - slot->privates[PRIV_GRAPH]  → id passed to GraphLibrary::draw()
**   - slot->privates[PRIV_ANGLE]  → angle_deg
**   - slot->privates[PRIV_SIZE]   → size_pct (100 = 1:1)
**   - slot->privates[PRIV_RED/GREEN/BLUE/ALPHA] → tint
*/

#include "raylib.h"
#include <cstring>
#include <cstdio>

namespace gfx {

static const int MAX_GRAPHS   = 512;
static const int MAX_TEXTURES = 64;

/* =========================================================
** TextureCache — loads each file once, indexed by int
** ========================================================= */
struct TextureCache
{
    struct Entry {
        char      path[256];
        Texture2D tex;
        bool      used;
    };

    Entry entries[MAX_TEXTURES];
    int   count;

    void init()  { memset(entries, 0, sizeof(entries)); count = 0; }

    void clear() {
        for (int i = 0; i < count; i++)
            if (entries[i].used) UnloadTexture(entries[i].tex);
        init();
    }

    /* Returns index, -1 on failure */
    int get_or_load(const char *path) {
        for (int i = 0; i < count; i++)
            if (entries[i].used && strcmp(entries[i].path, path) == 0)
                return i;
        if (count >= MAX_TEXTURES) {
            fprintf(stderr, "TextureCache: max textures reached\n");
            return -1;
        }
        int idx = count++;
        Entry &e = entries[idx];
        strncpy(e.path, path, 255);
        e.tex  = LoadTexture(path);
        e.used = true;
        return idx;
    }

    Texture2D *get(int idx) {
        return (idx >= 0 && idx < count && entries[idx].used)
               ? &entries[idx].tex : nullptr;
    }
};

/* =========================================================
** GraphDef — one sprite (possibly a clip of a larger texture)
** ========================================================= */
struct GraphDef
{
    int       tex_idx;   /* index into TextureCache */
    Rectangle clip;      /* source rect on texture */
    Vector2   pivot;     /* normalised pivot: 0.5,0.5 = centre */
    bool      valid;
};

/* =========================================================
** GraphLibrary
** ========================================================= */
struct GraphLibrary
{
    TextureCache cache;
    GraphDef     graphs[MAX_GRAPHS];
    int          count;  /* 0 = reserved (invalid id) */

    void init()  { cache.init(); memset(graphs, 0, sizeof(graphs)); count = 1; }
    void clear() { cache.clear(); count = 1; }

    /* Load full texture as single graph */
    int load(const char *path,
             float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Load a rectangular clip from a spritesheet */
    int load_clip(const char *path,
                  int cx, int cy, int cw, int ch,
                  float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Load num_frames consecutive horizontal frames from a spritesheet.
    ** first_frame is zero-based. Returns id of first frame (contiguous). */
    int load_anim(const char *path,
                  int frame_w, int frame_h,
                  int first_frame, int num_frames,
                  float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Get def (nullptr if invalid id) */
    GraphDef *get(int id) {
        return (id > 0 && id < count && graphs[id].valid) ? &graphs[id] : nullptr;
    }

    /* Draw at world/screen position — angle in degrees, size_pct: 100=1:1
    ** flip_h / flip_v: mirror the sprite */
    void draw(int id, float x, float y,
              float angle_deg, float size_pct, Color tint,
              bool flip_h = false, bool flip_v = false);

    /* Return the logical size of a graph (useful for AABB / collision) */
    Vector2 size(int id) {
        GraphDef *g = get(id);
        return g ? Vector2{ g->clip.width, g->clip.height } : Vector2{ 0, 0 };
    }
};

} /* namespace gfx */
