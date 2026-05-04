# Tutorial 06 — Classes

This tutorial shows classes with fields, methods, the `init` constructor, and instance access through `self`.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 06 — Classes
// ============================================================

// class has fields (var) and methods (def)
// init() is the constructor - called automatically when an instance is created
// self refers to the current instance

// --- Basic class ---
class Counter {
    var value;

    def init() {
        self.value = 0;
    }

    def increment() {
        self.value = self.value + 1;
    }

    def increment_by(n) {
        self.value = self.value + n;
    }

    def reset() {
        self.value = 0;
    }

    def get() {
        return self.value;
    }
}

var c = Counter();
c.increment();
c.increment();
c.increment_by(5);
print("counter: {c.get()}");    // 7
c.reset();
print("after reset: {c.get()}");  // 0

// --- Class with init parameters ---
class Vec2 {
    var x;
    var y;

    def init(x, y) {
        self.x = x;
        self.y = y;
    }

    def add(other) {
        return Vec2(self.x + other.x, self.y + other.y);
    }

    def scale(f) {
        return Vec2(self.x * f, self.y * f);
    }

    def length_sq() {
        // no native sqrt in this example - use squared distance
        return self.x * self.x + self.y * self.y;   // distance squared
    }

    def print_vec() {
        print("Vec2({self.x}, {self.y})");
    }
}

var a = Vec2(3, 4);
var b = Vec2(1, 2);
var sum = a.add(b);
sum.print_vec();                       // Vec2(4, 6)
a.scale(2.0).print_vec();              // Vec2(6, 8)
print("distance² of a: {a.length_sq()}");  // 25

// --- Class composition ---
class Timer {
    var duration;
    var remaining;
    var active;

    def init(duration) {
        self.duration = duration;
        self.remaining = duration;
        self.active = false;
    }

    def start() {
        self.active = true;
        self.remaining = self.duration;
    }

    def update(dt) {
        if (!self.active) { return false; }
        self.remaining = self.remaining - dt;
        if (self.remaining <= 0.0) {
            self.active = false;
            self.remaining = 0.0;
            return true;    // triggered
        }
        return false;
    }

    def progress() {
        return 1.0 - self.remaining / self.duration;
    }
}

class Weapon {
    var damage;
    var cooldown;

    def init(damage, cooldown) {
        self.damage = damage;
        self.cooldown = Timer(cooldown);
    }

    def fire() {
        if (!self.cooldown.active) {
            self.cooldown.start();
            print("BANG! damage={self.damage}");
            return true;
        }
        print("reloading... {int(self.cooldown.remaining * 10) / 10}s");
        return false;
    }

    def update(dt) {
        self.cooldown.update(dt);
    }
}

    var pistol = Weapon(25, 0.5);
    pistol.fire();          // BANG!
    pistol.fire();          // reloading...
    pistol.update(0.6);
    pistol.fire();          // BANG! (cooldown expired)
```

## How to run

```bash
zen examples/tutorial_06_classes.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_06_classes.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
