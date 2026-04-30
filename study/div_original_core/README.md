# DIV Original Core (extraido do projeto)

Esta pasta contem copias dos componentes originais do DIV relacionados com linguagem e VM.

## Ficheiros

- `divc.cpp`
  - Compilador original (lexer + parser + geracao de codigo)
- `ltlex.def`
  - Definicoes de tokens/palavras reservadas/simbolos
- `ltobj.def`
  - Objetos e funcoes predefinidas do compilador
- `inter.h`
  - Definicoes de opcodes e estruturas globais do interpretador
- `i.cpp`
  - Loop principal da VM e escalonamento de processos
- `kernel.cpp`
  - Implementacao dos opcodes (switch gigante da VM)

## Pontos de entrada mais importantes

### Lexer/parser/compilador (original)
- `divc.cpp` -> `analiza_ltlex()` carrega `ltlex.def`
- `divc.cpp` -> `lexico()` le tokens
- `divc.cpp` -> `sintactico()` faz analise sintatica
- `divc.cpp` -> `g1()`, `g2()`, `gen()` geram/otimizam bytecode em `mem[]`

### VM (original)
- `inter.h` -> opcodes `lcar`, `ladd`, `ljmp`, `lcal`, `lret`, etc.
- `i.cpp` -> `interprete()`, `exec_process()`, `nucleo_exec()`
- `kernel.cpp` -> `case` de cada opcode

## Nota

A pasta `study/div_vm_lab` e didatica e simplificada.
Se queres estudar o comportamento real do DIV, usa esta pasta `study/div_original_core`.
