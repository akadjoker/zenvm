# Duas VMs, um plano — fecho de 2026-09-07

O zenpy é um fork do zenvm. O VM é o mesmo; divergiram no compilador e nas
ferramentas. Hoje ficou provado que **nenhuma das duas é intrinsecamente mais
rápida**: cada uma tinha optimizações que a outra não tinha, e as diferenças
que restam são de front-end, não de motor.

A regra daqui para a frente: **o que uma aprende, a outra herda.**

---

## Onde estamos (mesma máquina, melhor de 3, checksums iguais nas 5)

| segundos | astar | dijkstra | quadtree | octree | hanoi | floodfill |
|---|---:|---:|---:|---:|---:|---:|
| **zen** | **0.042** | **0.152** | 0.041 | 0.059 | 0.051 | 0.011 |
| **zenpy** | 0.046 | 0.162 | 0.042 | **0.055** | **0.042** | 0.011 |
| Lua 5.4 | 0.044 | 0.163 | 0.055 | 0.077 | 0.067 | **0.009** |
| Wren 0.4 | 0.062 | 0.333 | 0.072 | 0.100 | 0.075 | 0.020 |
| CPython 3 | 0.070 | 0.253 | 0.077 | 0.115 | 0.091 | 0.027 |

As duas batem Lua e Wren em quase tudo. O zen ganha no pathfinding, o zenpy
nas árvores e no Hanói. Só o flood fill fica marginalmente para o Lua.

---

## O que foi feito hoje

### zenvm (branch `perf/compiler-codegen`)

Correcções, todas com teste:
- **Três stack buffer overflows no compilador** (struct/class com >64 campos,
  cadeia `if/elif` com >64 ramos). Rebentavam com código normal. Agora dão
  erro; o limite de ramos subiu para 255. O `switch` tinha uma guarda
  silenciosa que miscompilava a cauda de switches longos.
- **Argumentos trocados** ao chamar o resultado de um método dentro de outra
  expressão (`str(x) + str(m.get("sub")(10, 3))` dava -10 em vez de 7).
- **Tabela de nomes do disassembler** dessincronizada do enum: faltava
  `IDIV` e quatro entradas, por isso tudo a partir do índice 12 imprimia com
  o nome errado. Regenerada, com `static_assert` a prendê-la ao `OP_HALT`.
- **`error_msg_` truncava** erros longos (256 contra 512 bytes).

Performance:
- **Privates de processo em O(1)**: eram uma varredura linear do pool em cada
  leitura/escrita, ou seja quadrático no número de processos vivos. Com 2000
  processos, **47x mais rápido**, e o custo por processo passou a ser
  constante.
- **Três optimizações de codegen** trazidas do zenpy: fusão comparação+salto
  (o `LTJMPIFNOT` já existia no VM e nunca era emitido), retarget da
  aritmética para o registo destino (mata o `MOVE`), imediatos `ADDI`/`SUBI`.
  Ciclo `while` de 5M: 0.110 → 0.034.

Infraestrutura:
- **Suite reparada**: estava 47 passa / 27 falha, e nenhuma das 27 era
  regressão (snapshots gravados noutra máquina com caminhos absolutos,
  tempos gravados, testes que precisam do runner gráfico). Agora **59/0**,
  com 19 skipped e razão escrita.
- **Benchmarks contra Lua e Wren** (`bench/algo`), seis algoritmos, com
  tabela de checksums para a comparação ser verificável.
- **Profiler de opcodes** portado do zenpy.
- **`std` removido**: 11 usos em 4 ficheiros substituídos por `ct::String` e
  helpers próprios. `grep -rn "std::"` está vazio.

### zenpy (branch `perf/value-size`)

- **`Value` de 24 para 16 bytes.** A small-string optimization declarava um
  byte de padding que nunca era lido, e esse byte empurrava o union para 16
  e o `Value` para 24. Cada acesso a registo movia 50% mais memória. Isto
  explicava quase toda a diferença que parecia ser "semântica de Python":
  soma em ciclo 0.053 → 0.049, método em receptor tipado 0.100 → 0.079.

---

## Plano

### 1. Paridade de opcodes (zenvm ← zenpy)

O zenpy tem seis que o zenvm não tem. Por ordem de valor:

| opcode | o que faz | porquê |
|---|---|---|
| `RETURNNIL` | return sem valor | funções `void` são a maioria |
| `LTIJMPIFNOT`, `GTIJMPIFNOT` | compara com imediato e salta | `i < 10` sem carregar o 10 |
| `EQJMPIFNOT`, `NEJMPIFNOT` | `==`/`!=` fundidos com o salto | o zenvm só tem `<` e `<=` |
| `JMPIFNIL` | salta se nil | testes de nulidade |
| `INVOKE_VT_FAST` | invoke sem verificação de aridade | métodos em receptor tipado |

Todos já têm handler escrito e testado no zenpy. É copiar handler + emissão.

### 2. Guardas nas super-instruções (zenvm)

`OP_GETFIELD_MUL`/`SUB`, `OP_INVOKE_VT`, `OP_APPEND`/`SETADD` fazem
`as_instance()`/`as_array()` sem verificar. Um `nil` ali é segfault, não
erro. O `OP_GETFIELD_IDX` não fundido já mostra como fazer. **É o único
grupo com crash reproduzível que resta.**

### 3. Infraestrutura de teste (zenvm ← zenpy)

O zenpy tem e o zenvm não: `--stress-gc`, `--bytecode` (round-trip),
`--switch-dispatch` (o caminho que o MSVC compila), `tests/fuzz` (libFuzzer
+ fuzzer de mutação), `docs/internals`. Cada um destes apanhou bugs reais no
zenpy. Os três overflows de hoje saíram de gerar ficheiros à mão em cinco
minutos; um fuzzer encontra mais.

### 4. `zen_bind.hpp` (zenvm ← zenpy)

Bindings gerados da assinatura C++, com erro de tipo automático a nomear o
argumento. O zenvm tem 14 módulos nativos escritos à mão — é onde poupa mais.

### 5. O que o zenpy pode aprender do zenvm

- **`FORPREP`/`FORLOOP` mais barato**: o do zenvm escreve dois registos por
  iteração, o do zenpy três (contador, restante, variável visível). Quando o
  corpo não reatribui a variável do ciclo — o caso normal — dá para usar o
  contador directamente. Vale ~13% num ciclo apertado. **Por medir se
  compensa a complexidade.**
- **Tail calls** (`OP_TAILCALL`): o zenvm tem recursão de cauda ilimitada, o
  zenpy não.
- **Dispatch mais pequeno**: o `execute` do zenpy tem 114 KB contra 56 KB, e
  o `OP_ADD` sozinho tem 211 linhas contra 40. Testei com PGO e **não deu
  ganho**, por isso não é prioridade — mas é dívida de legibilidade.

### 6. Higiene das duas

- **Merge dos branches** para main: `perf/compiler-codegen` (zenvm),
  `perf/value-size` (zenpy).
- **zenvm**: o `Makefile` aponta para `src/`, que não existe desde que tudo
  passou para `libzen/src/`; `make` falha logo. E `CMAKE_BUILD_TYPE` vazio dá
  Debug com ASan+UBSan à força, por isso quem faz `cmake -B build && make`
  mede tempos errados.
- **zenvm**: escrever num `father` já morto é silenciosamente ignorado e ler
  dá `nil`, sem erro. Está no próprio tutorial dos processos, que imprime
  `nil` onde o comentário diz 999. Decidir: erro, ou `nil` documentado.
- **`tests/test_edge_cases.sh`** pendura para sempre: espera "stack overflow"
  de `def boom() { return boom(); }`, mas isso é tail call e o VM tem tail
  calls. O VM está certo, o teste está mal escrito.

---

## O método que funcionou hoje, para repetir

Falhei três hipóteses antes de encontrar o `Value` de 24 bytes. Achei que era
o `range()`, o `val_int` no `FORLOOP`, e a cache de instruções. Testei as
três e as três estavam erradas — o PGO, em particular, mostrou que o
tamanho do dispatch não custava nada.

O que resolveu foi o **profiler de opcodes**: mostrou o mesmo número de
dispatches nos dois VMs para o mesmo ciclo, com o dobro do tempo. Isso
excluiu o compilador e apontou para o custo por acesso a registo, o que levou
ao `sizeof(Value)`.

Medir antes de mexer, e guardar as hipóteses que falharam
(`zenpy/PERF_GAP_ZENVM.md`) — valem tanto como as que acertaram.
