#include "sorts.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SORTS_FASM
#define TEST_ASM 1
#else
#define TEST_ASM 0
#endif

static int cmp_u64(const void *pa, const void *pb) {
    const uint64_t a = *(const uint64_t *)pa;
    const uint64_t b = *(const uint64_t *)pb;
    return (a > b) - (a < b);
}

static uint64_t rng_state = UINT64_C(0x6a09e667f3bcc909);
static uint64_t next_u64(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

typedef void (*sort_fn)(uint64_t *, int, int);

static int check_one(const char *name, sort_fn sort, const uint64_t *input, int n) {
    uint64_t *expected = malloc((size_t)n * sizeof(*expected));
    uint64_t *actual = malloc((size_t)n * sizeof(*actual));
    if ((!expected || !actual) && n != 0) {
        free(expected); free(actual);
        fprintf(stderr, "allocation failed in %s, n=%d\n", name, n);
        return 0;
    }
    if (n > 0) {
        memcpy(expected, input, (size_t)n * sizeof(*expected));
        memcpy(actual, input, (size_t)n * sizeof(*actual));
        qsort(expected, (size_t)n, sizeof(*expected), cmp_u64);
        sort(actual, 0, n - 1);
        if (memcmp(expected, actual, (size_t)n * sizeof(*expected)) != 0) {
            fprintf(stderr, "FAIL %s n=%d\n", name, n);
            for (int i = 1; i < n; ++i) {
                if (actual[i - 1] > actual[i]) {
                    fprintf(stderr, " first inversion at %d: %" PRIu64 " > %" PRIu64 "\n",
                            i, actual[i - 1], actual[i]);
                    break;
                }
            }
            free(expected); free(actual);
            return 0;
        }
    } else {
        sort(actual, 0, -1);
    }
    free(expected); free(actual);
    return 1;
}

static int check_all(const char *dataset, const uint64_t *a, int n) {
    if (!check_one("cs_sort_u64_c", cs_sort_u64_c, a, n)) return 0;
    if (!check_one("cs3_gs_sort_u64_c", cs3_gs_sort_u64_c, a, n)) return 0;
#if TEST_ASM
    if (!check_one("cs_sort_u64_fasm", cs_sort_u64_fasm, a, n)) return 0;
    if (!check_one("cs3_gs_sort_u64_fasm", cs3_gs_sort_u64_fasm, a, n)) return 0;
#endif
    printf("PASS %-22s n=%d%s\n", dataset, n, TEST_ASM ? " (C+FASM)" : " (C)");
    return 1;
}

static int run_pattern(const char *name, int n, int pattern) {
    uint64_t *a = malloc((size_t)n * sizeof(*a));
    if (!a && n != 0) return 0;
    for (int i = 0; i < n; ++i) {
        switch (pattern) {
            case 0: /* Full-width random keys, including values above INT64_MAX. */
                a[i] = next_u64();
                break;
            case 1: /* Four duplicated keys at extreme values. */
                switch ((unsigned)i & 3u) {
                    case 0: a[i] = 0; break;
                    case 1: a[i] = UINT64_MAX; break;
                    case 2: a[i] = UINT64_C(1) << 63; break;
                    default: a[i] = (UINT64_C(1) << 53) + 7; break;
                }
                break;
            case 2: /* Already sorted full-domain values. */
                a[i] = (UINT64_MAX / (uint64_t)(n ? n : 1)) * (uint64_t)i;
                break;
            case 3: /* Reverse order. */
                a[i] = (UINT64_MAX / (uint64_t)(n ? n : 1)) * (uint64_t)(n - i);
                break;
            case 4: /* Cluster near 2^63 with a handful of far outliers. */
                a[i] = (UINT64_C(1) << 63) + (uint64_t)(next_u64() % 101u);
                if (i == 0) a[i] = 0;
                if (i == n / 2) a[i] = UINT64_MAX;
                break;
            default:
                a[i] = next_u64();
                break;
        }
    }
    const int ok = check_all(name, a, n);
    free(a);
    return ok;
}

int main(void) {
    const int sizes[] = {0, 1, 2, 23, 24, 25, 255, 256, 257, 1024, 10000};
    const struct { const char *name; int pattern; } patterns[] = {
        {"random_u64", 0}, {"duplicates_extremes", 1},
        {"sorted_u64", 2}, {"reverse_u64", 3}, {"cluster_outliers", 4}
    };
    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); ++si) {
        for (size_t pi = 0; pi < sizeof(patterns) / sizeof(patterns[0]); ++pi) {
            if (!run_pattern(patterns[pi].name, sizes[si], patterns[pi].pattern))
                return 1;
        }
    }
    return 0;
}
