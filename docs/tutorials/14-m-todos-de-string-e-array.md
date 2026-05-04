# Tutorial 14 — String and Array Methods

This tutorial documents useful string and array methods.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 14 — String and Array Methods
// ============================================================

// ==========================================================
// STRING
// ==========================================================

var s = "Hello World";

// --- Length ---
print(s.len());          // 11
print(len(s));           // 11  (equivalent global function)

// --- Case ---
print(s.upper());        // HELLO WORLD
print(s.lower());        // hello world

// --- Substring: sub(start, end) ---
print(s.sub(0, 5));      // Hello   (end index is exclusive)
print(s.sub(6, 11));     // World

// --- Search: find(pattern) -> index or -1 ---
print(s.find("World"));  // 6
print(s.find("xyz"));    // -1

// --- Replace: replace(old, new) ---
print(s.replace("World", "Zen"));   // Hello Zen

// --- Prefix / suffix ---
print(s.starts_with("Hello"));   // true
print(s.starts_with("World"));   // false
print(s.ends_with("World"));     // true
print(s.ends_with("Hello"));     // false

// --- Trim (remove whitespace at the ends) ---
var sp = "   spaces   ";
print(sp.trim());   // spaces

// --- Character by index ---
print(s.char_at(0));   // H
print(s.char_at(6));   // W

// --- Split ---
var csv = "one,two,three,four";
var parts = csv.split(",");
print(parts.len());    // 4
print(parts[0]);       // one
print(parts[3]);       // four

// split with no match -> array with 1 element
var sem = "hello".split(",");
print(sem.len());       // 1
print(sem[0]);          // hello

// --- Concatenation with + ---
var name = "Bu" + "Lang";
print(name);            // BuLang

// --- Comparison ---
print("abc" == "abc");   // true
print("abc" < "abd");    // true  (lexicographic)

// --- Verbatim strings (@ - no escaping) ---
var path = @"C:\users\djoker\file.txt";
print(path);   // C:\users\djoker\file.txt

// double quotes inside verbatim strings: ""
var q = @"she said ""hello""";
print(q);      // she said "hello"

// ==========================================================
// ARRAY  (extra methods beyond push/pop/len)
// ==========================================================

var arr = [3, 1, 4, 1, 5, 9, 2, 6];

// --- contains / index_of ---
print(arr.contains(5));       // true
print(arr.contains(99));      // false
print(arr.index_of(4));       // 2
print(arr.index_of(99));      // -1

// --- reverse (in-place) ---
var r = [1, 2, 3, 4, 5];
r.reverse();
print(r.join(","));   // 5,4,3,2,1

// --- slice(start, end) -> new array ---
var base = [10, 20, 30, 40, 50];
var sl = base.slice(1, 3);
print(sl.join(","));   // 20,30

// --- insert(index, value) ---
var ins = [1, 3, 4];
ins.insert(1, 2);
print(ins.join(","));   // 1,2,3,4

// --- remove(index) ---
var rem = [1, 2, 3, 4];
rem.remove(2);
print(rem.join(","));   // 1,2,4

// --- clear ---
var cl = [1, 2, 3];
cl.clear();
print(cl.len());   // 0

// --- join(separator) ---
var nums = [1, 2, 3, 4, 5];
print(nums.join(", "));   // 1, 2, 3, 4, 5
print(nums.join("-"));    // 1-2-3-4-5

// --- sort() ---
var desordenado = [3, 1, 4, 1, 5, 9, 2, 6];
desordenado.sort();
print(desordenado.join(","));   // 1,1,2,3,4,5,6,9

desordenado.sort("desc");
print(desordenado.join(","));   // 9,6,5,4,3,2,1,1

// string sort
var fruits = ["banana", "apple", "avocado", "cherry"];
fruits.sort();
print(fruits.join(", "));   // apple, avocado, banana, cherry

// --- Method chaining ---
print("Hello World".lower().len());   // 11

var sub = [10, 30, 20, 50, 40].slice(1, 4);
sub.sort();
print(sub.join(","));   // 20,30,50
```

## How to run

```bash
zen examples/tutorial_14_metodos.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_14_metodos.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
