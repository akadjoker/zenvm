# Plano de Otimizacao da DIV VM

## Objetivo

Reduzir lookups e indirecoes no runtime da VM, movendo a resolucao de funcoes e processos para compile time sempre que possivel, sem quebrar a semantica atual do DIV.

## Motivacao

O benchmark de `fib` mostrou que o custo principal continua no hot path da execucao:

- dispatch por opcode
- acesso a variaveis locais
- custo de chamada de funcao
- lookups de metadata em runtime

Ja houve ganho real ao cachear `local_slots`, por isso o proximo passo deve atacar o mesmo tipo de overhead: lookup de metadata por mapa no caminho quente.

## Estado Atual

- `FUNC_CALL` recebe `addr` no bytecode
- o runtime usa `function_locals_map.find(addr)` para descobrir `callee_local_count`
- `SPAWN` usa `entry_point` e metadata indireta para localizar layout de locals
- a `symtable` pertence ao compilador, mas ainda ha indirecoes no runtime baseadas em `addr`

## Direcao Tecnica

Separar claramente compilacao e runtime:

- compilador resolve nomes com `symtable`
- compilador atribui ids compactos estaveis
- bytecode passa a guardar ids, nao nomes nem metadata indireta
- runtime usa acesso direto por indice

## Fase 1: Funcoes por `function_id`

### Meta

Substituir `addr + function_locals_map.find(addr)` por `function_id + tabela direta`.

### Estrutura proposta

```cpp
struct FunctionInfo {
    int entry_ip;
    int local_count;
    std::string name;
};
```

No `VM`:

```cpp
std::vector<FunctionInfo> functions;
```

### Mudancas

1. O compilador atribui um `function_id` sequencial a cada funcao.
2. O compilador preenche `vm.functions[function_id]` com `entry_ip` e `local_count`.
3. O opcode `FUNC_CALL` passa a codificar `function_id` e `nargs`.
4. O runtime faz:

```cpp
int function_id = code[ip++];
int nargs = code[ip++];
const FunctionInfo& fn = functions[function_id];
int addr = fn.entry_ip;
int callee_local_count = fn.local_count;
```

### Beneficio esperado

- remover `unordered_map.find()` do hot path de chamada
- reduzir branches e indirecoes no `fib`
- preparar o caminho para dispatch mais agressivo

## Fase 2: Processos por `process_type_id`

### Meta

Separar tipo de processo de instancia viva.

### Distincao necessaria

- `process_type_id`: identidade estatica do processo compilado
- `pid`: identidade da instancia viva em runtime

### Estrutura proposta

```cpp
struct ProcessTypeInfo {
    int entry_ip;
    int local_init_offset;
    int local_count;
    std::string name;
};
```

No `VM`:

```cpp
std::vector<ProcessTypeInfo> process_types;
```

### Mudancas

1. O compilador atribui `process_type_id` a cada `process`.
2. O opcode `SPAWN` passa a carregar `process_type_id`, prioridade e `nargs`.
3. `spawn_from_values()` passa a receber o id do tipo de processo.
4. O runtime usa `process_types[process_type_id]` para obter `entry_ip`, locals e nome base.

### Beneficio esperado

- eliminar metadata indireta por `entry_point`
- simplificar `SPAWN`
- manter `pid` apenas para pai/filho, scheduler e acesso a instancias vivas

## Fase 3: Dispatch

Depois de estabilizar ids e tabelas diretas:

1. testar `goto dispatch table` com fallback para `switch`
2. medir apenas em release
3. manter fallback portavel para compiladores sem suporte

## Validacao

Cada fase deve ser validada com:

1. build release limpo
2. `--fib-bench 30 1`
3. `--fib-bench 30 5`
4. demo normal com `--quiet`

Se houver mudanca estrutural em headers, fazer sempre rebuild limpo porque o Makefile nao tem dependency tracking completo.

## Riscos

- misturar `function_id` com `entry_ip` e manter semantica duplicada durante a transicao
- confundir `process_type_id` com `pid`
- quebrar disassembly se ele assumir que operandos de `FUNC_CALL` e `SPAWN` sao enderecos
- introduzir caches invalidos atravessando `SPAWN`, que pode realocar `processes`

## Ordem Recomendada

1. implementar `FunctionInfo` e migrar `FUNC_CALL` para `function_id`
2. medir `fib`
3. implementar `ProcessTypeInfo` e migrar `SPAWN` para `process_type_id`
4. medir demo e comportamento de processos
5. so depois testar dispatch por labels-as-values