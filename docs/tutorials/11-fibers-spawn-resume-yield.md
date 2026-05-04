# Tutorial 11 — Fibers: `spawn` / `resume` / `yield`

This tutorial introduces fibers and explicit cooperative control flow through practical examples.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 11 — Fibers: spawn / resume / yield
// ============================================================

// Fibers are lightweight coroutines: functions that can be paused
// (yield) and resumed while keeping their internal state.
//
// spawn fn   -> creates a fiber from fn (does not run yet)
// resume(f)  -> runs the fiber until the next yield; returns the yielded value
// yield v    -> pauses the fiber and returns v to the caller
// When the fiber finishes, resume() returns nil.

// --- Basic fiber ---
def gen() {
    yield 1;
    yield 2;
    yield 3;
}

var f = spawn gen;
print(resume(f));   // 1
print(resume(f));   // 2
print(resume(f));   // 3
print(resume(f));   // nil  <- fiber finished

// --- Iterate a fiber with while ---
def count(max) {
    var i = 0;
    while (i < max) {
        yield i;
        i = i + 1;
    }
}

def make_range(n) {
    def gen() { count(n); }
    return gen;
}

var r = spawn make_range(5);
var v = resume(r);
while (v != nil) {
    print(v);       // 0 1 2 3 4
    v = resume(r);
}

// --- Bidirectional communication ---
// resume(f, value) sends a value into the fiber.
// yield receives that value as the result of an expression.

def echo_dobro() {
    var x = yield 0;     // pauses and receives a value from the next resume
    yield x * 2;
}

var e = spawn echo_dobro;
resume(e);           // starts until the first yield -> returns 0
print(resume(e, 7)); // sends 7 -> fiber yields 14 -> 14

// --- Accumulator (multiple sends) ---
def accumulator() {
    var sum = 0;
    loop {
        var v = yield sum;
        sum = sum + v;
    }
}

var ac = spawn accumulator;
resume(ac);             // initialize (yield 0)
print(resume(ac, 5));   // 5
print(resume(ac, 3));   // 8
print(resume(ac, 7));   // 15

// --- Fibonacci generator with a fiber ---
def fib_fiber() {
    var a = 0;
    var b = 1;
    loop {
        yield a;
        var t = a;
        a = b;
        b = t + b;
    }
}

var ff = spawn fib_fiber;
print(resume(ff));   // 0
print(resume(ff));   // 1
print(resume(ff));   // 1
print(resume(ff));   // 2
print(resume(ff));   // 3
print(resume(ff));   // 5
print(resume(ff));   // 8

// --- Multiple fibers in parallel ---
def make_seq(start, step) {
    def gen() {
        var n = start;
        loop {
            yield n;
            n = n + step;
        }
    }
    return gen;
}

var fa = spawn make_seq(0, 1);
var fb = spawn make_seq(100, 10);

// interleave results
print(resume(fa));   // 0
print(resume(fb));   // 100
print(resume(fa));   // 1
print(resume(fb));   // 110
print(resume(fa));   // 2
print(resume(fb));   // 120

// --- Fiber with even numbers (conditional yield) ---
def evens(max) {
    def gen() {
        var i = 0;
        while (i < max) {
            if (i % 2 == 0) { yield i; }
            i = i + 1;
        }
    }
    return gen;
}

var fp = spawn evens(10);
var vp = resume(fp);
while (vp != nil) {
    print(vp);   // 0 2 4 6 8
    vp = resume(fp);
}

// --- Fiber with a closure capturing an upvalue ---
def make_upvalue_gen() {
    var x = 0;
    def gen() {
        loop {
            x = x + 1;
            yield x;
        }
    }
    return gen;
}

var ug = make_upvalue_gen();
var fu = spawn ug;
print(resume(fu));   // 1
print(resume(fu));   // 2
print(resume(fu));   // 3

// --- Spawn with an anonymous function ---
var fanon = spawn def() { yield 10; yield 20; };
print(resume(fanon));   // 10
print(resume(fanon));   // 20
print(resume(fanon));   // nil
```

## How to run

```bash
zen examples/tutorial_11_fibers.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_11_fibers.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
