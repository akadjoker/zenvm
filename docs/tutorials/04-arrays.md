# Tutorial 04 — Arrays

This tutorial covers array creation, `push`, `pop`, indexed access, iteration, and practical patterns such as fast removal.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 04 — Arrays
// ============================================================

// --- Creation ---
var list = [];
var numbers = [1, 2, 3, 4, 5];

// --- push / pop / len ---
list.push(10);
list.push(20);
list.push(30);
print("size: {list.len()}");   // 3
print("last: {list.pop()}");    // 30
print("size: {list.len()}");   // 2

// --- Indexed access ---
var colors = ["red", "green", "blue"];
print(colors[0]);    // red
print(colors[2]);    // blue

// Modify an element
colors[1] = "yellow";
print(colors[1]);    // yellow

// --- Iterate ---
var i = 0;
while (i < numbers.len()) {
    print("numbers[{i}] = {numbers[i]}");
    i = i + 1;
}

// --- Swap trick for removing from the middle ---
// Replaces element i with the last one and pops - O(1)
def array_remove(arr, i) {
    var ultimo = arr.len() - 1;
    if (i != ultimo) {
        arr[i] = arr[ultimo];
    }
    arr.pop();
}

var items = [10, 20, 30, 40, 50];
array_remove(items, 1);   // removes 20
print("after removing index 1:");
var j = 0;
while (j < items.len()) {
    print("  {items[j]}");
    j = j + 1;
}

// --- Array as a simple queue ---
var queue = [];
queue.push("first");
queue.push("second");
queue.push("third");

// Removing from the front (manual shift with swap-remove does not preserve order,
// but for unordered event queues only the length may matter)
print("in queue: {queue.len()} items");

// --- Array of structs (see tutorial 05) ---
// var entidades = [];
// entidades.push(Ponto(10, 20));
```

## How to run

```bash
zen examples/tutorial_04_arrays.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_04_arrays.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
