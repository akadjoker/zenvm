# Tutorial 14 — Métodos de String e Array

Este tutorial documenta métodos úteis de strings e arrays.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 14 — Métodos de String e Array
// ============================================================

// ==========================================================
// STRING
// ==========================================================

var s = "Hello World";

// --- Tamanho ---
print(s.len());          // 11
print(len(s));           // 11  (função global equivalente)

// --- Caso ---
print(s.upper());        // HELLO WORLD
print(s.lower());        // hello world

// --- Sub-string: sub(inicio, fim) ---
print(s.sub(0, 5));      // Hello   (índice fim exclusivo)
print(s.sub(6, 11));     // World

// --- Procurar: find(padrão) → índice ou -1 ---
print(s.find("World"));  // 6
print(s.find("xyz"));    // -1

// --- Substituir: replace(antigo, novo) ---
print(s.replace("World", "Zen"));   // Hello Zen

// --- Prefixo / sufixo ---
print(s.starts_with("Hello"));   // true
print(s.starts_with("World"));   // false
print(s.ends_with("World"));     // true
print(s.ends_with("Hello"));     // false

// --- Trim (remove espaços nas extremidades) ---
var sp = "   espaços   ";
print(sp.trim());   // espaços

// --- Carácter por índice ---
print(s.char_at(0));   // H
print(s.char_at(6));   // W

// --- Split ---
var csv = "um,dois,três,quatro";
var partes = csv.split(",");
print(partes.len());    // 4
print(partes[0]);       // um
print(partes[3]);       // quatro

// split sem match → array com 1 elemento
var sem = "hello".split(",");
print(sem.len());       // 1
print(sem[0]);          // hello

// --- Concatenação com + ---
var nome = "Bu" + "Lang";
print(nome);            // BuLang

// --- Comparação ---
print("abc" == "abc");   // true
print("abc" < "abd");    // true  (lexicográfico)

// --- Strings verbatim (@ — sem escape) ---
var path = @"C:\users\djoker\ficheiro.txt";
print(path);   // C:\users\djoker\ficheiro.txt

// aspas duplas dentro de verbatim: ""
var q = @"ela disse ""olá""";
print(q);      // ela disse "olá"

// ==========================================================
// ARRAY  (métodos extra além de push/pop/len)
// ==========================================================

var arr = [3, 1, 4, 1, 5, 9, 2, 6];

// --- contains / index_of ---
print(arr.contains(5));       // true
print(arr.contains(99));      // false
print(arr.index_of(4));       // 2
print(arr.index_of(99));      // -1

// --- reverse (in-place) ---
var r = [1, 2, 3, 4, 5];
r.reverse();
print(r.join(","));   // 5,4,3,2,1

// --- slice(inicio, fim) → novo array ---
var base = [10, 20, 30, 40, 50];
var sl = base.slice(1, 3);
print(sl.join(","));   // 20,30

// --- insert(indice, valor) ---
var ins = [1, 3, 4];
ins.insert(1, 2);
print(ins.join(","));   // 1,2,3,4

// --- remove(indice) ---
var rem = [1, 2, 3, 4];
rem.remove(2);
print(rem.join(","));   // 1,2,4

// --- clear ---
var cl = [1, 2, 3];
cl.clear();
print(cl.len());   // 0

// --- join(separador) ---
var nums = [1, 2, 3, 4, 5];
print(nums.join(", "));   // 1, 2, 3, 4, 5
print(nums.join("-"));    // 1-2-3-4-5

// --- sort() ---
var desordenado = [3, 1, 4, 1, 5, 9, 2, 6];
desordenado.sort();
print(desordenado.join(","));   // 1,1,2,3,4,5,6,9

desordenado.sort("desc");
print(desordenado.join(","));   // 9,6,5,4,3,2,1,1

// sort de strings
var frutas = ["banana", "maçã", "abacate", "cereja"];
frutas.sort();
print(frutas.join(", "));   // abacate, banana, cereja, maçã

// --- Encadeamento de métodos ---
print("Hello World".lower().len());   // 11

var sub = [10, 30, 20, 50, 40].slice(1, 4);
sub.sort();
print(sub.join(","));   // 20,30,50
```

## Como correr

```bash
zen examples/tutorial_14_metodos.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_14_metodos.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
