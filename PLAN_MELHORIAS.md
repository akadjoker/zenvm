# zenvm — estado, correcções e plano

Sessão de 2026-09-07. Tudo o que está aqui foi medido ou reproduzido nesta
árvore, não é herdado do `bulangbugs.md` (esse foi escrito noutra máquina e
metade já não se aplica — ver secção 4).

---

## 1. Performance: zenvm está à frente do zenpy, não atrás

A expectativa era o contrário. Não é. Mesmos algoritmos, um em sintaxe zen,
outro em sintaxe python, os dois em Release `-O3`, melhor de 3:

| workload | zenvm | zenpy | |
|---|---:|---:|---|
| aritmética float, 1M iterações | 0.046 | 0.058 | zenvm 1.3x |
| chamada de função, 3M | 0.056 | 0.081 | zenvm 1.4x |
| método + campos, 2M | 0.072 | 0.091 | zenvm 1.3x |
| array push + index, 2M | 0.060 | 0.077 | zenvm 1.3x |
| mapa com chaves string, 200k | 0.095 | 0.159 | zenvm 1.7x |

Faz sentido: o zenpy paga semântica de Python que o zenvm não tem (inteiros
que são bool, comparação lexicográfica de listas, `%` floored, repr de float
compatível). O zenvm também tem opcodes que o zenpy não tem: `OP_CALLGLOBAL`,
os fundidos `OP_GETFIELD_MUL`/`SUB`, e os matemáticos directos (`OP_SIN`…).

**Conclusão prática**: não há dívida de performance a pagar aqui. O que o
zenpy tem e vale a pena trazer é outra coisa — ver secção 5.

---

## 2. Bugs corrigidos nesta sessão

### 2.1 Três stack buffer overflows no compilador (crash com código normal)

Encontrados com um build ASan/UBSan sobre ficheiros gerados. Não precisam de
código malicioso, só de um ficheiro grande:

- `struct` com mais de 64 campos → estoura `char fields[64][64]`
- `class` com mais de 64 campos → o mesmo buffer, e a cópia dos campos do pai
  podia estourar `ClassFieldInfo::fields` sozinha
- cadeia `if/elif` com mais de 64 ramos → estoura `end_jumps[64]`

Agora dão erro de compilação. O limite de ramos subiu para 255: 64 ramos numa
cadeia não é absurdo. O `switch` tinha um `if (end_count < 64)` silencioso que
deitava fora os jumps a partir do 64.º e compilava mal o resto — também dá
erro agora.

### 2.2 Argumentos trocados ao chamar o resultado de um método

```zen
var m = {};
m.set("sub", def(a, b) { return a - b; });
print(str(x) + str(m.get("sub")(10, 3)));   // dava -10, não 7
```

O invoke move o resultado para `dest` mas deixava o `next_reg` acima dos
registos que tinha usado. A chamada seguinte alocava os argumentos a partir
daí, e o `OP_CALL` exige-os imediatamente acima do callee: o callee lia um
registo velho como primeiro argumento e todos os outros deslizavam.

Só aparece quando a chamada é operando de outra coisa. Sozinha, `dest` e a
base do invoke coincidem e nada se move. A soma também esconde o bug — é por
isso que no `test_closures_advanced.zen` o `add(3,4)` dava certo e o
`sub(10,3)` dava -10.

### 2.3 Tabela de nomes do disassembler dessincronizada

Faltava `"IDIV"` e mais quatro entradas finais. Todo o opcode a partir do
índice 12 imprimia com o nome errado (`OP_IDIV` como "NEG", `OP_NEG` como
"ADD_OBJ", …) e os últimos quatro davam `???`. Regenerada a partir do enum,
com um `static_assert` a prender o tamanho ao `OP_HALT` para não voltar a
divergir.

### 2.4 `VM::error_msg_` truncava

Buffer de 256 bytes a receber uma mensagem construída num de 512: erros
longos chegavam cortados ao `pcall()`.

---

## 3. Performance: privates de processo em O(1)

`OP_PROC_GET`/`OP_PROC_SET` resolviam o processo corrente com `find_slot()`,
uma varredura linear do pool, **em cada leitura e escrita de private**. Dentro
de um `process` todo o `x`, `y`, `angle` nu é uma destas: o custo de um frame
crescia com o número de processos vivos. Quadrático precisamente naquilo que
um motor estilo DIV existe para escalar.

O escalonador já guarda o índice do slot corrente (`current_slot_idx_`) e o
`current_slot()` lê-o em O(1). Passou a ser usado para o caso `self`; só
`father`/`son` continuam a varrer, e agora varrem uma vez em vez de duas.

200 ticks, cada processo a ler e escrever dois privates por frame:

| processos | antes | depois | |
|---:|---:|---:|---|
| 100 | 0.0028s | 0.0008s | 3.6x |
| 500 | 0.0406s | 0.0039s | 10.5x |
| 2000 | 0.8356s | 0.0176s | **47x** |

O custo por processo passou a ser constante em vez de linear no tamanho do
pool.

---

## 4. Suite de testes: era inútil como rede de segurança, agora não é

Estava em `passed=47 failed=27`. Nenhuma das 27 era regressão do VM:

- 12 snapshots tinham o caminho absoluto da máquina antiga (`/media/ctw04578/...`)
  cozido lá dentro — nunca podiam passar aqui
- 8 tinham tempos de execução gravados (`seconds=0.577354`)
- 7 scripts nunca tiveram snapshot
- 13 precisam de um runner com gráficos (`zen_game`, `zen_gl`) ou de um módulo
  opcional; falhavam com "undefined variable" no CLI simples
- 2 (`struct_test`, `struct_native_binding`) chamam `Color()`, um struct nativo
  que **nenhum host neste repositório regista** — nunca correram

Agora: **59 passam, 0 falham, 19 skipped com razão escrita**. O runner
normaliza o caminho dentro dos tracebacks, mascara tempos e tira linhas em
branco à cabeça, por isso um snapshot é comparável entre máquinas.

As suites de shell (`tests/*.sh`) já estavam boas mas o default é
`./build/zen`, que não existe — é preciso `./tests/xxx.sh ./bin/zen`. Com o
binário certo: **320 asserções, todas passam.**

`tests/test_edge_cases.sh` pendura para sempre no primeiro teste: espera
"stack overflow" de `def boom() { return boom(); }`, mas isso é uma tail call
e o VM tem tail calls próprias, logo é um ciclo infinito. O VM está certo, o
teste é que está mal escrito.

### Auditoria antiga (`bulangbugs.md`)

Verificada uma a uma contra o código actual: **10 das 25 já estavam
corrigidas** (4, 5, 5b, 7, 8, 9, 14, 17, 18, 19). Das 15 que restam, as mais
concretas:

- **#12** (alta): `OP_GETFIELD_MUL`/`SUB` não verificam tipo nem bounds, ao
  contrário do `OP_GETFIELD_IDX` não fundido que tem as duas guardas. O
  peephole continua a emiti-los.
- **#11**: `OP_INVOKE_VT` faz `as_instance()` sem verificar.
- **#13**: `OP_APPEND`/`OP_SETADD` idem.
- **#1, #2, #10** (GC): `OBJ_STRUCT` não marca `s->def`, `OBJ_STRUCT_DEF` não
  marca `name` nem `field_names`. Reais no código, mas hoje inalcançáveis do
  script: `struct_declaration()` regista sempre a def como global, por isso
  está sempre viva. Passam a importar no dia em que houver structs locais ou
  defs criadas por natives.
- **#6**: `OP_FORLOOP` trunca `int64_t` para `int32_t` — ciclos com valores
  acima de 2^31 comportam-se mal.
- **#20**: `OP_DIV` não verifica divisão por zero (dá inf), enquanto o
  `OP_MOD` verifica. Inconsistente.

---

## 5. Plano, por ordem de valor

### Já feito nesta sessão
1. ~~Overflows do compilador~~
2. ~~Argumentos trocados na chamada encadeada~~
3. ~~Privates O(1)~~
4. ~~Suite a funcionar como gate~~

### A seguir

**5.1 Guardas nas super-instruções (#12, #11, #13).** É a família que resta
com crash reproduzível: um `as_instance()` sobre um `nil` é um segfault, não
um erro. O `OP_GETFIELD_IDX` já mostra como fazer (`is_instance` + bounds);
copiar isso para os fundidos. Custo: um branch previsível num sítio quente.
Medir o bunnymark antes e depois; se custar, emitir o fundido só quando o
compilador tem hint de classe.

**5.2 Fuzz + ASan como rotina.** O zenpy tem isto (`tests/fuzz`, libFuzzer,
fuzzer de mutação) e apanhou coisas reais. O zenvm não tem nada. Os três
overflows desta sessão saíram de gerar ficheiros à mão em cinco minutos —
um fuzzer de mutação sobre `tests/*.zen` encontra mais. Um build ASan já
existe agora (`build_asan`, `bin/zen_asan`).

**5.3 Truncações int64→int32 (#6, #24).** Mecânicas e isoladas.

**5.4 `find_slot` para `father`/`son`.** O `self` já é O(1); estes continuam
lineares. Com hierarquias fundas e muitos processos volta a doer. Um índice
id→slot (ou guardar o índice no próprio slot e validar o id) resolve.

**5.5 Comportamento com pai morto.** Hoje escrever num `father` que já morreu
é silenciosamente ignorado e ler dá `nil`, sem erro:

```zen
process filho() { father.x = 999; frame; print(father.x); }  // nil
```

Isto está no próprio `tutorial_07_processos.zen` e imprime `nil` onde o
comentário do tutorial diz 999. Em DIV é um footgun clássico. Decidir: erro,
ou `nil` documentado. Neste momento não é nenhum dos dois, é acidente.

**5.6 Makefile.** Aponta para `src/`, que não existe desde que tudo passou
para `libzen/src/`. `make` falha logo. Apagar ou arranjar.

**5.7 Build por omissão.** `CMAKE_BUILD_TYPE` vazio dá Debug, e Debug liga
ASan+UBSan à força. Quem faz `cmake -B build && make` fica com um binário
sanitizado e `-O0` e mede tempos errados. Default para Release e uma opção
explícita para os sanitizers.

### Não fazer agora

- **Micro-optimizar o dispatch.** O zenvm já ganha ao zenpy em tudo o que
  medi. O tempo rende mais em correcção (5.1, 5.2) do que em nanosegundos.
- **`<T>` reificados.** O `<T>` do zenvm é açúcar puro: `f<T>(a)` vira
  `f(T, a)`, sem suporte no VM. O modelo do zenpy (`generic_arity` separado,
  `OP_CALL_GENERIC`) é bem mais forte, mas é uma mudança de ABI e de bytecode.
  Vale a pena, mas depois de 5.1 e 5.2.

---

## 6. O que trazer do zenpy

Não é performance. É infraestrutura:

- **`zen_bind.hpp`** — bindings gerados da assinatura C++, com erro de tipo
  automático a nomear o argumento. O zenvm tem `ClassBuilder` cru; com 14
  módulos nativos, a poupança é grande.
- **Fuzzing e modos de teste** — `--stress-gc`, `--bytecode` (round-trip),
  `--switch-dispatch` (o caminho que o MSVC compila). Cada um destes apanhou
  bugs reais no zenpy.
- **`docs/internals/`** — uma página por subsistema, com os nomes reais do
  código.

E na direcção contrária, o que o zenvm tem e o zenpy não: o **modelo de
processos**. `process`, `frame`, `father`/`son`, `signal`, privates DIV,
escalonador com frame-speed. Funciona, está testado, e é a razão de ser desta
linguagem. Depois do 5.4 e 5.5 fica sólido.
