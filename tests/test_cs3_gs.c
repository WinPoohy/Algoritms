#include "sorts.h"
#include "sorts_instrumented.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_int(const void *pa, const void *pb) {
    int a = *(const int *)pa, b = *(const int *)pb;
    return (a > b) - (a < b);
}
static uint32_t rng = 0x9e3779b9u;
static uint32_t next_u32(void) {
    uint32_t x = rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng = x;
}
static int check_one(const char *name, int *src, int n) {
    int *expected = malloc((size_t)n * sizeof(*expected));
    int *work = malloc((size_t)n * sizeof(*work));
    int *inst = malloc((size_t)n * sizeof(*inst));
    if (!expected || !work || !inst) { free(expected); free(work); free(inst); return 0; }
    memcpy(expected, src, (size_t)n * sizeof(*src));
    qsort(expected, (size_t)n, sizeof(*expected), cmp_int);

    memcpy(work, src, (size_t)n * sizeof(*src));
    cs3_gs_sort_c(work, 0, n - 1);
    if (memcmp(work, expected, (size_t)n * sizeof(*work)) != 0) {
        fprintf(stderr, "FAIL: C hybrid on %s (n=%d)\n", name, n);
        free(expected); free(work); free(inst); return 0;
    }

    memcpy(inst, src, (size_t)n * sizeof(*src));
    g_cmp_instr = 0; g_mov_instr = 0;
    cs3_gs_sort_c_instr(inst, 0, n - 1);
    if (memcmp(inst, expected, (size_t)n * sizeof(*inst)) != 0) {
        fprintf(stderr, "FAIL: instrumented hybrid on %s (n=%d)\n", name, n);
        free(expected); free(work); free(inst); return 0;
    }
    if (n > 1 && g_cmp_instr == 0) {
        fprintf(stderr, "FAIL: zero comparison count on %s\n", name);
        free(expected); free(work); free(inst); return 0;
    }
    printf("PASS %-24s n=%-8d comparisons=%" PRIu64 " moves=%" PRIu64 "\n",
           name, n, g_cmp_instr, g_mov_instr);
    free(expected); free(work); free(inst); return 1;
}
static int run_pattern(const char *name, int n, int pattern) {
    int *a = malloc((size_t)n * sizeof(*a));
    if (!a) return 0;
    rng = 0x51ed270bu;
    for (int i = 0; i < n; ++i) {
        switch (pattern) {
            case 0: a[i] = (int)(next_u32() & 0x7fffffffU); break;
            case 1: a[i] = i; break;
            case 2: a[i] = n - i; break;
            case 3: a[i] = (int)(next_u32() % 4u); break;
            case 4: a[i] = (i % 100 < 95) ? 777777 : (int)(next_u32() & 0x7fffffffU); break;
            case 5: a[i] = (i % 2) ? INT_MIN : INT_MAX; break;
            case 6: a[i] = INT_MAX - i; break;
            case 7: a[i] = (int)((int64_t)i * 7919 - (int64_t)n * 4000); break;
            default: a[i] = 0; break;
        }
    }
    int ok = check_one(name, a, n); free(a); return ok;
}
int main(void) {
    const int sizes[] = {0, 1, 2, 23, 24, 255, 256, 257, 1024, 8191, 65536};
    int ok = 1;
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        int n = sizes[i];
        if (n == 0) continue;
        for (int p = 0; p < 8; ++p) {
            char label[64]; snprintf(label, sizeof(label), "pattern-%d", p);
            if (!run_pattern(label, n, p)) ok = 0;
        }
    }
    /* Empty-range and fallback-contract smoke tests. */
    int one = 42;
    cs3_gs_sort_c(&one, 0, 0);
    cs3_gs_sort_c_instr(&one, 0, 0);
    if (!ok) return 1;
    puts("All CS3-GS C and instrumented correctness tests passed.");
    return 0;
}
