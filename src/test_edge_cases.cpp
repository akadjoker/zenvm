/* =========================================================
** test_edge_cases.cpp — Comprehensive edge-case tests for zen collections.
** Validates correctness of Array, Map, and Set under boundary conditions.
** Build: g++ -std=c++11 -O2 -Isrc src/test_edge_cases.cpp src/memory.cpp -o test_edge_cases
** ========================================================= */

#include "memory.h"

#include <cstdio>
#include <cassert>
#include <cstring>

using namespace zen;

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name)                                   \
    do                                               \
    {                                                \
        tests_total++;                               \
        printf("  [%3d] %-50s ", tests_total, name); \
    } while (0)

#define PASS()          \
    do                  \
    {                   \
        tests_passed++; \
        printf("OK\n"); \
    } while (0)

/* =========================================================
** ARRAY EDGE CASES
** ========================================================= */

static void test_array_empty_ops()
{
    printf("\n--- Array: empty operations ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    TEST("empty array count == 0");
    assert(arr_count(arr) == 0);
    assert(arr_capacity(arr) == 0);
    PASS();

    TEST("pop from empty returns nil");
    Value v = array_pop(arr);
    assert(is_nil(v));
    PASS();

    TEST("array_get out of bounds returns nil");
    v = array_get(arr, 0);
    assert(is_nil(v));
    v = array_get(arr, -1);
    assert(is_nil(v));
    v = array_get(arr, 100);
    assert(is_nil(v));
    PASS();

    TEST("array_set on empty does nothing");
    array_set(&gc, arr, 0, val_int(42));
    assert(arr_count(arr) == 0);
    PASS();

    TEST("array_remove on empty does nothing");
    array_remove(arr, 0);
    assert(arr_count(arr) == 0);
    array_remove(arr, -1);
    assert(arr_count(arr) == 0);
    PASS();

    TEST("array_find on empty returns -1");
    assert(array_find(arr, val_int(0)) == -1);
    PASS();

    TEST("array_find_int on empty returns -1");
    assert(array_find_int(arr, 0) == -1);
    PASS();

    TEST("array_contains on empty is false");
    assert(!array_contains(arr, val_int(42)));
    PASS();

    TEST("array_reverse on empty is safe");
    array_reverse(arr);
    assert(arr_count(arr) == 0);
    PASS();

    TEST("array_sort_int on empty is safe");
    array_sort_int(arr);
    assert(arr_count(arr) == 0);
    PASS();

    TEST("array_clear on empty is safe");
    array_clear(arr);
    assert(arr_count(arr) == 0);
    PASS();
}

static void test_array_single_element()
{
    printf("\n--- Array: single element ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    TEST("push single element");
    array_push(&gc, arr, val_int(99));
    assert(arr_count(arr) == 1);
    assert(arr->data[0].as.integer == 99);
    PASS();

    TEST("get index 0");
    Value v = array_get(arr, 0);
    assert(v.as.integer == 99);
    PASS();

    TEST("get index 1 returns nil");
    v = array_get(arr, 1);
    assert(is_nil(v));
    PASS();

    TEST("set index 0");
    array_set(&gc, arr, 0, val_int(200));
    assert(arr->data[0].as.integer == 200);
    PASS();

    TEST("find single element");
    assert(array_find(arr, val_int(200)) == 0);
    assert(array_find(arr, val_int(99)) == -1);
    PASS();

    TEST("reverse single element (no-op)");
    array_reverse(arr);
    assert(arr->data[0].as.integer == 200);
    PASS();

    TEST("sort single element (no-op)");
    array_sort_int(arr);
    assert(arr->data[0].as.integer == 200);
    PASS();

    TEST("pop single element");
    v = array_pop(arr);
    assert(v.as.integer == 200);
    assert(arr_count(arr) == 0);
    PASS();

    TEST("pop again from now-empty");
    v = array_pop(arr);
    assert(is_nil(v));
    PASS();
}

static void test_array_boundary_insert_remove()
{
    printf("\n--- Array: insert/remove at boundaries ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    /* Push [10, 20, 30] */
    array_push(&gc, arr, val_int(10));
    array_push(&gc, arr, val_int(20));
    array_push(&gc, arr, val_int(30));

    TEST("insert at index 0 (front)");
    array_insert(&gc, arr, 0, val_int(5));
    assert(arr_count(arr) == 4);
    assert(arr->data[0].as.integer == 5);
    assert(arr->data[1].as.integer == 10);
    PASS();

    TEST("insert at count (end)");
    array_insert(&gc, arr, arr_count(arr), val_int(99));
    assert(arr_count(arr) == 5);
    assert(arr->data[4].as.integer == 99);
    PASS();

    TEST("insert at negative index clamps to 0");
    array_insert(&gc, arr, -5, val_int(1));
    assert(arr_count(arr) == 6);
    assert(arr->data[0].as.integer == 1);
    PASS();

    TEST("insert beyond count clamps to end");
    array_insert(&gc, arr, 1000, val_int(999));
    assert(arr_count(arr) == 7);
    assert(arr->data[6].as.integer == 999);
    PASS();

    TEST("remove at index 0");
    array_remove(arr, 0);
    assert(arr_count(arr) == 6);
    assert(arr->data[0].as.integer == 5);
    PASS();

    TEST("remove at last index");
    int32_t last = arr_count(arr) - 1;
    int32_t last_val = arr->data[last].as.integer;
    array_remove(arr, last);
    assert(arr_count(arr) == 5);
    (void)last_val;
    PASS();

    TEST("remove beyond count does nothing");
    int32_t before = arr_count(arr);
    array_remove(arr, 100);
    assert(arr_count(arr) == before);
    array_remove(arr, -1);
    assert(arr_count(arr) == before);
    PASS();
}

static void test_array_grow_trigger()
{
    printf("\n--- Array: growth triggers ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    TEST("first push allocates capacity 8");
    array_push(&gc, arr, val_int(0));
    assert(arr_capacity(arr) == 8);
    PASS();

    TEST("push to capacity boundary (8 elements)");
    for (int i = 1; i < 8; i++)
        array_push(&gc, arr, val_int(i));
    assert(arr_count(arr) == 8);
    assert(arr_capacity(arr) == 8);
    PASS();

    TEST("push beyond 8 doubles to 16");
    array_push(&gc, arr, val_int(8));
    assert(arr_count(arr) == 9);
    assert(arr_capacity(arr) == 16);
    PASS();

    TEST("fill to 16, push beyond doubles to 32");
    for (int i = 9; i < 16; i++)
        array_push(&gc, arr, val_int(i));
    assert(arr_capacity(arr) == 16);
    array_push(&gc, arr, val_int(16));
    assert(arr_capacity(arr) == 32);
    PASS();

    TEST("reserve non-power-of-2 rounds up correctly");
    ObjArray *arr2 = new_array(&gc);
    array_reserve(&gc, arr2, 10);
    /* 10 is not power of 2 but reserve uses exact cap */
    assert(arr_capacity(arr2) >= 10);
    PASS();
}

static void test_array_find_int_edge_cases()
{
    printf("\n--- Array: array_find_int edge cases ---\n");
    GC gc;
    gc_init(&gc);

    /* Test with counts that exercise the tail loop (non-8-aligned) */
    for (int n = 1; n <= 20; n++)
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < n; i++)
            array_push_int(&gc, arr, i * 10);

        char buf[64];

        snprintf(buf, sizeof(buf), "find_int first elem (n=%d)", n);
        TEST(buf);
        assert(array_find_int(arr, 0) == 0);
        PASS();

        snprintf(buf, sizeof(buf), "find_int last elem (n=%d)", n);
        TEST(buf);
        assert(array_find_int(arr, (n - 1) * 10) == n - 1);
        PASS();

        snprintf(buf, sizeof(buf), "find_int miss (n=%d)", n);
        TEST(buf);
        assert(array_find_int(arr, -1) == -1);
        assert(array_find_int(arr, n * 10) == -1);
        PASS();
    }

    TEST("find_int with exactly 8 elements (no tail)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 8; i++)
            array_push_int(&gc, arr, i);
        assert(array_find_int(arr, 7) == 7);
        assert(array_find_int(arr, 8) == -1);
    }
    PASS();

    TEST("find_int with 9 elements (1 in tail)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 9; i++)
            array_push_int(&gc, arr, i);
        assert(array_find_int(arr, 8) == 8); /* in tail */
        assert(array_find_int(arr, 0) == 0); /* in main loop */
    }
    PASS();

    TEST("find_int with 15 elements (7 in tail)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 15; i++)
            array_push_int(&gc, arr, i * 3);
        assert(array_find_int(arr, 14 * 3) == 14); /* last in tail */
        assert(array_find_int(arr, 8 * 3) == 8);   /* first in tail */
    }
    PASS();
}

static void test_array_push_n_edge_cases()
{
    printf("\n--- Array: array_push_n edge cases ---\n");
    GC gc;
    gc_init(&gc);

    TEST("push_n with 0 elements");
    {
        ObjArray *arr = new_array(&gc);
        array_push(&gc, arr, val_int(1));
        Value empty[1]; /* dummy, won't be read */
        array_push_n(&gc, arr, empty, 0);
        assert(arr_count(arr) == 1);
    }
    PASS();

    TEST("push_n from empty array");
    {
        ObjArray *arr = new_array(&gc);
        Value vals[3] = {val_int(10), val_int(20), val_int(30)};
        array_push_n(&gc, arr, vals, 3);
        assert(arr_count(arr) == 3);
        assert(arr->data[0].as.integer == 10);
        assert(arr->data[2].as.integer == 30);
    }
    PASS();

    TEST("push_n triggers growth");
    {
        ObjArray *arr = new_array(&gc);
        /* Fill to capacity 8 */
        for (int i = 0; i < 7; i++)
            array_push_int(&gc, arr, i);
        assert(arr_capacity(arr) == 8);
        /* Push 5 more — needs growth */
        Value vals[5];
        for (int i = 0; i < 5; i++)
            vals[i] = val_int(100 + i);
        array_push_n(&gc, arr, vals, 5);
        assert(arr_count(arr) == 12);
        assert(arr->data[7].as.integer == 100);
        assert(arr->data[11].as.integer == 104);
    }
    PASS();

    TEST("push_n large batch");
    {
        ObjArray *arr = new_array(&gc);
        const int N = 1000;
        Value *vals = (Value *)malloc(sizeof(Value) * N);
        for (int i = 0; i < N; i++)
            vals[i] = val_int(i);
        array_push_n(&gc, arr, vals, N);
        assert(arr_count(arr) == N);
        assert(arr->data[0].as.integer == 0);
        assert(arr->data[N - 1].as.integer == N - 1);
        free(vals);
    }
    PASS();
}

static void test_array_append_edge_cases()
{
    printf("\n--- Array: array_append edge cases ---\n");
    GC gc;
    gc_init(&gc);

    TEST("append empty src to empty dst");
    {
        ObjArray *dst = new_array(&gc);
        ObjArray *src = new_array(&gc);
        array_append(&gc, dst, src);
        assert(arr_count(dst) == 0);
    }
    PASS();

    TEST("append empty src to non-empty dst");
    {
        ObjArray *dst = new_array(&gc);
        array_push(&gc, dst, val_int(1));
        ObjArray *src = new_array(&gc);
        array_append(&gc, dst, src);
        assert(arr_count(dst) == 1);
    }
    PASS();

    TEST("append non-empty src to empty dst");
    {
        ObjArray *dst = new_array(&gc);
        ObjArray *src = new_array(&gc);
        array_push(&gc, src, val_int(10));
        array_push(&gc, src, val_int(20));
        array_append(&gc, dst, src);
        assert(arr_count(dst) == 2);
        assert(dst->data[0].as.integer == 10);
        assert(dst->data[1].as.integer == 20);
    }
    PASS();

    TEST("append preserves existing dst elements");
    {
        ObjArray *dst = new_array(&gc);
        array_push(&gc, dst, val_int(1));
        array_push(&gc, dst, val_int(2));
        ObjArray *src = new_array(&gc);
        array_push(&gc, src, val_int(3));
        array_push(&gc, src, val_int(4));
        array_append(&gc, dst, src);
        assert(arr_count(dst) == 4);
        assert(dst->data[0].as.integer == 1);
        assert(dst->data[3].as.integer == 4);
    }
    PASS();
}

static void test_array_reverse_edge_cases()
{
    printf("\n--- Array: array_reverse edge cases ---\n");
    GC gc;
    gc_init(&gc);

    TEST("reverse 2 elements");
    {
        ObjArray *arr = new_array(&gc);
        array_push(&gc, arr, val_int(1));
        array_push(&gc, arr, val_int(2));
        array_reverse(arr);
        assert(arr->data[0].as.integer == 2);
        assert(arr->data[1].as.integer == 1);
    }
    PASS();

    TEST("reverse 3 elements (odd)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 1; i <= 3; i++)
            array_push(&gc, arr, val_int(i));
        array_reverse(arr);
        assert(arr->data[0].as.integer == 3);
        assert(arr->data[1].as.integer == 2);
        assert(arr->data[2].as.integer == 1);
    }
    PASS();

    TEST("reverse 4 elements (even, exercises 2-pair unroll)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 1; i <= 4; i++)
            array_push(&gc, arr, val_int(i));
        array_reverse(arr);
        assert(arr->data[0].as.integer == 4);
        assert(arr->data[1].as.integer == 3);
        assert(arr->data[2].as.integer == 2);
        assert(arr->data[3].as.integer == 1);
    }
    PASS();

    TEST("reverse 5 elements (unroll + tail)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 1; i <= 5; i++)
            array_push(&gc, arr, val_int(i));
        array_reverse(arr);
        for (int i = 0; i < 5; i++)
            assert(arr->data[i].as.integer == 5 - i);
    }
    PASS();

    TEST("reverse 10 elements (multiple unroll iterations)");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 10; i++)
            array_push(&gc, arr, val_int(i));
        array_reverse(arr);
        for (int i = 0; i < 10; i++)
            assert(arr->data[i].as.integer == 9 - i);
    }
    PASS();

    TEST("reverse is self-inverse");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 7; i++)
            array_push(&gc, arr, val_int(i * 5));
        array_reverse(arr);
        array_reverse(arr);
        for (int i = 0; i < 7; i++)
            assert(arr->data[i].as.integer == i * 5);
    }
    PASS();
}

static void test_array_sort_edge_cases()
{
    printf("\n--- Array: array_sort_int edge cases ---\n");
    GC gc;
    gc_init(&gc);

    TEST("sort 2 elements (already sorted)");
    {
        ObjArray *arr = new_array(&gc);
        array_push(&gc, arr, val_int(1));
        array_push(&gc, arr, val_int(2));
        array_sort_int(arr);
        assert(arr->data[0].as.integer == 1);
        assert(arr->data[1].as.integer == 2);
    }
    PASS();

    TEST("sort 2 elements (reversed)");
    {
        ObjArray *arr = new_array(&gc);
        array_push(&gc, arr, val_int(5));
        array_push(&gc, arr, val_int(3));
        array_sort_int(arr);
        assert(arr->data[0].as.integer == 3);
        assert(arr->data[1].as.integer == 5);
    }
    PASS();

    TEST("sort already sorted array");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 20; i++)
            array_push(&gc, arr, val_int(i));
        array_sort_int(arr);
        for (int i = 0; i < 20; i++)
            assert(arr->data[i].as.integer == i);
    }
    PASS();

    TEST("sort reverse-sorted array");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 19; i >= 0; i--)
            array_push(&gc, arr, val_int(i));
        array_sort_int(arr);
        for (int i = 0; i < 20; i++)
            assert(arr->data[i].as.integer == i);
    }
    PASS();

    TEST("sort with duplicates");
    {
        ObjArray *arr = new_array(&gc);
        int vals[] = {5, 3, 5, 1, 3, 5, 1, 2, 2, 4};
        for (int i = 0; i < 10; i++)
            array_push(&gc, arr, val_int(vals[i]));
        array_sort_int(arr);
        for (int i = 1; i < 10; i++)
            assert(arr->data[i - 1].as.integer <= arr->data[i].as.integer);
    }
    PASS();

    TEST("sort all same values");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 15; i++)
            array_push(&gc, arr, val_int(7));
        array_sort_int(arr);
        for (int i = 0; i < 15; i++)
            assert(arr->data[i].as.integer == 7);
    }
    PASS();

    TEST("sort negative numbers");
    {
        ObjArray *arr = new_array(&gc);
        int vals[] = {-5, 3, -1, 0, 2, -10, 7};
        for (int i = 0; i < 7; i++)
            array_push(&gc, arr, val_int(vals[i]));
        array_sort_int(arr);
        int expected[] = {-10, -5, -1, 0, 2, 3, 7};
        for (int i = 0; i < 7; i++)
            assert(arr->data[i].as.integer == expected[i]);
    }
    PASS();

    TEST("sort 17 elements (triggers quicksort, not just isort)");
    {
        ObjArray *arr = new_array(&gc);
        int vals[] = {50, 3, 88, 12, 7, 99, 1, 45, 23, 67, 34, 56, 78, 9, 100, 2, 44};
        for (int i = 0; i < 17; i++)
            array_push(&gc, arr, val_int(vals[i]));
        array_sort_int(arr);
        for (int i = 1; i < 17; i++)
            assert(arr->data[i - 1].as.integer <= arr->data[i].as.integer);
    }
    PASS();
}

static void test_array_copy_edge_cases()
{
    printf("\n--- Array: array_copy edge cases ---\n");
    GC gc;
    gc_init(&gc);

    TEST("copy empty to empty");
    {
        ObjArray *src = new_array(&gc);
        ObjArray *dst = new_array(&gc);
        array_copy(&gc, dst, src);
        assert(arr_count(dst) == 0);
    }
    PASS();

    TEST("copy overwrites dst contents");
    {
        ObjArray *dst = new_array(&gc);
        array_push(&gc, dst, val_int(100));
        array_push(&gc, dst, val_int(200));
        array_push(&gc, dst, val_int(300));

        ObjArray *src = new_array(&gc);
        array_push(&gc, src, val_int(1));
        array_push(&gc, src, val_int(2));

        array_copy(&gc, dst, src);
        assert(arr_count(dst) == 2);
        assert(dst->data[0].as.integer == 1);
        assert(dst->data[1].as.integer == 2);
    }
    PASS();

    TEST("copy from larger src to smaller dst");
    {
        ObjArray *dst = new_array(&gc);
        ObjArray *src = new_array(&gc);
        for (int i = 0; i < 20; i++)
            array_push(&gc, src, val_int(i));
        array_copy(&gc, dst, src);
        assert(arr_count(dst) == 20);
        for (int i = 0; i < 20; i++)
            assert(dst->data[i].as.integer == i);
    }
    PASS();
}

static void test_array_clear_reuse()
{
    printf("\n--- Array: clear + reuse ---\n");
    GC gc;
    gc_init(&gc);

    TEST("clear preserves capacity, reuse works");
    {
        ObjArray *arr = new_array(&gc);
        for (int i = 0; i < 20; i++)
            array_push(&gc, arr, val_int(i));
        int32_t cap = arr_capacity(arr);
        array_clear(arr);
        assert(arr_count(arr) == 0);
        assert(arr_capacity(arr) == cap); /* capacity preserved */
        /* Reuse */
        for (int i = 0; i < 20; i++)
            array_push(&gc, arr, val_int(i * 2));
        assert(arr_count(arr) == 20);
        assert(arr->data[0].as.integer == 0);
        assert(arr->data[19].as.integer == 38);
    }
    PASS();
}

static void test_array_mixed_types()
{
    printf("\n--- Array: mixed value types ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    TEST("store nil, bool, int, float");
    array_push(&gc, arr, val_nil());
    array_push(&gc, arr, val_bool(true));
    array_push(&gc, arr, val_int(42));
    array_push(&gc, arr, val_float(3.14));
    assert(arr_count(arr) == 4);
    assert(is_nil(arr->data[0]));
    assert(arr->data[1].as.boolean == true);
    assert(arr->data[2].as.integer == 42);
    assert(arr->data[3].as.number == 3.14);
    PASS();

    TEST("find nil in mixed array");
    assert(array_find(arr, val_nil()) == 0);
    PASS();

    TEST("find bool in mixed array");
    assert(array_find(arr, val_bool(true)) == 1);
    PASS();

    TEST("find float in mixed array");
    assert(array_find(arr, val_float(3.14)) == 3);
    PASS();

    TEST("find non-existent type returns -1");
    assert(array_find(arr, val_int(999)) == -1);
    PASS();
}

/* =========================================================
** MAP EDGE CASES
** ========================================================= */

static void test_map_empty_ops()
{
    printf("\n--- Map: empty operations ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("empty map count == 0");
    assert(map_count(map) == 0);
    PASS();

    TEST("get from empty map");
    bool found;
    Value v = map_get(map, val_int(0), &found);
    assert(!found);
    assert(is_nil(v));
    PASS();

    TEST("delete from empty map");
    assert(!map_delete(map, val_int(0)));
    PASS();

    TEST("contains on empty map");
    assert(!map_contains(map, val_int(0)));
    PASS();

    TEST("clear on empty map is safe");
    map_clear(&gc, map);
    assert(map_count(map) == 0);
    PASS();

    TEST("keys/values on empty map");
    ObjArray *keys = new_array(&gc);
    ObjArray *vals = new_array(&gc);
    map_keys(&gc, map, keys);
    map_values(&gc, map, vals);
    assert(arr_count(keys) == 0);
    assert(arr_count(vals) == 0);
    PASS();
}

static void test_map_single_entry()
{
    printf("\n--- Map: single entry CRUD ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("insert first entry");
    bool is_new = map_set(&gc, map, val_int(42), val_int(100));
    assert(is_new);
    assert(map_count(map) == 1);
    PASS();

    TEST("get first entry");
    bool found;
    Value v = map_get(map, val_int(42), &found);
    assert(found);
    assert(v.as.integer == 100);
    PASS();

    TEST("contains first entry");
    assert(map_contains(map, val_int(42)));
    PASS();

    TEST("update existing key");
    is_new = map_set(&gc, map, val_int(42), val_int(200));
    assert(!is_new); /* updated, not new */
    assert(map_count(map) == 1);
    v = map_get(map, val_int(42), &found);
    assert(found && v.as.integer == 200);
    PASS();

    TEST("delete single entry");
    assert(map_delete(map, val_int(42)));
    assert(map_count(map) == 0);
    assert(!map_contains(map, val_int(42)));
    PASS();

    TEST("get after delete");
    v = map_get(map, val_int(42), &found);
    assert(!found);
    PASS();

    TEST("double delete returns false");
    assert(!map_delete(map, val_int(42)));
    PASS();
}

static void test_map_chain_operations()
{
    printf("\n--- Map: chain operations (collision handling) ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    /* Insert many entries that may collide in the same bucket */
    /* With 16 buckets, keys 0, 16, 32, 48 will land in same bucket */
    TEST("insert entries with same bucket");
    for (int i = 0; i < 4; i++)
    {
        map_set(&gc, map, val_int(i * 16), val_int(i));
    }
    assert(map_count(map) == 4);
    PASS();

    TEST("get all chained entries");
    for (int i = 0; i < 4; i++)
    {
        bool found;
        Value v = map_get(map, val_int(i * 16), &found);
        assert(found && v.as.integer == i);
    }
    PASS();

    TEST("delete head of chain");
    /* The last inserted goes to head */
    assert(map_delete(map, val_int(48)));
    assert(map_count(map) == 3);
    bool found;
    map_get(map, val_int(48), &found);
    assert(!found);
    /* Others still accessible */
    for (int i = 0; i < 3; i++)
    {
        Value v = map_get(map, val_int(i * 16), &found);
        assert(found && v.as.integer == i);
    }
    PASS();

    TEST("delete middle of chain");
    assert(map_delete(map, val_int(16)));
    assert(map_count(map) == 2);
    map_get(map, val_int(16), &found);
    assert(!found);
    /* First and third still ok */
    Value v = map_get(map, val_int(0), &found);
    assert(found && v.as.integer == 0);
    v = map_get(map, val_int(32), &found);
    assert(found && v.as.integer == 2);
    PASS();

    TEST("delete tail of chain");
    assert(map_delete(map, val_int(0)));
    assert(map_count(map) == 1);
    v = map_get(map, val_int(32), &found);
    assert(found && v.as.integer == 2);
    PASS();
}

static void test_map_grow_trigger()
{
    printf("\n--- Map: growth triggers ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("first insert allocates 16 buckets + 16 nodes");
    map_set(&gc, map, val_int(0), val_int(0));
    assert(map->bucket_count == 16);
    assert(map->capacity == 16);
    PASS();

    TEST("insert 14 entries stays at 16 buckets (LF check before insert)");
    for (int i = 1; i < 14; i++)
        map_set(&gc, map, val_int(i), val_int(i));
    assert(map_count(map) == 14);
    assert(map->bucket_count == 16); /* 13*8=104 < 16*7=112, no grow yet */
    PASS();

    TEST("15th insert triggers bucket growth to 32");
    map_set(&gc, map, val_int(14), val_int(14));
    assert(map_count(map) == 15);
    assert(map->bucket_count == 32); /* 14*8=112 >= 16*7=112 → grew */
    PASS();

    TEST("after growth, all entries still accessible");
    for (int i = 0; i < 15; i++)
    {
        bool found;
        Value v = map_get(map, val_int(i), &found);
        assert(found && v.as.integer == i);
    }
    PASS();

    TEST("insert 100 entries (multiple growths)");
    ObjMap *map2 = new_map(&gc);
    for (int i = 0; i < 100; i++)
        map_set(&gc, map2, val_int(i), val_int(i * 3));
    assert(map_count(map2) == 100);
    for (int i = 0; i < 100; i++)
    {
        bool found;
        Value v = map_get(map2, val_int(i), &found);
        assert(found && v.as.integer == i * 3);
    }
    PASS();
}

static void test_map_delete_reinsert()
{
    printf("\n--- Map: delete + re-insert (free list reuse) ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("delete all, re-insert reuses nodes");
    for (int i = 0; i < 16; i++)
        map_set(&gc, map, val_int(i), val_int(i));
    assert(map_count(map) == 16);

    /* Delete all */
    for (int i = 0; i < 16; i++)
        map_delete(map, val_int(i));
    assert(map_count(map) == 0);

    /* Capacity should still exist (nodes not freed) */
    assert(map->capacity > 0);

    /* Re-insert — should reuse free nodes */
    for (int i = 0; i < 16; i++)
        map_set(&gc, map, val_int(i + 100), val_int(i + 200));
    assert(map_count(map) == 16);

    for (int i = 0; i < 16; i++)
    {
        bool found;
        Value v = map_get(map, val_int(i + 100), &found);
        assert(found && v.as.integer == i + 200);
    }
    PASS();
}

static void test_map_clear_reuse()
{
    printf("\n--- Map: clear + reuse ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    for (int i = 0; i < 50; i++)
        map_set(&gc, map, val_int(i), val_int(i));

    TEST("clear resets everything");
    map_clear(&gc, map);
    assert(map_count(map) == 0);
    assert(map->buckets == nullptr);
    assert(map->nodes == nullptr);
    assert(map->capacity == 0);
    assert(map->bucket_count == 0);
    PASS();

    TEST("reuse after clear");
    for (int i = 0; i < 20; i++)
        map_set(&gc, map, val_int(i * 10), val_int(i * 10));
    assert(map_count(map) == 20);
    for (int i = 0; i < 20; i++)
    {
        bool found;
        Value v = map_get(map, val_int(i * 10), &found);
        assert(found && v.as.integer == i * 10);
    }
    PASS();
}

static void test_map_mixed_key_types()
{
    printf("\n--- Map: mixed key types ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("int, float, bool, nil as keys");
    map_set(&gc, map, val_int(1), val_int(10));
    map_set(&gc, map, val_float(1.5), val_int(20));
    map_set(&gc, map, val_bool(true), val_int(30));
    map_set(&gc, map, val_nil(), val_int(40));
    assert(map_count(map) == 4);
    PASS();

    TEST("retrieve each key type");
    bool found;
    Value v;
    v = map_get(map, val_int(1), &found);
    assert(found && v.as.integer == 10);
    v = map_get(map, val_float(1.5), &found);
    assert(found && v.as.integer == 20);
    v = map_get(map, val_bool(true), &found);
    assert(found && v.as.integer == 30);
    v = map_get(map, val_nil(), &found);
    assert(found && v.as.integer == 40);
    PASS();

    TEST("int(1) != float(1.0) as key");
    v = map_get(map, val_float(1.0), &found);
    assert(!found); /* different type, different key */
    PASS();

    TEST("bool(false) != nil as key");
    map_set(&gc, map, val_bool(false), val_int(50));
    assert(map_count(map) == 5);
    v = map_get(map, val_bool(false), &found);
    assert(found && v.as.integer == 50);
    PASS();
}

static void test_map_keys_values_ordering()
{
    printf("\n--- Map: keys/values collection ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    for (int i = 0; i < 10; i++)
        map_set(&gc, map, val_int(i), val_int(i * 100));

    TEST("map_keys returns all keys");
    ObjArray *keys = new_array(&gc);
    map_keys(&gc, map, keys);
    assert(arr_count(keys) == 10);
    /* All keys should be present (order not guaranteed) */
    for (int i = 0; i < 10; i++)
    {
        assert(array_contains(keys, val_int(i)));
    }
    PASS();

    TEST("map_values returns all values");
    ObjArray *vals = new_array(&gc);
    map_values(&gc, map, vals);
    assert(arr_count(vals) == 10);
    for (int i = 0; i < 10; i++)
    {
        assert(array_contains(vals, val_int(i * 100)));
    }
    PASS();
}

/* =========================================================
** SET EDGE CASES
** ========================================================= */

static void test_set_empty_ops()
{
    printf("\n--- Set: empty operations ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("empty set count == 0");
    assert(set_count(set) == 0);
    PASS();

    TEST("contains on empty set");
    assert(!set_contains(set, val_int(0)));
    PASS();

    TEST("remove from empty set");
    assert(!set_remove(set, val_int(0)));
    PASS();

    TEST("clear on empty set is safe");
    set_clear(&gc, set);
    assert(set_count(set) == 0);
    PASS();
}

static void test_set_single_element()
{
    printf("\n--- Set: single element ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("add single element");
    bool added = set_add(&gc, set, val_int(42));
    assert(added);
    assert(set_count(set) == 1);
    PASS();

    TEST("contains single element");
    assert(set_contains(set, val_int(42)));
    PASS();

    TEST("duplicate add returns false");
    added = set_add(&gc, set, val_int(42));
    assert(!added);
    assert(set_count(set) == 1);
    PASS();

    TEST("remove single element");
    assert(set_remove(set, val_int(42)));
    assert(set_count(set) == 0);
    PASS();

    TEST("double remove returns false");
    assert(!set_remove(set, val_int(42)));
    PASS();

    TEST("contains after remove");
    assert(!set_contains(set, val_int(42)));
    PASS();
}

static void test_set_duplicates()
{
    printf("\n--- Set: duplicate handling ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("add same key 100 times");
    for (int i = 0; i < 100; i++)
        set_add(&gc, set, val_int(7));
    assert(set_count(set) == 1);
    PASS();

    TEST("add 10 unique keys");
    for (int i = 0; i < 10; i++)
        set_add(&gc, set, val_int(i));
    /* key 7 already existed */
    assert(set_count(set) == 10);
    PASS();
}

static void test_set_grow_trigger()
{
    printf("\n--- Set: growth triggers ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("first add allocates 16 buckets");
    set_add(&gc, set, val_int(0));
    assert(set->bucket_count == 16);
    PASS();

    TEST("insert 100 entries (multiple growths)");
    for (int i = 1; i < 100; i++)
        set_add(&gc, set, val_int(i));
    assert(set_count(set) == 100);
    for (int i = 0; i < 100; i++)
        assert(set_contains(set, val_int(i)));
    PASS();
}

static void test_set_delete_reinsert()
{
    printf("\n--- Set: delete + re-insert ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    for (int i = 0; i < 20; i++)
        set_add(&gc, set, val_int(i));

    TEST("remove all, then re-add");
    for (int i = 0; i < 20; i++)
        set_remove(set, val_int(i));
    assert(set_count(set) == 0);

    for (int i = 0; i < 20; i++)
        set_add(&gc, set, val_int(i + 100));
    assert(set_count(set) == 20);
    for (int i = 0; i < 20; i++)
        assert(set_contains(set, val_int(i + 100)));
    PASS();

    TEST("interleaved add/remove");
    ObjSet *set2 = new_set(&gc);
    for (int i = 0; i < 50; i++)
    {
        set_add(&gc, set2, val_int(i));
        if (i % 3 == 0)
            set_remove(set2, val_int(i));
    }
    /* Count: 50 added - 17 removed (0,3,6,...,48) */
    int expected = 50 - 17;
    assert(set_count(set2) == expected);
    /* Verify removed ones are gone */
    for (int i = 0; i < 50; i += 3)
        assert(!set_contains(set2, val_int(i)));
    /* Verify others are present */
    for (int i = 0; i < 50; i++)
        if (i % 3 != 0)
            assert(set_contains(set2, val_int(i)));
    PASS();
}

static void test_set_clear_reuse()
{
    printf("\n--- Set: clear + reuse ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    for (int i = 0; i < 30; i++)
        set_add(&gc, set, val_int(i));

    TEST("clear resets everything");
    set_clear(&gc, set);
    assert(set_count(set) == 0);
    assert(set->buckets == nullptr);
    assert(set->nodes == nullptr);
    PASS();

    TEST("reuse after clear");
    for (int i = 0; i < 15; i++)
        set_add(&gc, set, val_int(i * 5));
    assert(set_count(set) == 15);
    for (int i = 0; i < 15; i++)
        assert(set_contains(set, val_int(i * 5)));
    PASS();
}

static void test_set_mixed_types()
{
    printf("\n--- Set: mixed value types ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("add int, float, bool, nil");
    set_add(&gc, set, val_int(1));
    set_add(&gc, set, val_float(1.0));
    set_add(&gc, set, val_bool(true));
    set_add(&gc, set, val_nil());
    assert(set_count(set) == 4); /* all distinct types */
    PASS();

    TEST("contains each type");
    assert(set_contains(set, val_int(1)));
    assert(set_contains(set, val_float(1.0)));
    assert(set_contains(set, val_bool(true)));
    assert(set_contains(set, val_nil()));
    PASS();

    TEST("int(1) and float(1.0) are different set members");
    assert(set_count(set) == 4);
    PASS();
}

/* =========================================================
** HASH FUNCTION EDGE CASES
** ========================================================= */

static void test_hash_sentinel_safety()
{
    printf("\n--- Hash: sentinel safety ---\n");
    GC gc;
    gc_init(&gc);

    /* Try to find a key that would hash to 0xFFFFFFFF */
    /* The hash function adds 1 when result == sentinel, so no key should break */
    TEST("map survives keys with extreme hash values");
    {
        ObjMap *map = new_map(&gc);
        /* INT_MIN and INT_MAX may produce sentinel-adjacent hashes */
        map_set(&gc, map, val_int(0), val_int(0));
        map_set(&gc, map, val_int(-1), val_int(-1));
        map_set(&gc, map, val_int(0x7FFFFFFF), val_int(1));
        map_set(&gc, map, val_int((int32_t)0x80000000), val_int(2));
        assert(map_count(map) == 4);
        bool found;
        int keys[] = {0, -1, 0x7FFFFFFF, (int32_t)0x80000000};
        for (int i = 0; i < 4; i++)
        {
            map_get(map, val_int(keys[i]), &found);
            assert(found);
        }
    }
    PASS();

    TEST("set survives extreme hash keys");
    {
        ObjSet *set = new_set(&gc);
        set_add(&gc, set, val_int(0));
        set_add(&gc, set, val_int(-1));
        set_add(&gc, set, val_int(0x7FFFFFFF));
        set_add(&gc, set, val_int((int32_t)0x80000000));
        assert(set_count(set) == 4);
        assert(set_contains(set, val_int(0)));
        assert(set_contains(set, val_int(-1)));
        assert(set_contains(set, val_int(0x7FFFFFFF)));
        assert(set_contains(set, val_int((int32_t)0x80000000)));
    }
    PASS();

    TEST("nil and bool hash correctly in map");
    {
        ObjMap *map = new_map(&gc);
        map_set(&gc, map, val_nil(), val_int(1));
        map_set(&gc, map, val_bool(true), val_int(2));
        map_set(&gc, map, val_bool(false), val_int(3));
        assert(map_count(map) == 3);
        bool found;
        Value v = map_get(map, val_nil(), &found);
        assert(found && v.as.integer == 1);
        v = map_get(map, val_bool(true), &found);
        assert(found && v.as.integer == 2);
        v = map_get(map, val_bool(false), &found);
        assert(found && v.as.integer == 3);
    }
    PASS();
}

/* =========================================================
** STRESS: many operations in sequence to catch memory issues
** ========================================================= */

static void test_array_stress_sequence()
{
    printf("\n--- Array: stress sequence ---\n");
    GC gc;
    gc_init(&gc);
    ObjArray *arr = new_array(&gc);

    TEST("push 1000, pop 500, push 500, verify");
    for (int i = 0; i < 1000; i++)
        array_push(&gc, arr, val_int(i));
    for (int i = 0; i < 500; i++)
        array_pop(arr);
    assert(arr_count(arr) == 500);
    for (int i = 0; i < 500; i++)
        array_push(&gc, arr, val_int(1000 + i));
    assert(arr_count(arr) == 1000);
    /* First 500 should be 0..499 */
    for (int i = 0; i < 500; i++)
        assert(arr->data[i].as.integer == i);
    /* Last 500 should be 1000..1499 */
    for (int i = 0; i < 500; i++)
        assert(arr->data[500 + i].as.integer == 1000 + i);
    PASS();
}

static void test_map_stress_sequence()
{
    printf("\n--- Map: stress sequence ---\n");
    GC gc;
    gc_init(&gc);
    ObjMap *map = new_map(&gc);

    TEST("insert 500, delete odd keys, verify even keys");
    for (int i = 0; i < 500; i++)
        map_set(&gc, map, val_int(i), val_int(i * 2));
    for (int i = 1; i < 500; i += 2)
        map_delete(map, val_int(i));
    assert(map_count(map) == 250);
    for (int i = 0; i < 500; i += 2)
    {
        bool found;
        Value v = map_get(map, val_int(i), &found);
        assert(found && v.as.integer == i * 2);
    }
    for (int i = 1; i < 500; i += 2)
    {
        bool found;
        map_get(map, val_int(i), &found);
        assert(!found);
    }
    PASS();

    TEST("re-insert deleted keys with new values");
    for (int i = 1; i < 500; i += 2)
        map_set(&gc, map, val_int(i), val_int(i * 99));
    assert(map_count(map) == 500);
    for (int i = 1; i < 500; i += 2)
    {
        bool found;
        Value v = map_get(map, val_int(i), &found);
        assert(found && v.as.integer == i * 99);
    }
    PASS();
}

static void test_set_stress_sequence()
{
    printf("\n--- Set: stress sequence ---\n");
    GC gc;
    gc_init(&gc);
    ObjSet *set = new_set(&gc);

    TEST("add 500, remove first 250, verify");
    for (int i = 0; i < 500; i++)
        set_add(&gc, set, val_int(i));
    for (int i = 0; i < 250; i++)
        set_remove(set, val_int(i));
    assert(set_count(set) == 250);
    for (int i = 0; i < 250; i++)
        assert(!set_contains(set, val_int(i)));
    for (int i = 250; i < 500; i++)
        assert(set_contains(set, val_int(i)));
    PASS();
}

/* =========================================================
** MAIN
** ========================================================= */

int main()
{
    printf("=== zen collections EDGE CASE tests ===\n");

    /* Array tests */
    test_array_empty_ops();
    test_array_single_element();
    test_array_boundary_insert_remove();
    test_array_grow_trigger();
    test_array_find_int_edge_cases();
    test_array_push_n_edge_cases();
    test_array_append_edge_cases();
    test_array_reverse_edge_cases();
    test_array_sort_edge_cases();
    test_array_copy_edge_cases();
    test_array_clear_reuse();
    test_array_mixed_types();
    test_array_stress_sequence();

    /* Map tests */
    test_map_empty_ops();
    test_map_single_entry();
    test_map_chain_operations();
    test_map_grow_trigger();
    test_map_delete_reinsert();
    test_map_clear_reuse();
    test_map_mixed_key_types();
    test_map_keys_values_ordering();
    test_map_stress_sequence();

    /* Set tests */
    test_set_empty_ops();
    test_set_single_element();
    test_set_duplicates();
    test_set_grow_trigger();
    test_set_delete_reinsert();
    test_set_clear_reuse();
    test_set_mixed_types();
    test_set_stress_sequence();

    /* Hash edge cases */
    test_hash_sentinel_safety();

    printf("\n=== %d / %d TESTS PASSED ===\n", tests_passed, tests_total);
    if (tests_passed == tests_total)
    {
        printf("ALL EDGE CASE TESTS OK!\n");
        return 0;
    }
    else
    {
        printf("FAILURES DETECTED!\n");
        return 1;
    }
}
