# BuLang / Zen Language Guide

BuLang, also named Zen in the script examples, is a lightweight scripting language with C-like blocks, dynamic values, functions, structs, classes, collections and cooperative execution features.

The language is intended to be simple to read and practical for game-engine scripting.

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
var quote = @"ela disse ""olá""";
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

## Core philosophy

BuLang should stay small, readable and focused. It does not need to replace C++; it can act as a gameplay scripting layer on top of a C++ engine.
