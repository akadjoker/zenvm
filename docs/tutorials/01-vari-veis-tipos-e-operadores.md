# Tutorial 01 — Variables, Types, and Operators

This tutorial introduces variables, primitive values, arithmetic, comparisons, logic, compound assignment, conversions, and string interpolation.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 01 — Variables, Types, and Operators
// ============================================================

// --- Variable declarations ---
var x = 10;
var y = 3.14;
var name = "BuLang";
var active = true;
var nothing = null;

print(x);        // 10
print(y);        // 3.14
print(name);     // BuLang
print(active);   // true

// --- Arithmetic operators ---
var a = 10;
var b = 3;

print(a + b);    // 13
print(a - b);    // 7
print(a * b);    // 30
print(a / b);    // 3.333...
print(a % b);    // 1  (remainder)

// --- Comparison operators ---
print(a == b);   // false
print(a != b);   // true
print(a > b);    // true
print(a <= b);   // false

// --- Logical operators ---
var p = true;
var q = false;

print(p && q);   // false  (also: p and q)
print(p || q);   // true   (also: p or q)
print(!p);       // false  (also: not p)

// --- Compound assignment ---
var n = 10;
n += 5;   print(n);   // 15
n -= 3;   print(n);   // 12
n *= 2;   print(n);   // 24

// --- Conversions ---
var f = 9.99;
print(int(f));   // 9  (truncates to integer)

// --- String interpolation ---
var points = 42;
print("Your score is: {points}");
print("Sum: {a + b}");
```

## How to run

```bash
zen examples/tutorial_01_variaveis.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_01_variaveis.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
