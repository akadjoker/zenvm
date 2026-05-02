# Zen Language — Syntax Reference (v0.5.0)

## Types

| Type | Examples | Notes |
|------|---------|-------|
| `nil` | `nil` | Falsy |
| `bool` | `true`, `false` | `false` is falsy |
| `int` | `42`, `-1`, `0xFF` | 64-bit signed |
| `float` | `3.14`, `1e10` | 64-bit double |
| `string` | `"hello"`, `@"raw"` | Immutable, UTF-8 |
| `array` | `[1, 2, 3]` | Dynamic, mixed types |
| `map` | `{"key": val}` | Hash table |
| `set` | `#{1, 2, 3}` | Unique values |
| `buffer` | `Int32Array(10)` | Typed, fixed-size |
| `function` | `def(x) { x * 2 }` | First-class, closures |
| `fiber` | `spawn gen` | Coroutine |

---

## Variables

```zen
var x = 10;              // declaration
var y;                   // nil by default
x = 20;                 // assignment
var (a, b, _) = func(); // multi-return destructuring (_ discards)
```

### Compound Assignment
```zen
x += 1;   x -= 1;   x *= 2;   x /= 2;
```

---

## Operators

### Precedence (low → high)

| Prec | Operators | Assoc |
|------|-----------|-------|
| 1 | `=` `+=` `-=` `*=` `/=` | Right |
| 2 | `or` `\|\|` | Left |
| 3 | `and` `&&` | Left |
| 4 | `\|` (bitwise OR) | Left |
| 5 | `^` (bitwise XOR) | Left |
| 6 | `&` (bitwise AND) | Left |
| 7 | `==` `!=` | Left |
| 8 | `<` `>` `<=` `>=` | Left |
| 9 | `<<` `>>` | Left |
| 10 | `+` `-` | Left |
| 11 | `*` `/` `%` | Left |
| 12 | `-x` `!x` `~x` `not x` | Right (prefix) |
| 13 | `.` `()` `[]` | Left |

### Notes
- Division `/` always returns float: `10 / 3` → `3.33333`
- Integer arithmetic wraps at 64-bit boundaries
- Bitwise ops: `&` `|` `^` `~` `<<` `>>` (operate on int64)
- Logical `and`/`or` short-circuit and return the deciding value

---

## Strings

```zen
var s = "hello\nworld";         // escape sequences: \n \t \\ \" \x41 \u00E9
var r = @"C:\no\escapes";      // verbatim string (double "" for quote)
var i = "x = {x}, y = {y}";   // string interpolation
var c = "a" + "b";             // concatenation (also int/float auto-coerce)
```

### String Methods
```zen
s.len()              // length
s.upper()            // uppercase
s.lower()            // lowercase
s.sub(start, end?)   // substring (0-indexed)
s.find("needle")     // index or -1
s.replace(old, new)  // replace all
s.starts_with(pre)   // bool
s.ends_with(suf)     // bool
s.trim()             // strip whitespace
s.char_at(i)         // single char string
s.split(sep)         // returns array
```

---

## Arrays

```zen
var a = [1, "two", 3.0, nil];  // mixed types
a[0] = 99;                     // index set (0-based)
print(a[1]);                   // index get
```

### Array Methods
```zen
a.push(val)        // append, returns array
a.pop()            // remove last, returns it
a.len()            // length
a.contains(val)    // bool
a.index_of(val)    // index or -1
a.reverse()        // in-place, returns array
a.slice(start, end?) // new array
a.insert(idx, val) // insert at position
a.remove(idx)      // remove at index
a.clear()          // empty array
a.join(sep?)       // string (default ",")
a.sort()           // ascending
a.sort("desc")     // descending
```

---

## Maps

```zen
var m = {"name": "zen", "version": 5};
m["key"] = val;      // set
print(m["key"]);     // get
```

### Map Methods
```zen
m.set(key, val)      // set
m.get(key, default?) // get with optional default
m.has(key)           // bool
m.delete(key)        // remove
m.keys()             // array of keys
m.values()           // array of values
m.size()             // count
m.clear()            // empty
```

---

## Sets

```zen
var s = #{1, 2, 3};    // set literal (deduplicates)
var e = #{};           // empty set
```

### Set Methods
```zen
s.add(val)      // add element
s.has(val)      // bool
s.delete(val)   // remove
s.size()        // count
s.clear()       // empty
s.values()      // array of values
```

---

## Typed Buffers

Fixed-size typed arrays for high-performance numeric data.

```zen
var a = Int32Array(1000);          // zero-initialized
var b = Float64Array([1.5, 2.7]);  // from array
a[0] = 42;                        // bounds-checked
print(a[0]);                       // typed read
```

### Buffer Types
| Constructor | Element | Bytes | Range |
|------------|---------|-------|-------|
| `Int8Array(n)` | int8 | 1 | -128..127 |
| `Int16Array(n)` | int16 | 2 | -32768..32767 |
| `Int32Array(n)` | int32 | 4 | -2³¹..2³¹-1 |
| `Uint8Array(n)` | uint8 | 1 | 0..255 |
| `Uint16Array(n)` | uint16 | 2 | 0..65535 |
| `Uint32Array(n)` | uint32 | 4 | 0..4294967295 |
| `Float32Array(n)` | float32 | 4 | ±3.4e38 |
| `Float64Array(n)` | float64 | 8 | ±1.8e308 |

### Buffer Methods
```zen
buf.len()       // element count
buf.fill(val)   // fill all elements
buf.byte_len()  // total bytes
```

---

## Control Flow

```zen
// if / elif / else
if (x > 0) {
    print("positive");
} elif (x == 0) {
    print("zero");
} else {
    print("negative");
}

// while
while (x < 100) { x += 1; }

// for (C-style)
for (var i = 0; i < 10; i += 1) { print(i); }

// foreach
foreach (item in array) { print(item); }

// loop (infinite)
loop { if (done) { break; } }

// do-while
do { x += 1; } while (x < 10);

// switch
switch (val) {
    case 1: { print("one"); }
    case 2: { print("two"); }
    default: { print("other"); }
}

// jump
break;
continue;
return expr;
return a, b, c;   // multi-return
```

---

## Functions

```zen
// named function
def add(a, b) {
    return a + b;
}

// anonymous function (closure)
var double = def(x) { return x * 2; };

// closures capture enclosing variables
def counter(start) {
    var n = start;
    def inc() { n += 1; return n; }
    return inc;
}
var c = counter(0);
print(c());  // 1
print(c());  // 2

// recursive
def fib(n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}
```

---

## Fibers (Coroutines)

```zen
// create fiber from function
def gen() {
    yield 1;
    yield 2;
    yield 3;
}
var f = spawn gen;

// resume and receive yielded values
var a = resume(f);     // 1
var b = resume(f);     // 2
var c = resume(f);     // 3
var d = resume(f);     // nil (dead fiber)

// send values into fiber
def echo() {
    var msg = yield nil;
    print(msg);
}
var f = spawn echo;
resume(f);             // run to first yield
resume(f, "hello");    // sends "hello", prints it

// game loop yield
frame;                 // yields with speed=100
```

---

## Built-in Functions

These compile directly to opcodes (zero call overhead):

```zen
print(a, b, c);   // prints space-separated + newline
len(x);           // string/array/buffer length
clock();          // high-res time in seconds (float)

// Math
sin(x)   cos(x)   tan(x)
asin(x)  acos(x)  atan(x)  atan2(y, x)
sqrt(x)  pow(b, e)  log(x)  exp(x)
abs(x)   floor(x)  ceil(x)
deg(x)   rad(x)    // radians↔degrees
```

---

## Type Conversion

```zen
// implicit
"value: " + 42         // int→string in concatenation
"pi = " + 3.14        // float→string in concatenation

// numeric coercion in arithmetic
2 + 3.0               // → 5.0 (int promoted to float)
10 / 3                // → 3.33333 (division always float)
```

---

## Hex Literals

```zen
var x = 0xFF;         // 255
var y = 0xDEADBEEF;   // 3735928559
```

---

## Comments

```zen
// single-line comment
/* multi-line
   block comment */
```

---

## Semicolons

All statements require semicolons:
```zen
var x = 10;
print(x);
x += 1;
```

Block statements (`if`, `while`, `for`, `def`, etc.) do NOT need trailing semicolons:
```zen
if (x > 0) { print("yes"); }    // no ; after }
def foo() { return 1; }         // no ; after }
```

---

## Scope

- `var` creates a local in the current block scope
- Blocks `{ }` create new scopes
- Functions capture upvalues (closure semantics)
- No global scope — top-level is the script scope

---

## Classes

```zen
class Entity {
    var x;
    var y;
    var hp;

    def init(x, y, hp) {
        self.x = x;
        self.y = y;
        self.hp = hp;
    }

    def damage(amount) {
        self.hp = self.hp - amount;
    }
}

// Herança
class Boss : Entity {
    var rage;
    def init(x, y) {
        self.x = x;
        self.y = y;
        self.hp = 500;
        self.rage = 0;
    }
    def damage(amount) {
        self.hp = self.hp - amount;
        self.rage = self.rage + 10;
    }
}

var b = Boss(10, 20);
b.damage(50);
```

- Herança single-level (`: Parent`)
- `self` para aceder fields e métodos
- `init` chamado automaticamente na construção
- Override de métodos da classe pai
- Vtable dispatch O(1) para todos os métodos

## C++ Class Bindings

### Classe básica (GC-managed)

```cpp
// Script cria, GC destrói. Dados vivem no native_data (void*).
auto *cls = vm.def_class("Sprite")
    .ctor(sprite_ctor)          // void* fn(VM*, int argc, Value* args)
    .dtor(sprite_dtor)          // void fn(VM*, void* data)
    .method("get_x", sprite_get_x, 0)
    .method("set_pos", sprite_set_pos, 2)
    .end();
```

### Classe persistente (C++ owns lifetime)

```cpp
// C++ cria, C++ destrói. GC nunca toca.
auto *cls = vm.def_class("Transform")
    .ctor(transform_ctor)
    .dtor(transform_dtor)
    .persistent(true)           // malloc directo, fora do GC
    .constructable(false)       // script NÃO pode fazer Transform()
    .method("translate", transform_translate, 2)
    .end();

// C++ cria
Value t = vm.make_instance(cls, args, nargs);

// Script usa (via global)
vm.def_global("player_transform", t);

// C++ destrói quando quiser
vm.destroy_instance(t);
```

### Native constructor

```cpp
// Retorna void* — o objecto C++ real.
// Recebe args do script (Sprite(10, 20) → argc=2, args=[10,20]).
static void *sprite_ctor(VM *vm, int argc, Value *args) {
    SpriteData *s = new SpriteData();
    s->x = (argc > 0) ? (float)args[0].as.integer : 0.0f;
    s->y = (argc > 1) ? (float)args[1].as.integer : 0.0f;
    return s;
}
```

### Native destructor

```cpp
// Chamado pelo GC (non-persistent) ou por destroy_instance (persistent).
static void sprite_dtor(VM *vm, void *data) {
    delete (SpriteData *)data;
}
```

### Native method

```cpp
// Signature: int fn(VM *vm, Value *args, int nargs)
// args[0] = self (instância), args[1..] = argumentos
// Escreve return value em args[0], retorna número de returns.
static int sprite_get_x(VM *vm, Value *args, int nargs) {
    SpriteData *s = zen_instance_data<SpriteData>(args[0]);
    args[0] = val_int((int64_t)s->x);
    return 1;  // 1 valor retornado
}

static int sprite_set_pos(VM *vm, Value *args, int nargs) {
    SpriteData *s = zen_instance_data<SpriteData>(args[0]);
    s->x = (float)args[1].as.integer;
    s->y = (float)args[2].as.integer;
    return 0;  // void
}
```

### Script herda classe C++

```zen
// Sprite definido em C++ com ctor/dtor/methods.
// Player herda: native_ctor corre (cria SpriteData*), depois init script.
class Player : Sprite {
    var score;
    def init() {
        self.score = 0;
    }
    def add_score(n) {
        self.score = self.score + n;
    }
}
```

O `native_data` do parent propaga automaticamente para subclasses.

### Bidireccional (C++ ↔ Script)

```cpp
// C++ cria objecto
Value t = vm.make_instance(transform_class, nullptr, 0);
vm.def_global("player_transform", t);

// Script controla
// player_transform.translate(10, 5);

// C++ lê resultado de volta
TransformData *td = zen_instance_data<TransformData>(t);
printf("x = %f\n", td->x);  // modificado pelo script

// C++ chama método no objecto
Value args[] = { val_int(100), val_int(200) };
vm.invoke(t, "translate", args, 2);
```

### Helpers

| Helper | Descrição |
|--------|-----------|
| `zen_instance_data<T>(val)` | Cast `native_data` para `T*` |
| `val_is_instance_of(val, "Sprite")` | Type check por nome |
| `vm.make_instance(klass, args, n)` | Cria instância (chama ctor + init) |
| `vm.destroy_instance(val)` | Destrói persistent (chama dtor + free) |
| `vm.invoke(val, "method", args, n)` | Chama método por nome |
| `vm.invoke(val, slot, args, n)` | Chama método por vtable slot O(1) |
| `vm.find_selector("name", len)` | Obtém slot index dum método |

### ClassBuilder API

| Método | Descrição |
|--------|-----------|
| `.field("name")` | Field script (Value) |
| `.method("name", fn, arity)` | Método nativo |
| `.ctor(fn)` | Constructor C++ (retorna `void*`) |
| `.dtor(fn)` | Destructor C++ |
| `.persistent(true)` | Instâncias fora do GC |
| `.constructable(false)` | Script não pode instanciar |
| `.parent("Base")` | Herança |
| `.end()` | Regista classe como global |

### Regras de lifetime

| Tipo | Quem cria | Quem destrói | native_data |
|------|-----------|--------------|-------------|
| Normal (GC) | Script ou C++ | GC (chama dtor) | Opcional |
| Persistent | Só C++ | Só C++ (`destroy_instance`) | Quase sempre |
| Non-constructable | Só C++ | Depende de persistent | Sim |

## Reserved (not yet implemented)

- `import` — module system (planned)
- `..` — range operator (token exists)
