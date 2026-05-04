# Tutorial 12 — Typed Buffers

This tutorial introduces typed buffers through practical examples.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 12 — Typed Buffers
// ============================================================

// Typed buffers are fixed-size arrays with a numeric element type.
// They are allocated in one go, provide O(1) access, and avoid
// boxing their values - ideal for low-level data such as geometry,
// pixels, audio samples, and similar workloads.
//
// Available types:
//   Int8Array    Uint8Array
//   Int16Array   Uint16Array
//   Int32Array   Uint32Array
//   Float32Array Float64Array

// --- Creation ---
var ai = Int32Array(4);          // 4 zeroed integers
var af = Float64Array(3);        // 3 zeroed doubles
var ab = Uint8Array([65,66,67]); // initialize from an array

// --- Read and write ---
ai[0] = 10;
ai[1] = 20;
ai[2] = 30;
ai[3] = 40;
print(ai[0]);   // 10
print(ai[3]);   // 40

// --- len() and byte_len() ---
print(ai.len());       // 4
print(ai.byte_len());  // 16  (4 × 4 bytes)

var fb = Float64Array(4);
print(fb.byte_len());  // 32  (4 × 8 bytes)

// --- fill() - fills every element ---
var buf = Int32Array(5);
buf.fill(99);
print(buf[0]);   // 99
print(buf[4]);   // 99

// --- Truncation and overflow ---
var ti = Int32Array(1);
ti[0] = 3.9;
print(ti[0]);   // 3  (float truncated to int)

var tu = Uint8Array(1);
tu[0] = 256;
print(tu[0]);   // 0  (overflow: 256 % 256)

tu[0] = 255;
print(tu[0]);   // 255

// --- Uint8Array as a byte buffer ---
var bytes = Uint8Array([65, 66, 67]);  // 'A', 'B', 'C'
print(bytes[0]);   // 65
print(bytes[1]);   // 66
print(bytes[2]);   // 67

// --- Float32Array for graphics ---
var verts = Float32Array(6);   // 2 triangles × 3 coords
verts[0] = 0.0;  verts[1] = 0.5;   // top
verts[2] = -0.5; verts[3] = -0.5;  // left
verts[4] = 0.5;  verts[5] = -0.5;  // right
print(verts[1]);   // 0.5
print(verts[4]);   // 0.5

// --- Writing in a loop ---
var table = Int32Array(10);
var i = 0;
while (i < 10) {
    table[i] = i * i;
    i = i + 1;
}
print(table[5]);   // 25
print(table[9]);   // 81

// --- Performance comparison: buffer vs normal array ---
// For 1 million writes, Int32Array is much faster
// than a dynamic array because there is no boxing and less GC pressure.

var large = Int32Array(1000000);
var j = 0;
while (j < 1000000) {
    large[j] = j;
    j = j + 1;
}
print(large[999999]);   // 999999

// --- len() as a global function also works ---
var buf2 = Uint8Array(7);
print(len(buf2));   // 7

// --- Signed vs unsigned types ---
var s8  = Int8Array(1);
s8[0] = -50;
print(s8[0]);   // -50

var u16 = Uint16Array(1);
u16[0] = 65535;
print(u16[0]);   // 65535

var s16 = Int16Array(1);
s16[0] = -1000;
print(s16[0]);   // -1000

var u32 = Uint32Array(1);
u32[0] = 4000000000;
print(u32[0]);   // 4000000000
```

## How to run

```bash
zen examples/tutorial_12_buffers.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_12_buffers.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
