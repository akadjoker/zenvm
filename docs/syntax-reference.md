# BuLang / Zen Syntax Reference

## File extension

The examples use `.zen`. You may also use `.bu` if your VM/tool accepts it.

## Variables

```zen
var name = value;
```

Examples:

```zen
var x = 10;
var name = "Player";
var alive = true;
var target = null;
```

## Primitive values

```zen
10          // integer
3.14        // float
"text"      // string
true        // boolean
false       // boolean
null        // null value
```

## Operators

Arithmetic:

```zen
+  -  *  /  %
```

Comparison:

```zen
==  !=  >  >=  <  <=
```

Logical:

```zen
&&  ||  !
and or not
```

Assignment:

```zen
=  +=  -=  *=  /=  %=
```

Bitwise:

```zen
&  |  ^  ~  <<  >>
```

## Functions

```zen
def name(a, b) {
    return a + b;
}
```

## If / else

```zen
if (condition) {
    // body
} else {
    // body
}
```

## While

```zen
while (condition) {
    // body
}
```

## For

```zen
for (var i = 0; i < 10; i = i + 1) {
    print(i);
}
```

## Infinite loop

```zen
loop {
    if (done) {
        break;
    }
}
```

## Do while

```zen
do {
    x = x + 1;
} while (x < 3);
```

## Arrays

```zen
var a = [];
var b = [1, 2, 3];

b.push(4);
print(b[0]);
b[1] = 20;
```

## Structs

```zen
struct Point { x, y }

var p = Point(10, 20);
print(p.x);
p.x = 50;
```

## Classes

```zen
class Player {
    var hp;

    def init(hp) {
        self.hp = hp;
    }

    def damage(v) {
        self.hp = self.hp - v;
    }
}
```

## Maps

```zen
var m = {};
m.set("name", "Djoker");
print(m.get("name"));
```

## Sets

```zen
var s = #{1, 2, 3};
s.add(4);
print(s.has(1));
```

## Processes

```zen
process name() {
    loop {
        frame;
    }
}
```

## Modules

```zen
import math;
```

The exact module system depends on your VM implementation, but the tutorial examples show `import` support.
