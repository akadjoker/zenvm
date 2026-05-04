# libzen — known issues

## Open

(none)

---

## Resolved

### 4. `foreach` only works correctly for arrays and typed buffers

Fix: added `OP_ITER_ELEM` opcode that supports ordinal iteration over
arrays, buffers, strings, maps (keys), and sets (elements).
Also added two-variable form: `foreach (i, x in collection)`.
Covered by `tests/test_foreach.zen`.

### 1. Class-type hint leaks across globals after a function call

Global-typing layer attributed class hints to unrelated globals.
Fix: corrected hint propagation. Covered by `tests/test_call_hint_leak.zen`.

### 2. `intern_selector` slot beyond ~120 returns stale vtable Value

Root cause: fixed `MethodInfo methods[64]` buffer in class compilation.
Fix: now dynamic. Covered by `tests/test_many_methods_invoke.zen`.

### 3. Compiler does not free temp registers in long `acc = acc + expr` sequences

Fix: `next_reg` restored after expression statements; compiler bails out
before bytecode emission when errors are present.
