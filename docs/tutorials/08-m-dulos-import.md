# Tutorial 08 — Módulos (import)

Este tutorial mostra esta parte da linguagem através de exemplos práticos.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 08 — Módulos (import)
// ============================================================

import math;

// --- math.random ---

// inteiro entre 0 e N-1
var dado = math.random(6) + 1;
print("dado: {dado}");

// inteiro num intervalo [min, max]
var x = math.random(100, 200);
print("x entre 100 e 200: {x}");

// simulação de lançamento de moeda
var cara_ou_coroa = math.random(2);   // 0 ou 1
if (cara_ou_coroa == 0) {
    print("cara");
} else {
    print("coroa");
}

// --- Velocidades aleatórias ---
def velocidade_aleatoria(mag) {
    var vx = math.random(mag * 2) - mag;
    var vy = math.random(mag * 2) - mag;
    return vx;   // simplificado — zen não tem multi-return
}

var i = 0;
while (i < 5) {
    var vx = math.random(10) - 5;
    var vy = math.random(10) - 10;
    print("particula {i}: vx={vx} vy={vy}");
    i = i + 1;
}

// --- Escala normalizada ---
// Converter random int numa float [0.0, 1.0)
var r_float = math.random(1000) / 1000.0;
print("float aleatório: {r_float}");

// Float num intervalo [lo, hi)
def random_float(lo, hi) {
    return lo + math.random(10000) / 10000.0 * (hi - lo);
}

print("entre 2.0 e 5.0: {random_float(2.0, 5.0)}");

// --- Exemplo prático: gerar nuvem de pontos ---
import math;

struct Ponto { x, y }

var nuvem = [];
var j = 0;
while (j < 10) {
    var px = math.random(1280);
    var py = math.random(720);
    nuvem.push(Ponto(px, py));
    j = j + 1;
}

print("gerados {nuvem.len()} pontos:");
var k = 0;
while (k < nuvem.len()) {
    var p = nuvem[k];
    print("  ({p.x}, {p.y})");
    k = k + 1;
}
```

## Como correr

```bash
zen examples/tutorial_08_modulos.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_08_modulos.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
