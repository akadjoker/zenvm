# Zen Module System — Design Plan

## Syntax

```zen
import math;                    // import module "math" → var math = <module>
import "path/file.zen";         // import from file path
import math { sin, cos };       // selective import (future)
```

## Architecture

### 1. Compile-time: `import` statement
- Lexer: `TOK_IMPORT` already exists
- Compiler: `parse_import()` emits `OP_IMPORT R[dest], K[name_idx]`
- The name can be an identifier (`math`) or a string literal (`"./lib/utils.zen"`)

### 2. VM: `OP_IMPORT` handler
```
OP_IMPORT A, Bx
  A  = destination register
  Bx = constant index (string: module name)
```

Execution:
1. Check `loaded_modules` cache (ObjMap) — if found, return cached value
2. Call the **resolver callback** to find the file path
3. Read the file source
4. Compile to ObjFunc
5. Execute in a new fiber/call-frame (module scope)
6. The module's return value (or its scope as a map) becomes the module value
7. Cache in `loaded_modules`
8. Store in R[A]

### 3. Resolver Callback (host-configurable)

```cpp
// C API for host application
typedef const char* (*ZenModuleResolver)(
    const char* module_name,     // "math" or "./lib/utils"
    const char* importer_path,   // path of the file doing the import
    void* userdata               // host context
);

void zen_set_module_resolver(VM* vm, ZenModuleResolver resolver, void* userdata);
```

**Default resolver** (built-in):
1. If name starts with `./` or `../` → relative to importer's directory
2. Otherwise → search in `ZEN_PATH` directories (colon-separated)
3. Append `.zen` extension if not present
4. Return absolute path or NULL (not found)

**Custom resolver** examples:
- Game engine: resolve from virtual filesystem / packed assets
- Editor: resolve from workspace root
- Embedded: resolve from compiled-in module table

### 4. Module Return Convention

A module file is just a Zen script. What it "exports" is its **return value**:

```zen
// lib/vec2.zen
def new(x, y) { return {"x": x, "y": y}; }
def add(a, b) { return new(a["x"]+b["x"], a["y"]+b["y"]); }
return {"new": new, "add": add};
```

Usage:
```zen
import "lib/vec2.zen";       // vec2 = {"new": fn, "add": fn}  
// OR with name binding:
import vec2;                 // looks for vec2.zen in ZEN_PATH
var p = vec2.new(1, 2);
```

### 5. File Reader Callback

```cpp
typedef struct {
    const char* source;   // file content (null-terminated)
    size_t length;
} ZenFileContent;

typedef ZenFileContent (*ZenFileReader)(
    const char* resolved_path,
    void* userdata
);

void zen_set_file_reader(VM* vm, ZenFileReader reader, void* userdata);
```

Default: `fopen` + `fread`. Custom: virtual FS, HTTP, embedded data.

### 6. Circular Import Protection

- Before executing, mark module as "loading" in cache
- If re-imported while loading → return partial/nil (like Node.js)
- After execution, replace with final value

### 7. Built-in Modules (native)

Pre-registered in `loaded_modules` without file lookup:

| Module | Contents |
|--------|----------|
| `math` | PI, E, random, clamp, lerp, map_range |
| `io` | read_file, write_file, exists |
| `os` | time, env, exit, args |
| `string` | format, byte, char, rep |

Registered via:
```cpp
void zen_register_module(VM* vm, const char* name, Value module_value);
```

---

## Implementation Steps

### Phase 1: Core (minimum viable)
1. `OP_IMPORT` opcode
2. Compiler: parse `import "path";` and `import name;`
3. VM: loaded_modules cache (ObjMap on VM struct)
4. Default file resolver (relative + ZEN_PATH)
5. Default file reader (fopen/fread)
6. Compile + execute module file, cache return value
7. Tests: basic import, circular, not-found error

### Phase 2: Host Integration  
8. `zen_set_module_resolver()` API
9. `zen_set_file_reader()` API
10. `zen_register_module()` for native modules

### Phase 3: Stdlib
11. Built-in `math` module
12. Built-in `io` module
13. Built-in `os` module

### Phase 4: Polish
14. Selective import: `import math { sin, cos };`
15. Module-level caching of compiled bytecode
16. Source maps / error traces across modules

---

## Key Decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Module value | return statement | Simple, explicit, no magic |
| Cache key | resolved absolute path | Handles aliases correctly |
| Resolver | callback, not hardcoded | Embeddable in any host |
| Execution | same VM, new scope | Share GC, strings, etc. |
| Circular | return nil/partial | Pragmatic, matches Node.js |
