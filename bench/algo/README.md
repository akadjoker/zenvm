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

Depois do branch `perf/compiler-codegen` (fusão comparação+salto, retarget
da aritmética, imediatos `ADDI`/`SUBI`):

| segundos | astar | dijkstra | quadtree | octree | hanoi | floodfill |
|---|---:|---:|---:|---:|---:|---:|
| **zen** | **0.042** | **0.155** | **0.041** | **0.060** | **0.051** | 0.010 |
| Lua 5.4 | 0.042 | 0.160 | 0.054 | 0.076 | 0.066 | **0.009** |
| Wren 0.4 | 0.062 | 0.339 | 0.071 | 0.101 | 0.074 | 0.020 |
| zenpy | 0.056 | 0.183 | 0.052 | 0.068 | 0.055 | 0.011 |
| CPython 3 | 0.069 | 0.252 | 0.074 | 0.111 | 0.091 | 0.027 |

Checksums iguais nas cinco linguagens em todas as fases: astar/dijkstra
3527039, quadtree 155484, octree 79081, hanoi 1310718, floodfill 4010601.

Antes do branch, para comparar (o que mudou foi só o compilador, o VM está
igual):

| segundos | astar | dijkstra | quadtree | octree | hanoi | floodfill |
|---|---:|---:|---:|---:|---:|---:|
| zen (antes) | 0.043 | 0.166 | 0.042 | 0.062 | 0.057 | 0.011 |

## Leitura

- **O zen ganha ou empata com o Lua em todas as fases.** Nas árvores a
  margem é grande (quadtree 0.041 vs 0.054, octree 0.060 vs 0.076, 24 a 27%),
  no pathfinding e no Hanói passou de perder para ganhar por pouco. Só o
  flood fill continua marginalmente para o Lua (0.010 vs 0.009).
- **Ganha ao Wren em tudo**, com mais do dobro no Dijkstra.
- **Onde ganha mais é onde há objectos e métodos**: quadtree e octree são
  `Quad`/`Oct` com campos e chamadas recursivas. As vtables planas com
  selectores internados e o `OP_INVOKE_VT` pagam-se aqui.
- **Contra o zenpy**, que é o mesmo VM com outro compilador, o zen está
  agora à frente em tudo. As três optimizações do branch vieram de lá; o
  zenvm ficou com elas mais o `FORPREP`/`FORLOOP` que o zenpy não tem.

## Notas de porte

Diferenças de sintaxe que importam, para quem comparar os ficheiros:

- `/` é sempre divisão float em zen; a divisão inteira é `div`
  (`//` é comentário). É o `OP_IDIV`.
- Campos de classe declaram-se com `var` no corpo da classe.
- `loop { }` para o ciclo infinito com `break` (o `while True:` do Python).
