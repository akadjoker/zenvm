# BuLang / Zen Documentation Pack

BuLang/Zen is a small scripting language designed for gameplay logic, tools and real-time experiments inside a custom C++ engine or VM.

This pack was generated from the `.zen` tutorial files and contains:

- `docs/language-guide.md` — guided overview of the language
- `docs/syntax-reference.md` — compact syntax reference
- `docs/quick-reference.md` — fast syntax cheat sheet
- `docs/standard-library.md` — built-ins, containers and useful methods
- `docs/tutorials/` — tutorial pages generated from the source examples
- `examples/` — original `.zen` tutorial files
- `help/` — plain text help files suitable for `zen --help topic`

## Quick example

```zen
var name = "BuLang";
var score = 42;

print("Hello, {name}!");
print("Score: {score}");

def add(a, b) {
    return a + b;
}

print(add(10, 20));
```

## Tutorial index

- [01 — Variáveis, Tipos e Operadores](docs/tutorials/01-vari-veis-tipos-e-operadores.md) — source: `examples/tutorial_01_variaveis.zen`
- [02 — Controlo de Fluxo](docs/tutorials/02-controlo-de-fluxo.md) — source: `examples/tutorial_02_controlo.zen`
- [03 — Funções (def)](docs/tutorials/03-fun-es-def.md) — source: `examples/tutorial_03_funcoes.zen`
- [04 — Arrays](docs/tutorials/04-arrays.md) — source: `examples/tutorial_04_arrays.zen`
- [05 — Structs](docs/tutorials/05-structs.md) — source: `examples/tutorial_05_structs.zen`
- [06 — Classes](docs/tutorials/06-classes.md) — source: `examples/tutorial_06_classes.zen`
- [07 — Processos e Concorrência](docs/tutorials/07-processos-e-concorr-ncia.md) — source: `examples/tutorial_07_processos.zen`
- [08 — Módulos (import)](docs/tutorials/08-m-dulos-import.md) — source: `examples/tutorial_08_modulos.zen`
- [09 — Projecto Prático: Snake](docs/tutorials/09-projecto-pr-tico-snake.md) — source: `examples/tutorial_09_snake.zen`
- [10 — Closures e Upvalues](docs/tutorials/10-closures-e-upvalues.md) — source: `examples/tutorial_10_closures.zen`
- [11 — Fibers: spawn / resume / yield](docs/tutorials/11-fibers-spawn-resume-yield.md) — source: `examples/tutorial_11_fibers.zen`
- [12 — Typed Buffers](docs/tutorials/12-typed-buffers.md) — source: `examples/tutorial_12_buffers.zen`
- [13 — Maps e Sets](docs/tutorials/13-maps-e-sets.md) — source: `examples/tutorial_13_maps_sets.zen`
- [14 — Métodos de String e Array](docs/tutorials/14-m-todos-de-string-e-array.md) — source: `examples/tutorial_14_metodos.zen`
- [15 — Math Built-ins, Operadores Bitwise e do-while](docs/tutorials/15-math-built-ins-operadores-bitwise-e-do-while.md) — source: `examples/tutorial_15_math_bitwise.zen`

## Running examples

Adjust the command to match your executable name:

```bash
zen examples/tutorial_01_variaveis.zen
```

or:

```bash
bulang examples/tutorial_01_variaveis.zen
```

## Suggested command line help

```bash
zen --help
zen --help variables
zen --help functions
zen --help arrays
zen --help processes
zen --examples
```
