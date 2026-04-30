/* =========================================================
** test_collections.cpp — Stress tests for zen collections
** Compares ObjArray/ObjMap/ObjSet against std equivalents.
** Build: g++ -std=c++11 -O2 -Isrc src/test_collections.cpp src/memory.cpp -o test_collections
** ========================================================= */

#include "memory.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

using namespace zen;

/* ---- timing ---- */
static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

#define BENCH(label, code) do { \
    double t0 = now_sec(); \
    code; \
    double dt = now_sec() - t0; \
    printf("  %-40s %8.3f ms\n", label, dt * 1000.0); \
} while(0)

/* =========================================================
** Array stress
** ========================================================= */

static void test_array_push_pop(int N) {
    printf("\n--- Array push/pop (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);

    BENCH("zen array_push", {
        for (int i = 0; i < N; i++)
            array_push(&gc, arr, val_int(i));
    });
    assert(arr_count(arr) == N);

    BENCH("zen array_pop", {
        for (int i = 0; i < N; i++)
            array_pop(arr);
    });
    assert(arr_count(arr) == 0);

    /* std::vector comparison */
    std::vector<int> vec;
    BENCH("std::vector push_back", {
        for (int i = 0; i < N; i++)
            vec.push_back(i);
    });
    BENCH("std::vector pop_back", {
        for (int i = 0; i < N; i++)
            vec.pop_back();
    });

    /* Game loop pattern: pre-allocated, clear+refill */
    array_clear(arr);
    array_reserve(&gc, arr, N);
    BENCH("zen array_push (pre-alloc)", {
        array_clear(arr);
        for (int i = 0; i < N; i++)
            array_push(&gc, arr, val_int(i));
    });

    vec.clear();
    vec.reserve(N);
    BENCH("std::vector push (pre-alloc)", {
        vec.clear();
        for (int i = 0; i < N; i++)
            vec.push_back(i);
    });
}

static void test_array_random_access(int N) {
    printf("\n--- Array random access (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);
    for (int i = 0; i < N; i++)
        array_push(&gc, arr, val_int(i));

    /* Random reads */
    volatile int32_t sink = 0;
    srand(42);
    BENCH("zen array_get random", {
        for (int i = 0; i < N; i++) {
            int idx = rand() % N;
            Value v = array_get(arr, idx);
            sink += v.as.integer;
        }
    });
    (void)sink;

    /* Random writes */
    BENCH("zen array_set random", {
        for (int i = 0; i < N; i++) {
            int idx = rand() % N;
            array_set(&gc, arr, idx, val_int(idx * 2));
        }
    });
}

static void test_array_sort(int N) {
    printf("\n--- Array sort (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);
    srand(123);
    for (int i = 0; i < N; i++)
        array_push(&gc, arr, val_int(rand() % (N * 10)));

    /* Copy for std comparison */
    std::vector<int> vec(N);
    for (int i = 0; i < N; i++)
        vec[i] = arr->data[i].as.integer;

    BENCH("zen array_sort_int", {
        array_sort_int(arr);
    });

    /* Verify sorted */
    for (int i = 1; i < N; i++)
        assert(arr->data[i - 1].as.integer <= arr->data[i].as.integer);

    BENCH("std::sort", {
        std::sort(vec.begin(), vec.end());
    });
}

static void test_array_find(int N) {
    printf("\n--- Array find (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);
    for (int i = 0; i < N; i++)
        array_push(&gc, arr, val_int(i));

    int found = 0;
    BENCH("zen array_find (hit)", {
        for (int i = 0; i < 1000; i++) {
            int32_t idx = array_find(arr, val_int(rand() % N));
            if (idx >= 0) found++;
        }
    });
    assert(found == 1000);

    BENCH("zen array_contains (miss)", {
        for (int i = 0; i < 1000; i++) {
            array_contains(arr, val_int(N + i));
        }
    });
}

static void test_array_insert_remove(int N) {
    printf("\n--- Array insert/remove middle (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);
    for (int i = 0; i < N; i++)
        array_push(&gc, arr, val_int(i));

    BENCH("zen array_insert middle x1000", {
        for (int i = 0; i < 1000; i++)
            array_insert(&gc, arr, arr_count(arr) / 2, val_int(-i));
    });
    assert(arr_count(arr) == N + 1000);

    BENCH("zen array_remove middle x1000", {
        for (int i = 0; i < 1000; i++)
            array_remove(arr, arr_count(arr) / 2);
    });
    assert(arr_count(arr) == N);
}

/* =========================================================
** Map stress
** ========================================================= */

static void test_map_insert_lookup(int N) {
    printf("\n--- Map insert+lookup (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjMap* map = new_map(&gc);

    BENCH("zen map_set", {
        for (int i = 0; i < N; i++)
            map_set(&gc, map, val_int(i), val_int(i * 3));
    });
    assert(map_count(map) == N);

    /* Lookup all */
    int hits = 0;
    BENCH("zen map_get (all hit)", {
        for (int i = 0; i < N; i++) {
            bool found;
            Value v = map_get(map, val_int(i), &found);
            if (found && v.as.integer == i * 3) hits++;
        }
    });
    assert(hits == N);

    /* Contains miss */
    BENCH("zen map_contains (all miss)", {
        for (int i = 0; i < N; i++)
            map_contains(map, val_int(N + i));
    });

    /* std comparison */
    std::unordered_map<int, int> smap;
    BENCH("std::unordered_map insert", {
        for (int i = 0; i < N; i++)
            smap[i] = i * 3;
    });

    hits = 0;
    BENCH("std::unordered_map find", {
        for (int i = 0; i < N; i++) {
            auto it = smap.find(i);
            if (it != smap.end() && it->second == i * 3) hits++;
        }
    });
    assert(hits == N);
}

static void test_map_delete_stress(int N) {
    printf("\n--- Map delete stress (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjMap* map = new_map(&gc);
    for (int i = 0; i < N; i++)
        map_set(&gc, map, val_int(i), val_int(i));

    /* Delete half */
    BENCH("zen map_delete (half)", {
        for (int i = 0; i < N; i += 2)
            map_delete(map, val_int(i));
    });
    assert(map_count(map) == N / 2);

    /* Re-insert deleted keys */
    BENCH("zen map_set (re-insert over tombstones)", {
        for (int i = 0; i < N; i += 2)
            map_set(&gc, map, val_int(i), val_int(i * 100));
    });
    assert(map_count(map) == N);

    /* Verify all present */
    for (int i = 0; i < N; i++) {
        bool found;
        map_get(map, val_int(i), &found);
        assert(found);
    }
}

static void test_map_keys_values(int N) {
    printf("\n--- Map keys/values (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjMap* map = new_map(&gc);
    for (int i = 0; i < N; i++)
        map_set(&gc, map, val_int(i), val_int(i * 2));

    ObjArray* keys = new_array(&gc);
    ObjArray* vals = new_array(&gc);

    BENCH("zen map_keys", {
        map_keys(&gc, map, keys);
    });
    BENCH("zen map_values", {
        map_values(&gc, map, vals);
    });

    assert(arr_count(keys) == N);
    assert(arr_count(vals) == N);
}

/* =========================================================
** Set stress
** ========================================================= */

static void test_set_add_contains(int N) {
    printf("\n--- Set add+contains (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjSet* set = new_set(&gc);

    BENCH("zen set_add", {
        for (int i = 0; i < N; i++)
            set_add(&gc, set, val_int(i));
    });
    assert(set_count(set) == N);

    /* Duplicates should not increase count */
    for (int i = 0; i < 100; i++)
        set_add(&gc, set, val_int(i));
    assert(set_count(set) == N);

    int hits = 0;
    BENCH("zen set_contains (all hit)", {
        for (int i = 0; i < N; i++)
            if (set_contains(set, val_int(i))) hits++;
    });
    assert(hits == N);

    BENCH("zen set_contains (all miss)", {
        for (int i = 0; i < N; i++)
            set_contains(set, val_int(N + i));
    });

    /* std comparison */
    std::unordered_set<int> sset;
    BENCH("std::unordered_set insert", {
        for (int i = 0; i < N; i++)
            sset.insert(i);
    });

    hits = 0;
    BENCH("std::unordered_set find", {
        for (int i = 0; i < N; i++)
            if (sset.count(i)) hits++;
    });
    assert(hits == N);
}

static void test_set_remove_stress(int N) {
    printf("\n--- Set remove stress (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjSet* set = new_set(&gc);
    for (int i = 0; i < N; i++)
        set_add(&gc, set, val_int(i));

    BENCH("zen set_remove (all)", {
        for (int i = 0; i < N; i++)
            set_remove(set, val_int(i));
    });
    assert(set_count(set) == 0);

    /* Re-add after clear */
    BENCH("zen set_add (after removes, tombstones)", {
        for (int i = 0; i < N; i++)
            set_add(&gc, set, val_int(i));
    });
    assert(set_count(set) == N);
}

/* =========================================================
** Mixed stress: interleaved ops, checks correctness
** ========================================================= */

static void test_mixed_stress(int N) {
    printf("\n--- Mixed interleaved stress (N=%d) ---\n", N);
    GC gc; gc_init(&gc);

    ObjArray* arr = new_array(&gc);
    ObjMap* map   = new_map(&gc);
    ObjSet* set   = new_set(&gc);

    BENCH("interleaved push+set+add", {
        for (int i = 0; i < N; i++) {
            array_push(&gc, arr, val_int(i));
            map_set(&gc, map, val_int(i), val_int(i * 7));
            set_add(&gc, set, val_int(i));
        }
    });

    assert(arr_count(arr) == N);
    assert(map_count(map) == N);
    assert(set_count(set) == N);

    /* Verify consistency */
    int ok = 0;
    for (int i = 0; i < N; i++) {
        bool found;
        Value v = map_get(map, val_int(i), &found);
        if (found && v.as.integer == i * 7
            && set_contains(set, val_int(i))
            && array_get(arr, i).as.integer == i) {
            ok++;
        }
    }
    assert(ok == N);

    /* Mass delete from map and set */
    BENCH("interleaved delete+remove", {
        for (int i = 0; i < N; i += 3) {
            map_delete(map, val_int(i));
            set_remove(set, val_int(i));
        }
    });

    printf("  Correctness: all assertions passed.\n");
}

/* =========================================================
** Main
** ========================================================= */

int main() {
    printf("=== zen collections stress test ===\n");
    printf("(comparing against std:: where applicable)\n");

    /* Small N for correctness, large for perf */
    int sizes[] = { 100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000 };

    for (int s : sizes) {
        printf("\n========================================\n");
        printf("  N = %d\n", s);
        printf("========================================\n");

        test_array_push_pop(s);
        if (s <= 1000000) test_array_random_access(s);
        if (s <= 50000) test_array_insert_remove(s);
        if (s <= 1000000) test_array_sort(s);
        if (s <= 50000) test_array_find(s);
        test_map_insert_lookup(s);
        test_map_delete_stress(s);
        if (s <= 100000) test_map_keys_values(s);
        test_set_add_contains(s);
        test_set_remove_stress(s);
        if (s <= 1000000) test_mixed_stress(s);
    }

    printf("\n=== ALL STRESS TESTS PASSED ===\n");
    return 0;
}
