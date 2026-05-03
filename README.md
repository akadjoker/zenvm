# ZenVM / BuLang 🚀

**BuLang** (or Zen) is a small, lightweight scripting language designed for gameplay logic, tools, and real-time experiments inside a custom C++ engine. 
**ZenVM** is its bytecode compiler and virtual machine written in C++.

It is designed to be fast and focused on game creation and simulations, taking inspiration from the simplicity of Lua, the syntax of C++/JS, and the powerful cooperative process system of the classic *DIV Games Studio*.

---

## ✨ Main Features

- **Cooperative Processes & Concurrency:** The heart of the language. Spawn game entities with `spawn` and manage their lifecycle cooperatively using `frame` and `loop`.
- **Zero-Copy C++ Integration (Native Structs):** Bind C++ structures directly to the script. Values are read from raw memory buffers without boxing or intermediate conversions (ideal for *Raylib*-style patterns).
- **Object-Oriented & Operators:** Support for single-inheritance, polymorphism via vtables, and operator overloading (`__add__`, `__sub__`, etc.).
- **Advanced Memory Management:** Asynchronous Tri-Color Garbage Collector (similar to Lua 5.x) that minimizes execution pauses.
- **Data Structures:** Dynamic Arrays, Maps (O(1) Hash Tables), Sets, and *Typed Buffers* (`Float32Array`, `Uint8Array`) for low-level, high-performance data.
- **Zen IDE:** Includes a fully-featured code editor written in Python/PyQt6, with multi-tab support, syntax highlighting, bytecode dumping, and integrated execution.

---

## 💻 A Taste of the Code

Here is a classic example of how **Processes** work in BuLang. The code runs in cooperative parallel:

```zen
// A game entity that falls with gravity
process ball(x, y, vx, vy) {
    loop {
        vy += 0.5;      // Gravity
        x += vx;
        y += vy;

        // Simple collision
        if (y > 720) { 
            y = 720;  
            vy = -vy * 0.7; // Bounce
        }

        print("Ball at: ({int(x)}, {int(y)})");
        
        // Yield control to the engine until the next frame
        frame;
    }
}

// Spawn multiple balls running in parallel
ball(100, 0,  2, 0);
ball(200, 0, -3, 1);
```

## Tutorial index
---

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
## 🛠️ Começar a usar

## Running examples
### 1. Compilar a VM (C++)
*(Instruções de compilação dependem do teu sistema de build, ex: CMake ou Makefile)*
```bash
# Exemplo de compilação
make
```
Isto irá gerar o executável `zen` na pasta `bin/`.

Adjust the command to match your executable name:
### 2. O IDE (Zen IDE)
O projeto inclui um editor próprio para facilitar o desenvolvimento. Requer `Python 3` e `PyQt6`.

```bash
zen examples/tutorial_01_variaveis.zen
pip install PyQt6
python tools/editor.py --zen ./bin/zen
```
No IDE podes:
- Escrever código com formatação e *syntax highlighting*.
- Executar scripts diretamente com a tecla `F5`.
- Inspecionar o *disassembly* de bytecode (`F8`).
- Compilar para binário `.znb` (`F9`).

or:
### 3. Linha de Comandos
Podes correr ficheiros `.zen` ou gerar bytecode pela linha de comandos:

```bash
bulang examples/tutorial_01_variaveis.zen
# Correr um script
./bin/zen meu_script.zen

# Executar uma linha de código (-e)
./bin/zen -e 'print("Olá Mundo!");'

# Ver disassembly e compilar para bytecode .znb
./bin/zen --dis --dump my_script.znb meu_script.zen
```

## Suggested command line help
---

```bash
zen --help
zen --help variables
zen --help functions
zen --help arrays
zen --help processes
zen --examples
```
## 📚 Tutoriais e Documentação

Explora a pasta `docs/tutorials/` para aprenderes mais sobre a linguagem:

- `04-arrays.md` - Arrays e manipulação de listas.
- `07-processos-e-concorr-ncia.md` - Como gerir Processos (Fibers).
- `10-closures-e-upvalues.md` - Funções de primeira classe e Closures.
- `12-typed-buffers.md` - Buffers de memória eficientes (`Int32Array`, `Float32Array`).

---

## 🏗️ Arquitetura da Máquina Virtual

A arquitetura encontra-se em `libzen/`. Alguns pontos altos de design:
- `object.h` define a taxonomia de memória baseada em `ObjType`.
- Todo o *heap* é gerido pelo coletor de lixo em cadeia ligada (linked list de *gc_next*).
- **Fibers** (`ObjFiber`) mantêm a sua própria *call stack* e *register stack* independentes, permitindo que processos fiquem "adormecidos" sem bloquear a thread do SO.
- Emissão de bytecode usa uma interface `Emitter` segura contra realocações de memória, emitindo opcodes de 32-bits (semelhante ao ARM/Lua).

---

## 🐛 Estado Atual / Roadmap

A linguagem está em desenvolvimento ativo. Podes consultar o ficheiro `BUGS.md` para um histórico atualizado do trabalho em curso.

**Foco atual:**
- Refinamento do sistema de Classes e *Type Hints* no compilador.
- Estabilização do *Operator Overloading*.
- Importação de módulos nativos (Plugins C++ e scripts externos).

---

## Licença
Este projeto encontra-se em desenvolvimento e é distribuído "as is" para fins de estudo e prototipagem.
