# Tutorial 15 — Math Built-ins, Bitwise Operators, and `do while`

This tutorial shows math built-ins, bitwise operators, and the `do while` loop.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 15 — Math Built-ins, Bitwise Operators, and do-while
// ============================================================

// ==========================================================
// MATH BUILT-INS (no import needed - built into the core)
// ==========================================================

// --- Trigonometry ---
print(sin(0));         // 0
print(cos(0));         // 1
print(tan(0));         // 0

// atan2(y, x) -> angle in radians
print(atan2(1, 1));    // ~0.785398 (π/4)

// --- Angle conversion ---
print(rad(180));       // ~3.14159  (degrees -> radians)
print(deg(3.14159));   // ~180      (radians -> degrees)

// --- Power and square root ---
print(sqrt(16));       // 4
print(sqrt(2));        // ~1.41421
print(pow(2, 10));     // 1024
print(pow(3, 3));      // 27

// --- Absolute value ---
print(abs(-99));       // 99
print(abs(5));         // 5

// --- Rounding ---
print(floor(2.9));     // 2
print(ceil(2.1));      // 3

// --- Logarithm / exponential ---
print(log(1));         // 0
print(exp(0));         // 1

// --- Time (clock returns a float in seconds) ---
var t0 = clock();
// ... some operation ...
var t1 = clock();
print(t1 > t0);        // true  (time passed)

// --- Practical use: normalized direction ---
def normalize(dx, dy) {
    var l = sqrt(dx * dx + dy * dy);
    if (l == 0) { return [0, 0]; }
    return [dx / l, dy / l];
}

var dir = normalize(3, 4);
print(dir[0]);   // 0.6
print(dir[1]);   // 0.8

// --- Distance between two points ---
def distance(x1, y1, x2, y2) {
    var dx = x2 - x1;
    var dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

print(distance(0, 0, 3, 4));   // 5

// --- Angle between two points ---
def angle(x1, y1, x2, y2) {
    return atan2(y2 - y1, x2 - x1);
}

var ang = angle(0, 0, 1, 0);
print(deg(ang));   // 0  (pointing right)

// ==========================================================
// OPERADORES BITWISE
// ==========================================================

// AND - both bits set to 1
print(0xFF & 0x0F);    // 15   (0000 1111)

// OR - at least one bit set to 1
print(0xF0 | 0x0F);    // 255  (1111 1111)

// XOR - different bits
print(0xFF ^ 0x0F);    // 240  (1111 0000)

// NOT - invert every bit
print(~0);             // -1
print(~1);             // -2

// Shift left
print(1 << 4);         // 16
print(1 << 8);         // 256

// Arithmetic shift right
print(16 >> 2);        // 4
print(-1 >> 1);        // -1  (arithmetic: preserves sign)

// --- Practical use: bit flags ---
var FLAG_ACTIVE   = 1 << 0;   // 0001
var FLAG_VISIBLE  = 1 << 1;   // 0010
var FLAG_COLLIDE  = 1 << 2;   // 0100
var FLAG_ENEMY    = 1 << 3;   // 1000

var state = 0;
state = state | FLAG_ACTIVE;
state = state | FLAG_VISIBLE;
state = state | FLAG_ENEMY;

print(state & FLAG_ACTIVE  != 0);   // true
print(state & FLAG_COLLIDE != 0);   // false

// Remove a flag
state = state & ~FLAG_VISIBLE;
print(state & FLAG_VISIBLE != 0);   // false

// Toggle a flag
state = state ^ FLAG_COLLIDE;
print(state & FLAG_COLLIDE != 0);   // true

// --- Extract color channels from packed RGBA ---
var rgba_color = 0xFF8040E0;
var r = (rgba_color >> 24) & 0xFF;
var g = (rgba_color >> 16) & 0xFF;
var b = (rgba_color >>  8) & 0xFF;
var a =  rgba_color       & 0xFF;
print(r);   // 255
print(g);   // 128
print(b);   // 64
print(a);   // 224

// ==========================================================
// DO-WHILE
// ==========================================================

// Always executes at least once before checking the condition

var x = 0;
do {
    x = x + 1;
} while (x < 3);
print(x);   // 3

// With a false condition from the start: still executes once
var y = 10;
do {
    y = y + 1;
} while (false);
print(y);   // 11  <- executed once

// --- Input read pattern (classic do-while use) ---
// Simulates: "ask at least once, repeat if invalid"
var attempts = 0;
var value = -1;
do {
    attempts = attempts + 1;
    value = attempts * 3;   // simulate input
} while (value < 10);
print("attempts: {attempts}");   // 4 (3,6,9 fail; 12 ok)

// --- Nested do-while ---
var outer = 0;
do {
    outer = outer + 1;
    var inner = 0;
    do {
        inner = inner + 1;
        if (inner == 3) { break; }
    } while (inner < 10);
    print("{outer} {inner}");   // 1 3 / 2 3 / 3 3
} while (outer < 3);
```

## How to run

```bash
zen examples/tutorial_15_math_bitwise.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_15_math_bitwise.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
