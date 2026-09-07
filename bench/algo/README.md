# algo_bench — zen contra Lua e Wren

Os mesmos seis algoritmos escritos em cada linguagem, sem truques: A* e
Dijkstra numa grelha 128x128, quadtree e octree (build + range query),
Torres de Hanói e flood fill. Portados do `algo_bench` do zenpy, que por
sua vez os tem em Python, Lua e Wren.

O gerador de números é o mesmo LCG inteiro em todas as linguagens, por isso
**todas têm de imprimir o mesmo checksum em cada fase**. É essa a verificação
de que a comparação é honesta — o runner imprime a tabela de checksums a
seguir aos tempos.

```
./run.sh                 # usa ../../bin/zen
./run.sh /caminho/zen
```

## Resultados (2026-09-07, mesma máquina, melhor de 3)

| segundos | astar | dijkstra | quadtree | octree | hanoi | floodfill |
|---|---:|---:|---:|---:|---:|---:|
| **zen** | 0.044 | 0.166 | **0.043** | **0.064** | 0.057 | 0.011 |
| Lua 5.4 | **0.042** | **0.160** | 0.057 | 0.077 | 0.066 | **0.009** |
| Wren 0.4 | 0.062 | 0.342 | 0.071 | 0.102 | 0.075 | 0.020 |
| zenpy | 0.056 | 0.184 | 0.051 | 0.067 | **0.055** | 0.011 |
| CPython 3 | 0.070 | 0.254 | 0.074 | 0.109 | 0.092 | 0.027 |

Checksums iguais nas cinco linguagens em todas as fases: astar/dijkstra
3527039, quadtree 155484, octree 79081, hanoi 1310718, floodfill 4010601.

## Leitura

- **O zen está ao nível do Lua.** Ganha nas árvores (quadtree 0.043 vs
  0.057, octree 0.064 vs 0.077 — 25 a 30% mais rápido), perde por pouco no
  pathfinding e no flood fill. Não há aqui uma linguagem lenta e outra
  rápida: são a mesma classe.
- **Ganha ao Wren em tudo**, com margem grande no Dijkstra (0.166 vs 0.342,
  mais do dobro).
- **Onde o zen ganha é onde há objectos e métodos**: quadtree e octree são
  `Quad`/`Oct` com campos e chamadas de método recursivas. As vtables planas
  com selectores internados e o `OP_INVOKE_VT` pagam-se aqui. Onde perde é
  em ciclos apertados sobre arrays (o heap binário do pathfinding), onde o
  Lua tem anos de afinação.
- **Contra o zenpy** o zen ganha no pathfinding e nas árvores, empata em
  Hanói e flood fill. Consistente com os micro-benchmarks: o zenpy paga
  semântica de Python que o zen não tem.

## Notas de porte

Diferenças de sintaxe que importam, para quem comparar os ficheiros:

- `/` é sempre divisão float em zen; a divisão inteira é `div`
  (`//` é comentário). É o `OP_IDIV`.
- Campos de classe declaram-se com `var` no corpo da classe.
- `loop { }` para o ciclo infinito com `break` (o `while True:` do Python).
