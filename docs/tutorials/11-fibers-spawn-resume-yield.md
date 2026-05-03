# Tutorial 11 — Fibers: spawn / resume / yield

Este tutorial mostra esta parte da linguagem através de exemplos práticos.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 11 — Fibers: spawn / resume / yield
// ============================================================

// Fibers são corrotinas leves: funções que podem ser pausadas
// (yield) e retomadas (resume) mantendo o seu estado interno.
//
// spawn fn   → cria uma fiber a partir de fn (não executa ainda)
// resume(f)  → executa a fiber até ao próximo yield; retorna o valor
// yield v    → pausa a fiber e devolve v ao chamador
// Quando a fiber termina, resume() devolve nil.

// --- Fiber básica ---
def gen() {
    yield 1;
    yield 2;
    yield 3;
}

var f = spawn gen;
print(resume(f));   // 1
print(resume(f));   // 2
print(resume(f));   // 3
print(resume(f));   // nil  ← fiber terminou

// --- Iterar uma fiber com while ---
def contar(max) {
    var i = 0;
    while (i < max) {
        yield i;
        i = i + 1;
    }
}

def make_range(n) {
    def gen() { contar(n); }
    return gen;
}

var r = spawn make_range(5);
var v = resume(r);
while (v != nil) {
    print(v);       // 0 1 2 3 4
    v = resume(r);
}

// --- Comunicação bidirecional ---
// resume(f, valor) envia um valor para dentro da fiber.
// O yield recebe esse valor como resultado de expressão.

def echo_dobro() {
    var x = yield 0;     // pausa, recebe valor no resume seguinte
    yield x * 2;
}

var e = spawn echo_dobro;
resume(e);           // arranca até ao primeiro yield → devolve 0
print(resume(e, 7)); // envia 7 → fiber faz yield 14 → 14

// --- Acumulador (múltiplos envios) ---
def acumulador() {
    var soma = 0;
    loop {
        var v = yield soma;
        soma = soma + v;
    }
}

var ac = spawn acumulador;
resume(ac);             // inicializa (yield 0)
print(resume(ac, 5));   // 5
print(resume(ac, 3));   // 8
print(resume(ac, 7));   // 15

// --- Gerador de Fibonacci com fiber ---
def fib_fiber() {
    var a = 0;
    var b = 1;
    loop {
        yield a;
        var t = a;
        a = b;
        b = t + b;
    }
}

var ff = spawn fib_fiber;
print(resume(ff));   // 0
print(resume(ff));   // 1
print(resume(ff));   // 1
print(resume(ff));   // 2
print(resume(ff));   // 3
print(resume(ff));   // 5
print(resume(ff));   // 8

// --- Múltiplas fibers em paralelo ---
def make_seq(inicio, passo) {
    def gen() {
        var n = inicio;
        loop {
            yield n;
            n = n + passo;
        }
    }
    return gen;
}

var fa = spawn make_seq(0, 1);
var fb = spawn make_seq(100, 10);

// intercalar resultados
print(resume(fa));   // 0
print(resume(fb));   // 100
print(resume(fa));   // 1
print(resume(fb));   // 110
print(resume(fa));   // 2
print(resume(fb));   // 120

// --- Fiber com números pares (yield condicional) ---
def pares(max) {
    def gen() {
        var i = 0;
        while (i < max) {
            if (i % 2 == 0) { yield i; }
            i = i + 1;
        }
    }
    return gen;
}

var fp = spawn pares(10);
var vp = resume(fp);
while (vp != nil) {
    print(vp);   // 0 2 4 6 8
    vp = resume(fp);
}

// --- Fiber com closure capturando upvalue ---
def make_upvalue_gen() {
    var x = 0;
    def gen() {
        loop {
            x = x + 1;
            yield x;
        }
    }
    return gen;
}

var ug = make_upvalue_gen();
var fu = spawn ug;
print(resume(fu));   // 1
print(resume(fu));   // 2
print(resume(fu));   // 3

// --- Spawn com função anónima ---
var fanon = spawn def() { yield 10; yield 20; };
print(resume(fanon));   // 10
print(resume(fanon));   // 20
print(resume(fanon));   // nil
```

## Como correr

```bash
zen examples/tutorial_11_fibers.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_11_fibers.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
