# ZenVM / Zen

Zen is a small embeddable scripting language for gameplay logic, tools, experiments, and host-driven applications. ZenVM is the C++ compiler and register-based virtual machine that compiles `.zen` source into bytecode and executes it directly or from dumped `.znb` bundles.

The project is tuned for game-style workloads: lightweight scripting, cooperative processes, compact bytecode, and practical C++ interop.

## Main Features

- Cooperative processes with `process`, `frame`, `loop`, `father`, and `son`
- Script structs, classes, methods, inheritance, and operator overloading
- Arrays, maps, sets, typed buffers, fibers, and bytecode dumping/loading
- Optional type hints for struct/class parameters and return values to improve compile-time field resolution
- Computed-goto VM dispatch, direct field opcodes, and fused superinstructions for hot paths
- C++ bindings for native functions, classes, and native-backed structs

## Example

```zen
struct Vec3 { x, y, z }

def length_sq(v: Vec3) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

process ball(x, y, vx, vy) {
    loop {
        vy = vy + 0.5;
        x = x + vx;
        y = y + vy;
        print("ball=({int(x)}, {int(y)})");
        frame;
    }
}

print(length_sq(Vec3(1, 2, 3)));
ball(100, 0, 2, 0);
advance_process();
```

## What Changed Recently

- Function, method, lambda, and process parameters can now use optional struct/class type hints
- Global functions and methods can now declare optional return type hints with `def foo(x: MyType) : MyType { ... }`
- Type hints are used by the compiler to resolve field access to `GETFIELD_IDX` at compile time when possible
- The peephole optimizer can now fuse direct field access plus multiply into `GETFIELD_MUL`
- Bytecode dump/load now round-trips script structs in addition to functions, closures, classes, collections, and processes

Type hints are optional. They are currently intended for known script types such as structs and classes, especially in code that sits on a C++ ↔ script boundary.

## Building

### CMake

```bash
cmake -S . -B build
cmake --build build -j
```

### Makefile

```bash
make
```

The main executable is generated as `bin/zen`.

## Running Source Files

```bash
./bin/zen examples/tutorial_01_variaveis.zen
./bin/zen -e 'print("Hello, world!");'
./bin/zen --dis examples/tutorial_05_structs.zen
```

## Dumping And Running Bytecode

```bash
./bin/zen --dump app.znb app.zen
./bin/zen app.znb
./bin/zen --dis app.znb
```

Pure script constructs now round-trip through bytecode, including:

- functions and closures
- structs and classes
- arrays, maps, sets, and typed buffers
- processes and fibers
- built-in math/runtime features compiled to opcodes

Host-owned runtime objects still need host registration at startup. Native-backed classes, raw pointers, and host-specific plugins are not serialized as standalone bytecode payloads.

## Zen IDE

The repository includes a lightweight editor written in Python/PyQt6.

```bash
pip install PyQt6
python tools/editor.py --zen ./bin/zen
```

The editor supports syntax highlighting, execution, disassembly inspection, and bytecode dumping.

## Documentation

- [Language Guide](docs/language-guide.md)
- [Syntax Reference](docs/syntax-reference.md)
- [Quick Reference](docs/quick-reference.md)
- [Standard Library Notes](docs/standard-library.md)
- [Documentation Index](docs/index.md)

## Tutorial Index

- [01 — Variables, Types, and Operators](docs/tutorials/01-vari-veis-tipos-e-operadores.md) — source: `examples/tutorial_01_variaveis.zen`
- [02 — Control Flow](docs/tutorials/02-controlo-de-fluxo.md) — source: `examples/tutorial_02_controlo.zen`
- [03 — Functions](docs/tutorials/03-fun-es-def.md) — source: `examples/tutorial_03_funcoes.zen`
- [04 — Arrays](docs/tutorials/04-arrays.md) — source: `examples/tutorial_04_arrays.zen`
- [05 — Structs](docs/tutorials/05-structs.md) — source: `examples/tutorial_05_structs.zen`
- [06 — Classes](docs/tutorials/06-classes.md) — source: `examples/tutorial_06_classes.zen`
- [07 — Processes and Concurrency](docs/tutorials/07-processos-e-concorr-ncia.md) — source: `examples/tutorial_07_processos.zen`
- [08 — Modules (`import`)](docs/tutorials/08-m-dulos-import.md) — source: `examples/tutorial_08_modulos.zen`
- [09 — Practical Project: Snake](docs/tutorials/09-projecto-pr-tico-snake.md) — source: `examples/tutorial_09_snake.zen`
- [10 — Closures and Upvalues](docs/tutorials/10-closures-e-upvalues.md) — source: `examples/tutorial_10_closures.zen`
- [11 — Fibers: `spawn` / `resume` / `yield`](docs/tutorials/11-fibers-spawn-resume-yield.md) — source: `examples/tutorial_11_fibers.zen`
- [12 — Typed Buffers](docs/tutorials/12-typed-buffers.md) — source: `examples/tutorial_12_buffers.zen`
- [13 — Maps and Sets](docs/tutorials/13-maps-e-sets.md) — source: `examples/tutorial_13_maps_sets.zen`
- [14 — String and Array Methods](docs/tutorials/14-m-todos-de-string-e-array.md) — source: `examples/tutorial_14_metodos.zen`
- [15 — Math Built-ins, Bitwise Operators, and `do while`](docs/tutorials/15-math-built-ins-operadores-bitwise-e-do-while.md) — source: `examples/tutorial_15_math_bitwise.zen`

## VM Architecture

Most VM internals live under `libzen/`.

- `object.h` defines the runtime object taxonomy and heap-managed object layouts
- the heap is managed by a tri-color linked-list GC
- fibers keep independent call/register stacks so suspended processes do not block the host thread
- bytecode emission is handled by `Emitter`, which writes 32-bit instructions and supports patching and peephole rewrites

## Project Status

The language is still evolving. See `BUGS.md` and the tests under `tests/` for the current state of the implementation.

## License

This project is distributed as-is for study, experimentation, and prototyping.
