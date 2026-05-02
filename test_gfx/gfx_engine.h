#pragma once
/*
** gfx_engine.h — 2D Game Engine API (Raylib backend, libzen scripting)
**
** Designed to be used as a host for libzen VM.
** libzen has ZERO knowledge of this file.
**
** Usage pattern:
**   GfxEngine gfx;
**   gfx.init(1280, 720, "My Game");
**   gfx.attach_vm(&vm);           // hooks on_process_start/update/end
**   gfx.define_region(1, 0,0, 640,720);
**   gfx.define_region(2, 640,0, 640,720);
**   // main loop:
**   while (!gfx.should_close()) {
**       vm.tick_processes(gfx.dt());
**       gfx.render_frame(&vm);
**   }
**   gfx.shutdown();
*/

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "graph.h"
#include "quadtree.h"
#include <cstring>
#include <cstdint>

namespace gfx {

/* =========================================================
** Constants
** ========================================================= */
static const int MAX_REGIONS    = 8;
static const int MAX_BG_LAYERS  = 4;   /* per region */
static const int MAX_SHAKE_TIME = 60;  /* frames */

/* =========================================================
** CType — matches PRIV_CTYPE values in libzen VM
** ========================================================= */
enum CType : int { C_SCROLL = 0, C_SCREEN = 1 };

/* =========================================================
** BgLayer — scrolling/parallax background for a region
**
** DIV equivalent:
**   scroll(region, bg_fpg, bg_graph, fg_fpg, fg_graph, flags)
**   scroll_x[region] / scroll_y[region]
**
** Improvements over DIV:
**   - Per-layer parallax factor (factor_x/y: 0=fixed, 1=full scroll, 0.5=parallax)
**   - Independent tile x/y
**   - Stretch (scale texture to any size)
**   - Manual offset (scroll_x/y) writable by script every frame
** ========================================================= */
struct BgLayer
{
    int   graph_id;     /* GraphLibrary id (0 = inactive) */
    float scroll_x;     /* manual pixel offset X (writable by script, DIV scroll_x) */
    float scroll_y;     /* manual pixel offset Y */
    float factor_x;     /* parallax: 0=fixed, 1=moves with camera, 0.5=half-speed */
    float factor_y;
    float stretch_x;    /* horizontal scale of the texture (1.0 = pixel-perfect) */
    float stretch_y;
    bool  tile_x;       /* repeat horizontally */
    bool  tile_y;       /* repeat vertically */
    bool  active;
};

/* =========================================================
** CameraState — per-region camera
**
** Wraps Raylib Camera2D with extras:
**   - offset always = region centre (so target = world point at centre)
**   - follow_id: if > 0, camera lerps towards that process's x/y
**   - shake: screen-space offset decays over time
** ========================================================= */
struct CameraState
{
    Camera2D cam;           /* Raylib camera */
    float    zoom_target;   /* smooth zoom target */
    float    zoom_speed;    /* lerp speed (0=instant) */
    int      follow_id;     /* process id to follow (0 = manual) */
    float    follow_lerp;   /* 0=instant snap, 1=never moves */
    float    shake_x;       /* current shake offset (screen space) */
    float    shake_y;
    float    shake_decay;   /* per-frame decay (e.g. 0.85) */
    float    shake_mag;     /* current magnitude */
};

/* =========================================================
** Region — viewport + camera + background layers
**
** DIV equivalent: define_region(num, x, y, w, h)
** ========================================================= */
struct Region
{
    /* Screen rectangle */
    int x, y, w, h;

    /* Camera */
    CameraState camera;

    /* Background layers (rendered before world processes) */
    BgLayer bg[MAX_BG_LAYERS];

    /* Settings */
    Color   clear_color;
    bool    active;
    float   z_min;      /* only render processes with z >= z_min */
    float   z_max;      /* only render processes with z <= z_max (0 = no limit) */
};

/* =========================================================
** GfxEngine — main engine object
**
** One instance per application. Owns:
**   - Window (via Raylib)
**   - Regions
**   - GraphLibrary
**   - QuadTree (rebuilt per frame for collision)
** ========================================================= */
struct GfxEngine
{
    /* ---- Lifecycle ---- */

    /* Open window and initialise engine */
    void init(int screen_w, int screen_h, const char *title,
              int target_fps = 60);

    /* Shutdown (call before Raylib CloseWindow) */
    void shutdown();

    bool should_close() const { return WindowShouldClose(); }
    float dt() const { return GetFrameTime(); }

    /* ---- Region API ---- */

    /* Define a clipping region. idx: 1..MAX_REGIONS-1 (0 reserved) */
    void define_region(int idx, int x, int y, int w, int h,
                       Color clear = {15, 20, 35, 255});

    /* Deactivate a region */
    void remove_region(int idx);

    /* ---- Camera API ---- */

    /* Set camera target (world point to centre on) */
    void set_camera(int region, float world_x, float world_y);

    /* Set camera zoom (1.0 = pixel-perfect) */
    void set_camera_zoom(int region, float zoom, float lerp_speed = 0.0f);

    /* Set camera rotation in degrees */
    void set_camera_rotation(int region, float angle_deg);

    /* Follow a process (lerp: 0=instant, 0.9=smooth) */
    void camera_follow(int region, int process_id, float lerp = 0.1f);

    /* Stop following, keep current position */
    void camera_unfollow(int region);

    /* Shake camera (screen-space, pixels) */
    void camera_shake(int region, float magnitude, float decay = 0.85f);

    /* Get camera target (world coords of centre) */
    Vector2 get_camera_target(int region) const;

    /* Convert screen point to world coords for a region */
    Vector2 screen_to_world(int region, float sx, float sy) const;

    /* Convert world coords to screen point for a region */
    Vector2 world_to_screen(int region, float wx, float wy) const;

    /* ---- Background Layer API ---- */

    /* Set a background layer for a region.
    **   layer: 0..MAX_BG_LAYERS-1 (0=furthest back)
    **   factor: parallax (0=fixed sky, 1=sticks to world, 0.5=slow parallax)
    **   stretch: scale (1.0=natural size)
    **   tile: repeat when texture smaller than region */
    void set_bg(int region, int layer, int graph_id,
                float factor_x = 1.0f, float factor_y = 1.0f,
                float stretch_x = 1.0f, float stretch_y = 1.0f,
                bool tile_x = true, bool tile_y = true);

    /* Scroll a background layer manually (DIV: scroll_x[r] += n) */
    void scroll_bg(int region, int layer, float dx, float dy);

    /* Set absolute scroll position */
    void set_bg_scroll(int region, int layer, float x, float y);

    /* Deactivate a background layer */
    void remove_bg(int region, int layer);

    /* ---- Graph (Sprite) API ---- */

    /* Load full texture as a graph. Returns graph id */
    int load_graph(const char *path,
                   float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Load a rectangular clip from a spritesheet */
    int load_clip(const char *path, int cx, int cy, int cw, int ch,
                  float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Load N animation frames from a spritesheet.
    ** Returns id of first frame (frames are contiguous ids) */
    int load_anim(const char *path, int frame_w, int frame_h,
                  int first_frame, int num_frames,
                  float pivot_x = 0.5f, float pivot_y = 0.5f);

    /* Get natural size of a graph */
    Vector2 graph_size(int graph_id) const;

    /* ---- Render ---- */

    /* Full frame render. Call between nothing — handles BeginDrawing/EndDrawing.
    ** Reads process privates via for_each_process callback. */
    void render_frame(void *vm_ptr);  /* void* to avoid including zen/vm.h here */

    /* ---- Collision ---- */

    /* Rebuild quadtree from current process pool (called internally by render_frame) */
    void rebuild_quadtree(void *vm_ptr);

    /* Query collision against a type (ObjFunc* cast to intptr_t).
    ** Returns array of hit process ids into out[]. */
    int query_collision(float x, float y, float hw, float hh,
                        intptr_t type_id, int *out, int max_out);

    /* ---- Debug ---- */
    bool debug_quadtree;   /* draw QuadTree overlay */
    bool debug_aabb;       /* draw process AABB */
    bool debug_regions;    /* draw region borders */

    /* ---- Internal state (public for inline access) ---- */
    Region       regions_[MAX_REGIONS];
    GraphLibrary graphs_;
    QuadTree     qt_;
    int          screen_w_, screen_h_;
    float        dt_;

private:
    void _render_region(int idx, void *vm_ptr);
    void _render_bg_layer(const Region &reg, const BgLayer &bg);
    void _update_camera(Region &reg, void *vm_ptr, float dt);
    void _draw_process_slot(void *slot_ptr, bool in_world);
};

/* =========================================================
** Render transform helpers (rlgl direct — DIV-style quads)
**
** These bypass DrawTexturePro for batching efficiency and
** to support skew/arbitrary matrix transforms.
**
** make_transform() = LÖVE-style 2D transform matrix:
**   combines translate + rotate + scale + skew + origin pivot
** ========================================================= */

/* Build a 2D transform matrix.
**   x,y     : world position
**   angle   : rotation in RADIANS
**   sx,sy   : scale (1.0 = normal)
**   ox,oy   : origin offset in PIXELS (pivot point, pre-scale)
**   kx,ky   : skew (0 = no skew) */
Matrix make_transform(float x, float y, float angle,
                      float sx, float sy,
                      float ox, float oy,
                      float kx = 0.0f, float ky = 0.0f);

/* Render a textured quad using the given transform.
** clip: source rect on texture.
** w,h: destination size (pixels, pre-transform).
** flipX/Y: mirror.
** col: tint. */
void render_clip(Texture2D tex, Rectangle clip,
                 int w, int h,
                 bool flip_x, bool flip_y,
                 Color col, const Matrix &m);

/* Convenience: render using GraphDef from library */
void render_graph(const GraphLibrary &lib, int graph_id,
                  float x, float y, float angle_deg,
                  float size_pct, Color tint,
                  bool flip_x = false, bool flip_y = false);

} /* namespace gfx */
