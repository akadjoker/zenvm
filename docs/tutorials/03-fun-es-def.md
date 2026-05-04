# Tutorial 03 — Functions (`def`)

This tutorial explains functions declared with `def`, parameters, `return`, composition, and recursion.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 03 — Functions (def)
// ============================================================

// --- Basic definition ---
def greet() {
    print("Hello, World!");
}

greet();

// --- With parameters ---
def add(a, b) {
    return a + b;
}

var result = add(3, 7);
print("3 + 7 = {result}");

// --- With multiple return paths ---
def max_value(a, b) {
    if (a > b) { return a; }
    return b;
}

print("max(10, 25) = {max_value(10, 25)}");
print("max(99, 1)  = {max_value(99, 1)}");

// --- Helper functions / composition ---
def clamp(v, lo, hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

def normalize(v, min_v, max_v) {
    var c = clamp(v, min_v, max_v);
    return (c - min_v) / (max_v - min_v);
}

print(normalize(50, 0, 100));   // 0.5
print(normalize(-10, 0, 100));  // 0.0  (clamped)
print(normalize(120, 0, 100));  // 1.0  (clamped)

// --- Recursive functions ---
def factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

print("5! = {factorial(5)}");    // 120
print("10! = {factorial(10)}");  // 3628800

// --- Functions used by gameplay logic ---
def hit(mx, my, x, y, w, h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

var insideRectangle = hit(50, 50, 10, 10, 100, 100);
print("point inside? {insideRectangle}");  // true
```

## How to run

```bash
zen examples/tutorial_03_funcoes.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_03_funcoes.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
