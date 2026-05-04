# Tutorial 09 — Practical Project: Snake

This tutorial walks through a small Snake-style gameplay example written in pure script logic.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 09 — Practical Project: Snake
// (pure logic - no graphics API)
// ============================================================

import math;

// --- World constants ---
var COLS = 20;
var ROWS = 15;
var DIR_UP    = 0;
var DIR_DOWN  = 1;
var DIR_LEFT  = 2;
var DIR_RIGHT = 3;

// --- Structs ---
struct Cell { x, y }

// --- Random food generation ---
def new_food() {
    return Cell(math.random(COLS), math.random(ROWS));
}

// --- Check whether a cell is inside the snake ---
def snake_has(snake, x, y) {
    var i = 0;
    while (i < snake.len()) {
        var seg = snake[i];
        if (seg.x == x && seg.y == y) { return true; }
        i = i + 1;
    }
    return false;
}

// --- Move snake ---
// Returns: 0 = ok, 1 = ate, -1 = died
def move(snake, dir, food) {
    var head = snake[0];
    var nx = head.x;
    var ny = head.y;

    if (dir == DIR_UP)    { ny = ny - 1; }
    if (dir == DIR_DOWN)  { ny = ny + 1; }
    if (dir == DIR_LEFT)  { nx = nx - 1; }
    if (dir == DIR_RIGHT) { nx = nx + 1; }

    // wall collision
    if (nx < 0 or nx >= COLS or ny < 0 or ny >= ROWS) {
        return -1;
    }

    // self collision
    if (snake_has(snake, nx, ny)) {
        return -1;
    }

    var new_head = Cell(nx, ny);

    // grow or move
    var ate = (nx == food.x && ny == food.y);
    if (!ate) {
        // remove tail (order matters here, so pop the last segment)
        snake.pop();
    }

    // insert new head at the front: rebuild the array
    var new_snake = [];
    new_snake.push(new_head);
    var i = 0;
    while (i < snake.len()) {
        new_snake.push(snake[i]);
        i = i + 1;
    }
    // Copy back into snake (Zen passes arrays by reference)
    snake.clear();
    var j = 0;
    while (j < new_snake.len()) {
        snake.push(new_snake[j]);
        j = j + 1;
    }

    if (ate) { return 1; }
    return 0;
}

// --- Print grid (ASCII) ---
def print_grid(snake, food) {
    var y = 0;
    while (y < ROWS) {
        var line = "";
        var x = 0;
        while (x < COLS) {
            if (snake_has(snake, x, y)) {
                if (x == snake[0].x && y == snake[0].y) {
                    line = "{line}O";   // head
                } else {
                    line = "{line}#";   // body
                }
            } else {
                if (x == food.x && y == food.y) {
                    line = "{line}*";   // food
                } else {
                    line = "{line}.";
                }
            }
            x = x + 1;
        }
        print(line);
        y = y + 1;
    }
}

// --- Game loop (20-step simulation) ---
var snake = [];
snake.push(Cell(5, 5));
snake.push(Cell(4, 5));
snake.push(Cell(3, 5));

var food = new_food();
var dir = DIR_RIGHT;
var score = 0;
var alive = true;
var step = 0;

while (step < 20 && alive) {
    // Simple AI: keep moving or turn randomly
    var r = math.random(4);
    if (r == 0 && dir != DIR_LEFT)  { dir = DIR_RIGHT; }
    if (r == 1 && dir != DIR_RIGHT) { dir = DIR_LEFT;  }
    if (r == 2 && dir != DIR_DOWN)  { dir = DIR_UP;    }
    if (r == 3 && dir != DIR_UP)    { dir = DIR_DOWN;  }

    var res = move(snake, dir, food);

    if (res == -1) {
        print("=== GAME OVER ===");
        alive = false;
    } else {
        if (res == 1) {
            score = score + 1;
            print("Ate! score={score} size={snake.len()}");
            food = new_food();
        }
    }

    step = step + 1;
}

print("--- Final state ---");
print_grid(snake, food);
print("Score: {score}  Size: {snake.len()}");
```

## How to run

```bash
zen examples/tutorial_09_snake.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_09_snake.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
