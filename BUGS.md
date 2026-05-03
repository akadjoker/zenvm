# BUGS

## 1. Regressão no compilador: campo de classe perde hint de tipo

- Estado: ABERTO
- Área: compilador
- Regra: corrigir no compilador, não usar remendo na VM

### Sintoma

- [`tests/test_bytecode_classes_nested.zen`](tests/test_bytecode_classes_nested.zen) produz:
  - `Sprite(hero@nil)`
  - `Sprite(hero@0)`
- O erro já aparece no source normal, antes do roundtrip de bytecode.

### Causa provável

- Em `libzen/src/compiler_expressions.cpp`, `dot_expr()` emite `OP_GETFIELD` / `OP_GETFIELD_IDX`,
  mas o registo resultante não volta a receber hint de classe.
- Depois, `binary()` decide mal entre opcode base e opcode de objeto.
- Exemplo afetado:
  - `self.pos = self.pos + delta`
  - `"Sprite(" + self.name + "@" + self.pos + ")"`

### Impacto

- A expressão entre objetos pode cair no caminho base.
- Stringificação de objetos dentro de expressões compostas degrada para `nil`, `0` ou representação genérica.


## 2. Alteração indevida em `VM::invoke()`

- Estado: REVERTER / CONGELADO
- Área: VM
- Ficheiro: `libzen/src/vm.cpp`

### O que foi feito

- `VM::invoke(const char *method_name, ...)` passou a desviar nomes de operators para `invoke_operator()`.

### Porque isto é problema

- O trabalho de operators foi adiado.
- Esta alteração mete lógica de feature nova dentro da VM sem necessidade.
- Não resolve o bug principal descrito acima.

### Ação

- Reverter esta alteração antes de continuar qualquer trabalho de operators.


## 2.b. Bugs de operators anotados mas adiados

- Estado: ADIADO
- Prioridade: baixa por agora
- Regra: não mexer nisto nesta fase, apenas registar

### Notas

- Qualquer bug ligado a `__add__`, `__sub__`, `__mul__`, `__div__`, `__mod__`, `__eq__`, `__lt__`, `__le__`, `__str__`
  ou reflected operators fica fora do foco atual.
- Não usar isto como desculpa para mexer na base da VM.
- Se algum teste falhar por causa de operator overloading, registar aqui e deixar para fase própria.

### Bugs/regressões já observados e adiados

1. Desvio indevido de method name para operator path em `VM::invoke()`.
2. Heurística agressiva de object-op em `Compiler::binary()`.
3. Casos de `__str__` em expressões mistas com strings podem mascarar outros bugs de compilação.
4. Roundtrip de bytecode com classes que dependem de operators ainda não deve ser usado como referência principal.

### Política temporária

- Ao investigar falhas atuais, separar sempre:
  - bugs de acesso a campos / type hints / emissão de bytecode;
  - bugs de operator overloading.
- Se a falha depender de operators, marcar como `ADIADO`.


## 3. Cobertura de bytecode ainda insuficiente para módulos e plugins

- Estado: ABERTO
- Área: testes de bytecode

### Já existe

- `tests/test_bytecode_roundtrip.zen`
- `tests/test_bytecode_processes.zen`
- `tests/test_bytecode_classes_nested.zen`
- `tests/test_bytecode_cli.sh`

### Falta adicionar

- roundtrip de `import math`
- roundtrip de `using math`
- roundtrip de plugin `import hello`
- validação explícita de funções nativas / constantes nativas após dump + load


## 4. Snapshots de GC não são determinísticos

- Estado: ABERTO / TEMPORARIAMENTE SKIP
- Ficheiros:
  - `tests/test_gc.zen`
  - `tests/test_gc2.zen`
  - `tests/snapshot_skip.txt`

### Sintoma

- Valores como `mem_used()`, `objects`, `freed` e `after` variam entre execuções.
- O comportamento funcional parece correto, mas a snapshot textual falha.

### Ação

- Ou converter estes testes para invariantes/faixas estáveis,
- ou mantê-los fora da snapshot suite.


## 5. `test_bytecode_classes_nested` está bloqueado pelo bug de hints

- Estado: BLOQUEADO

### Nota

- Enquanto o bug do item 1 existir, este teste não valida bytecode de forma limpa,
  porque já falha na execução normal do source.
