# Tutorial 13 — Maps and Sets

This tutorial covers key-value maps and sets for collections without duplicates.

## Goal

Learn the main syntax and patterns for this topic in BuLang/Zen.

## Full code

```zen
// ============================================================
// Tutorial 13 — Maps and Sets
// ============================================================

// Map  {}      - key->value table (hash map)
// Set  #{}     - collection of unique values

// ==========================================================
// MAP
// ==========================================================

// --- Creation and set/get ---
var m = {};
m.set("name", "Luis");
m.set("age", 30);
m.set("active", true);

print(m.get("name"));     // Luis
print(m.get("age"));      // 30
print(m.get("active"));   // true

// --- get with a default value ---
print(m.get("email", "n/a"));   // n/a  <- key does not exist

// --- has / size ---
print(m.has("name"));      // true
print(m.has("email"));     // false
print(m.size());           // 3

// --- delete ---
m.set("temp", 99);
print(m.size());           // 4
m.delete("temp");
print(m.size());           // 3
print(m.has("temp"));      // false

// --- keys / values ---
var config = {};
config.set("w", 1280);
config.set("h", 720);
config.set("fps", 60);

var keys = config.keys();
print("keys: {keys.len()}");   // 3

var values = config.values();
// values have no guaranteed order - check them individually
print(config.get("w"));    // 1280
print(config.get("fps"));  // 60

// --- clear ---
var tmp = {};
tmp.set("x", 1);
tmp.clear();
print(tmp.size());   // 0

// --- Map as an entity record ---
def create_player(name, hp, level) {
    var p = {};
    p.set("name", name);
    p.set("hp", hp);
    p.set("level", level);
    return p;
}

var j1 = create_player("Djoker", 100, 5);
var j2 = create_player("Bot",    60,  2);

print(j1.get("name"));    // Djoker
print(j2.get("hp"));      // 60

j1.set("hp", j1.get("hp") - 25);
print(j1.get("hp"));      // 75

// --- Map of maps (nested) ---
var world = {};
world.set("players", create_player("Hero", 200, 10));
world.set("boss",    create_player("Dragon", 1000, 50));

var boss = world.get("boss");
print(boss.get("name"));    // Dragon
print(boss.get("hp"));      // 1000

// ==========================================================
// SET
// ==========================================================

// --- Creation and has ---
var s = #{10, 20, 30};
print(s.size());     // 3
print(s.has(20));    // true
print(s.has(99));    // false

// --- Automatic deduplication ---
var sd = #{1, 1, 2, 2, 3};
print(sd.size());    // 3

// --- add / delete ---
var tags = #{"zen", "bulang"};
tags.add("c++");
print(tags.size());       // 3
print(tags.has("c++"));   // true

tags.delete("bulang");
print(tags.size());           // 2
print(tags.has("bulang"));    // false

// --- clear ---
var st = #{1, 2, 3};
st.clear();
print(st.size());    // 0

// --- values() -> convert to array ---
var numbers = #{5, 3, 1, 4, 2};
var arr = numbers.values();
print(arr.len());    // 5
// (order is not guaranteed)

// --- Empty set ---
var empty = #{};
print(empty.size());    // 0

// --- Set of strings ---
var words = #{"hello", "world", "hello"};
print(words.size());          // 2  (deduplicated)
print(words.has("world"));   // true

// --- Practical case: unique visited IDs ---
var visited = #{};
visited.add(42);
visited.add(7);
visited.add(42);   // duplicate
visited.add(13);
print("visited: {visited.size()}");   // 3

def already_seen(id) {
    return visited.has(id);
}

print(already_seen(7));    // true
print(already_seen(99));   // false
```

## How to run

```bash
zen examples/tutorial_13_maps_sets.zen
```

or adjust the command to match your executable name:

```bash
bulang examples/tutorial_13_maps_sets.zen
```

## What to look for

- The syntax is direct and uses `{` and `}` blocks.
- The examples use `print()` to show the expected result.
- Inline comments explain each section of the example.

## Suggested exercise

Change the example values, run it again, and confirm that the output changes as expected.
