# ZEN — Estado Actual e Plano de Estabilização

## O que está feito e FUNCIONA (testado):
- ✅ `print(42);` → imprime "42\n"
- ✅ `var x = 42;` → compila sem crash
- ✅ Lexer: todos tokens, Unicode, string interpolation
- ✅ Compiler: canAssign (assignment a = b, a.x = y, a[i] = z, +=/-=/etc)
- ✅ Compiler: do_while com JMPIF condicional
- ✅ CMake Debug com ASan+UBSan
- ✅ `./zen -e "code"` funciona

## O que NÃO funciona / Bugs conhecidos:

### Bug 1: `print(1, 2, 3);` — output vazio
- Multi-arg print não imprime nada
- Causa provável: `expression(-1)` com `-1` como dest está a usar registos temporários
  que sobrescrevem uns aos outros entre iterações do do-while

### Bug 2: `var x = 10; x = x + 5; print(x);` — output vazio  
- Arithmetic + assignment + global variables
- Causa provável: `x = x + 5` pode não estar a emitir SETGLOBAL correctamente,
  ou o OP_ADD pode não ter o resultado no registo certo

### Bug 3: `zen_debug` binary não está no CMake
- CMakeLists.txt define `zen_debug` mas não está a buildar

## Erros que cometemos (APRENDER):
1. **new_func não inicializava code_capacity/const_capacity/lines/source** → segfault
2. **OP_TOSTRING adicionado a opcodes.h mas não à dispatch_table** → OOB access
3. **zen_main.cpp usava vm.init()/vm.free() que não existem** → VM usa construtor/destrutor
4. **Emitter default constructor não existia** → CompilerState não compilava

## REGRA FUNDAMENTAL:
> Quando adicionas um opcode a opcodes.h, TENS que:
> 1. Adicionar `&&lbl_OP_XXX` à dispatch_table (posição correcta!)
> 2. Adicionar `CASE(OP_XXX) { ... NEXT(); }` handler
> 3. Adicionar ao disassembler (debug.cpp)

## Plano de estabilização (próxima sessão):

### Fase 1: Debug do que temos (30 min)
1. Buildar `zen_debug` (com ZEN_DEBUG_TRACE_EXEC) 
2. Correr `print(42);` com trace → ver bytecode emitido
3. Correr `var x = 10; x = x + 5; print(x);` com trace → ver o que falha
4. Correr `print(1, 2, 3);` com trace → ver o que falha
5. Fixar cada bug um a um COM TESTE

### Fase 2: Testes unitários para o compilador (1h)
1. Criar `test_compiler.cpp` — compila strings, verifica bytecode emitido
2. Testar: literals, var decl, assignment, arithmetic, print
3. Testar: if/else, while, for, break/continue
4. Testar: functions, closures, upvalues
5. Testar: edge cases (panic mode, errors, loops com erros)

### Fase 3: Testes do lexer (30 min)
1. Criar `test_lexer.cpp` — tokeniza strings, verifica sequência de tokens
2. Unicode, interpolation, keywords, edge cases

### Fase 4: Primeiros programas reais (30 min)
1. hello.zen: `print("Hello, World!");`
2. fib.zen: fibonacci recursivo
3. loop.zen: while/for com break
4. interp.zen: string interpolation

### Fase 5: Style (pode ser depois)
- Allman braces reformat em todos os ficheiros

## Arquitectura (referência rápida):
```
Source → Lexer → Tokens → Compiler → ObjFunc (bytecode) → VM (execute)
                                  ↓
                              Emitter (emit_abc, emit_abx, jump patching)
```

## Ficheiros chave:
- `src/lexer.h` + `src/lexer.cpp` — Tokenizer
- `src/compiler.h` — Declarations
- `src/compiler.cpp` — Infrastructure (advance, consume, scope, registers)
- `src/compiler_expressions.cpp` — Pratt parser (prefix/infix)
- `src/compiler_statements.cpp` — Statements (var, def, if, while, etc)
- `src/emitter.h` + `src/emitter.cpp` — Bytecode emission
- `src/vm.h` + `src/vm.cpp` — VM lifecycle
- `src/vm_dispatch.cpp` — Opcode dispatch (computed goto)
- `src/opcodes.h` — All opcodes enum + encoding macros
- `src/object.h` — Object types (ObjFunc, ObjString, ObjArray, etc)
- `src/memory.h` + `src/memory.cpp` — GC, allocators, new_func, etc
- `src/zen_main.cpp` — CLI entry point (./zen file.zen, ./zen -e "code", REPL)
- `CMakeLists.txt` — Build system (Debug=ASan+UBSan, Release=O2)

## Build commands:
```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc) zen
ASAN_OPTIONS=detect_leaks=0 ./zen -e 'print(42);'
```
