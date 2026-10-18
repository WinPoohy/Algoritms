#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

/*
 * Generating-sequence utility following Artamonov's CAlgorithm recurrence:
 *   B_i = ceil(A / c), A <- A - B_i, until A == 0.
 * For this CAlgorithm variant we accept 2 <= c <= 3. This utility generates
 * the threshold sequence S; it does NOT generate the array Q to be sorted.
 */
static int parse_positive_ll(const char *s, long long *out) {
    char *end = NULL; errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || end == s || *end != '\0' || v <= 0) return 0;
    *out = v; return 1;
}

/* For c in [2,3], c = significand / 2^51 for its exact binary-double
 * representation. Compute ceil(n/c) as integer arithmetic so LLONG_MAX is safe. */
static long long ceil_div_c_ll(long long n, double c) {
    if (n <= 0) return 0;
    int exponent = 0;
    const double fraction = frexp(c, &exponent);
    if (exponent == 2 && c >= 2.0 && c <= 3.0) {
        const uint64_t significand = (uint64_t)ldexp(fraction, 53);
        const __uint128_t numerator = ((__uint128_t)(uint64_t)n) << 51;
        __uint128_t quotient = numerator / significand;
        if (numerator % significand != 0) ++quotient;
        return (long long)quotient;
    }
    return (long long)ceill((long double)n / (long double)c);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <n> <c in [2,3]>\n", argv[0]);
        fprintf(stderr, "Example: %s 100 2.5\n", argv[0]);
        return 2;
    }
    long long n = 0;
    if (!parse_positive_ll(argv[1], &n)) {
        fprintf(stderr, "n must be a positive signed 64-bit integer.\n");
        return 2;
    }
    char *end = NULL; errno = 0;
    double c = strtod(argv[2], &end);
    if (errno || end == argv[2] || *end != '\0' || !isfinite(c) || c < 2.0 || c > 3.0) {
        fprintf(stderr, "c must be finite and in [2,3] for this sequence family.\n");
        return 2;
    }

    long long a = n;
    long long count = 0;
    printf("Artamonov generating sequence for n=%lld, c=%.6g:\nS = [", n, c);
    while (a > 0) {
        long long b = ceil_div_c_ll(a, c);
        if (b < 1) b = 1;
        if (b > a) b = a;
        printf("%s%lld", count ? ", " : "", b);
        a -= b;
        ++count;
    }
    printf("]\nLength = %lld\n", count);
    return 0;
}
