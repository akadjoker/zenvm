# Tutorial 10 — Closures e Upvalues

Este tutorial mostra esta parte da linguagem através de exemplos práticos.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 10 — Closures e Upvalues
// ============================================================

// Uma closure é uma função que "captura" variáveis do
// âmbito onde foi definida. Em Zen, def dentro de def cria
// uma closure automaticamente.

// --- Closure básica ---
def make_getter() {
    var x = 42;
    def get() { return x; }   // get captura x
    return get;
}

var g = make_getter();
print(g());   // 42 — x ainda existe dentro da closure

// --- Closure modifica a variável capturada (upvalue) ---
def make_contador() {
    var n = 0;
    def inc() {
        n = n + 1;
        return n;
    }
    return inc;
}

var c = make_contador();
print(c());   // 1
print(c());   // 2
print(c());   // 3

// --- Instâncias independentes ---
// Cada chamada a make_contador cria o seu próprio n
var a = make_contador();
var b = make_contador();
print(a());   // 1
print(a());   // 2
print(b());   // 1  ← independente de a
print(a());   // 3

// --- Closure captura parâmetro ---
def make_adder(n) {
    def add(x) { return x + n; }
    return add;
}

var add10 = make_adder(10);
var add5  = make_adder(5);
print(add10(3));   // 13
print(add5(3));    // 8

// --- Closures partilham o mesmo upvalue ---
def make_par() {
    var x = 0;
    def inc() { x = x + 1; }
    def get() { return x; }
    inc(); inc(); inc();
    return get;
}

var get = make_par();
print(get());   // 3

// --- Closures aninhadas (3 níveis) ---
def nivel_a() {
    var x = 100;
    def nivel_b() {
        def nivel_c() { return x; }   // captura x de a
        return nivel_c;
    }
    return nivel_b;
}

var b = nivel_a();
var c2 = b();
print(c2());   // 100

// --- Funções de ordem superior ---
def apply(f, x) { return f(x); }

var dobrar = make_adder(0);   // add(x) = x+0… melhor:
def make_mult(n) {
    def mult(x) { return x * n; }
    return mult;
}

var triple = make_mult(3);
print(apply(triple, 7));    // 21
print(apply(triple, 10));   // 30

// --- Gerador de Fibonacci com closure ---
def fib_gen() {
    var a = 0;
    var b = 1;
    def next() {
        var t = a;
        a = b;
        b = t + b;
        return t;
    }
    return next;
}

var fib = fib_gen();
print(fib());   // 0
print(fib());   // 1
print(fib());   // 1
print(fib());   // 2
print(fib());   // 3
print(fib());   // 5
print(fib());   // 8

// --- Função anónima (def sem nome) ---
var sq = def(n) { return n * n; };
print(sq(5));    // 25
print(sq(12));   // 144
```

## Como correr

```bash
zen examples/tutorial_10_closures.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_10_closures.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
