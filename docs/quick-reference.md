# BuLang Quick Reference

```zen
// variables
var x = 10;
var name = "Player";
var ok = true;
var none = null;

// function
def add(a, b) {
    return a + b;
}

// condition
if (x > 5) {
    print("big");
} else {
    print("small");
}

// loop
for (var i = 0; i < 10; i = i + 1) {
    print(i);
}

// array
var a = [1, 2, 3];
a.push(4);

// struct
struct Vec2 { x, y }
var p = Vec2(10, 20);

// class
class Counter {
    var value;
    def init() { self.value = 0; }
    def inc() { self.value = self.value + 1; }
}

// map
var m = {};
m.set("hp", 100);

// set
var s = #{1, 2, 3};
s.add(4);
```
