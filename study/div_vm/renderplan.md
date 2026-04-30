# Render Plan for DIV VM

## Goal

Recreate the visual semantics of DIV using a modern renderer such as Raylib or SDL, without copying the old software-renderer implementation details.

The plan is to preserve:

- regions as screen clipping/viewports
- scroll and parallax semantics
- camera-follow behavior
- draw ordering by `z`
- compatibility with multiple visible regions/scrolls

## Core Principle

Do not port the original VGA buffer logic line by line.

Port the semantics instead:

- old DIV software buffers -> modern draw commands
- clipping rectangles -> scissor/clip rects
- manual painter selection by `z` -> sorted render queue
- process-follow camera -> logical camera state per scroll

## DIV Semantics to Preserve

### Region

In DIV, a region is a rectangular clipping area in screen coordinates.

Equivalent modern representation:

```cpp
struct RegionState {
    int id;
    int x;
    int y;
    int width;
    int height;
};
```

Raylib mapping:

- `BeginScissorMode(x, y, width, height)`
- `EndScissorMode()`

SDL mapping:

- `SDL_RenderSetClipRect()`

### Scroll

In DIV, each scroll has:

- `z`
- `camera`
- `ratio`
- `speed`
- `region1`
- `region2`
- manual positions for foreground/background planes

Equivalent modern representation:

```cpp
struct ScrollState {
    int id;
    int z;
    int camera_pid;
    int ratio;
    int speed;
    int region1;
    int region2;
    float x0;
    float y0;
    float x1;
    float y1;
    bool enabled;
};
```

### Camera

The DIV scroll camera is not an abstract camera object. It is the process id of the process being followed.

That means the renderer should resolve camera position from process state each frame.

### Z ordering

DIV paints elements by `z`, with larger `z` painted first.

That means smaller `z` ends up visually on top.

The original engine selects the next highest `z` repeatedly. In the new renderer we should sort once per frame.

## Modern Rendering Model

### Draw Command

```cpp
enum class DrawType {
    Process,
    Mode7,
    Scroll,
    Text,
    Mouse,
    Drawing,
};

struct DrawCommand {
    int z;
    int type_order;
    int region_id;
    int scroll_id;
    int process_id;
    DrawType type;
};
```

`type_order` is needed to reproduce DIV tie-breaking behavior between categories with equal `z`.

### Frame Pipeline

1. Update process logic.
2. Update all scrolls from their `camera_pid`.
3. Build a render command list.
4. Sort commands by `z` descending and `type_order` descending.
5. Execute commands by region using scissor clipping.

## Region Strategy

Regions should be treated as independent clipped render passes.

Example:

```cpp
for (const RegionState& region : regions) {
    BeginScissorMode(region.x, region.y, region.width, region.height);

    for (const DrawCommand& cmd : commands_for_region[region.id]) {
        execute_draw_command(cmd);
    }

    EndScissorMode();
}
```

This supports:

- split screen
- HUD windows
- multiple clipped play areas
- multiple views of the same world if desired

## Scroll Update Logic

The update should follow the original DIV behavior from `mover_scroll()`:

- if `camera_pid != 0`, follow that process automatically
- `region1` acts like a lock/dead zone
- `region2` acts like an outer zone / unconstrained zone
- `speed` limits how fast the camera moves
- `ratio` affects parallax background movement

Pseudo-flow:

```cpp
if (scroll.camera_pid != 0) {
    target = process_position(scroll.camera_pid);
    delta = compute_follow_delta(target, scroll, region1, region2, speed);
    scroll.x0 += delta.x;
    scroll.y0 += delta.y;
    scroll.x1 += delta.x * 100 / ratio;
    scroll.y1 += delta.y * 100 / ratio;
} else {
    // manual mode
}
```

## Drawing Scrolls

### Simple Scroll

For simple scroll:

1. clip to the region
2. draw the map/layer with offset `(x0, y0)`
3. draw the sprites visible in that scroll

### Parallax Scroll

For parallax scroll:

1. clip to the region
2. draw back layer using `(x1, y1)`
3. draw front layer using `(x0, y0)`
4. draw sprites on top

No software circular buffers are required in a modern renderer.

## Collision and Regions

Do not treat regions as the main collision system.

Use this split:

- `region`: visual clipping and `out_region()` style screen tests
- collision: separate system based on geometry or optional masks

Recommended collision progression:

1. AABB first
2. optional circle/OBB if needed
3. pixel-perfect only when explicitly necessary

If DIV-compatible behavior is desired for multi-scroll objects:

- allow rendering in multiple scrolls/regions
- but collision may use only the first active scroll, matching original behavior

## Sorting Strategy

Use `std::sort`, not repeated max-search.

Example comparator:

```cpp
std::sort(commands.begin(), commands.end(),
    [](const DrawCommand& a, const DrawCommand& b) {
        if (a.z != b.z) return a.z > b.z;
        return a.type_order > b.type_order;
    });
```

This is simpler and more scalable than the original painter loop.

## Recommended Raylib Mapping

- region clipping: `BeginScissorMode`
- textures/sprites: `DrawTexturePro`
- tilemaps/backgrounds: custom draw loop with camera offset
- camera math: custom logic or `Camera2D` where convenient
- batching: command queue sorted once per frame

## Recommended Implementation Order

1. Add `RegionState` and `ScrollState` data structures.
2. Implement `define_region()` semantics.
3. Implement `mover_scroll()` semantics in modernized form.
4. Implement draw command queue with `z` sorting.
5. Add simple scroll rendering.
6. Add parallax rendering.
7. Add `out_region()` semantics.
8. Add collision system separately.

## Non-Goals

At this stage, do not attempt to recreate:

- VGA memory layout
- line-by-line software copying
- old software renderer optimizations tied to DOS hardware

These are implementation artifacts, not the essential behavior we want to preserve.