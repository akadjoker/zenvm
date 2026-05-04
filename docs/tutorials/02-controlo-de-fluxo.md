# Tutorial 02 — Control Flow

This tutorial shows how to control program flow with conditions, loops, and infinite loops managed with `break`.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 02 — Control Flow
// ============================================================

// --- if / else ---
var x = 15;

if (x > 10) {
    print("greater than 10");
} else {
    print("less than or equal to 10");
}

// chained if
var grade = 75;
if (grade >= 90) {
    print("Excellent");
} else {
    if (grade >= 70) {
        print("Good");
    } else {
        if (grade >= 50) {
            print("Enough");
        } else {
            print("Failing");
        }
    }
}

// --- while ---
var i = 0;
while (i < 5) {
    print("i = {i}");
    i = i + 1;
}

// --- for ---
for (var j = 0; j < 5; j = j + 1) {
    print("j = {j}");
}

// --- infinite loop + break ---
// loop runs forever until it finds a break
var counter = 0;
loop {
    counter = counter + 1;
    if (counter >= 3) {
        break;
    }
    print("loop: {counter}");
}
print("left loop at: {counter}");

// --- Combining conditions ---
var age = 20;
var hasTicket = true;

if (age >= 18 && hasTicket) {
    print("can enter");
}

if (age < 16 or not hasTicket) {
    print("cannot enter");
}
```

## How to run

```bash
zen examples/tutorial_02_controlo.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_02_controlo.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
