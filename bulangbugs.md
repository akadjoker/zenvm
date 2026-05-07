# BuLang VM — Bug Audit Report

Análise do código em `/media/ctw04578/data/projects/zen/zen/libzen`

---

## Critical (5)

### 1. GC: OBJ_STRUCT não marca o `def` pointer
**Ficheiro:** `libzen/src/memory.cpp` ~line 1425  
O `gc_blacken` para `OBJ_STRUCT` marca os field values mas nunca marca o `ObjStructDef*` via `s->def`. Se o StructDef só é alcançável por instâncias (não por global), o GC pode sweepá-lo → use-after-free.

**Fix:** Adicionar `gc_mark_obj(gc, (Obj *)s->def);` antes do loop de fields.

---

### 2. GC: OBJ_STRUCT_DEF não marca `name` nem `field_names`
**Ficheiro:** `libzen/src/memory.cpp` ~line 1422  
O case está vazio ("field names are interned strings, no GC refs") mas strings internadas SÃO `Obj*` geridos pelo GC. Se o GC as fizer sweep, o StructDef fica com dangling pointers.

**Fix:**
```cpp
case OBJ_STRUCT_DEF: {
    ObjStructDef *def = (ObjStructDef *)obj;
    gc_mark_obj(gc, (Obj *)def->name);
    for (int32_t i = 0; i < def->num_fields; i++)
        gc_mark_obj(gc, (Obj *)def->field_names[i]);
    break;
}
```

---

### 3. `free_obj` para OBJ_FUNC usa `code_count` em vez de `code_capacity`
**Ficheiro:** `libzen/src/memory.cpp` ~line 1516  
O emitter aloca com `code_capacity` (via realloc), mas free_obj passa `code_count` ao `zen_free`. Corrompe o counter de bytes alocados.

**Fix:** Usar `code_capacity` e `const_capacity` no free, ou shrink arrays em `Emitter::end()`.

---

### 4. OP_CALL com classes: sem bounds check no `stack_top`
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 1145  
`fiber->stack_top = new_frame->base + fn->num_regs` sem verificar se excede `fiber->stack_capacity`. Recursão profunda com funções de muitos registos → heap buffer overflow.

**Fix:** Adicionar check: `if (fiber->stack_top > fiber->stack + fiber->stack_capacity) RT_ERROR("stack overflow");`

---

### 5. OP_SUPER_INVOKE: avanço de `ip` errado para native calls
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 2222  
Após native method dispatch em super invoke, o código faz `ip += 2` mas a instrução é 3 words. O ip fica a apontar para word3 (constante) e tenta decodificar como opcode → crash.

**Fix:** O path nativo deve fazer `ip += 3` (skip all 3 words).

---

## High (7 + 1)

### 5b. OP_CALL destrói variáveis locais usadas como callee (CONFIRMADO em zenpy)
**Ficheiro:** `libzen/src/compiler_expressions.cpp` — `call_expr()`  
`OP_CALL base, nargs, nresults` escreve o resultado em `R[base]`. Se o callee é uma variável local (ex: nested function `inc` no reg 1), após a chamada `R[1] = return_value` (nil). A segunda chamada a `inc()` encontra nil → "attempt to call non-function".

**Reprodução:**
```python
def outer():
    def inc():
        print("hello")
    inc()
    inc()  # CRASH: R[base] foi overwritten com nil
outer()
```

**Fix:** No compiler, em `call_expr()`, se `callee < state_->next_reg` (é um registo ocupado), copiar para registo fresco antes de emitir OP_CALL:
```cpp
if (base < saved_next) {
    base = alloc_reg();
    emit_move(base, callee);
}
```

---

### 6. OP_FORLOOP trunca `int64_t` para `int32_t`
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 2612  
`int32_t counter = R[a].as.integer + R[a+2].as.integer;` — mas `.as.integer` é `int64_t`. Valores > 2^31 causam loops infinitos.

**Fix:** Usar `int64_t`.

---

### 7. OP_MOD trunca para `int32_t` antes do modulo
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 677  
`int32_t divisor = vc.as.integer;` trunca se > 2^31.

**Fix:** Usar `int64_t`.

---

### 8. OP_ABS trunca `int64_t` para `int32_t`
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 2556  
Cast para `(int32_t)(-(uint32_t)v.as.integer)` perde bits altos.

**Fix:** Usar `(int64_t)(-(uint64_t)v.as.integer)`.

---

### 9. `string_append_inplace` pode criar dangling refs após GC
**Ficheiro:** `libzen/src/memory.cpp` ~line 254  
Quando `zen_realloc` move o ObjString, o código patcha o GC linked list, mas Values no stack que apontam para o endereço antigo ficam dangling.

**Fix:** Só usar in-place append para strings com referência única (freshly created, não partilhadas). Considerar remover o path de realloc.

---

### 10. GC_NATIVE_STRUCT_DEF: não marca field `name` strings
**Ficheiro:** `libzen/src/memory.cpp` ~line 1437  
`NativeStructDef` contém `ObjString *name` e fields com `ObjString *name` — nunca marcados.

**Fix:** Marcar def->name e cada field_names[i].

---

### 11. OP_INVOKE_VT: sem type check no receiver
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 2123  
`ObjInstance *inst = as_instance(R[base]);` sem verificar se é instance. Se for nil/int → segfault.

**Fix:** Adicionar `if (!is_instance(R[base])) RT_ERROR(...);`

---

### 12. OP_GETFIELD_MUL/SUB super-instruções: sem bounds check
**Ficheiro:** `libzen/src/vm_dispatch.cpp` ~line 2638  
Cast direto sem verificar tipo nem bounds do field index. Peephole optimizer incorreto → crash.

**Fix:** Adicionar type guard + bounds check.

---

## Medium (7)

### 13. OP_APPEND/OP_SETADD: sem type check no receiver
`as_array(R[A])` sem verificar se é realmente array. Nil → crash.

### 14. `new_string_concat`: strings longas ficam com `hash = 0`
Hash 0 causa clustering em maps (todas no bucket 0 → O(n)). Duas strings iguais mas longas não comparam por pointer.

### 15. OP_CLOSURE pode trigger GC durante upvalue capture
Aloca upvalues array após criar closure. GC pode correr entre os dois allocs (safe por sorte, frágil).

### 16. `call_closure` em vm.cpp: `ret_reg` calculation via `frame_count - 2`
Pode ser out-of-bounds se chamado de C++ embedding com 1 frame.

### 17. `dump_value_rec`: map iteration sem null check em `map->nodes`
Crash para maps vazios com capacity > 0.

### 18. `LoopCtx::breaks[64]` overflow unchecked
Loop com >64 breaks → stack buffer overflow na compilação.

### 19. GC mark roots: pool fibers não são marcados como obj
`new_fiber` adiciona ao GC list mas `gc_mark_roots` não chama `gc_mark_obj` no fiber → pode ser swept.

---

## Low (6)

### 20. OP_DIV não verifica divisão por zero
Produz inf/NaN silenciosamente (inconsistente com OP_MOD que verifica).

### 21. `qsort` em array.sort usa `static bool s_desc`
Não é thread-safe.

### 22. `intern_table_grow` não verifica falha de `calloc`
NULL → crash.

### 23. `zen_alloc` não verifica falha de `malloc`
NULL → crash.

### 24. OP_FRAME_N: sem bounds check no `frame_speed`
Trunca `int64_t` para `int32_t`.

### 25. `UpvalDesc::index` é `uint8_t`
Limita upvalue capture a 256 — wrap silencioso acima disso.

---

## Resumo

| Severidade | Count | Áreas chave |
|------------|-------|-------------|
| Critical   | 5     | GC marking, memory accounting, stack overflow, wrong ip advance |
| High       | 7     | Truncação int64→int32, dangling ptrs, missing type checks |
| Medium     | 7     | Safety no dispatch, compiler bounds |
| Low        | 6     | OOM checks, thread safety |

**Os mais perigosos**: #1, #2, #5 (crashes reproduzíveis com código normal que use structs + GC pressure ou super calls para métodos nativos).

# Sessão 7 Mai 2026 — Bytecode Dump/Load Fix

## Contexto
- Zen VM: VM antiga (BuLang) adaptada para correr um subset de Python (.py)
- Register-based, 32-bit instructions, computed goto dispatch, C++11
- Bytecode serialization herdada do BuLang precisava de limpeza

## Bugs encontrados e corrigidos

### 1. Segfault no load — BuLang remnants no reader
**Sintoma**: `bin/zen /tmp/fib.zenbc` → segfault 139

**Causa**: O reader (`read_func`) ainda lia campos do BuLang que o writer já não escrevia:
- `u8 is_process` — conceito de "process" do BuLang (não existe em Zen)
- `u8[16] param_privates` — private params do BuLang

**Fix**: Removidos do reader. Também removidos:
- `BC_STRUCT_DEF` do enum ConstantTag (structs eram BuLang)
- `BytecodeStats.processes` (campo inútil)

### 2. Segfault em `resolve_native_globals()` — init_fn não chamado
**Sintoma**: Crash em `strcmp()` com null pointer ao iterar `lib->constants`

**Causa**: `register_lib()` apenas guarda o ponteiro da lib. As `math_constants[]` 
são inicializadas lazily por `math_lib_init()` (o `init_fn`), que só é chamado 
dentro de `open_lib_globals()`. Quando o bytecode é carregado, `resolve_native_globals()` 
itera todas as libs registadas incluindo constantes — mas `math_constants[].name` 
ainda era NULL porque `init_fn` nunca foi invocado.

**Fix**: No início de `resolve_native_globals()`, chamar `libs_[li]->init_fn(this)` 
para todas as libs registadas antes de iterar.

### 3. Default args e is_generator não serializados
**Sintoma**: 
- "expected at least 2 args but got 1" (tests 20, 22, 23)
- "yield outside of fiber" (test 19)

**Causa**: `write_func`/`read_func` não serializavam:
- `int32_t default_count` + `Value defaults[]` 
- `bool is_generator`

**Fix**: Adicionados ao formato após upvalues e antes de name/source:
```
[upvals...] | i32 default_count | [defaults × Value] | u8 is_generator | optional_string name | optional_string source
```

## Formato bytecode final (v2.1)

```
ZENBC (5 bytes magic)
u16 major (2)
u16 minor (1)  
u32 flags (0 ou BC_STRIP_DEBUG)
--- global_names ---
u32 count
per-global: u8 present, [string if 1], u8 has_value, [value if 1]
--- selectors ---
u32 count
per-selector: string name
--- main function (recursive) ---
```

Per function:
```
i32 arity
i32 num_regs
i32 code_count
i32 line_count (= code_count ou 0 se stripped)
i32 const_count
i32 upvalue_count
[code × u32]
[lines × i32] (se line_count > 0)
[constants × tagged_value]
[upvals × (u8 index, u8 is_local)]
i32 default_count
[defaults × tagged_value]
u8 is_generator
optional_string name
optional_string source
```

## Resultado final
- 25/25 testes .py passam from source ✓
- 25/25 testes passam bytecode round-trip ✓ (bench_math difere por timing)
- 24/24 testes C++ embedding passam ✓

## Ficheiros modificados
- `libzen/src/bytecode.cpp` — writer + reader (defaults, is_generator, remove BuLang)
- `libzen/src/vm.cpp` — `resolve_native_globals()` chama init_fn
- `libzen/include/zen/bytecode.h` — remove BytecodeStats.processes
- `cli/main.cpp` — remove printf processes

---

# Projecto zenpy — Análise Completa (Mai 2026)

Repositório: `/media/ctw04578/data/projects/zen/zenpy`  
Base: Zen VM (fork do BuLang) adaptada para correr Python (.py) nativo.

---

## Bugs Encontrados e Corrigidos no zenpy

### BUG-Z1 — Upvalue de parâmetro não fechado antes de MOVE no return
**Ficheiro:** `compiler_statements.cpp`, `compiler.cpp`  
**Estado:** CORRIGIDO

**Sintoma:** `make_adder(10)(5)` devolvia `5` em vez de `15`. Upvalues de locais de corpo funcionavam mas upvalues de **parâmetros** não.

**Causa:** `return_statement()` emitia `CLOSURE → MOVE R[0] ← closure → RETURN`. O `MOVE` destruía o valor de `x` (parâmetro em R[0]) **antes** de `OP_RETURN` fechar o upvalue. Parâmetros têm `scope_depth` exterior ao bloco, logo `end_scope()` não os fechava.

**Fix:** Adicionada `Compiler::close_captured_locals()` que emite `OP_CLOSE` no registo mais baixo com `captured=true`, chamada em `return_statement()` **antes** de qualquer MOVE para R[0].

**Impacto BuLang:** ⚠️ Este bug **existe no BuLang original**. Qualquer closure que capture um parâmetro (não uma local do corpo) e seja devolvida com `return` pode ter o mesmo problema.

---

### BUG-Z2 — Vararg pack bypassado pelo inline de OP_CALL
**Ficheiro:** `vm_dispatch.cpp`  
**Estado:** CORRIGIDO

**Sintoma:** `def greet(*names)` recebia a string `"Alice"` directamente em vez de `["Alice"]` — iterava caracteres.

**Causa:** `VM::call_closure()` em `vm.cpp` fazia o pack de `*args` num `ObjArray`, mas `CASE(OP_CALL)` em `vm_dispatch.cpp` tem um **hot path inline** que cria a `CallFrame` directamente sem chamar `call_closure()`.

**Fix:** Lógica de vararg pack duplicada no inline de `OP_CALL` em `vm_dispatch.cpp`.

**Convenção arity negativo:**
- `def f(*args)` → `fn->arity = -1`
- `def f(a, *args)` → `fn->arity = -2`
- No VM: `min_args = (-arity) - 1`

**Impacto BuLang:** ⚠️ BuLang tem varargs? Verificar se o mesmo hot path existe e se o pack é feito.

---

### BUG-Z3 — `CONFIGURE_DEPENDS` glob não detecta ficheiros novos
**Ficheiro:** `libzen/CMakeLists.txt`  
**Estado:** WORKAROUND CONHECIDO

**Sintoma:** Criar `builtin_math.cpp` novo não era compilado sem re-configure explícito.

**Workaround:** `cmake --fresh build` ou `cmake -B build` após adicionar `.cpp` novos.

---

### BUG-Z4 — `math_constants[]` static init com `val_float()` não é constexpr
**Ficheiro:** `libzen/src/builtin_math.cpp`  
**Estado:** CORRIGIDO

**Causa:** `Value` usa union — inicializar membro não-padrão estaticamente requer constexpr mas `val_float()` não é constexpr.

**Fix:** `init_fn` na `NativeLib` preenche o array lazily no primeiro import.

---

### BUG-Z5 — `OBJ_RANGE` em falta em switches → warnings / UB
**Ficheiro:** `debug.cpp`, `memory.cpp`, `vm_dispatch.cpp`  
**Estado:** CORRIGIDO

**Fix:** Adicionado `case OBJ_RANGE:` em todos os switches sobre `ObjType`.

---

### BUG-Z6 — OP_CALL destrói variável local usada como callee (idêntico ao #5b do audit)
**Estado:** DOCUMENTADO (ver secção High #5b acima)

Confirmado no zenpy: chamar uma nested function duas vezes crashava porque `R[base]` era overwritten com o valor de retorno na primeira call.

---

## Padrões Arquitecturais Identificados no zenpy

### Hot path duplicação em vm_dispatch.cpp
`OP_CALL` tem um caminho inline que replica (parcialmente) `VM::call_closure()`. Qualquer alteração a `call_closure()` (vararg pack, default args, stack check) **deve ser replicada** no inline. Risco de drift elevado.

### scope_depth de parâmetros vs corpo
Parâmetros: `depth = scope_depth` após o primeiro `begin_scope()`.  
Corpo da função: abre scope adicional via `colon_block()` → `end_scope()` só fecha locais com `depth > scope_depth_do_corpo`.  
Parâmetros ficam num scope exterior — apenas fechados em `OP_RETURN` ou por `OP_CLOSE` explícito.

---

## Features Python Portadas para o zenpy (não existiam no BuLang)

As seguintes features foram implementadas de raiz no zenpy e **seriam valiosas em BuLang**:

### ✅ Já implementadas no zenpy

| Feature | Notas |
|---------|-------|
| `def f(x=0)` — default params | Serializados no bytecode v2.1 |
| `return a, b` — tuple return | Multi-return limpo sem wrapper |
| `a, b = f()` — multi-assign | Desempacotamento de função e de literal |
| `def f(*args)` — variadics | arity negativo, pack em ObjArray |
| `f(*arr)` — unpack at call site | OP_UNPACK_CALL ou similar |
| `nonlocal x` — scope explícito | Upvalue write sem ambiguidade |
| `None` / `True` / `False` como keywords | Sem precisar de import |
| `not in` / `is not` | Operadores compostos |
| Chained comparisons `0 < x < 10` | Sem eval duplo do middle |
| f-strings `f"hello {name}"` | OP_FSTR ou concat inline |
| Slices `a[1:3]`, `a[::-1]` | OP_SLICE com step |
| Negative indexing `a[-1]` | Normalização no GETINDEX |
| List comprehensions `[x for x in arr if c]` | Fiber-like scope isolado |
| Dict/set comprehensions | Idem |
| Ternary `x if cond else y` | Sem nova keyword |
| `assert expr [, msg]` | OP_ASSERT |
| `//` floor div, `**` power | OP_IDIV, OP_POW |
| `//=`, `**=` augmented assign | |
| `enumerate()`, `zip()`, `map()`, `filter()` | Builtins nativos |
| `isinstance()`, `type()` | Introspection |
| `del x[i]` | OP_DELINDEX |
| `global` / `nonlocal` (parsed) | |
| Generators / `yield` | OP_FOR_ITER + fiber |
| Type hints (silently ignored) | `x: int = 1` |
| Tuples `(a, b)` como valores | |
| Set literals `{1, 2, 3}` | |
| `in` operator (containment) | Arrays, strings, dicts |
| String methods: split/join/strip/replace/find/upper/lower/startswith | |
| List methods: append/pop/insert/remove/sort/reverse/index | |
| Dict methods: keys/values/items/get | |
| `len()` como intrinsic | OP_LEN |
| `eval()` | OP_EVAL |
| `import math` module system | |
| Decorators `@` | Upvalue/closure fix incluído |
| `\` line continuation | |
| Semicolons `;` entre statements | |

### 🔲 Ainda por implementar no zenpy (TODO)

| Feature | Prioridade | Notas |
|---------|------------|-------|
| `try/except [Type as e]` | **Alta** | Plano completo em `TRY_EXCEPT_PLAN.md` |
| `**kwargs` | Alta | `def f(**kw)` |
| Inline `if x: y` (single-line) | Média | Parser lookahead |
| `for/else`, `while/else` | Média | |
| `with` statement | Média | Context managers |
| `a, *b = [1,2,3]` rest unpack | Baixa | |
| Walrus `:=` | Baixa | |
| Multiple inheritance / MRO | Baixa | |
| Dunder methods `__str__`, `__add__` | Baixa | |
| Decorators com args `@deco(arg)` | Baixa | |

---

## O que Levar de Volta para BuLang

Funcionalidades do zenpy que melhorariam BuLang directamente:

1. **Default params** — `def f(x=0, y=1)` — muito pedido, já funciona bem no zenpy
2. **Multi-return limpo** — `return a, b` + `a, b = f()` — BuLang só tem via `(a,b)` literals
3. **Negative indexing** — `arr[-1]` — trivial de implementar, enorme ergonomia
4. **Chained comparisons** — `0 < x < 10` — evita bugs comuns
5. **f-strings** — `f"x={val}"` — superior ao `"x=" + str(val)` actual
6. **Slices** — `arr[1:3]` — arrays ficam muito mais usáveis
7. **`not in`** — `if x not in arr` — mais legível que `if !arr.contains(x)`
8. **`assert`** — debugging limpo sem print
9. **Augmented assign `//=`, `**=`** — consistência com `+=`, `-=`
10. **`nonlocal`** explícito — evita ambiguidade no BuLang atual com closures

---

## Estado Geral do zenpy

- 25/25 testes `.py` from source ✓
- 25/25 testes bytecode round-trip ✓
- 24/24 testes C++ embedding ✓
- Bytecode format v2.1 estável
- Closures/upvalues correctos (BUG-Z1 corrigido)
- Varargs correctos (BUG-Z2 corrigido)
- Próximo passo: `try/except` (plano em `TRY_EXCEPT_PLAN.md`)
