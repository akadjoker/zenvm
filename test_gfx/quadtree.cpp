#include "quadtree.h"

namespace gfx {

void QuadTree::init(float wx, float wy, float ww, float wh)
{
    _world = { wx, wy, ww, wh };
    clear();
}

void QuadTree::clear()
{
    num_nodes = 0;
    _alloc(_world); /* root = node 0 */
}

int QuadTree::_alloc(const QTRect &b)
{
    if (num_nodes >= QT_MAX_NODES) return 0; /* overflow → root */
    int idx = num_nodes++;
    QuadNode &n = nodes[idx];
    n.bounds = b;
    n.count  = 0;
    n.children[0] = n.children[1] = n.children[2] = n.children[3] = -1;
    return idx;
}

QTRect QuadTree::_child_rect(const QTRect &b, int q) const
{
    float hw = b.w * 0.5f;
    float hh = b.h * 0.5f;
    switch (q) {
    case 0: return { b.x,      b.y,      hw, hh }; /* TL */
    case 1: return { b.x + hw, b.y,      hw, hh }; /* TR */
    case 2: return { b.x,      b.y + hh, hw, hh }; /* BL */
    default:return { b.x + hw, b.y + hh, hw, hh }; /* BR */
    }
}

void QuadTree::_split(int idx, int depth)
{
    /* Allocate 4 children */
    for (int q = 0; q < 4; q++)
        nodes[idx].children[q] = _alloc(_child_rect(nodes[idx].bounds, q));

    /* Re-insert existing items into children */
    int saved_count = nodes[idx].count;
    QTItem saved[QT_MAX_ITEMS];
    memcpy(saved, nodes[idx].items, saved_count * sizeof(QTItem));
    nodes[idx].count = 0;

    for (int i = 0; i < saved_count; i++)
        _insert(idx, saved[i], depth + 1);
}

void QuadTree::_insert(int idx, const QTItem &item, int depth)
{
    QuadNode &n = nodes[idx];
    bool is_leaf = (n.children[0] == -1);

    if (is_leaf) {
        /* Split if full and not at max depth */
        if (n.count >= QT_MAX_ITEMS && depth < QT_MAX_DEPTH) {
            _split(idx, depth);
            /* Fall through to child insertion below */
        } else {
            /* Store here */
            if (n.count < QT_MAX_ITEMS)
                n.items[n.count++] = item;
            return;
        }
    }

    /* Try to fit item into exactly one child */
    for (int q = 0; q < 4; q++) {
        int cidx = nodes[idx].children[q];
        if (cidx >= 0 && nodes[cidx].bounds.contains(item.bounds)) {
            _insert(cidx, item, depth + 1);
            return;
        }
    }

    /* Straddles multiple children — store at this node */
    if (nodes[idx].count < QT_MAX_ITEMS)
        nodes[idx].items[nodes[idx].count++] = item;
}

void QuadTree::insert(const QTItem &item)
{
    _insert(0, item, 0);
}

int QuadTree::_query(int idx, const QTRect &rect,
                      QTItem *out, int max_out, int found) const
{
    if (idx < 0 || found >= max_out) return found;
    const QuadNode &n = nodes[idx];
    if (!n.bounds.overlaps(rect)) return found;

    for (int i = 0; i < n.count && found < max_out; i++)
        if (n.items[i].bounds.overlaps(rect))
            out[found++] = n.items[i];

    for (int q = 0; q < 4; q++)
        found = _query(n.children[q], rect, out, max_out, found);

    return found;
}

int QuadTree::query(const QTRect &rect, QTItem *out, int max_out) const
{
    return _query(0, rect, out, max_out, 0);
}

int QuadTree::_query_type(int idx, const QTRect &rect, intptr_t type_id,
                           QTItem *out, int max_out, int found) const
{
    if (idx < 0 || found >= max_out) return found;
    const QuadNode &n = nodes[idx];
    if (!n.bounds.overlaps(rect)) return found;

    for (int i = 0; i < n.count && found < max_out; i++)
        if (n.items[i].type_id == type_id && n.items[i].bounds.overlaps(rect))
            out[found++] = n.items[i];

    for (int q = 0; q < 4; q++)
        found = _query_type(n.children[q], rect, type_id, out, max_out, found);

    return found;
}

int QuadTree::query_type(const QTRect &rect, intptr_t type_id,
                          QTItem *out, int max_out) const
{
    return _query_type(0, rect, type_id, out, max_out, 0);
}

void QuadTree::_draw_debug(int idx, Color col) const
{
    if (idx < 0) return;
    const QuadNode &n = nodes[idx];
    if (n.count > 0 || n.children[0] >= 0)
        DrawRectangleLinesEx({ n.bounds.x, n.bounds.y, n.bounds.w, n.bounds.h }, 1, col);
    for (int q = 0; q < 4; q++)
        _draw_debug(n.children[q], col);
}

void QuadTree::draw_debug(Color col) const
{
    _draw_debug(0, col);
}

} /* namespace gfx */
