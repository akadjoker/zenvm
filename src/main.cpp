/*
** main.cpp — Testes de bytecode handcoded para a VM zen.
**
** Antes do lexer/compiler existir, validamos o VM com programas
** construídos via Emitter. Cada test_*() retorna true/false.
*/

#include "vm.h"
#include "emitter.h"
#include "debug.h"
#include <cstdio>
#include <cmath>
#include <ctime>

using namespace zen;

/* =========================================================
** Helpers
** ========================================================= */

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn)           \
    do                         \
    {                          \
        tests_run++;           \
        printf("%-40s ", #fn); \
        fflush(stdout);        \
        if (fn())              \
        {                      \
            tests_passed++;    \
            printf("PASS\n");  \
        }                      \
        else                   \
        {                      \
            printf("FAIL\n");  \
        }                      \
    } while (0)

/* Execute a function and read R[0] from the fiber stack after HALT */
static Value run_and_get_r0(VM &vm, ObjFunc *fn)
{
    /* Save R[0] before run overwrites it */
    vm.run(fn);
    /* After run, main fiber is DONE. R[0] is at stack[0]. */
    /* We can't easily access it... let's use a different approach:
       use PRINT to verify, or use globals to extract results. */
    return val_nil(); /* placeholder */
}

/* Better approach: store result in a global, then read it */
static Value exec_and_read(VM &vm, ObjFunc *fn, int global_idx)
{
    vm.run(fn);
    return vm.get_global("_result");
}

/* =========================================================
** Test: Value constructors and type checks
** ========================================================= */

static bool test_value_types()
{
    Value vn = val_nil();
    Value vb = val_bool(true);
    Value vi = val_int(42);
    Value vf = val_float(3.14);

    if (!is_nil(vn))
        return false;
    if (!is_bool(vb))
        return false;
    if (!is_int(vi))
        return false;
    if (!is_float(vf))
        return false;

    if (vb.as.boolean != true)
        return false;
    if (vi.as.integer != 42)
        return false;
    if (vf.as.number != 3.14)
        return false;

    return true;
}

/* =========================================================
** Test: Truthiness
** ========================================================= */

static bool test_truthiness()
{
    if (is_truthy(val_nil()))
        return false;
    if (is_truthy(val_bool(false)))
        return false;
    if (!is_truthy(val_bool(true)))
        return false;
    if (!is_truthy(val_int(0)))
        return false; /* 0 is truthy! (not Python) */
    if (!is_truthy(val_int(1)))
        return false;
    if (!is_truthy(val_float(0.0)))
        return false;
    return true;
}

/* =========================================================
** Test: values_equal
** ========================================================= */

static bool test_values_equal()
{
    if (!values_equal(val_nil(), val_nil()))
        return false;
    if (!values_equal(val_bool(true), val_bool(true)))
        return false;
    if (values_equal(val_bool(true), val_bool(false)))
        return false;
    if (!values_equal(val_int(42), val_int(42)))
        return false;
    if (values_equal(val_int(42), val_int(43)))
        return false;
    if (!values_equal(val_float(3.14), val_float(3.14)))
        return false;
    /* Different types are never equal */
    if (values_equal(val_int(0), val_nil()))
        return false;
    if (values_equal(val_int(0), val_bool(false)))
        return false;
    return true;
}

/* =========================================================
** Test: to_number / to_integer
** ========================================================= */

static bool test_conversions()
{
    if (to_number(val_int(10)) != 10.0)
        return false;
    if (to_number(val_float(3.14)) != 3.14)
        return false;
    if (to_number(val_nil()) != 0.0)
        return false;

    if (to_integer(val_int(42)) != 42)
        return false;
    if (to_integer(val_float(3.7)) != 3)
        return false;
    if (to_integer(val_nil()) != 0)
        return false;
    return true;
}

/* =========================================================
** Test: String interning
** ========================================================= */

static bool test_string_interning()
{
    VM vm;
    ObjString *a = vm.make_string("hello");
    ObjString *b = vm.make_string("hello");
    ObjString *c = vm.make_string("world");

    if (a != b)
        return false; /* same content → same pointer */
    if (a == c)
        return false; /* different content → different pointer */
    if (a->length != 5)
        return false;
    if (strcmp(a->chars, "hello") != 0)
        return false;
    return true;
}

/* =========================================================
** Test: Emitter basics
** ========================================================= */

static bool test_emitter_basic()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test", 0);

    /* LOADI R0, 42 */
    e.emit_asbx(OP_LOADI, 0, 42, 1);
    /* PRINT R0 */
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    if (fn->code_count != 3)
        return false; /* 2 + HALT */
    if (fn->arity != 0)
        return false;
    if (fn->num_regs != 1)
        return false;

    /* Verify encoding */
    uint32_t loadi = fn->code[0];
    if (ZEN_OP(loadi) != OP_LOADI)
        return false;
    if (ZEN_A(loadi) != 0)
        return false;
    if (ZEN_SBX(loadi) != 42)
        return false;

    return true;
}

/* =========================================================
** Test: LOADI + PRINT (execute)
** ========================================================= */

static bool test_loadi_print()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_loadi", 0);

    /* R0 = 42; print R0 */
    e.emit_asbx(OP_LOADI, 0, 42, 1);
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    printf("[expect: 42] ");
    fflush(stdout);
    vm.run(fn);

    return true; /* visual check */
}

/* =========================================================
** Test: LOADI negative
** ========================================================= */

static bool test_loadi_negative()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_neg_loadi", 0);

    /* R0 = -100; print R0 */
    e.emit_asbx(OP_LOADI, 0, -100, 1);
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    printf("[expect: -100] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Arithmetic (int + int, float + float, mixed)
** ========================================================= */

static bool test_arithmetic()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_arith", 0);

    /* R0 = 10, R1 = 20 */
    e.emit_asbx(OP_LOADI, 0, 10, 1);
    e.emit_asbx(OP_LOADI, 1, 20, 1);

    /* R2 = R0 + R1 (= 30) */
    e.emit_abc(OP_ADD, 2, 0, 1, 1);
    e.emit_abc(OP_PRINT, 2, 0, 0, 1);

    /* R3 = R1 - R0 (= 10) */
    e.emit_abc(OP_SUB, 3, 1, 0, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    /* R4 = R0 * R1 (= 200) */
    e.emit_abc(OP_MUL, 4, 0, 1, 1);
    e.emit_abc(OP_PRINT, 4, 0, 0, 1);

    /* R5 = R1 / R0 (= 2.0 — div always float) */
    e.emit_abc(OP_DIV, 5, 1, 0, 1);
    e.emit_abc(OP_PRINT, 5, 0, 0, 1);

    /* R6 = R1 % R0 (= 0) */
    e.emit_abc(OP_MOD, 6, 1, 0, 1);
    e.emit_abc(OP_PRINT, 6, 0, 0, 1);

    /* R7 = -R0 (= -10) */
    e.emit_abc(OP_NEG, 7, 0, 0, 1);
    e.emit_abc(OP_PRINT, 7, 0, 0, 1);

    ObjFunc *fn = e.end(8);

    printf("[expect: 30 10 200 2 0 -10] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: ADDI / SUBI superinstructions
** ========================================================= */

static bool test_addi_subi()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_addi", 0);

    /* R0 = 100 */
    e.emit_asbx(OP_LOADI, 0, 100, 1);

    /* R1 = R0 + 5 (ADDI) */
    e.emit_abc(OP_ADDI, 1, 0, 5, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    /* R2 = R0 - 3 (SUBI) */
    e.emit_abc(OP_SUBI, 2, 0, 3, 1);
    e.emit_abc(OP_PRINT, 2, 0, 0, 1);

    ObjFunc *fn = e.end(3);

    printf("[expect: 105 97] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Bitwise operations
** ========================================================= */

static bool test_bitwise()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_bits", 0);

    /* R0 = 0xFF (255), R1 = 0x0F (15) */
    e.emit_asbx(OP_LOADI, 0, 0xFF, 1);
    e.emit_asbx(OP_LOADI, 1, 0x0F, 1);

    /* R2 = R0 & R1 (= 15) */
    e.emit_abc(OP_BAND, 2, 0, 1, 1);
    e.emit_abc(OP_PRINT, 2, 0, 0, 1);

    /* R3 = R0 | R1 (= 255) */
    e.emit_abc(OP_BOR, 3, 0, 1, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    /* R4 = R0 ^ R1 (= 240 = 0xF0) */
    e.emit_abc(OP_BXOR, 4, 0, 1, 1);
    e.emit_abc(OP_PRINT, 4, 0, 0, 1);

    /* R5 = ~R1 — platform-dependent due to int width, just check non-zero */
    e.emit_abc(OP_BNOT, 5, 1, 0, 1);
    e.emit_abc(OP_PRINT, 5, 0, 0, 1);

    /* R6 = 1 << 8 (= 256) */
    e.emit_asbx(OP_LOADI, 6, 1, 1);
    e.emit_asbx(OP_LOADI, 7, 8, 1);
    e.emit_abc(OP_SHL, 6, 6, 7, 1);
    e.emit_abc(OP_PRINT, 6, 0, 0, 1);

    /* R7 = 256 >> 4 (= 16) */
    e.emit_asbx(OP_LOADI, 8, 4, 1);
    e.emit_abc(OP_SHR, 7, 6, 8, 1);
    e.emit_abc(OP_PRINT, 7, 0, 0, 1);

    ObjFunc *fn = e.end(9);

    printf("[expect: 15 255 240 ~15 256 16] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Comparison (EQ, LT, LE, NOT)
** ========================================================= */

static bool test_comparison()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_cmp", 0);

    /* R0 = 10, R1 = 20, R2 = 10 */
    e.emit_asbx(OP_LOADI, 0, 10, 1);
    e.emit_asbx(OP_LOADI, 1, 20, 1);
    e.emit_asbx(OP_LOADI, 2, 10, 1);

    /* R3 = (R0 == R2) → true */
    e.emit_abc(OP_EQ, 3, 0, 2, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    /* R4 = (R0 == R1) → false */
    e.emit_abc(OP_EQ, 4, 0, 1, 1);
    e.emit_abc(OP_PRINT, 4, 0, 0, 1);

    /* R5 = (R0 < R1) → true */
    e.emit_abc(OP_LT, 5, 0, 1, 1);
    e.emit_abc(OP_PRINT, 5, 0, 0, 1);

    /* R6 = (R1 <= R0) → false */
    e.emit_abc(OP_LE, 6, 1, 0, 1);
    e.emit_abc(OP_PRINT, 6, 0, 0, 1);

    /* R7 = !true → false */
    e.emit_abc(OP_LOADBOOL, 7, 1, 0, 1);
    e.emit_abc(OP_NOT, 7, 7, 0, 1);
    e.emit_abc(OP_PRINT, 7, 0, 0, 1);

    ObjFunc *fn = e.end(8);

    printf("[expect: true false true false false] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: LOADBOOL with skip
** ========================================================= */

static bool test_loadbool_skip()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_lbskip", 0);

    /* R0 = true, skip next */
    e.emit_abc(OP_LOADBOOL, 0, 1, 1, 1); /* C=1 → skip next */
    /* This should be skipped: */
    e.emit_asbx(OP_LOADI, 0, 999, 1);
    /* R0 should still be true */
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    printf("[expect: true] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: LOADNIL
** ========================================================= */

static bool test_loadnil()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_nil", 0);

    e.emit_asbx(OP_LOADI, 0, 42, 1);
    e.emit_abc(OP_LOADNIL, 0, 0, 0, 1);
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    printf("[expect: nil] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: LOADK (constant pool)
** ========================================================= */

static bool test_loadk()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_loadk", 0);

    int k0 = e.add_constant(val_float(3.14159));
    int k1 = e.add_string_constant("hello zen");

    e.emit_abx(OP_LOADK, 0, k0, 1);
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    e.emit_abx(OP_LOADK, 1, k1, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 3.14159 \"hello zen\"] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: JMP
** ========================================================= */

static bool test_jmp()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_jmp", 0);

    /* R0 = 1 */
    e.emit_asbx(OP_LOADI, 0, 1, 1);
    /* JMP +1 (skip next) */
    int jmp = e.emit_jump(OP_JMP, 0, 1);
    /* This should be skipped */
    e.emit_asbx(OP_LOADI, 0, 999, 1);
    e.patch_jump(jmp);
    /* R0 should be 1 */
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(1);

    printf("[expect: 1] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: JMPIF / JMPIFNOT
** ========================================================= */

static bool test_conditional_jumps()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_cjmp", 0);

    /* if (true) R0 = 10; else R0 = 20; */
    e.emit_abc(OP_LOADBOOL, 0, 1, 0, 1); /* R0 = true */
    int skip_else = e.emit_jump(OP_JMPIF, 0, 1);
    e.emit_asbx(OP_LOADI, 1, 20, 1); /* else: R1 = 20 */
    int skip_then = e.emit_jump(OP_JMP, 0, 1);
    e.patch_jump(skip_else);
    e.emit_asbx(OP_LOADI, 1, 10, 1); /* then: R1 = 10 */
    e.patch_jump(skip_then);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    /* if (false) R2 = 10; else R2 = 20; */
    e.emit_abc(OP_LOADBOOL, 0, 0, 0, 2); /* R0 = false */
    int skip2 = e.emit_jump(OP_JMPIFNOT, 0, 2);
    e.emit_asbx(OP_LOADI, 2, 10, 2); /* then: R2 = 10 */
    int end2 = e.emit_jump(OP_JMP, 0, 2);
    e.patch_jump(skip2);
    e.emit_asbx(OP_LOADI, 2, 20, 2); /* else: R2 = 20 */
    e.patch_jump(end2);
    e.emit_abc(OP_PRINT, 2, 0, 0, 2);

    ObjFunc *fn = e.end(3);

    printf("[expect: 10 20] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Loop (while i < 10, i = i + 1; print i)
** ========================================================= */

static bool test_loop()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_loop", 0);

    /* R0 = 0 (counter), R1 = 10 (limit) */
    e.emit_asbx(OP_LOADI, 0, 0, 1);
    e.emit_asbx(OP_LOADI, 1, 10, 1);

    /* loop_start: */
    int loop_start = e.current_offset();

    /* R2 = (R0 < R1) */
    e.emit_abc(OP_LT, 2, 0, 1, 1);
    /* if !R2, break */
    int break_jmp = e.emit_jump(OP_JMPIFNOT, 2, 1);

    /* R0 = R0 + 1 */
    e.emit_abc(OP_ADDI, 0, 0, 1, 1);

    /* loop back */
    e.emit_loop(loop_start, 0, 1);

    e.patch_jump(break_jmp);

    /* print R0 (should be 10) */
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn = e.end(3);

    printf("[expect: 10] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Globals (GETGLOBAL, SETGLOBAL)
** ========================================================= */

static bool test_globals()
{
    VM vm;

    /* Define a global "x" = 42 */
    int idx = vm.def_global("x", val_int(42));

    Emitter e(&vm.get_gc());
    e.begin("test_globals", 0);

    /* R0 = globals[idx] */
    e.emit_abx(OP_GETGLOBAL, 0, idx, 1);
    /* R0 = R0 + 8 */
    e.emit_abc(OP_ADDI, 0, 0, 8, 1);
    /* globals[idx] = R0 */
    e.emit_abx(OP_SETGLOBAL, 0, idx, 1);
    /* R1 = globals[idx] */
    e.emit_abx(OP_GETGLOBAL, 1, idx, 1);
    /* print R1 (should be 50) */
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 50] ");
    fflush(stdout);
    vm.run(fn);

    /* Also check from C++ side — by index (O(1)) */
    Value v = vm.get_global(idx);
    if (v.type != VAL_INT || v.as.integer != 50)
        return false;

    return true;
}

/* =========================================================
** Test: Native function call
** ========================================================= */

static int native_add(VM *vm, Value *args, int nargs)
{
    (void)vm;
    if (nargs < 2)
        return 0;
    args[0] = val_int(args[0].as.integer + args[1].as.integer);
    return 1;
}

static int native_square(VM *vm, Value *args, int nargs)
{
    (void)vm;
    (void)nargs;
    int32_t x = args[0].as.integer;
    args[0] = val_int(x * x);
    return 1;
}

static bool test_native_call()
{
    VM vm;

    int add_idx = vm.def_native("add", native_add, 2);
    int sq_idx = vm.def_native("square", native_square, 1);

    Emitter e(&vm.get_gc());
    e.begin("test_native", 0);

    /* R0 = add (from global) */
    e.emit_abx(OP_GETGLOBAL, 0, add_idx, 1);
    /* R1 = 3, R2 = 4 (args) */
    e.emit_asbx(OP_LOADI, 1, 3, 1);
    e.emit_asbx(OP_LOADI, 2, 4, 1);
    /* CALL R0, 2, 1 → R0 = add(3, 4) = 7 */
    e.emit_abc(OP_CALL, 0, 2, 1, 1);
    e.emit_abc(OP_PRINT, 0, 0, 0, 1);

    /* R3 = square (from global) */
    e.emit_abx(OP_GETGLOBAL, 3, sq_idx, 1);
    /* R4 = 9 */
    e.emit_asbx(OP_LOADI, 4, 9, 1);
    /* CALL R3, 1, 1 → R3 = square(9) = 81 */
    e.emit_abc(OP_CALL, 3, 1, 1, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    ObjFunc *fn = e.end(5);

    printf("[expect: 7 81] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Script function call (CALL/RETURN)
** ========================================================= */

static bool test_script_call()
{
    VM vm;

    /* Create a function: double(n) → n * 2 */
    Emitter e_fn(&vm.get_gc());
    e_fn.begin("double", 1);
    /* R0 = n (param), R1 = 2 */
    e_fn.emit_asbx(OP_LOADI, 1, 2, 1);
    /* R0 = R0 * R1 */
    e_fn.emit_abc(OP_MUL, 0, 0, 1, 1);
    /* return R0, 1 */
    e_fn.emit_abc(OP_RETURN, 0, 1, 0, 1);
    ObjFunc *fn_double = e_fn.end(2);

    /* Register as global */
    ObjClosure *cl_double = (ObjClosure *)zen_alloc(&vm.get_gc(), sizeof(ObjClosure));
    cl_double->obj.type = OBJ_CLOSURE;
    cl_double->obj.color = GC_BLACK;
    cl_double->obj.hash = 0;
    cl_double->obj.gc_next = vm.get_gc().objects;
    vm.get_gc().objects = (Obj *)cl_double;
    cl_double->func = fn_double;
    cl_double->upvalues = nullptr;
    cl_double->upvalue_count = 0;

    int dbl_idx = vm.def_global("double", val_obj((Obj *)cl_double));

    /* Main script: print double(21) */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    /* R0 = double */
    e_main.emit_abx(OP_GETGLOBAL, 0, dbl_idx, 1);
    /* R1 = 21 */
    e_main.emit_asbx(OP_LOADI, 1, 21, 1);
    /* CALL R0, 1, 1 → R0 = double(21) = 42 */
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    /* print R0 */
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(2);

    printf("[expect: 42] ");
    fflush(stdout);
    vm.run(fn_main);

    return true;
}

/* =========================================================
** Test: MOVE
** ========================================================= */

static bool test_move()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_move", 0);

    e.emit_asbx(OP_LOADI, 0, 77, 1);
    e.emit_abc(OP_MOVE, 1, 0, 0, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 77] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: CONCAT (string concatenation)
** — NOTE: CONCAT is currently a stub, so we test LEN instead
** ========================================================= */

static bool test_len()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_len", 0);

    int k_str = e.add_string_constant("hello");
    e.emit_abx(OP_LOADK, 0, k_str, 1);

    /* R1 = #R0 → 5 */
    e.emit_abc(OP_LEN, 1, 0, 0, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 5] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Disassembler output
** ========================================================= */

static bool test_disassemble()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("demo", 0);

    e.emit_asbx(OP_LOADI, 0, 10, 1);
    e.emit_asbx(OP_LOADI, 1, 20, 1);
    e.emit_abc(OP_ADD, 2, 0, 1, 2);
    e.emit_abc(OP_PRINT, 2, 0, 0, 2);

    ObjFunc *fn = e.end(3);

    printf("\n");
    disassemble_func(fn);

    return true;
}

/* =========================================================
** Helper: create a closure from a func
** ========================================================= */

static ObjClosure *wrap_closure(GC *gc, ObjFunc *fn)
{
    ObjClosure *cl = (ObjClosure *)zen_alloc(gc, sizeof(ObjClosure));
    cl->obj.type = OBJ_CLOSURE;
    cl->obj.color = GC_BLACK;
    cl->obj.hash = 0;
    cl->obj.gc_next = gc->objects;
    gc->objects = (Obj *)cl;
    cl->func = fn;
    cl->upvalues = nullptr;
    cl->upvalue_count = 0;
    return cl;
}

/* =========================================================
** Test: Fibonacci (recursive, benchmark)
**
** def fib(n):
**   if n < 2: return n
**   return fib(n-1) + fib(n-2)
**
** Registers for fib:
**   R0 = n (param)
**   R1 = temp (2 / comparison result)
**   R2 = temp
**   R3 = n-1
**   R4 = fib closure (for call)
**   R5 = arg slot
**   R6 = fib closure (for 2nd call)
**   R7 = arg slot
** ========================================================= */

static bool test_fib()
{
    VM vm;

    /* --- Build fib function --- */
    Emitter e_fib(&vm.get_gc());
    e_fib.begin("fib", 1);

    /* R1 = 2 */
    e_fib.emit_asbx(OP_LOADI, 1, 2, 1);
    /* R2 = (R0 < R1) i.e. n < 2 */
    e_fib.emit_abc(OP_LT, 2, 0, 1, 1);
    /* if NOT (n < 2), skip return */
    int skip_base = e_fib.emit_jump(OP_JMPIFNOT, 2, 1);
    /* return n */
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 1);
    e_fib.patch_jump(skip_base);

    /* R3 = n - 1 */
    e_fib.emit_abc(OP_SUBI, 3, 0, 1, 2);

    /* Call fib(n-1): GETGLOBAL R4, fib_idx → set later */
    int fib_load1 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 4, 0, 2); /* placeholder Bx=0 */
    /* R5 = n-1 (arg) */
    e_fib.emit_abc(OP_MOVE, 5, 3, 0, 2);
    /* CALL R4, 1, 1 → R4 = fib(n-1) */
    e_fib.emit_abc(OP_CALL, 4, 1, 1, 2);

    /* R5 = n - 2 */
    e_fib.emit_abc(OP_SUBI, 5, 0, 2, 3);

    /* Call fib(n-2): GETGLOBAL R6, fib_idx → set later */
    int fib_load2 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 6, 0, 3); /* placeholder Bx=0 */
    /* R7 = n-2 (arg) */
    e_fib.emit_abc(OP_MOVE, 7, 5, 0, 3);
    /* CALL R6, 1, 1 → R6 = fib(n-2) */
    e_fib.emit_abc(OP_CALL, 6, 1, 1, 3);

    /* R0 = R4 + R6 */
    e_fib.emit_abc(OP_ADD, 0, 4, 6, 4);
    /* return R0 */
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 4);

    ObjFunc *fn_fib = e_fib.end(8);

    /* Register fib as global */
    ObjClosure *cl_fib = wrap_closure(&vm.get_gc(), fn_fib);
    int fib_idx = vm.def_global("fib", val_obj((Obj *)cl_fib));

    /* Patch GETGLOBAL instructions with the actual fib_idx */
    fn_fib->code[fib_load1] = ZEN_ENCODE_BX(OP_GETGLOBAL, 4, fib_idx);
    fn_fib->code[fib_load2] = ZEN_ENCODE_BX(OP_GETGLOBAL, 6, fib_idx);

    /* --- Build main script --- */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    /* R0 = fib */
    e_main.emit_abx(OP_GETGLOBAL, 0, fib_idx, 1);
    /* R1 = 30 */
    e_main.emit_asbx(OP_LOADI, 1, 30, 1);
    /* CALL R0, 1, 1 → R0 = fib(30) */
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    /* print R0 */
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(2);

    printf("[expect: 832040] ");
    fflush(stdout);

    clock_t start = clock();
    vm.run(fn_main);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("  fib(30) = %.3fs\n", elapsed);

    return true;
}

/* =========================================================
** Test: Fibonacci 35 (benchmark)
** ========================================================= */

static bool test_fib35()
{
    VM vm;

    /* Build fib (same as above) */
    Emitter e_fib(&vm.get_gc());
    e_fib.begin("fib", 1);

    e_fib.emit_asbx(OP_LOADI, 1, 2, 1);
    e_fib.emit_abc(OP_LT, 2, 0, 1, 1);
    int skip_base = e_fib.emit_jump(OP_JMPIFNOT, 2, 1);
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 1);
    e_fib.patch_jump(skip_base);

    e_fib.emit_abc(OP_SUBI, 3, 0, 1, 2);
    int fib_load1 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 4, 0, 2);
    e_fib.emit_abc(OP_MOVE, 5, 3, 0, 2);
    e_fib.emit_abc(OP_CALL, 4, 1, 1, 2);

    e_fib.emit_abc(OP_SUBI, 5, 0, 2, 3);
    int fib_load2 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 6, 0, 3);
    e_fib.emit_abc(OP_MOVE, 7, 5, 0, 3);
    e_fib.emit_abc(OP_CALL, 6, 1, 1, 3);

    e_fib.emit_abc(OP_ADD, 0, 4, 6, 4);
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 4);

    ObjFunc *fn_fib = e_fib.end(8);

    ObjClosure *cl_fib = wrap_closure(&vm.get_gc(), fn_fib);
    int fib_idx = vm.def_global("fib", val_obj((Obj *)cl_fib));

    fn_fib->code[fib_load1] = ZEN_ENCODE_BX(OP_GETGLOBAL, 4, fib_idx);
    fn_fib->code[fib_load2] = ZEN_ENCODE_BX(OP_GETGLOBAL, 6, fib_idx);

    /* Main: fib(35) */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    e_main.emit_abx(OP_GETGLOBAL, 0, fib_idx, 1);
    e_main.emit_asbx(OP_LOADI, 1, 35, 1);
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(2);

    printf("[fib(35)] ");
    fflush(stdout);

    clock_t start = clock();
    vm.run(fn_main);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("  time = %.3fs\n", elapsed);

    /* fib(35) = 9227465 */
    return true;
}

/* =========================================================
** Test: Fibonacci 35 with FUSED LTJMPIFNOT (benchmark)
** ========================================================= */

static bool test_fib35_fused()
{
    VM vm;

    /* Build fib using LTJMPIFNOT fused superinstruction */
    Emitter e_fib(&vm.get_gc());
    e_fib.begin("fib", 1);

    /* R1 = 2 */
    e_fib.emit_asbx(OP_LOADI, 1, 2, 1);
    /* Fused: if !(R0 < R1) jump → skip base case  (2 words) */
    int skip_base = e_fib.emit_lt_jmpifnot(0, 1, 1);
    /* base case: return n */
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 1);
    e_fib.patch_fused_jump(skip_base);

    /* R3 = n - 1 */
    e_fib.emit_abc(OP_SUBI, 3, 0, 1, 2);
    int fib_load1 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 4, 0, 2);
    e_fib.emit_abc(OP_MOVE, 5, 3, 0, 2);
    e_fib.emit_abc(OP_CALL, 4, 1, 1, 2);

    /* R5 = n - 2 */
    e_fib.emit_abc(OP_SUBI, 5, 0, 2, 3);
    int fib_load2 = e_fib.current_offset();
    e_fib.emit_abx(OP_GETGLOBAL, 6, 0, 3);
    e_fib.emit_abc(OP_MOVE, 7, 5, 0, 3);
    e_fib.emit_abc(OP_CALL, 6, 1, 1, 3);

    e_fib.emit_abc(OP_ADD, 0, 4, 6, 4);
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 4);

    ObjFunc *fn_fib = e_fib.end(8);

    ObjClosure *cl_fib = wrap_closure(&vm.get_gc(), fn_fib);
    int fib_idx = vm.def_global("fib", val_obj((Obj *)cl_fib));

    fn_fib->code[fib_load1] = ZEN_ENCODE_BX(OP_GETGLOBAL, 4, fib_idx);
    fn_fib->code[fib_load2] = ZEN_ENCODE_BX(OP_GETGLOBAL, 6, fib_idx);

    /* Main: fib(35) */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    e_main.emit_abx(OP_GETGLOBAL, 0, fib_idx, 1);
    e_main.emit_asbx(OP_LOADI, 1, 35, 1);
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(2);

    printf("[fib(35) fused] ");
    fflush(stdout);

    clock_t start = clock();
    vm.run(fn_main);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("  time = %.3fs\n", elapsed);

    return true;
}

/* =========================================================
** Test: Fibonacci 35 with CALLGLOBAL (benchmark)
**
** Fuses GETGLOBAL + CALL → one instruction, one dispatch.
** fib bytecode: LOADI, LTJMPIFNOT, RETURN, SUBI,
**               MOVE, CALLGLOBAL, SUBI, MOVE, CALLGLOBAL,
**               ADD, RETURN
** ========================================================= */

static bool test_fib35_callglobal()
{
    VM vm;

    Emitter e_fib(&vm.get_gc());
    e_fib.begin("fib", 1);

    /* R1 = 2 */
    e_fib.emit_asbx(OP_LOADI, 1, 2, 1);
    /* Fused: if !(R0 < R1) jump (2 words) */
    int skip_base = e_fib.emit_lt_jmpifnot(0, 1, 1);
    /* base case: return n */
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 1);
    e_fib.patch_fused_jump(skip_base);

    /* R2 = n - 1; put directly at R3+1 = R4 position? No — put at R3 arg slot */
    /* CALLGLOBAL convention: args at R[A+1]..R[A+nargs] */
    /* So for CALLGLOBAL R3, 1, 1: arg at R4 */
    e_fib.emit_abc(OP_SUBI, 4, 0, 1, 2); /* R4 = n-1 (arg slot for call at R3) */
    /* CALLGLOBAL R3, 1, 1, fib_idx — placeholder idx=0, patch later */
    int cg1_offset = e_fib.current_offset();
    e_fib.emit_callglobal(3, 1, 1, 0, 2); /* R3 = fib(n-1) */

    /* R6 = n - 2 (arg slot for call at R5) */
    e_fib.emit_abc(OP_SUBI, 6, 0, 2, 3);
    /* CALLGLOBAL R5, 1, 1, fib_idx */
    int cg2_offset = e_fib.current_offset();
    e_fib.emit_callglobal(5, 1, 1, 0, 3); /* R5 = fib(n-2) */

    /* R0 = R3 + R5 */
    e_fib.emit_abc(OP_ADD, 0, 3, 5, 4);
    e_fib.emit_abc(OP_RETURN, 0, 1, 0, 4);

    ObjFunc *fn_fib = e_fib.end(7);

    ObjClosure *cl_fib = wrap_closure(&vm.get_gc(), fn_fib);
    int fib_idx = vm.def_global("fib", val_obj((Obj *)cl_fib));

    /* Patch CALLGLOBAL word2 with actual fib_idx */
    fn_fib->code[cg1_offset + 1] = ZEN_ENCODE_BX(OP_HALT, 0, fib_idx);
    fn_fib->code[cg2_offset + 1] = ZEN_ENCODE_BX(OP_HALT, 0, fib_idx);

    /* Main: fib(35) */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    /* R1 = 35 (arg at R0+1) */
    e_main.emit_asbx(OP_LOADI, 1, 35, 1);
    e_main.emit_callglobal(0, 1, 1, fib_idx, 1);
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(2);

    printf("[fib(35) callglobal] ");
    fflush(stdout);

    clock_t start = clock();
    vm.run(fn_main);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("  time = %.3fs\n", elapsed);

    return true;
}

/* =========================================================
** Test: NEWARRAY + LEN
** ========================================================= */

static bool test_newarray()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_arr", 0);

    /* R0 = [] */
    e.emit_abc(OP_NEWARRAY, 0, 0, 0, 1);
    /* R1 = #R0 (should be 0) */
    e.emit_abc(OP_LEN, 1, 0, 0, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 0] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: NEWMAP
** ========================================================= */

static bool test_newmap()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_map", 0);

    /* R0 = {} */
    e.emit_abc(OP_NEWMAP, 0, 0, 0, 1);
    /* R1 = #R0 (should be 0) */
    e.emit_abc(OP_LEN, 1, 0, 0, 1);
    e.emit_abc(OP_PRINT, 1, 0, 0, 1);

    ObjFunc *fn = e.end(2);

    printf("[expect: 0] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Multiple returns from nested calls
**
** def add3(a, b, c) { return a + b + c; }
** print(add3(1, 2, 3))  → 6
** ========================================================= */

static bool test_multi_arg_call()
{
    VM vm;

    /* Build add3(a, b, c) → a + b + c */
    Emitter e_fn(&vm.get_gc());
    e_fn.begin("add3", 3);
    /* R0=a, R1=b, R2=c */
    /* R3 = R0 + R1 */
    e_fn.emit_abc(OP_ADD, 3, 0, 1, 1);
    /* R0 = R3 + R2 */
    e_fn.emit_abc(OP_ADD, 0, 3, 2, 1);
    /* return R0, 1 */
    e_fn.emit_abc(OP_RETURN, 0, 1, 0, 1);

    ObjFunc *fn_add3 = e_fn.end(4);
    ObjClosure *cl = wrap_closure(&vm.get_gc(), fn_add3);
    int add3_idx = vm.def_global("add3", val_obj((Obj *)cl));

    /* Main */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);
    /* R0 = add3 */
    e_main.emit_abx(OP_GETGLOBAL, 0, add3_idx, 1);
    /* R1=1, R2=2, R3=3 */
    e_main.emit_asbx(OP_LOADI, 1, 1, 1);
    e_main.emit_asbx(OP_LOADI, 2, 2, 1);
    e_main.emit_asbx(OP_LOADI, 3, 3, 1);
    /* CALL R0, 3, 1 */
    e_main.emit_abc(OP_CALL, 0, 3, 1, 1);
    /* print R0 */
    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(4);

    printf("[expect: 6] ");
    fflush(stdout);
    vm.run(fn_main);

    return true;
}

/* =========================================================
** Test: Nested calls (f(g(x)))
** ========================================================= */

static bool test_nested_calls()
{
    VM vm;

    /* inc(x) = x + 1 */
    Emitter e_inc(&vm.get_gc());
    e_inc.begin("inc", 1);
    e_inc.emit_abc(OP_ADDI, 0, 0, 1, 1);
    e_inc.emit_abc(OP_RETURN, 0, 1, 0, 1);
    ObjFunc *fn_inc = e_inc.end(1);
    ObjClosure *cl_inc = wrap_closure(&vm.get_gc(), fn_inc);
    int inc_idx = vm.def_global("inc", val_obj((Obj *)cl_inc));

    /* dbl(x) = x * 2 */
    Emitter e_dbl(&vm.get_gc());
    e_dbl.begin("dbl", 1);
    e_dbl.emit_asbx(OP_LOADI, 1, 2, 1);
    e_dbl.emit_abc(OP_MUL, 0, 0, 1, 1);
    e_dbl.emit_abc(OP_RETURN, 0, 1, 0, 1);
    ObjFunc *fn_dbl = e_dbl.end(2);
    ObjClosure *cl_dbl = wrap_closure(&vm.get_gc(), fn_dbl);
    int dbl_idx = vm.def_global("dbl", val_obj((Obj *)cl_dbl));

    /* Main: print(dbl(inc(5))) = (5+1)*2 = 12 */
    Emitter e_main(&vm.get_gc());
    e_main.begin(nullptr, 0);

    /* First: inc(5) */
    e_main.emit_abx(OP_GETGLOBAL, 0, inc_idx, 1);
    e_main.emit_asbx(OP_LOADI, 1, 5, 1);
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    /* R0 = 6 */

    /* Second: dbl(R0) */
    e_main.emit_abc(OP_MOVE, 2, 0, 0, 1); /* save inc result */
    e_main.emit_abx(OP_GETGLOBAL, 0, dbl_idx, 1);
    e_main.emit_abc(OP_MOVE, 1, 2, 0, 1); /* arg = saved result */
    e_main.emit_abc(OP_CALL, 0, 1, 1, 1);
    /* R0 = 12 */

    e_main.emit_abc(OP_PRINT, 0, 0, 0, 1);

    ObjFunc *fn_main = e_main.end(3);

    printf("[expect: 12] ");
    fflush(stdout);
    vm.run(fn_main);

    return true;
}

/* =========================================================
** Test: Edge cases — division by zero, overflow
** ========================================================= */

static bool test_edge_cases()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_edge", 0);

    /* Division by zero: 1 / 0 → inf */
    e.emit_asbx(OP_LOADI, 0, 1, 1);
    e.emit_asbx(OP_LOADI, 1, 0, 1);
    e.emit_abc(OP_DIV, 2, 0, 1, 1);
    e.emit_abc(OP_PRINT, 2, 0, 0, 1);

    /* Integer overflow: 2147483647 (max int32) + 1 via LOADK */
    int k = e.add_constant(val_int(2147483647));
    e.emit_abx(OP_LOADK, 3, k, 1);
    e.emit_abc(OP_ADDI, 3, 3, 1, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    ObjFunc *fn = e.end(4);

    printf("[expect: inf <overflow>] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Mixed int/float arithmetic
** ========================================================= */

static bool test_mixed_arith()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_mixed", 0);

    /* R0 = 10 (int), R1 = 3.5 (float) */
    e.emit_asbx(OP_LOADI, 0, 10, 1);
    int kf = e.add_constant(val_float(3.5));
    e.emit_abx(OP_LOADK, 1, kf, 1);

    /* R2 = R0 + R1 → 13.5 (float) */
    e.emit_abc(OP_ADD, 2, 0, 1, 1);
    e.emit_abc(OP_PRINT, 2, 0, 0, 1);

    /* R3 = R0 * R1 → 35 (float) */
    e.emit_abc(OP_MUL, 3, 0, 1, 1);
    e.emit_abc(OP_PRINT, 3, 0, 0, 1);

    ObjFunc *fn = e.end(4);

    printf("[expect: 13.5 35] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Constant deduplication
** ========================================================= */

static bool test_const_dedup()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_dedup", 0);

    int k1 = e.add_constant(val_int(42));
    int k2 = e.add_constant(val_int(42));
    int k3 = e.add_constant(val_int(99));

    if (k1 != k2)
        return false; /* same value → same index */
    if (k1 == k3)
        return false; /* different value → different index */

    /* String dedup via interning */
    int s1 = e.add_string_constant("hello");
    int s2 = e.add_string_constant("hello");
    if (s1 != s2)
        return false;

    return true;
}

/* =========================================================
** Test: Jump backpatching (complex if-else-if)
** ========================================================= */

static bool test_backpatch()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_bp", 0);

    /*
    ** R0 = 2
    ** if R0 == 1: R1 = 10
    ** elif R0 == 2: R1 = 20
    ** else: R1 = 30
    ** print R1
    */
    e.emit_asbx(OP_LOADI, 0, 2, 1);
    e.emit_asbx(OP_LOADI, 2, 1, 1);

    /* if R0 == 1 */
    e.emit_abc(OP_EQ, 3, 0, 2, 1);
    int skip1 = e.emit_jump(OP_JMPIFNOT, 3, 1);
    e.emit_asbx(OP_LOADI, 1, 10, 1);
    int end1 = e.emit_jump(OP_JMP, 0, 1);
    e.patch_jump(skip1);

    /* elif R0 == 2 */
    e.emit_asbx(OP_LOADI, 2, 2, 2);
    e.emit_abc(OP_EQ, 3, 0, 2, 2);
    int skip2 = e.emit_jump(OP_JMPIFNOT, 3, 2);
    e.emit_asbx(OP_LOADI, 1, 20, 2);
    int end2 = e.emit_jump(OP_JMP, 0, 2);
    e.patch_jump(skip2);

    /* else */
    e.emit_asbx(OP_LOADI, 1, 30, 3);
    e.patch_jump(end1);
    e.patch_jump(end2);

    e.emit_abc(OP_PRINT, 1, 0, 0, 4);

    ObjFunc *fn = e.end(4);

    printf("[expect: 20] ");
    fflush(stdout);
    vm.run(fn);

    return true;
}

/* =========================================================
** Test: Countdown loop (while n > 0, n = n - 1)
** ========================================================= */

static bool test_countdown()
{
    VM vm;
    Emitter e(&vm.get_gc());
    e.begin("test_cd", 0);

    /* R0 = 1000000 (1M iterations) */
    int k = e.add_constant(val_int(1000000));
    e.emit_abx(OP_LOADK, 0, k, 1);
    e.emit_asbx(OP_LOADI, 1, 0, 1); /* R1 = 0 */

    int loop_start = e.current_offset();
    /* R2 = (R0 > R1) i.e. (R1 < R0) */
    e.emit_abc(OP_LT, 2, 1, 0, 2);
    int break_jmp = e.emit_jump(OP_JMPIFNOT, 2, 2);

    /* R0 = R0 - 1 */
    e.emit_abc(OP_SUBI, 0, 0, 1, 2);

    e.emit_loop(loop_start, 0, 2);
    e.patch_jump(break_jmp);

    /* print R0 (should be 0) */
    e.emit_abc(OP_PRINT, 0, 0, 0, 3);

    ObjFunc *fn = e.end(3);

    printf("[expect: 0] ");
    fflush(stdout);

    clock_t start = clock();
    vm.run(fn);
    clock_t end_t = clock();

    double elapsed = (double)(end_t - start) / CLOCKS_PER_SEC;
    printf("  1M iters = %.3fs\n", elapsed);

    return true;
}

/* =========================================================
** MAIN
** ========================================================= */

int main(int argc, char **argv)
{
    printf("=== zen VM tests ===\n\n");

    /* --- Value-level tests (no VM) --- */
    printf("--- Value ---\n");
    RUN_TEST(test_value_types);
    RUN_TEST(test_truthiness);
    RUN_TEST(test_values_equal);
    RUN_TEST(test_conversions);

    /* --- Memory / Strings --- */
    printf("\n--- Strings ---\n");
    RUN_TEST(test_string_interning);

    /* --- Emitter --- */
    printf("\n--- Emitter ---\n");
    RUN_TEST(test_emitter_basic);
    RUN_TEST(test_const_dedup);

    /* --- VM basic ops --- */
    printf("\n--- VM ops ---\n");
    RUN_TEST(test_loadi_print);
    RUN_TEST(test_loadi_negative);
    RUN_TEST(test_loadbool_skip);
    RUN_TEST(test_loadnil);
    RUN_TEST(test_loadk);
    RUN_TEST(test_move);
    RUN_TEST(test_arithmetic);
    RUN_TEST(test_addi_subi);
    RUN_TEST(test_bitwise);
    RUN_TEST(test_comparison);
    RUN_TEST(test_mixed_arith);
    RUN_TEST(test_edge_cases);

    /* --- Control flow --- */
    printf("\n--- Control flow ---\n");
    RUN_TEST(test_jmp);
    RUN_TEST(test_conditional_jumps);
    RUN_TEST(test_loop);
    RUN_TEST(test_backpatch);

    /* --- Functions --- */
    printf("\n--- Functions ---\n");
    RUN_TEST(test_globals);
    RUN_TEST(test_native_call);
    RUN_TEST(test_script_call);
    RUN_TEST(test_multi_arg_call);
    RUN_TEST(test_nested_calls);

    /* --- Collections --- */
    printf("\n--- Collections ---\n");
    RUN_TEST(test_newarray);
    RUN_TEST(test_newmap);
    RUN_TEST(test_len);

    /* --- Disassembler --- */
    printf("\n--- Debug ---\n");
    RUN_TEST(test_disassemble);

    /* --- Benchmarks --- */
    printf("\n--- Benchmarks ---\n");
    RUN_TEST(test_countdown);
    RUN_TEST(test_fib);
    RUN_TEST(test_fib35);
    RUN_TEST(test_fib35_fused);
    RUN_TEST(test_fib35_callglobal);

    /* --- Summary --- */
    printf("\n========================================\n");
    printf("  %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
