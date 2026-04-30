# Graphics Notes from Original DIV

## Purpose

This document captures the original DIV graphics semantics that are useful for a modern reimplementation.

It is not a copy of the old renderer internals. It is a summary of how the original engine behaves.

## Regions

DIV regions are rectangular clipping areas in screen coordinates.

Original structure:

```cpp
typedef struct _t_region {
  int x0,x1;
  int y0,y1;
} t_region;
```

Key points:

- regions are screen-space rectangles
- they are used for clipping and visibility checks
- they are not the main collision system

Related original sources:

- `study/div_original_core/inter.h`
- `src/div32run/f.cpp::define_region()`
- `src/div32run/c.cpp::out_region()`

## Scroll

Original DIV scroll structure:

```cpp
struct _scroll {
  int z;
  int camera;
  int ratio;
  int speed;
  int region1;
  int region2;
  int x0;
  int y0;
  int x1;
  int y1;
};
```

Key points:

- `camera` is the process id followed by the scroll
- `ratio` controls parallax movement of the background plane
- `speed` limits scroll movement speed
- `region1` acts as a lock/dead zone
- `region2` acts as an outer zone or unrestricted zone
- `x0/y0` are the foreground plane coordinates
- `x1/y1` are the secondary plane coordinates for parallax/manual positioning

Related original sources:

- `study/div_original_core/inter.h`
- `study/div_original_core/ltobj.def`
- `src/div32run/s.cpp::mover_scroll()`

## Camera

In the 2D scroll model, the camera is not a standalone entity.

It is the followed process id stored in the scroll.

That means camera motion is derived from process position and rules in `mover_scroll()`.

For Mode7-like behavior, camera becomes more geometric and uses:

- followed process id
- camera height
- distance
- horizon
- angle

Related original sources:

- `study/div_original_core/ltobj.def`
- `src/div32run/s.cpp::pinta_m7()`

## Z Ordering

DIV uses `z` as draw priority.

Important behavior:

- larger `z` is painted first
- smaller `z` ends up visually in front

The original engine does not sort once globally.
Instead it repeatedly selects the next unpainted highest-`z` element.

Categories participating in the same painter pass:

- process sprites
- scrolls
- mode7
- text
- mouse
- drawings

Tie-breaking is not neutral. Category order matters when `z` is equal.

Related original sources:

- `src/div32run/inter.h::_Z`
- `src/div32run/i.cpp` painter loop

## Ctype and Cnumber

Important process-side fields:

- `ctype=0`: screen-space object
- `ctype=1`: scroll-space object
- `ctype=2`: mode7-space object
- `cnumber`: bitmask of scrolls/mode7 instances where the object is visible

This means an object may be visible in multiple scrolls.

However, collision handling in the original engine is more limited than render visibility.

## Out_region

`out_region(id, region)` checks whether a process graphic is outside a screen region.

Behavior:

- for screen-space objects, it tests sprite bounds directly against the region rectangle
- for scroll-space objects, it first projects the object into the scroll viewport and then tests the region

This is a region/visibility test, not a general-purpose collision routine.

## Collision

Original DIV collision is separate from regions.

Behavior:

- build a clipped bounding area for the current sprite
- allocate a temporary collision buffer
- rasterize the sprite mask into that buffer
- compare against other processes

Important limitation from the original engine:

- objects visible in multiple scrolls only collide in the first active scroll considered by the runtime

This means render visibility and collision scope are not fully symmetric.

Related original source:

- `src/div32run/c.cpp::collision()`

## What Should Be Preserved in a Modern Reimplementation

Preserve:

- region as rectangular clipping/viewports
- scroll follow semantics
- `region1` and `region2` camera behavior
- `ratio` parallax semantics
- `z` ordering semantics
- object visibility masks via `cnumber`

Do not preserve literally:

- VGA software copy loops
- circular framebuffer internals
- software renderer-specific memory tricks

## Recommended Modern Interpretation

- regions -> scissor rectangles
- scrolls -> logical cameras with follow rules
- painter loop -> sorted draw command queue
- collision -> separate geometric or mask-based system

This preserves behavior while avoiding dependence on legacy renderer internals.