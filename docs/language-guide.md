# BuLang / Zen Language Guide

BuLang, also referred to as Zen in the examples, is a lightweight scripting language with C-like blocks, dynamic values, functions, structs, classes, collections, fibers, and cooperative process execution.

The language is meant to stay small, readable, and practical for gameplay scripting and host-driven tools.

## Values

Variables are declared with `var` and can store different kinds of values:

```zen
var x = 10;
var y = 3.14;
var name = "BuLang";
var active = true;
var nothing = null;
```

## Comments

```zen
// single line comment

/*
   block comment
*/
```

## Strings and interpolation

Strings use double quotes. Expressions can be embedded with `{}`:

```zen
var points = 42;
print("points: {points}");
print("next: {points + 10}");
```

Verbatim strings use `@` and are useful for paths:

```zen
var path = @"C:\users\djoker\file.txt";
var quote = @"she said ""hello""";
```

## Functions

Functions are declared with `def`:

```zen
def add(a, b) {
    return a + b;
}

print(add(2, 3));
```

Functions can call other functions and can be recursive.

Optional type hints can be attached to parameters and return values when the type is a known struct or class:

```zen
struct Vec3 { x, y, z }

def add(a: Vec3, b: Vec3) : Vec3 {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
```

These hints are optional. They are currently used by the compiler to improve compile-time field resolution and generate more direct bytecode.

## Control flow

BuLang supports `if`, `else`, `while`, `for`, `loop`, `break`, `continue` and `do while`.

```zen
if (x > 10) {
    print("big");
} else {
    print("small");
}

for (var i = 0; i < 10; i = i + 1) {
    print(i);
}

loop {
    break;
}
```

Logical operators can use symbol form or word form:

```zen
if (alive && visible) { print("ok"); }
if (alive and visible) { print("ok"); }
if (not alive) { print("dead"); }
```

## Structs

Structs are lightweight records with named fields:

```zen
struct Vec2 { x, y }

var p = Vec2(10, 20);
print(p.x);
p.y = 50;
```

Use structs for simple data objects.

## Classes

Classes contain fields and methods. `init()` is the constructor and `self` references the current instance.

```zen
class Counter {
    var value;

    def init() {
        self.value = 0;
    }

    def inc() {
        self.value = self.value + 1;
    }
}

var c = Counter();
c.inc();
```

Methods can also use the same optional type hint syntax:

```zen
class Quat {
    var x, y, z, w;

    def __mul__(other: Quat) : Quat {
        return self;
    }
}
```

## Collections

Arrays:

```zen
var a = [1, 2, 3];
a.push(4);
print(a[0]);
```

Maps:

```zen
var m = {};
m.set("hp", 100);
print(m.get("hp"));
```

Sets:

```zen
var s = #{1, 2, 3};
s.add(4);
print(s.has(2));
```

## Processes and fibers

The examples include cooperative runtime concepts such as processes, `frame`, `spawn`, `yield` and `resume`.

This makes the language useful for game logic, scripted animations, AI behaviours and update loops.

```zen
process blink() {
    loop {
        print("tick");
        frame;
    }
}

blink();
advance_process();
```

## Bytecode

Zen source can be compiled to `.znb` bytecode and loaded later without shipping the original script source:

```bash
./bin/zen --dump game.znb game.zen
./bin/zen game.znb
```

Pure script features such as functions, closures, structs, classes, collections, and processes round-trip through the bytecode loader. Host-owned native objects still need to be registered by the embedding application.

## Core philosophy

BuLang should stay small, readable, and focused. It does not need to replace C++; it is a scripting layer that complements a C++ runtime.
