# Python side of the zen-vs-python micro-benchmark suite.
# Mirrors bench.zen 1:1. Output: "BENCH <name> <seconds>" per scenario.
import time

def bench(name, fn):
    t0 = time.perf_counter()
    r = fn()
    dt = time.perf_counter() - t0
    print(f"BENCH {name} {dt}")
    return r

N = 3_000_000

# 1. int arithmetic in a while loop
def int_loop():
    s = 0
    i = 0
    while i < N:
        s = s + i
        i = i + 1
    return s
bench("int_loop", int_loop)

# 2. float arithmetic
def float_loop():
    s = 0.0
    i = 0
    while i < N:
        s = s + 0.25
        i = i + 1
    return s
bench("float_loop", float_loop)

# 3. function call overhead
def ident(x):
    return x

def calls():
    s = 0
    i = 0
    while i < N:
        s = s + ident(i)
        i = i + 1
    return s
bench("calls", calls)

# 4. recursive fib
def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)
bench("fib25", lambda: fib(25))

# 5. string building with +=
def str_concat():
    s = ""
    i = 0
    while i < 20000:
        s += "x"
        i = i + 1
    return len(s)
bench("str_concat", str_concat)

# 6. string methods: split + join + upper
def str_methods():
    base = "alpha,beta,gamma,delta,epsilon"
    acc = 0
    i = 0
    while i < 50000:
        parts = base.split(",")
        acc = acc + len(parts) + len("-".join(parts).upper())
        i = i + 1
    return acc
bench("str_methods", str_methods)

# 7. array push + indexed read-modify-write
def array_ops():
    a = []
    i = 0
    while i < 300000:
        a.append(i)
        i = i + 1
    s = 0
    i = 0
    while i < 300000:
        s = s + a[i]
        a[i] = s
        i = i + 1
    return s
bench("array_ops", array_ops)

# 8. map set/get with string keys
def map_ops():
    m = {}
    i = 0
    while i < 100000:
        m["k" + str(i % 1000)] = i
        i = i + 1
    s = 0
    i = 0
    while i < 100000:
        s = s + m["k" + str(i % 1000)]
        i = i + 1
    return s
bench("map_ops", map_ops)

# 9. class method dispatch
class Vec:
    __slots__ = ("x",)
    def __init__(self):
        self.x = 0
    def add(self, n):
        self.x = self.x + n
        return self.x

def method_dispatch():
    v = Vec()
    i = 0
    while i < 1_000_000:
        v.add(1)
        i = i + 1
    return v.x
bench("method_dispatch", method_dispatch)

# 10. closure invocation (captured variable)
def closures():
    total = 0
    def addr(n):
        nonlocal total
        total = total + n
    i = 0
    while i < 1_000_000:
        addr(1)
        i = i + 1
    return total
bench("closures", closures)

# 11. foreach over numeric range
def foreach_range():
    s = 0
    for i in range(N):
        s = s + i
    return s
bench("foreach_range", foreach_range)

# 12. object allocation churn (GC pressure)
def alloc_churn():
    keep = 0
    i = 0
    while i < 200000:
        v = Vec()
        v.add(i)
        keep = keep + (v.x % 2)
        i = i + 1
    return keep
bench("alloc_churn", alloc_churn)

print("DONE")
