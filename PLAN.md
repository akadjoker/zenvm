# ZEN — Plano v0.4.0

## Estado actual (v0.3.0) — 300 testes OK

**Funciona:** lexer, compiler, VM (computed goto), closures, upvalues,
fibers (spawn/resume/yield), for/while/do-while/break/continue,
string interpolation, arrays (literal + GETINDEX/SETINDEX), maps,
bitwise ops, compound assignment (+=/-=/*=/etc), numeric for.

**Não funciona:** `.method()` syntax, classes, GC collect, typed buffers.

---

## v0.4.0 — OP_INVOKE + Type Methods

### Design

`expr.method(args)` compila para:
```
OP_INVOKE  A=dest, B=receiver, C=argcount  | Kx=method_name_idx
```

O VM faz dispatch por tipo do receiver. Lógica separada por ficheiro:

```
src/vm_dispatch.cpp        — main dispatch loop (inclui os invoke_*.inl)
src/invoke_array.inl       — array methods (push, pop, remove, insert, slice, sort, reverse)
src/invoke_string.inl      — string methods (len, sub, find, upper, lower, split, trim, replace)
src/invoke_map.inl         — map methods (set, get, has, delete, keys, values, size)
src/invoke_buffer.inl      — typed buffer methods (len, fill, copy_from, ptr, size_bytes)
```

### Array methods
```
arr.push(val)              → append, returns new length
arr.pop()                  → remove+return last
arr.insert(idx, val)       → insert at position
arr.remove(idx)            → remove at position, returns removed
arr.slice(start, end)      → new array [start, end)
arr.sort()                 → in-place sort (numeric/string)
arr.reverse()              → in-place reverse
arr.len()                  → length (also: len(arr) builtin stays)
arr.contains(val)          → bool
arr.join(sep)              → string
```

### String methods
```
str.len()                  → length in bytes
str.sub(start, end)        → substring [start, end)
str.find(needle)           → index or -1
str.upper()                → new uppercase string
str.lower()                → new lowercase string
str.split(sep)             → array of strings
str.trim()                 → strip whitespace
str.replace(old, new)      → new string with replacements
str.starts_with(prefix)    → bool
str.ends_with(suffix)      → bool
str.char_at(idx)           → single-char string
```

### Map methods
```
map.set(key, val)          → set key (same as map[key] = val)
map.get(key)               → get or nil
map.has(key)               → bool
map.delete(key)            → remove key
map.keys()                 → array of keys
map.values()               → array of values
map.size()                 → number of entries
```

### Typed Buffers (for OpenGL/binary)
```
Int8Array(size || array)            → signed 8-bit buffer
Int16Array(size || array)           → signed 16-bit buffer
Int32Array(size || array)           → signed 32-bit buffer
Uint8Array(size || array)           → unsigned 8-bit buffer
Uint16Array(size || array)          → unsigned 16-bit buffer
Uint32Array(size || array)          → unsigned 32-bit buffer
Float32Array(size || array)         → float 32-bit buffer
Float64Array(size || array)         → float 64-bit buffer
```

**Methods:**
```
buf[i]                     → get element (GETINDEX dispatch)
buf[i] = val               → set element (SETINDEX dispatch)
buf.len()                  → number of elements
```

**Usage with OpenGL:**
```zen
var verts = Float32Array(9);
verts[0] = -0.5; verts[1] = -0.5; verts[2] = 0.0;
verts[3] =  0.5; verts[4] = -0.5; verts[5] = 0.0;
verts[6] =  0.0; verts[7] =  0.5; verts[8] = 0.0;
glBufferData(GL_ARRAY_BUFFER, verts.size_bytes(), verts.ptr(), GL_STATIC_DRAW);
```

---

## Implementação — Ordem e Passos

### Step 1: OP_INVOKE opcode
**Files:** opcodes.h, compiler_expressions.cpp, vm_dispatch.cpp, debug.cpp

1. Add `OP_INVOKE` to opcodes.h (encoding: ABx where A=dest/receiver, B=argcount, Kx=method name index)
2. Compiler: when parsing `expr.name(args)`, emit OP_INVOKE instead of OP_GETFIELD+OP_CALL
3. VM dispatch: type-switch on receiver, then string-match method name
4. Disassembler: show `INVOKE R[A].method(B args)`

### Step 2: Array methods (invoke_array.inl)
**File:** src/invoke_array.inl (included in vm_dispatch.cpp)

Methods: push, pop, insert, remove, slice, sort, reverse, len, contains, join, clear

### Step 3: String methods (invoke_string.inl)
**File:** src/invoke_string.inl

Methods: len, sub, find, upper, lower, split, trim, replace, starts_with, ends_with, char_at, byte_at

### Step 4: Map methods (invoke_map.inl)
**File:** src/invoke_map.inl

Methods: set, get, has, delete, keys, values, size, clear

### Step 5: ObjTypedBuffer + invoke_buffer.inl
**Files:** object.h, memory.h, memory.cpp, src/invoke_buffer.inl

1. Add ObjTypedBuffer to object.h (enum BufType, void* data, int count, int capacity, int elem_size)
2. Register constructors as globals: Int8Array, Int16Array, Int32Array, Uint8Array, Uint16Array, Uint32Array, Float32Array, Float64Array
3. GETINDEX/SETINDEX: add branch for OBJ_TYPED_BUFFER
4. Methods: len, size_bytes, fill, copy_from, ptr, slice, add, clear, reserve, pack

### Step 6: Tests
**File:** tests/test_invoke.sh

One test script per feature group, stress test at end.

---

## Tags

- v0.3.0 — current (arrays, string coercion, error handling)
- v0.4.0 — OP_INVOKE + array methods + string methods + map methods
- v0.5.0 — typed buffers (Int32Array, Float32Array, etc)

---

## Regras

> Quando adicionas um opcode a opcodes.h:
> 1. Adicionar `&&lbl_OP_XXX` à dispatch_table (posição correcta!)
> 2. Adicionar `CASE(OP_XXX) { ... NEXT(); }` handler
> 3. Adicionar ao disassembler (debug.cpp)

---

## Build
```bash
cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc) zen
./tests/test_lexer_compiler.sh ./build/zen
./tests/test_closures.sh ./build/zen
./tests/test_fibers.sh ./build/zen
./tests/test_edge_cases.sh ./build/zen
./tests/zen_stress.sh ./build/zen
```

## Architecture
```
Source → Lexer → Compiler → ObjFunc (bytecode) → VM (execute)
                    ↓
                Emitter (emit_abc, emit_abx, jump patching)
```
