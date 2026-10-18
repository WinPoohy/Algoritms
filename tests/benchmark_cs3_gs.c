#define _POSIX_C_SOURCE 200809L
#include "sorts.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t rng_state = 0xA341316Cu;
static uint32_t next_u32(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

static int cmp_int(const void *pa, const void *pb) {
    const int a = *(const int *)pa;
    const int b = *(const int *)pb;
    return (a > b) - (a < b);
}

typedef void (*sort_fn)(int *, int);
static void run_qsort(int *a, int n) { qsort(a, (size_t)n, sizeof(*a), cmp_int); }
static void run_cs3(int *a, int n) { cs_sort_c(a, 0, n - 1); }
static void run_cs3gs(int *a, int n) { cs3_gs_sort_c(a, 0, n - 1); }

typedef struct {
    const char *name;
    sort_fn fn;
    uint64_t samples[101];
    uint64_t median;
    int valid;
} BenchAlgorithm;

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(2);
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t median_u64(uint64_t *v, int n) {
    for (int i = 1; i < n; ++i) {
        uint64_t x = v[i];
        int j = i;
        while (j > 0 && v[j - 1] > x) { v[j] = v[j - 1]; --j; }
        v[j] = x;
    }
    return v[n / 2];
}

static int generate_case(const char *name, int *a, int n) {
    rng_state = 0xA341316Cu;
    if (strcmp(name, "random_nonnegative_int") == 0) {
        for (int i = 0; i < n; ++i) a[i] = (int)(next_u32() & 0x7fffffffU);
    } else if (strcmp(name, "sorted_ascending") == 0) {
        for (int i = 0; i < n; ++i) a[i] = i;
    } else if (strcmp(name, "sorted_reverse") == 0) {
        for (int i = 0; i < n; ++i) a[i] = n - i;
    } else if (strcmp(name, "repeated_4_keys") == 0) {
        for (int i = 0; i < n; ++i) a[i] = (int)(next_u32() & 3U);
    } else if (strcmp(name, "skewed_with_outliers") == 0) {
        for (int i = 0; i < n; ++i)
            a[i] = (i % 100 < 95) ? 777777 : (int)(next_u32() & 0x7fffffffU);
    } else if (strcmp(name, "random_signed_int") == 0) {
        for (int i = 0; i < n; ++i) a[i] = (int)next_u32();
    } else {
        return 0;
    }
    return 1;
}

static int run_case(FILE *out, const char *case_name, int n, int repeats,
                    int *source, int *work, int *expected) {
    if (!generate_case(case_name, source, n)) return 0;
    memcpy(expected, source, (size_t)n * sizeof(*source));
    run_qsort(expected, n);

    BenchAlgorithm algs[] = {
        {"libc_qsort_int", run_qsort, {0}, 0, 1},
        {"CS3_ThreePivots_C", run_cs3, {0}, 0, 1},
        {"CS3_GS_ThreePivots_C", run_cs3gs, {0}, 0, 1}
    };
    const size_t count = sizeof(algs) / sizeof(algs[0]);

    for (size_t ai = 0; ai < count; ++ai) {
        for (int r = 0; r < repeats; ++r) {
            memcpy(work, source, (size_t)n * sizeof(*source));
            const uint64_t start = now_ns();
            algs[ai].fn(work, n);
            const uint64_t stop = now_ns();
            algs[ai].samples[r] = stop - start;
            if (memcmp(work, expected, (size_t)n * sizeof(*work)) != 0)
                algs[ai].valid = 0;
        }
        algs[ai].median = median_u64(algs[ai].samples, repeats);
    }

    const uint64_t qsort_ns = algs[0].median;
    for (size_t ai = 0; ai < count; ++ai) {
        const double speedup = algs[ai].median ? (double)qsort_ns / (double)algs[ai].median : 0.0;
        fprintf(out, "%s,%s,%d,%d,%" PRIu64 ",%.6f,%s\n",
                case_name, algs[ai].name, n, repeats, algs[ai].median,
                speedup, algs[ai].valid ? "PASS" : "FAIL");
        printf("%-23s %-24s N=%-8d median=%9.3f ms speedup=%5.2fx %s\n",
               case_name, algs[ai].name, n, (double)algs[ai].median / 1e6,
               speedup, algs[ai].valid ? "PASS" : "FAIL");
        if (!algs[ai].valid) return 0;
    }
    return 1;
}

static int parse_int(const char *s, int min, int max, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v < min || v > max) return 0;
    *out = (int)v;
    return 1;
}

int main(int argc, char **argv) {
    int n = 1000000, repeats = 7;
    const char *path = "results/hybrid_benchmark.csv";
    if (argc > 1 && !parse_int(argv[1], 1, 10000000, &n)) {
        fprintf(stderr, "Usage: %s [N [repeats [output.csv]]]\n", argv[0]); return 2;
    }
    if (argc > 2 && !parse_int(argv[2], 1, 101, &repeats)) {
        fprintf(stderr, "repeats must be 1..101\n"); return 2;
    }
    if (argc > 3) path = argv[3];
    int *source = malloc((size_t)n * sizeof(*source));
    int *work = malloc((size_t)n * sizeof(*work));
    int *expected = malloc((size_t)n * sizeof(*expected));
    if (!source || !work || !expected) {
        fprintf(stderr, "out of memory for N=%d\n", n);
        free(source); free(work); free(expected); return 2;
    }
    FILE *out = fopen(path, "w");
    if (!out) { perror(path); free(source); free(work); free(expected); return 2; }
    fprintf(out, "Case,Algorithm,N,Repeats,Median_ns,Speedup_vs_qsort,Validation\n");
    const char *cases[] = {"random_nonnegative_int", "random_signed_int", "sorted_ascending",
                           "sorted_reverse", "repeated_4_keys", "skewed_with_outliers"};
    int ok = 1;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (!run_case(out, cases[i], n, repeats, source, work, expected)) ok = 0;
    if (fclose(out) != 0) { perror("fclose"); ok = 0; }
    free(source); free(work); free(expected);
    if (!ok) { fprintf(stderr, "At least one case failed validation. See %s\n", path); return 1; }
    printf("Wrote %s\n", path);
    return 0;
}
