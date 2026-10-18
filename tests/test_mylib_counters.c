#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t g_cmp;
extern uint64_t g_mov_mylib;
extern void get_sequence(uint64_t, double, uint64_t **, uint64_t *);
extern void cs_sort(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_sort(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs4_sort(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_opt_sort(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs4_opt_sort(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_opt_sort_impr(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern uint64_t qs3_sort(uint64_t *, int64_t);
extern uint64_t yar_sort(uint64_t *, int64_t);
extern void heapsort(uint64_t *, int64_t);
extern void radix_sort(uint64_t *, int64_t);
extern void cs_sort_par_orig(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs_sort_par_fused(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern int g_maxt;
extern int64_t g_minlen;

typedef void (*SeqSort)(uint64_t *, uint64_t *, uint64_t, uint64_t);
typedef uint64_t (*ReturnSort)(uint64_t *, int64_t);
typedef void (*VoidSort)(uint64_t *, int64_t);

typedef struct {
    const char *name;
    SeqSort sequence_sort;
    ReturnSort return_sort;
    VoidSort void_sort;
    int radix;
} TestAlgorithm;

static int cmp_u64(const void *pa, const void *pb) {
    const uint64_t a = *(const uint64_t *)pa;
    const uint64_t b = *(const uint64_t *)pb;
    return (a > b) - (a < b);
}

static int test_one(const TestAlgorithm *alg, const uint64_t *source,
                    const uint64_t *expected, size_t n,
                    uint64_t *sequence, uint64_t sequence_len) {
    uint64_t *work = malloc(n * sizeof(*work));
    if (!work) return 0;
    memcpy(work, source, n * sizeof(*work));
    __atomic_store_n(&g_cmp, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mov_mylib, 0, __ATOMIC_RELAXED);
    if (alg->sequence_sort)
        alg->sequence_sort(work, sequence, (uint64_t)n, sequence_len);
    else if (alg->return_sort)
        (void)alg->return_sort(work, (int64_t)n);
    else
        alg->void_sort(work, (int64_t)n);
    const uint64_t cmp = (uint64_t)__atomic_load_n(&g_cmp, __ATOMIC_RELAXED);
    const uint64_t mov = __atomic_load_n(&g_mov_mylib, __ATOMIC_RELAXED);
    const int sorted_ok = memcmp(work, expected, n * sizeof(*work)) == 0;
    const int counter_ok = (alg->radix ? cmp == 0 : cmp > 0) && mov > 0;
    printf("%s: sorted=%s cmp=%llu moves=%llu counters=%s\n",
           alg->name, sorted_ok ? "PASS" : "FAIL",
           (unsigned long long)cmp, (unsigned long long)mov,
           counter_ok ? "PASS" : "FAIL");
    free(work);
    return sorted_ok && counter_ok;
}

int main(void) {
    enum { N = 2048 };
    uint64_t *source = malloc(N * sizeof(*source));
    uint64_t *expected = malloc(N * sizeof(*expected));
    uint64_t *sequence = NULL;
    uint64_t sequence_len = 0;
    if (!source || !expected) {
        free(source); free(expected);
        return 2;
    }
    for (size_t i = 0; i < N; ++i) {
        /* Deterministic high 64-bit keys, plus duplicate keys and edge values. */
        source[i] = (((uint64_t)(i * UINT64_C(2654435761)) << 32) |
                     (uint64_t)((i * 97u) % 65521u));
        if ((i % 37u) == 0) source[i] = UINT64_MAX;
        if ((i % 41u) == 0) source[i] = 0;
        if ((i % 19u) == 0) source[i] = UINT64_C(1) << 63;
    }
    memcpy(expected, source, N * sizeof(*expected));
    qsort(expected, N, sizeof(*expected), cmp_u64);
    uint64_t maxv = 0;
    for (size_t i = 0; i < N; ++i) if (source[i] > maxv) maxv = source[i];
    get_sequence(maxv ? maxv : 1, 2.0, &sequence, &sequence_len);
    if (!sequence || sequence_len == 0) {
        fprintf(stderr, "failed to create generating sequence\n");
        free(source); free(expected); free(sequence);
        return 2;
    }

    g_maxt = 4;
    g_minlen = 512;
    const TestAlgorithm algorithms[] = {
        {"mylib.cs_sort", cs_sort, NULL, NULL, 0},
        {"mylib.cs2_sort", cs2_sort, NULL, NULL, 0},
        {"mylib.cs4_sort", cs4_sort, NULL, NULL, 0},
        {"mylib.cs2_opt_sort", cs2_opt_sort, NULL, NULL, 0},
        {"mylib.cs4_opt_sort", cs4_opt_sort, NULL, NULL, 0},
        {"mylib.cs2_opt_sort_impr", cs2_opt_sort_impr, NULL, NULL, 0},
        {"mylib.qs3_sort", NULL, qs3_sort, NULL, 0},
        {"mylib.yar_sort", NULL, yar_sort, NULL, 0},
        {"mylib.heapsort", NULL, NULL, heapsort, 0},
        {"mylib.radix_sort", NULL, NULL, radix_sort, 1},
        {"mylib.cs_sort_par_orig", cs_sort_par_orig, NULL, NULL, 0},
        {"mylib.cs_sort_par_fused", cs_sort_par_fused, NULL, NULL, 0}
    };
    int ok = 1;
    for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); ++i)
        ok &= test_one(&algorithms[i], source, expected, N, sequence, sequence_len);

    free(source); free(expected); free(sequence);
    return ok ? 0 : 1;
}
