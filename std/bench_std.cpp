/* =========================================================
** bench_std.cpp — Stress test for zen/std classes
** Compares Vector/HashMap/HashSet against std:: equivalents
** Build: g++ -std=c++11 -O2 -I. bench_std.cpp -o bench_std
** ========================================================= */

#include "config.hpp"
#include "vector.hpp"
#include "map.hpp"
#include "set.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

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
    printf("  %-45s %8.3f ms\n", label, dt * 1000.0); \
} while(0)

/* ---- Hashers ---- */
struct IntHasher {
    size_t operator()(int k) const { return (size_t)((uint32_t)k * 2654435761u); }
};
struct IntEq {
    bool operator()(int a, int b) const { return a == b; }
};

/* =========================================================
** Vector tests
** ========================================================= */
static void test_vector(int N) {
    printf("\n--- Vector push/pop (N=%d) ---\n", N);

    {
        Vector<int> v;
        BENCH("zen Vector push", {
            for (int i = 0; i < N; i++)
                v.push(i);
        });
        assert((int)v.size() == N);

        BENCH("zen Vector pop", {
            for (int i = 0; i < N; i++)
                v.pop();
        });
        assert(v.size() == 0);
    }

    {
        std::vector<int> v;
        BENCH("std::vector push_back", {
            for (int i = 0; i < N; i++)
                v.push_back(i);
        });
        BENCH("std::vector pop_back", {
            for (int i = 0; i < N; i++)
                v.pop_back();
        });
    }

    /* Random access */
    printf("\n--- Vector random access (N=%d) ---\n", N);
    {
        Vector<int> v;
        for (int i = 0; i < N; i++) v.push(i);

        volatile int sink = 0;
        srand(42);
        BENCH("zen Vector[] random read", {
            for (int i = 0; i < N; i++)
                sink += v[rand() % N];
        });

        srand(42);
        BENCH("zen Vector[] random write", {
            for (int i = 0; i < N; i++)
                v[rand() % N] = i;
        });
        (void)sink;
    }
}

/* =========================================================
** HashMap tests
** ========================================================= */
static void test_map(int N) {
    printf("\n--- HashMap insert+lookup (N=%d) ---\n", N);

    {
        HashMap<int, int, IntHasher, IntEq> map;

        BENCH("zen HashMap set", {
            for (int i = 0; i < N; i++)
                map.set(i, i * 3);
        });
        assert((int)map.count == N);

        int hits = 0;
        BENCH("zen HashMap get (all hit)", {
            for (int i = 0; i < N; i++) {
                int val;
                if (map.get(i, &val) && val == i * 3) hits++;
            }
        });
        assert(hits == N);

        hits = 0;
        BENCH("zen HashMap contains (all miss)", {
            for (int i = 0; i < N; i++) {
                if (map.contains(N + i)) hits++;
            }
        });
        assert(hits == 0);
    }

    {
        std::unordered_map<int, int> smap;
        BENCH("std::unordered_map insert", {
            for (int i = 0; i < N; i++)
                smap[i] = i * 3;
        });

        int hits = 0;
        BENCH("std::unordered_map find", {
            for (int i = 0; i < N; i++) {
                auto it = smap.find(i);
                if (it != smap.end() && it->second == i * 3) hits++;
            }
        });
        assert(hits == N);
    }

    /* Delete stress */
    printf("\n--- HashMap delete stress (N=%d) ---\n", N);
    {
        HashMap<int, int, IntHasher, IntEq> map;
        for (int i = 0; i < N; i++)
            map.set(i, i);

        BENCH("zen HashMap erase (half)", {
            for (int i = 0; i < N; i += 2)
                map.erase(i);
        });
        assert((int)map.count == N / 2);

        BENCH("zen HashMap set (re-insert tombstones)", {
            for (int i = 0; i < N; i += 2)
                map.set(i, i * 100);
        });
        assert((int)map.count == N);
    }
}

/* =========================================================
** HashSet tests
** ========================================================= */
static void test_set(int N) {
    printf("\n--- HashSet add+contains (N=%d) ---\n", N);

    {
        HashSet<int, IntHasher, IntEq> set;

        BENCH("zen HashSet insert", {
            for (int i = 0; i < N; i++)
                set.insert(i);
        });
        assert((int)set.count == N);

        int hits = 0;
        BENCH("zen HashSet contains (all hit)", {
            for (int i = 0; i < N; i++) {
                if (set.contains(i)) hits++;
            }
        });
        assert(hits == N);

        hits = 0;
        BENCH("zen HashSet contains (all miss)", {
            for (int i = 0; i < N; i++) {
                if (set.contains(N + i)) hits++;
            }
        });
        assert(hits == 0);
    }

    {
        std::unordered_set<int> sset;
        BENCH("std::unordered_set insert", {
            for (int i = 0; i < N; i++)
                sset.insert(i);
        });

        int hits = 0;
        BENCH("std::unordered_set find", {
            for (int i = 0; i < N; i++) {
                if (sset.count(i)) hits++;
            }
        });
        assert(hits == N);
    }

    /* Remove stress */
    printf("\n--- HashSet remove stress (N=%d) ---\n", N);
    {
        HashSet<int, IntHasher, IntEq> set;
        for (int i = 0; i < N; i++)
            set.insert(i);

        BENCH("zen HashSet erase (all)", {
            for (int i = 0; i < N; i++)
                set.erase(i);
        });
        assert(set.count == 0);

        BENCH("zen HashSet insert (after erase, tombstones)", {
            for (int i = 0; i < N; i++)
                set.insert(i);
        });
        assert((int)set.count == N);
    }
}

/* ========================================================= */
int main() {
    printf("=== zen/std classes stress test ===\n");
    printf("(Vector, HashMap, HashSet vs std::)\n");

    int sizes[] = { 100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000 };

    for (int s : sizes) {
        printf("\n========================================\n");
        printf("  N = %d\n", s);
        printf("========================================\n");

        test_vector(s);
        test_map(s);
        test_set(s);
    }

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
