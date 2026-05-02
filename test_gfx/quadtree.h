#pragma once
/*
** quadtree.h — Dynamic axis-aligned quadtree for 2D collision broadphase.
**
** Usage (rebuild every frame):
**   qt.clear();
**   for each process: qt.insert({ id, type_id, bounds });
**   // query:
**   QTItem results[64];
**   int n = qt.query_type(my_bounds, bullet_type_id, results, 64);
**
** Design choices:
**   - Static node pool (QT_MAX_NODES) — zero allocation, cache friendly
**   - Rebuilt from scratch each frame (simple, correct, fast enough for DIV-scale games)
**   - Items that straddle child boundaries are stored at the splitting node
**   - query_type() filters by type_id — maps to PRIV_TYPE cast to intptr_t
**
** VM integration (future):
**   QTItem { slot->id, (intptr_t)as_obj(slot->privates[PRIV_TYPE]),
**             { x-sw/2, y-sh/2, sw, sh } }
**   collision(type X) → query_type(my_bounds, (intptr_t)X_func, ...)
*/

#include "raylib.h"
#include <cstdint>
#include <cstring>

namespace gfx {

/* =========================================================
** QTRect — axis-aligned bounding box
** ========================================================= */
struct QTRect
{
    float x, y, w, h;

    bool overlaps(const QTRect &o) const {
        return x < o.x + o.w && x + w > o.x &&
               y < o.y + o.h && y + h > o.y;
    }
    bool contains(const QTRect &o) const {
        return o.x >= x && o.y >= y &&
               o.x + o.w <= x + w && o.y + o.h <= y + h;
    }
};

/* =========================================================
** QTItem — what gets stored in the tree
** ========================================================= */
struct QTItem
{
    int      id;       /* process id */
    intptr_t type_id;  /* ObjFunc* cast — for type-based queries */
    QTRect   bounds;
};

/* =========================================================
** QuadTree
** ========================================================= */
static const int QT_MAX_ITEMS  = 6;    /* items per node before split */
static const int QT_MAX_DEPTH  = 7;    /* max subdivision depth */
static const int QT_MAX_NODES  = 4096; /* static pool — zero alloc */
static const int QT_MAX_RESULT = 256;  /* max results from query */

struct QuadNode
{
    QTRect bounds;
    QTItem items[QT_MAX_ITEMS];
    int    count;
    int    children[4]; /* indices into pool, -1 = leaf */
};

struct QuadTree
{
    QuadNode nodes[QT_MAX_NODES];
    int      num_nodes;

    /* world_bounds is the root rectangle — should cover your entire world */
    void init(float world_x, float world_y, float world_w, float world_h);

    /* Clear all nodes (keeps world_bounds), call every frame before inserting */
    void clear();

    /* Insert a process into the tree */
    void insert(const QTItem &item);

    /* Query all items whose bounds overlap rect */
    int query(const QTRect &rect, QTItem *out, int max_out) const;

    /* Query only items with matching type_id */
    int query_type(const QTRect &rect, intptr_t type_id,
                   QTItem *out, int max_out) const;

    /* Debug: draw all non-empty node boundaries */
    void draw_debug(Color col) const;

private:
    QTRect _world;

    int  _alloc(const QTRect &b);
    void _split(int idx, int depth);
    void _insert(int idx, const QTItem &item, int depth);
    int  _query(int idx, const QTRect &rect,
                QTItem *out, int max_out, int found) const;
    int  _query_type(int idx, const QTRect &rect, intptr_t type_id,
                     QTItem *out, int max_out, int found) const;
    void _draw_debug(int idx, Color col) const;
    QTRect _child_rect(const QTRect &b, int q) const;
};

} /* namespace gfx */
