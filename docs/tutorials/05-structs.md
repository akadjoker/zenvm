# Tutorial 05 — Structs

This tutorial shows structs as lightweight named-field records, useful for points, colors, rectangles, and simple data containers.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 05 — Structs
// ============================================================

// struct is a lightweight data type with named fields
// It does not have methods - for that use class (see tutorial 06)

// --- Definition ---
struct Point  { x, y }
struct Color  { r, g, b, a }
struct Rect   { x, y, w, h }

// --- Creating instances ---
var p = Point(100, 200);
print("p.x = {p.x}");   // 100
print("p.y = {p.y}");   // 200

var c = Color(255, 128, 0, 255);
print("color: ({c.r}, {c.g}, {c.b}, {c.a})");

// --- Field mutation ---
p.x = 50;
p.y = 75;
print("p moved: ({p.x}, {p.y})");

c.r = 0;
c.b += 100;
print("changed color: ({c.r}, {c.g}, {c.b})");

// --- Structs in functions ---
def move_point(pt, dx, dy) {
    pt.x = pt.x + dx;
    pt.y = pt.y + dy;
}

var pos = Point(0, 0);
move_point(pos, 10, 5);
print("pos after move: ({pos.x}, {pos.y})");

// --- Structs in arrays ---
var points = [];
points.push(Point(0, 0));
points.push(Point(50, 100));
points.push(Point(200, 150));

var i = 0;
while (i < points.len()) {
    var pt = points[i];
    print("point {i}: ({pt.x}, {pt.y})");
    i = i + 1;
}

// --- Struct returned from a function ---
def make_centered_rect(cx, cy, w, h) {
    return Rect(cx - w / 2, cy - h / 2, w, h);
}

var r = make_centered_rect(320, 240, 100, 60);
print("rect: ({r.x}, {r.y}) {r.w}x{r.h}");

// --- AABB collision with structs ---
def aabb(a, b) {
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

var rA = Rect(10, 10, 50, 50);
var rB = Rect(40, 40, 50, 50);
var rC = Rect(200, 200, 30, 30);

print("A collides with B? {aabb(rA, rB)}");   // true
print("A collides with C? {aabb(rA, rC)}");   // false
```

## How to run

```bash
zen examples/tutorial_05_structs.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_05_structs.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
