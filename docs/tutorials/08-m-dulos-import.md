# Tutorial 08 — Modules (`import`)

This tutorial introduces module imports through practical examples.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 08 — Modules (import)
// ============================================================

import math;

// --- math.random ---

// integer between 0 and N-1
var die = math.random(6) + 1;
print("die: {die}");

// integer in a [min, max] range
var x = math.random(100, 200);
print("x between 100 and 200: {x}");

// coin flip simulation
var heads_or_tails = math.random(2);   // 0 or 1
if (heads_or_tails == 0) {
    print("heads");
} else {
    print("tails");
}

// --- Random velocities ---
def random_velocity(mag) {
    var vx = math.random(mag * 2) - mag;
    var vy = math.random(mag * 2) - mag;
    return vx;   // simplified - Zen has no multi-return here
}

var i = 0;
while (i < 5) {
    var vx = math.random(10) - 5;
    var vy = math.random(10) - 10;
    print("particle {i}: vx={vx} vy={vy}");
    i = i + 1;
}

// --- Normalized scale ---
// Convert a random int into a float in [0.0, 1.0)
var r_float = math.random(1000) / 1000.0;
print("random float: {r_float}");

// Float num intervalo [lo, hi)
def random_float(lo, hi) {
    return lo + math.random(10000) / 10000.0 * (hi - lo);
}

print("between 2.0 and 5.0: {random_float(2.0, 5.0)}");

// --- Practical example: generate a point cloud ---
import math;

struct Point { x, y }

var cloud = [];
var j = 0;
while (j < 10) {
    var px = math.random(1280);
    var py = math.random(720);
    cloud.push(Point(px, py));
    j = j + 1;
}

print("generated {cloud.len()} points:");
var k = 0;
while (k < cloud.len()) {
    var p = cloud[k];
    print("  ({p.x}, {p.y})");
    k = k + 1;
}
```

## How to run

```bash
zen examples/tutorial_08_modulos.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_08_modulos.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
