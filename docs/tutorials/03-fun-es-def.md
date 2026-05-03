# Tutorial 03 — Funções (def)

Este tutorial explica funções declaradas com def, parâmetros, return, composição e recursão.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 03 — Funções (def)
// ============================================================

// --- Definição básica ---
def saudar() {
    print("Olá, Mundo!");
}

saudar();

// --- Com parâmetros ---
def somar(a, b) {
    return a + b;
}

var resultado = somar(3, 7);
print("3 + 7 = {resultado}");

// --- Com return múltiplos caminhos ---
def maximo(a, b) {
    if (a > b) { return a; }
    return b;
}

print("max(10, 25) = {maximo(10, 25)}");
print("max(99, 1)  = {maximo(99, 1)}");

// --- Funções auxiliares / composição ---
def clamp(v, lo, hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

def normalizar(v, min_v, max_v) {
    var c = clamp(v, min_v, max_v);
    return (c - min_v) / (max_v - min_v);
}

print(normalizar(50, 0, 100));   // 0.5
print(normalizar(-10, 0, 100));  // 0.0  (clampado)
print(normalizar(120, 0, 100));  // 1.0  (clampado)

// --- Funções recursivas ---
def factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

print("5! = {factorial(5)}");    // 120
print("10! = {factorial(10)}");  // 3628800

// --- Funções como valores de lógica de jogo ---
def hit(mx, my, x, y, w, h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

var dentroRetangulo = hit(50, 50, 10, 10, 100, 100);
print("ponto dentro? {dentroRetangulo}");  // true
```

## Como correr

```bash
zen examples/tutorial_03_funcoes.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_03_funcoes.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
