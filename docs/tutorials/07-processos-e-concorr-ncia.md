# Tutorial 07 — Processes and Concurrency

This tutorial introduces cooperative processes, scheduling, and parent/child process relationships through practical examples.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 07 — Processes and Concurrency
// ============================================================

// process is the central cooperative runtime construct in BuLang.
// Calling a process spawns it and runs it in cooperative parallel.
// frame  -> suspends the current process until the next tick.
// loop   -> infinite loop (the process lives until break or return).
// father -> access to the parent process (the one that spawned this one).
// son    -> access to the most recent child process.

// --- Simple process ---
process count_to(n) {
    var i = 1;
    while (i <= n) {
        print("counting: {i}");
        i = i + 1;
        frame;   // yield control until the next tick
    }
    print("reached {n}!");
}

count_to(5);   // spawn - runs in parallel

// --- Infinite loop with break ---
process clock(name) {
    var ticks = 0;
    loop {
        ticks = ticks + 1;
        print("[{name}] tick {ticks}");
        if (ticks >= 3) { break; }
        frame;
    }
    print("[{name}] stopped");
}

clock("A");
clock("B");

// --- Parent/child communication through father/son ---

process child(msg) {
    print("child says: {msg}");
    print("child sees parent.x = {father.x}");

    father.x = 999;           // modify the parent's field
    frame;

    print("child: after frame, parent.x = {father.x}");
}

process parent(x, y) {
    print("parent: ({x}, {y})");
    child("hello parent!");            // spawn child

    print("parent sees child.x = {son.x}");   // access child fields
    frame;

    print("parent: after frame, x = {x}");   // child changed x to 999
}

parent(100, 200);

// --- Process as a game entity ---
process ball(x, y, vx, vy) {
    loop {
        // simple physics
        vy = vy + 0.5;      // gravity
        x = x + vx;
        y = y + vy;

        // wall collision
        if (x < 0)    { x = 0;    vx = -vx * 0.8; }
        if (x > 1280) { x = 1280; vx = -vx * 0.8; }
        if (y > 720)  { y = 720;  vy = -vy * 0.7; }

        print("ball: ({int(x)}, {int(y)})");
        frame;
    }
}

// Spawn 3 balls with different velocities
ball(640, 0,  2,  0);
ball(640, 0, -3,  1);
ball(640, 0,  1, -2);

// --- Spawner: creates processes dynamically ---
process spawner() {
    var count = 0;
    loop {
        if (count < 5) {
            ball(640, 0, count - 2, -count);
            count = count + 1;
        }
        frame;
    }
}

spawner();

for(var i=0;i<1000;i=i+1)
{
     advance_process();
}
```

## How to run

```bash
zen examples/tutorial_07_processos.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_07_processos.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
