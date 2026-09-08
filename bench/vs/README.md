# zenvm vs zenpy — where does the time go?

The two VMs share an interpreter core (zenpy is a fork), so a gap between
them on the same algorithm is either a codegen difference or a handler
difference, and the profile says which.

## Running

Both binaries must be built with the per-opcode profiler:

```
cmake -B build-p -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-DZEN_OPCODE_PROFILE
cmake --build build-p -j
```

Then:

```
./run.sh              # every case
./run.sh arith        # one case
ZENPY=/path/to/zen ./run.sh
```

The profiler's `rdtsc` roughly doubles runtime, so cycle columns compare
only against each other. Take wall-clock numbers from a build without it.

## Cases

Each case is the same program in both languages, isolating one cost:

| case | isolates |
|---|---|
| `calls` | global function calls, recursion, frame setup |
| `arith` | integer arithmetic in a tight loop, nothing else |
| `fields` | instance field read/write through a method |
| `arrays` | array indexing in a loop |

Both versions must print the same first line — the harness says so when
they do not, and the profiles are meaningless if they disagree.

## What it found (2026-09-08)

| case | zenvm | zenpy | dispatches |
|---|---:|---:|---|
| calls | 0.07 | 0.07 | zenpy -19% |
| arith | 0.15 | **0.27** | zenpy -7% |
| fields | 0.08 | 0.09 | zenpy -13% |
| arrays | 0.06 | 0.07 | zenpy -8% |

zenpy executes *fewer* instructions in every case — its compiler is ahead
(CALLGLOBAL, immediate-operand compare-and-branch), and it still loses.

The cost is inside the handlers, not in the code generated:

| handler | zenpy | zenvm |
|---|---:|---:|
| `OP_ADD` | 211 lines | 85 |
| `OP_MUL` | 71 | 30 |
| `OP_GETGLOBAL` | 13 | 6 |

zenpy's `OP_ADD` carries Python semantics inline — string concatenation,
alias detection, in-place `s += x`, operator overloading — so every integer
`+` in a loop pays for code it never reaches. That is why `arith` (nothing
but `+ * -`) is the worst case at +80%, while `calls` ties.

Note this is not the same thing the PGO experiment tested. PGO reorders
code by profile; it does not shorten the path a single ADD walks through.
