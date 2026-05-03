# Tutorial 01 — Variáveis, Tipos e Operadores

Este tutorial apresenta variáveis, valores básicos, operadores aritméticos, comparação, lógica, atribuição composta, conversões e interpolação de strings.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 01 — Variáveis, Tipos e Operadores
// ============================================================

// --- Declaração de variáveis ---
var x = 10;
var y = 3.14;
var nome = "BuLang";
var activo = true;
var nada = null;

print(x);        // 10
print(y);        // 3.14
print(nome);     // BuLang
print(activo);   // true

// --- Operadores aritméticos ---
var a = 10;
var b = 3;

print(a + b);    // 13
print(a - b);    // 7
print(a * b);    // 30
print(a / b);    // 3.333...
print(a % b);    // 1  (resto)

// --- Operadores de comparação ---
print(a == b);   // false
print(a != b);   // true
print(a > b);    // true
print(a <= b);   // false

// --- Operadores lógicos ---
var p = true;
var q = false;

print(p && q);   // false  (também: p and q)
print(p || q);   // true   (também: p or q)
print(!p);       // false  (também: not p)

// --- Atribuição composta ---
var n = 10;
n += 5;   print(n);   // 15
n -= 3;   print(n);   // 12
n *= 2;   print(n);   // 24

// --- Conversões ---
var f = 9.99;
print(int(f));   // 9  (trunca para inteiro)

// --- Interpolação de strings ---
var pontos = 42;
print("A tua pontuação é: {pontos}");
print("Soma: {a + b}");
```

## Como correr

```bash
zen examples/tutorial_01_variaveis.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_01_variaveis.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
