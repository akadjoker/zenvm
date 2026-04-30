import sys
import time


def fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def main() -> int:
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    acc = 0
    start_ms = time.perf_counter_ns() // 1_000_000
    for _ in range(repeats):
        acc += fib(n)
    elapsed_ms = time.perf_counter_ns() // 1_000_000 - start_ms
    print(acc)
    print(elapsed_ms)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())