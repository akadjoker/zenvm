# ZEN 2D Game Engine — Plano Gráfico

## Filosofia
- **DIV-inspired** — process system + privates + regions + scroll
- **Raylib backend** — rlgl para batching/quad rendering directo
- **Sem loop no background** — tiles e scroll backgrounds são `DrawLayer` explícito
- **Sem Box2D ainda** — colisão própria (AABB + circle), Box2D como plugin futuro
- **Script-first** — a engine é o host C++, toda a lógica é .zen

---

## Arquitectura de Layers (render order)

```
Frame render:
  for each Region (scissor):
    BeginMode2D(camera)          <- world space
      Layer: BACKGROUND          z = -1000...-100  (tilemaps, parallax)
      Layer: WORLD               z = -99...999     (processos c_scroll)
      Layer: FOREGROUND          z = 1000...9999   (fx, partículas)
    EndMode2D()
    Layer: SCREEN                (processos c_screen / HUD)
```

Profundidade = `PRIV_Z`. Sort por z antes de render. Processos com mesmo z = ordem de criação.

---

## Privates novos necessários

| Private        | Tipo   | Descrição |
|----------------|--------|-----------|
| `PRIV_REGION`  | int    | região (0 = todas) |
| `PRIV_CTYPE`   | int    | 0=c_scroll, 1=c_screen |
| `PRIV_FLIPX`   | int    | flip horizontal |
| `PRIV_FLIPY`   | int    | flip vertical |

---

## Rendering

### Quad renderer (rlgl directo)

Cada sprite é um quad com transformação matricial:
```
Matrix = setTransformation(x, y, angle_rad, sx, sy, ox, oy, 0, 0)
```
onde `ox,oy` = pivot * size (centro por defeito).

Vantagem: skew gratuito, matrix stack, sem overhead de DrawTexturePro para batches grandes.

```cpp
struct rQuad {
    Texture2D tex;
    struct Vert { float x,y,z, tx,ty; Color col; } v[4];
};

void render_quad(const rQuad &q, const Matrix &m);
void render_clip(Texture2D tex, Rectangle clip,
                 int w, int h, bool flipX, bool flipY,
                 Color col, const Matrix &m);
Matrix make_transform(float x, float y, float angle,
                      float sx, float sy, float ox, float oy);
```

`GraphLibrary::draw()` usa `render_clip` internamente.

---

## Background / Scroll Layers

Cada Region tem até `MAX_BG_LAYERS=4` layers de background:

```cpp
struct BgLayer {
    int     graph_id;      // texture da GraphLibrary
    float   scroll_x;      // câmara multiplier (1.0 = move com câmara, 0=fixo)
    float   scroll_y;
    float   offset_x;      // offset manual do script
    float   offset_y;
    bool    repeat_x;      // tile horizontal
    bool    repeat_y;      // tile vertical
    bool    active;
};
```

Render:
```
BeginMode2D(camera * scroll_factor)
  draw tiled/single texture covering viewport
EndMode2D()
```

Script:
```zen
set_bg(region, layer, graph, scroll_x, scroll_y)
set_bg_offset(region, layer, ox, oy)
```

---

## Tilemap

```cpp
struct Tilemap {
    int      tileset_graph;     // GraphLibrary id (spritesheet)
    int      tile_w, tile_h;    // pixels per tile
    int      cols, rows;        // map dimensions
    uint16_t *data;             // tile ids (0 = empty)
    int      region;
    float    z;
};
```

Render: apenas tiles visíveis no viewport (frustum cull por rect).  
Script:
```zen
var map = load_tilemap("level1.tmj", region=1, z=-50)
get_tile(map, tx, ty)
set_tile(map, tx, ty, tile_id)
```

---

## Colisão

### Broadphase — QuadTree (já implementada)
- Rebuild por frame
- `query_type(bounds, type_id)` → candidatos

### Narrowphase — 2 shapes por processo
| `PRIV_GRAPH` shape | Narrowphase |
|--------------------|-------------|
| circle (graph=0)   | circle-circle, circle-AABB |
| rect               | AABB-AABB (OBB futuro) |

### Native `collision(type X)`
```
1. get my bounds from current_slot privates
2. qt.query_type(my_bounds, type_X_id, results, MAX)
3. narrowphase filter
4. return first hit id (or 0)
```

### Tilemap collision
```zen
var hit = collision_tile(map, x, y, w, h)
```
Verifica tiles sólidos no rect dado.

---

## Partículas

Pool C++ gerido pelo host, **não são processos**:

```cpp
struct Particle {
    float x, y, vx, vy;
    float life, max_life;
    float angle, spin;
    float size, size_end;
    Color col_start, col_end;
    int   graph;
};

struct ParticleSystem {
    Particle pool[MAX_PARTICLES]; // 4096
    int      count;
    int      region;
    float    z;
};
```

Native:
```zen
emit(x, y, count, angle, spread, speed, life, graph)
```

Tick: `life -= dt`, interpola cor/size, move, remove dead.  
Render: dentro do render loop por região, z-sorted.

---

## Câmara / Região API

```zen
// Definir região (chamado no host C++ antes do script, ou via native)
define_region(1, 0, 0, 640, 720)
define_region(2, 640, 0, 640, 720)

// Scroll (move câmara)
set_camera(region, x, y)
set_camera_zoom(region, zoom)

// Background layers
set_bg(region, layer=0, graph=ship_bg, scroll_x=0.3, scroll_y=0)
```

---

## Box2D (futuro — plugin)

- Plugin `zen_box2d.so` carregado via `vm.try_load_plugin("box2d")`
- Na criação do processo (`on_process_start`): cria `b2Body` se `PRIV_PHYSICS=1`
- No update: sync `b2Body position/angle` → `PRIV_X/Y/ANGLE`
- Colisão: callbacks Box2D → signal Zen via `vm.signal_process()`
- PRIV_MASS, PRIV_FRICTION, PRIV_RESTITUTION para configurar

---

## Fases de implementação

### Fase 1 — Integração VM + GfxEngine (próximo)
- [ ] `PRIV_REGION`, `PRIV_CTYPE` no enum
- [ ] `GfxEngine` struct no host com regions + GraphLibrary + QuadTree
- [ ] `on_process_start/update/end` ligados ao GfxEngine
- [ ] render loop: `render_region()` com `for_each_process`
- [ ] Natives: `load_graph`, `load_anim`, `set_camera`

### Fase 2 — Quad renderer + flip
- [ ] `render_clip()` com `make_transform()` (rlgl directo)
- [ ] `PRIV_FLIPX`, `PRIV_FLIPY`
- [ ] Anti-artifact UV stretch toggle

### Fase 3 — Backgrounds + Tilemap
- [ ] `BgLayer` por região, `set_bg()` native
- [ ] `Tilemap` struct + `load_tilemap()` (suporte .tmj / raw)
- [ ] `get_tile` / `set_tile` natives
- [ ] Tilemap collision native

### Fase 4 — Colisão completa
- [ ] `collision(type X)` native com QuadTree + narrowphase
- [ ] `collision_tile()` native
- [ ] Debug overlay (AABB + QT)

### Fase 5 — Partículas
- [ ] `ParticleSystem` pool C++
- [ ] `emit()` native
- [ ] Render dentro do region loop

### Fase 6 — Box2D plugin (futuro)
- [ ] Plugin API `zen_box2d`
- [ ] Sync privates ↔ b2Body
- [ ] Collision callbacks → signals
