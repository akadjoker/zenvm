# Tutorial 10 — Closures and Upvalues

This tutorial introduces closures and captured variables through practical examples.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 10 — Closures and Upvalues
// ============================================================

// A closure is a function that "captures" variables from
// the scope where it was defined. In Zen, def inside def
// creates a closure automatically.

// --- Basic closure ---
def make_getter() {
    var x = 42;
    def get() { return x; }   // get captures x
    return get;
}

var g = make_getter();
print(g());   // 42 - x still exists inside the closure

// --- Closure modifies the captured variable (upvalue) ---
def make_counter() {
    var n = 0;
    def inc() {
        n = n + 1;
        return n;
    }
    return inc;
}

var c = make_counter();
print(c());   // 1
print(c());   // 2
print(c());   // 3

// --- Independent instances ---
// Each call to make_counter creates its own n
var a = make_counter();
var b = make_counter();
print(a());   // 1
print(a());   // 2
print(b());   // 1  <- independent from a
print(a());   // 3

// --- Closure captures a parameter ---
def make_adder(n) {
    def add(x) { return x + n; }
    return add;
}

var add10 = make_adder(10);
var add5  = make_adder(5);
print(add10(3));   // 13
print(add5(3));    // 8

// --- Closures share the same upvalue ---
def make_pair() {
    var x = 0;
    def inc() { x = x + 1; }
    def get() { return x; }
    inc(); inc(); inc();
    return get;
}

var get = make_pair();
print(get());   // 3

// --- Nested closures (3 levels) ---
def level_a() {
    var x = 100;
    def level_b() {
        def level_c() { return x; }   // captures x from a
        return level_c;
    }
    return level_b;
}

var b = level_a();
var c2 = b();
print(c2());   // 100

// --- Higher-order functions ---
def apply(f, x) { return f(x); }

var double = make_adder(0);   // add(x) = x+0 ... better:
def make_mult(n) {
    def mult(x) { return x * n; }
    return mult;
}

var triple = make_mult(3);
print(apply(triple, 7));    // 21
print(apply(triple, 10));   // 30

// --- Fibonacci generator with a closure ---
def fib_gen() {
    var a = 0;
    var b = 1;
    def next() {
        var t = a;
        a = b;
        b = t + b;
        return t;
    }
    return next;
}

var fib = fib_gen();
print(fib());   // 0
print(fib());   // 1
print(fib());   // 1
print(fib());   // 2
print(fib());   // 3
print(fib());   // 5
print(fib());   // 8

// --- Anonymous function (unnamed def) ---
var sq = def(n) { return n * n; };
print(sq(5));    // 25
print(sq(12));   // 144
```

## How to run

```bash
zen examples/tutorial_10_closures.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_10_closures.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
