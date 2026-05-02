/*
** test_gfx — DIV-style graphics engine prototype
**
** Demonstrates:
**   GraphLibrary  : graph id -> sprite (fallback to shapes when id <= 0)
**   QuadTree      : rebuilt every frame, used for collision broadphase
**   Regions       : scissor + Camera2D per region (scroll)
**   c_type        : C_SCROLL (world space) / C_SCREEN (HUD)
**   z-order       : processes drawn low-z first
**
** Controls:
**   WASD          : move region-1 camera
**   Arrow keys    : move region-2 camera
**   +/-           : zoom region-1
**   R             : reset cameras
**   Q             : toggle quadtree debug overlay
**   G             : toggle AABB debug
*/

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "graph.h"
#include "quadtree.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

enum CType { C_SCROLL = 0, C_SCREEN = 1 };

/* =========================================================
** BgLayer — per-region scrolling/parallax background
**
**   factor_x/y : parallax coefficient
**                0.0 = fixed (sky)
**                0.5 = half-speed (mid-ground parallax)
**                1.0 = moves at world speed (ground-locked)
**   tile_x/y   : repeat texture to fill region
**   stretch_x/y: scale the texture (1.0 = natural size)
**   scroll_x/y : manual pixel offset (writable every frame)
** ========================================================= */
struct BgLayer
{
    int   graph_id;
    float scroll_x, scroll_y;
    float factor_x, factor_y;
    float stretch_x, stretch_y; /* 0 = natural tex size, >0 = absolute pixels */
    bool  tile_x, tile_y;
    bool  active;
};
static const int MAX_BG_LAYERS = 4;

/* =========================================================
** Region
** ========================================================= */
struct Region
{
    int      x, y, w, h;
    Camera2D camera;
    bool     active;
    Color    clear_color;
    BgLayer  bg[MAX_BG_LAYERS];
};
static const int MAX_REGIONS = 8;

/* =========================================================
** FakeProcess — mirrors VM::ProcessSlot privates layout.
** Future VM: all fields from slot->privates[PRIV_*]
** ========================================================= */
struct FakeProcess
{
    float  x, y;
    int    z;
    int    graph;       /* GraphLibrary id, <=0 = geometric fallback */
    float  angle;       /* degrees */
    float  size;        /* 100 = 1:1 */
    int    alpha;
    bool   show;
    unsigned char r, g, b;
    int    region;      /* 0 = visible in all regions */
    int    ctype;
    float  sizex, sizey;
    int    type_id;     /* fake PRIV_TYPE: 0=enemy 1=player 2=pickup */
    float  vx, vy;
    const char *label;
    bool   highlighted;
};

/* =========================================================
** render_bg_layer
**
** Draws one BgLayer in screen space (inside scissor, BEFORE BeginMode2D).
** reg  : the region it belongs to (for scissor rect + camera)
** layer: the BgLayer to draw
**
** Parallax formula:
**   The camera is at (cam.target.x, cam.target.y) in world space.
**   We want the BG to shift by factor * camera_pos (so at factor=0 it
**   never moves, at factor=1 it appears to move exactly with the world).
**
**   offset = cam_target * factor + scroll_x
**
**   To tile we just wrap offset with fmodf and draw a grid of copies
**   large enough to cover the region.
** ========================================================= */
static void render_bg_layer(const Region &reg, const BgLayer &bg,
                             gfx::GraphLibrary &graphs)
{
    if (!bg.active || bg.graph_id <= 0) return;
    gfx::GraphDef *def = graphs.get(bg.graph_id);
    if (!def) return;
    Texture2D *tex = graphs.cache.get(def->tex_idx);
    if (!tex || tex->id == 0) return;

    float tw = (bg.stretch_x > 0.0f) ? bg.stretch_x : def->clip.width;
    float th = (bg.stretch_y > 0.0f) ? bg.stretch_y : def->clip.height;

    /* Camera world-top-left → pixel offset into background */
    float cx = reg.camera.target.x - reg.camera.offset.x / reg.camera.zoom;
    float cy = reg.camera.target.y - reg.camera.offset.y / reg.camera.zoom;
    float ox = cx * bg.factor_x + bg.scroll_x;
    float oy = cy * bg.factor_y + bg.scroll_y;

    /* Screen rect the quad occupies */
    float qx = (float)reg.x;
    float qy = (float)reg.y;
    float qw = (float)reg.w;
    float qh = (float)reg.h;

    /* UV: offset in tile units, size = how many tiles fit in the quad */
    float u0 = bg.tile_x ? ( ox / tw)            : 0.0f;
    float v0 = bg.tile_y ? ( oy / th)            : 0.0f;
    float u1 = bg.tile_x ? (u0 + qw / tw)        : 1.0f;
    float v1 = bg.tile_y ? (v0 + qh / th)        : 1.0f;

    /* UV within the atlas clip (normalised to full texture) */
    float ttw = (float)tex->width;
    float tth = (float)tex->height;
    float cu  = def->clip.x / ttw;
    float cv  = def->clip.y / tth;
    float csw = def->clip.width  / ttw;
    float csh = def->clip.height / tth;

    /* Remap u/v into the clip's UV space */
    u0 = cu + u0 * csw;  u1 = cu + u1 * csw;
    v0 = cv + v0 * csh;  v1 = cv + v1 * csh;

    SetTextureWrap(*tex, TEXTURE_WRAP_REPEAT);
    rlCheckRenderBatchLimit(4);
    rlSetTexture(tex->id);
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlTexCoord2f(u0, v0); rlVertex2f(qx,      qy);
        rlTexCoord2f(u0, v1); rlVertex2f(qx,      qy + qh);
        rlTexCoord2f(u1, v1); rlVertex2f(qx + qw, qy + qh);
        rlTexCoord2f(u1, v0); rlVertex2f(qx + qw, qy);
    rlEnd();
    rlSetTexture(0);
}

/* =========================================================
** load_gen_graph — add a programmatic Texture2D to GraphLibrary
** The GraphLibrary::clear() will call UnloadTexture on it normally.
** ========================================================= */
static int load_gen_graph(gfx::GraphLibrary &lib, Texture2D tex,
                           float pivot_x = 0.5f, float pivot_y = 0.5f)
{
    if (lib.count >= gfx::MAX_GRAPHS)   return 0;
    if (lib.cache.count >= gfx::MAX_TEXTURES) return 0;

    int tidx = lib.cache.count++;
    gfx::TextureCache::Entry &e = lib.cache.entries[tidx];
    strncpy(e.path, "__gen__", sizeof(e.path) - 1);
    e.tex  = tex;
    e.used = true;

    int gid = lib.count++;
    lib.graphs[gid].tex_idx = tidx;
    lib.graphs[gid].clip    = {0, 0, (float)tex.width, (float)tex.height};
    lib.graphs[gid].pivot   = {pivot_x, pivot_y};
    lib.graphs[gid].valid   = true;
    return gid;
}

/* =========================================================
** Draw one process
** ========================================================= */
static void draw_process(const FakeProcess &p, gfx::GraphLibrary &graphs,
                          bool show_aabb)
{
    if (!p.show) return;
    float scale = p.size / 100.0f;
    Color col = p.highlighted
        ? Color{255,80,80,255}
        : Color{p.r, p.g, p.b, (unsigned char)p.alpha};

    if (graphs.get(p.graph)) {
        graphs.draw(p.graph, p.x, p.y, p.angle, p.size, col);
    } else {
        float rad = p.angle * DEG2RAD, ca = cosf(rad), sa = sinf(rad);
        switch (p.type_id % 3) {
        case 0:
            DrawCircle((int)p.x, (int)p.y, 12.0f*scale, col);
            break;
        case 1: {
            float hw=16*scale, hh=10*scale;
            Vector2 v[4]={
                {p.x+(-hw*ca - -hh*sa), p.y+(-hw*sa + -hh*ca)},
                {p.x+( hw*ca - -hh*sa), p.y+( hw*sa + -hh*ca)},
                {p.x+( hw*ca -  hh*sa), p.y+( hw*sa +  hh*ca)},
                {p.x+(-hw*ca -  hh*sa), p.y+(-hw*sa +  hh*ca)},
            };
            DrawTriangle(v[0],v[1],v[2],col); DrawTriangle(v[0],v[2],v[3],col);
            break; }
        default: {
            float sz=18*scale;
            DrawTriangle(
                {p.x+ca*sz,              p.y+sa*sz},
                {p.x-ca*sz*.5f+sa*sz*.5f,p.y-sa*sz*.5f-ca*sz*.5f},
                {p.x-ca*sz*.5f-sa*sz*.5f,p.y-sa*sz*.5f+ca*sz*.5f}, col);
            break; }
        }
    }
    if (show_aabb)
        DrawRectangleLinesEx({p.x-p.sizex, p.y-p.sizey, p.sizex*2, p.sizey*2},
                              1, {255,255,0,120});
    if (p.label)
        DrawText(p.label, (int)p.x+14, (int)p.y-8, 10, WHITE);
}

/* =========================================================
** Render one region
** ========================================================= */
static void render_region(int ridx, const Region &reg,
                           std::vector<FakeProcess> &pool,
                           gfx::GraphLibrary &graphs,
                           const gfx::QuadTree &qt,
                           bool show_qt, bool show_aabb)
{
    if (!reg.active) return;

    std::vector<const FakeProcess*> world, screen;
    for (const auto &p : pool) {
        if (!p.show) continue;
        if (p.region != 0 && p.region != ridx) continue;
        (p.ctype == C_SCROLL ? world : screen).push_back(&p);
    }
    auto cmp_z = [](const FakeProcess *a, const FakeProcess *b){ return a->z < b->z; };
    std::sort(world.begin(), world.end(), cmp_z);
    std::sort(screen.begin(), screen.end(), cmp_z);

    BeginScissorMode(reg.x, reg.y, reg.w, reg.h);
    DrawRectangle(reg.x, reg.y, reg.w, reg.h, reg.clear_color);

    /* --- Background layers (screen space, before world) --- */
    for (int i = 0; i < MAX_BG_LAYERS; i++)
        render_bg_layer(reg, reg.bg[i], graphs);

    BeginMode2D(reg.camera);
    for (int gx=-2000; gx<=2000; gx+=100) DrawLine(gx,-2000,gx,2000,{40,40,60,140});
    for (int gy=-2000; gy<=2000; gy+=100) DrawLine(-2000,gy,2000,gy,{40,40,60,140});
    DrawCircle(0,0,5,{255,80,80,200});

    if (show_qt) qt.draw_debug({50,200,100,70});

    for (const auto *p : world)  draw_process(*p, graphs, show_aabb);
    EndMode2D();

    for (const auto *p : screen) {
        FakeProcess local = *p; local.x += reg.x; local.y += reg.y;
        draw_process(local, graphs, false);
    }

    DrawRectangleLines(reg.x, reg.y, reg.w, reg.h, {100,200,255,50});
    EndScissorMode();
}

/* =========================================================
** Tick + collision broadphase
** ========================================================= */
static void tick(std::vector<FakeProcess> &pool, float dt)
{
    for (auto &p : pool) {
        if (p.ctype == C_SCREEN) continue;
        p.x += p.vx*dt; p.y += p.vy*dt; p.angle += 30.0f*dt;
        if (p.x> 600||p.x<-600) p.vx=-p.vx;
        if (p.y> 400||p.y<-400) p.vy=-p.vy;
        p.highlighted = false;
    }
}

static void collision_pass(std::vector<FakeProcess> &pool, gfx::QuadTree &qt)
{
    qt.clear();
    for (int i = 0; i < (int)pool.size(); i++) {
        const auto &p = pool[i];
        if (p.ctype != C_SCROLL) continue;
        qt.insert({i, (intptr_t)p.type_id,
                   {p.x-p.sizex, p.y-p.sizey, p.sizex*2, p.sizey*2}});
    }
    /* Demo: player/pickup test collision against enemies (type_id 0) */
    for (auto &p : pool) {
        if (p.ctype != C_SCROLL || p.type_id == 0) continue;
        gfx::QTRect bounds = {p.x-p.sizex, p.y-p.sizey, p.sizex*2, p.sizey*2};
        gfx::QTItem results[gfx::QT_MAX_RESULT];
        if (qt.query_type(bounds, 0, results, gfx::QT_MAX_RESULT) > 0)
            p.highlighted = true;
    }
}

/* =========================================================
** Main
** ========================================================= */
int main(void)
{
    const int SW=1280, SH=720;
    InitWindow(SW, SH, "test_gfx — GraphLibrary + QuadTree + Regions");
    SetTargetFPS(60);

    Region regions[MAX_REGIONS] = {};
    regions[1] = {0,    0, SW/2, SH, {{(float)(SW/4),(float)(SH/2)},{0,0},0,1.0f}, true, {15,20,35,255}};
    regions[2] = {SW/2, 0, SW/2, SH, {{(float)(SW/4),(float)(SH/2)},{0,0},0,0.6f}, true, {25,15,30,255}};

    gfx::GraphLibrary graphs;
    graphs.init();

    /* --- Procedural test textures for BgLayer demo --- */

    /* Layer 0 — Sky: vertical gradient (dark blue → lighter blue), stars
    ** factor=0 → completamente fixo, não se move com a câmara
    ** stretch=region size → cobre tudo sem tilar */
    {
        const int W = 256, H = 256;
        Image img = GenImageGradientLinear(W, H, 270,
                                           {8, 12, 48, 255},
                                           {30, 60, 120, 255});
        for (int i = 0; i < 80; i++) {
            int sx = (i * 73 + 7)  % W;
            int sy = (i * 37 + 11) % (H * 2 / 3);  /* stars only in top 2/3 */
            unsigned char br = (unsigned char)(180 + (i * 17) % 75);
            ImageDrawPixel(&img, sx, sy, {br, br, (unsigned char)(br+20), 255});
        }
        int bg_sky = load_gen_graph(graphs, LoadTextureFromImage(img), 0, 0);
        UnloadImage(img);
        /* stretch = absolute pixel size = fill the whole region */
        regions[1].bg[0] = {bg_sky, 0,0, 0.0f,0.0f, (float)(SW/2),(float)SH, false,false, true};
        regions[2].bg[0] = {bg_sky, 0,0, 0.0f,0.0f, (float)(SW/2),(float)SH, false,false, true};
    }

    /* Layer 1 — Far mountains: silhouette strip, parallax 0.2
    ** tile_x=true, tile_y=false → repete só na horizontal
    ** stretch_y=0 → altura natural da textura */
    {
        const int W = 256, H = 96;
        Image img = GenImageColor(W, H, {0, 0, 0, 0});
        /* draw jagged mountain silhouette */
        for (int x = 0; x < W; x++) {
            int peak = (int)(H * 0.3f + H * 0.4f *
                fabsf(sinf(x * 0.06f) * cosf(x * 0.03f + 1.2f)));
            for (int y = H - peak; y < H; y++)
                ImageDrawPixel(&img, x, y, {35, 45, 70, 200});
        }
        int bg_mnt = load_gen_graph(graphs, LoadTextureFromImage(img), 0, 0);
        UnloadImage(img);
        /* anchor to bottom of region: scroll_y = SH - H = 720-96 = 624 */
        regions[1].bg[1] = {bg_mnt, 0,(float)(SH-H), 0.2f,0.0f, 0,0, true,false, true};
        regions[2].bg[1] = {bg_mnt, 0,(float)(SH-H), 0.2f,0.0f, 0,0, true,false, true};
    }

    /* Layer 2 — Near trees: taller silhouette strip, parallax 0.5 */
    {
        const int W = 128, H = 160;
        Image img = GenImageColor(W, H, {0, 0, 0, 0});
        for (int x = 0; x < W; x++) {
            /* trunk */
            int trunk_h = 30 + (x * 11) % 20;
            int crown_h = 50 + (x * 7)  % 40;
            int base = H - trunk_h;
            for (int y = base; y < H; y++)
                ImageDrawPixel(&img, x, y, {25, 18, 10, 240});
            /* crown: triangle-ish */
            for (int y = base - crown_h; y < base; y++) {
                int half = (int)((base - y) * 0.4f + 2);
                int cx2  = x - half;
                int ex   = x + half;
                if (cx2 < 0) cx2 = 0; if (ex >= W) ex = W-1;
                for (int px = cx2; px <= ex; px++)
                    ImageDrawPixel(&img, px, y, {15, 45, 20, 230});
            }
        }
        int bg_trees = load_gen_graph(graphs, LoadTextureFromImage(img), 0, 0);
        UnloadImage(img);
        regions[1].bg[2] = {bg_trees, 0,(float)(SH-H), 0.5f,0.0f, 0,0, true,false, true};
        regions[2].bg[2] = {bg_trees, 0,(float)(SH-H), 0.5f,0.0f, 0,0, true,false, true};
    }

    /* Layer 3 — Ground tiles: full parallax (1.0), tiled */
    {
        const int W = 64, H = 32;
        Image img = GenImageColor(W, H, {55, 38, 20, 255});
        /* brick rows */
        for (int y = 0; y < H; y += 8) {
            for (int x = 0; x < W; x++)
                ImageDrawPixel(&img, x, y, {30, 20, 10, 255});
            int offset = ((y / 8) % 2) * (W / 2);
            for (int x = 0; x < W; x += W/2)
                ImageDrawPixel(&img, (x + offset) % W, y+1, {30, 20, 10, 255});
        }
        int bg_floor = load_gen_graph(graphs, LoadTextureFromImage(img), 0, 0);
        UnloadImage(img);
        /* anchor to bottom: fill last 32px of region */
        regions[1].bg[3] = {bg_floor, 0,(float)(SH-H), 1.0f,0.0f, 0,0, true,false, true};
        regions[2].bg[3] = {bg_floor, 0,(float)(SH-H), 1.0f,0.0f, 0,0, true,false, true};
    }

    /* Future: graphs.load_anim("ship.png", 32,32, 0,8); */

    gfx::QuadTree qt;
    qt.init(-1000,-1000,2000,2000);

    std::vector<FakeProcess> pool;
    auto mk = [](float x,float y,float vx,float vy,int z,int tid,
                 int reg,int ctype,unsigned char r,unsigned char g,unsigned char b,
                 const char *lbl) -> FakeProcess {
        FakeProcess p={}; p.x=x;p.y=y;p.vx=vx;p.vy=vy;p.z=z;
        p.type_id=tid;p.region=reg;p.ctype=ctype;p.r=r;p.g=g;p.b=b;
        p.alpha=255;p.size=100;p.show=true;p.sizex=16;p.sizey=16;
        p.graph=-1;p.label=lbl;return p;
    };

    /* Region 1: enemies(0), player(1), pickup(2) */
    pool.push_back(mk( 100, 50,  80, 40,1,0,1,C_SCROLL,220, 80, 80,"E1"));
    pool.push_back(mk(-120, 80, -60, 90,1,0,1,C_SCROLL,220, 80, 80,"E2"));
    pool.push_back(mk( 200,-90,  50,-70,1,0,1,C_SCROLL,220, 80, 80,"E3"));
    pool.push_back(mk(-200,120, 110,-40,1,0,1,C_SCROLL,220, 80, 80,"E4"));
    pool.push_back(mk(  50,200, -90, 60,1,0,1,C_SCROLL,220, 80, 80,"E5"));
    pool.push_back(mk(   0,  0,  40,-30,2,1,1,C_SCROLL, 80,180,255,"PL"));
    pool.push_back(mk( 150,150, -50, 80,0,2,1,C_SCROLL, 80,220, 80,"PK"));
    /* Region 2 */
    pool.push_back(mk(-150, 80, 100,-50,1,0,2,C_SCROLL,255,140, 50,"X1"));
    pool.push_back(mk(  50,-150,-80, 60,1,0,2,C_SCROLL,255,140, 50,"X2"));
    pool.push_back(mk(  80, 80,  60, 60,2,1,2,C_SCROLL, 80,180,255,"P2"));
    /* Shared (region 0) */
    {auto p=mk(0,0,0,0,0,2,0,C_SCROLL,255,255,255,"*");p.alpha=150;p.size=200;pool.push_back(p);}
    /* HUD */
    {auto h=mk(10,SH-30,0,0,10,0,1,C_SCREEN, 80,220, 80,"HUD-L");pool.push_back(h);}
    {auto h=mk(10,SH-30,0,0,10,0,2,C_SCREEN,220, 80,180,"HUD-R");pool.push_back(h);}

    bool show_qt=true, show_aabb=true;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        float spd = 200.0f*dt;
        if (IsKeyDown(KEY_W)) regions[1].camera.target.y-=spd;
        if (IsKeyDown(KEY_S)) regions[1].camera.target.y+=spd;
        if (IsKeyDown(KEY_A)) regions[1].camera.target.x-=spd;
        if (IsKeyDown(KEY_D)) regions[1].camera.target.x+=spd;
        if (IsKeyDown(KEY_UP))    regions[2].camera.target.y-=spd;
        if (IsKeyDown(KEY_DOWN))  regions[2].camera.target.y+=spd;
        if (IsKeyDown(KEY_LEFT))  regions[2].camera.target.x-=spd;
        if (IsKeyDown(KEY_RIGHT)) regions[2].camera.target.x+=spd;
        if (IsKeyDown(KEY_EQUAL)) regions[1].camera.zoom=Clamp(regions[1].camera.zoom+dt,0.1f,5.f);
        if (IsKeyDown(KEY_MINUS)) regions[1].camera.zoom=Clamp(regions[1].camera.zoom-dt,0.1f,5.f);
        if (IsKeyPressed(KEY_R)) {
            regions[1].camera={{(float)(SW/4),(float)(SH/2)},{0,0},0,1.0f};
            regions[2].camera={{(float)(SW/4),(float)(SH/2)},{0,0},0,0.6f};
        }
        if (IsKeyPressed(KEY_Q)) show_qt=!show_qt;
        if (IsKeyPressed(KEY_G)) show_aabb=!show_aabb;
        /* Manual scroll on mid layer (B key) */
        if (IsKeyDown(KEY_B)) { regions[1].bg[1].scroll_x += 100.0f*dt; regions[2].bg[1].scroll_x += 100.0f*dt; }

        tick(pool, dt);
        collision_pass(pool, qt);

        BeginDrawing();
        ClearBackground(BLACK);
        for (int i=1;i<MAX_REGIONS;i++)
            render_region(i,regions[i],pool,graphs,qt,show_qt,show_aabb);

        DrawFPS(SW/2-40,4);
        int hits=0; for(const auto&p:pool)if(p.highlighted)hits++;
        char buf[80]; snprintf(buf,sizeof(buf),"QT nodes:%d  col hits:%d",qt.num_nodes,hits);
        DrawText(buf, SW/2-120, SH-18, 10, {180,255,180,200});
        DrawText("WASD=cam1 Arrows=cam2 +/-=zoom R=reset Q=qt G=aabb",
                 10,SH-18,10,{180,180,180,200});
        DrawText("Region 1: enemies(red) player(blue) pickup(green)  highlighted=collision",
                 10,SH-34,10,{200,200,200,160});
        EndDrawing();
    }

    graphs.clear();
    CloseWindow();
    return 0;
}
