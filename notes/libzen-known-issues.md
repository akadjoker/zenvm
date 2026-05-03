# libzen — known issues (discovered during review)

These are pre-existing issues in the language/compiler/VM that were
uncovered while writing `tests/test_many_selectors.zen` for the
selector-table grow change. They are **not** caused by the review
branch — both also reproduce on `main`. Filed here so they aren't
forgotten.

## 1. Class-type hint leaks across globals after a function call

Reproducer (one-liner):

```bash
./bin/zen -e 'class A { def m() { return 5; } } var x = A(); var fails = 0; def batch(o) { return o.m(); } fails = fails + batch(x); print(fails);'
```

Output:

```
[zen runtime error] object does not implement operator +
  at <cmdline>() [<cmdline>:1]
```

Both `fails` (declared as `0`) and `batch(x)` are integers at runtime,
yet the compiler emits `OP_ADD_OBJ` for `fails + batch(x)`. The most
likely cause is the global-typing layer in
[`compiler_expressions.cpp`](libzen/src/compiler_expressions.cpp:1)
attributing a class hint to `fails` (or to the temp register that
receives the call result) when nearby globals are class instances.

Workaround used in `tests/test_many_selectors.zen`: avoid `+` with a
function-call result on the rhs.

## 2. `intern_selector` slot beyond ~120 returns stale vtable Value (not a selector-array bug)

Reproducer: a class with ~120+ methods. With 300 methods,
`m119` fails:

```
[zen runtime error] Many.m119() expects 294326728 args but got 0
```

The "expects N args but got 0" pattern with an unrealistic N reads
garbage from `ObjFunc::arity`, suggesting `klass->vtable[slot]` points
to wrong/uninitialised memory for slots beyond a certain index.

Reproduces identically on `main` with the previous fixed-size
`selectors_[256]` array, so this is **not** caused by the dynamic
selector table on this branch. Suspect the per-class vtable grow code
at [`compiler_statements.cpp:728`](libzen/src/compiler_statements.cpp:728)
or `OP_INVOKE_VT` boundary handling.

Investigation pointer:

* `klass->vtable_size` grow logic
* selector slot vs. vtable slot decoupling
* Bytecode emitted for `OP_INVOKE_VT` at slots > 128

## 3. (Cosmetic) compiler does not free temp registers across a long
   sequence of `acc = acc + expr` statements

`sum = sum + x.mNNN()` repeated ~125 times at the same scope hits

```
Too many registers needed (expression too complex).
```

…and on top of that the compiler emits a follow-up segfault when the
script is run with these errors present (the compiler should bail out
cleanly before handing bytecode to the VM).

Each `expr` should release its temporary when assigned back into `acc`,
keeping register pressure flat. Currently it grows.

Workaround: split into smaller scopes / functions.
