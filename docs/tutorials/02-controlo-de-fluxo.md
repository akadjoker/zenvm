# Tutorial 02 — Controlo de Fluxo

Este tutorial mostra como controlar a execução do programa com condições, ciclos e loops infinitos controlados por break.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 02 — Controlo de Fluxo
// ============================================================

// --- if / else ---
var x = 15;

if (x > 10) {
    print("maior que 10");
} else {
    print("menor ou igual a 10");
}

// if encadeado
var nota = 75;
if (nota >= 90) {
    print("Excelente");
} else {
    if (nota >= 70) {
        print("Bom");
    } else {
        if (nota >= 50) {
            print("Suficiente");
        } else {
            print("Insuficiente");
        }
    }
}

// --- while ---
var i = 0;
while (i < 5) {
    print("i = {i}");
    i = i + 1;
}

// --- for ---
for (var j = 0; j < 5; j = j + 1) {
    print("j = {j}");
}

// --- loop infinito + break ---
// loop corre para sempre até encontrar break
var contador = 0;
loop {
    contador = contador + 1;
    if (contador >= 3) {
        break;
    }
    print("loop: {contador}");
}
print("saiu do loop em: {contador}");

// --- Combinação de condições ---
var idade = 20;
var temBilhete = true;

if (idade >= 18 && temBilhete) {
    print("pode entrar");
}

if (idade < 16 or not temBilhete) {
    print("não pode entrar");
}
```

## Como correr

```bash
zen examples/tutorial_02_controlo.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_02_controlo.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
