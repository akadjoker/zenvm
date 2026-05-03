# Tutorial 06 — Classes

Este tutorial mostra classes com campos, métodos, construtor init e acesso à instância através de self.

## Objetivo

Aprender a sintaxe e os padrões principais deste tópico em BuLang/Zen.

## Código completo

```zen
// ============================================================
// Tutorial 06 — Classes
// ============================================================

// class tem campos (var) e métodos (def)
// init() é o construtor — chamado automaticamente ao criar instância
// self refere-se à instância actual

// --- Classe básica ---
class Contador {
    var valor;

    def init() {
        self.valor = 0;
    }

    def incrementar() {
        self.valor = self.valor + 1;
    }

    def incrementar_por(n) {
        self.valor = self.valor + n;
    }

    def resetar() {
        self.valor = 0;
    }

    def obter() {
        return self.valor;
    }
}

var c = Contador();
c.incrementar();
c.incrementar();
c.incrementar_por(5);
print("contador: {c.obter()}");    // 7
c.resetar();
print("após reset: {c.obter()}");  // 0

// --- Classe com parâmetros no init ---
class Vec2 {
    var x;
    var y;

    def init(x, y) {
        self.x = x;
        self.y = y;
    }

    def somar(outro) {
        return Vec2(self.x + outro.x, self.y + outro.y);
    }

    def escalar(f) {
        return Vec2(self.x * f, self.y * f);
    }

    def comprimento() {
        // sem sqrt nativo neste exemplo — usa aproximação
        return self.x * self.x + self.y * self.y;   // distância²
    }

    def print_v() {
        print("Vec2({self.x}, {self.y})");
    }
}

var a = Vec2(3, 4);
var b = Vec2(1, 2);
var soma = a.somar(b);
soma.print_v();                         // Vec2(4, 6)
a.escalar(2.0).print_v();              // Vec2(6, 8)
print("dist² de a: {a.comprimento()}");  // 25

// --- Composição de classes ---
class Timer {
    var duracao;
    var restante;
    var activo;

    def init(duracao) {
        self.duracao = duracao;
        self.restante = duracao;
        self.activo = false;
    }

    def iniciar() {
        self.activo = true;
        self.restante = self.duracao;
    }

    def update(dt) {
        if (!self.activo) { return false; }
        self.restante = self.restante - dt;
        if (self.restante <= 0.0) {
            self.activo = false;
            self.restante = 0.0;
            return true;    // disparou!
        }
        return false;
    }

    def progresso() {
        return 1.0 - self.restante / self.duracao;
    }
}

class Arma {
    var dano;
    var cooldown;

    def init(dano, cooldown) {
        self.dano = dano;
        self.cooldown = Timer(cooldown);
    }

    def disparar() {
        if (!self.cooldown.activo) {
            self.cooldown.iniciar();
            print("BANG! dano={self.dano}");
            return true;
        }
        print("a recarregar... {int(self.cooldown.restante * 10) / 10}s");
        return false;
    }

    def update(dt) {
        self.cooldown.update(dt);
    }
}

var pistola = Arma(25, 0.5);
pistola.disparar();          // BANG!
pistola.disparar();          // a recarregar...
pistola.update(0.6);
pistola.disparar();          // BANG! (cooldown expirou)
```

## Como correr

```bash
zen examples/tutorial_06_classes.zen
```

ou ajusta para o nome real do teu executável:

```bash
bulang examples/tutorial_06_classes.zen
```

## O que observar

- A sintaxe é direta e usa blocos com `{` e `}`.
- Os exemplos usam `print()` para mostrar o resultado esperado.
- Comentários no próprio código explicam cada secção.

## Exercício sugerido

Altera os valores do exemplo, corre outra vez e confirma se o output muda como esperas.
