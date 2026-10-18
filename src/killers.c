#include "killers.h"

#include <stdlib.h>
#include <limits.h>
#include <stddef.h>

static uint64_t R = 42;

void killer_set_seed(uint64_t seed) { R = seed; }

static uint64_t krnd(void) {
  R = R * 6364136223846793005ULL + 1442695040888963407ULL;
  return R;
}

static int cmpu64(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}

void killer_pipeorgan(uint64_t *A, int n) {
  if (A == NULL || n <= 0) return;
  const int mid = n / 2;
  for (int i = 0; i < mid; i++) A[i] = (uint64_t)i + 1;
  for (int i = mid; i < n; i++) A[i] = (uint64_t)n - i;
}

void killer_m3killer(uint64_t *A, int n) {
  if (A == NULL || n <= 0) return;
  const int k = n / 2;
  for (int i = 0; i < n; i++)
    A[i] = (i % 2 == 0) ? (uint64_t)(i / 2) + 1 : (uint64_t)k + (uint64_t)(i / 2) + 1;
  for (int i = 1; i < k; i += 2)
    if (i + 1 < n) { uint64_t tmp = A[i]; A[i] = A[i + 1]; A[i + 1] = tmp; }
}

void killer_pipeorganmx(uint64_t *A, int n, uint64_t max_val) {
  if (A == NULL || n <= 0) return;
  if (n == 1) { A[0] = max_val; return; }

  const int mid = n / 2;
  if (mid <= 0) { A[0] = max_val; return; }

  /* Compute floor(k*max_val/mid) exactly. The product can need up to 95
   * bits for int-sized n, so use a 128-bit intermediate rather than double:
   * double would merge distinct uint64_t keys once max_val exceeds 2^53. */
  for (int i = 0; i < mid; ++i) {
    const __uint128_t product = (__uint128_t)(uint64_t)(i + 1) * max_val;
    A[i] = (uint64_t)(product / (uint64_t)mid);
  }
  for (int i = mid; i < n; ++i) {
    const __uint128_t product = (__uint128_t)(uint64_t)(n - i) * max_val;
    const __uint128_t value = product / (uint64_t)mid;
    A[i] = (value > max_val) ? max_val : (uint64_t)value;
  }
}

void killer_unevenseq(uint64_t *A, int n, uint64_t max_val) {
  if (A == NULL || n <= 0) return;
  if (max_val < 4) {
    /* Avoid underflow in mid-delta and handle max_val==0 without modulo 0. */
    for (int i = 0; i < n; ++i)
      A[i] = (max_val == 0) ? 0 : (krnd() % (max_val + 1));
    return;
  }

  const uint64_t mid_val = max_val / 2;
  uint64_t delta = max_val / 10000;
  if (delta == 0) delta = 1;
  if (delta > mid_val) delta = mid_val;
  /* Keep a non-empty upper tail while avoiding addition overflow. */
  if (delta >= max_val - mid_val) delta = max_val - mid_val - 1;
  const uint64_t low_edge = mid_val - delta;
  const uint64_t upper_edge = mid_val + delta;
  const uint64_t middle_span = delta * 2 + 1; /* delta <= max_val/10000 */
  const uint64_t lower_span = low_edge;
  const uint64_t upper_span = max_val - upper_edge;

  for (int i = 0; i < n; ++i) {
    const uint64_t selector = krnd() % 100;
    if (selector < 90) {
      A[i] = low_edge + (krnd() % middle_span);
    } else if ((krnd() & 1u) == 0) {
      A[i] = (lower_span > 0) ? (krnd() % lower_span) : 0;
    } else {
      A[i] = upper_edge + 1 + ((upper_span > 0) ? (krnd() % upper_span) : 0);
    }
  }
}

void killer_unevensorted(uint64_t *A, int n, uint64_t max_val) {
  if (A == NULL || n <= 0) return;
  killer_unevenseq(A, n, max_val);
  qsort(A, n, sizeof(uint64_t), cmpu64);
}

void killer_0to255(uint64_t *A, int n) {
  if (A == NULL || n <= 0) return;
  for (int i = 0; i < n; i++) A[i] = krnd() % 256;
}

void killer_rep(uint64_t *A, int n, uint64_t k) {
  if (A == NULL || n <= 0) return;
  if (k == 0) { for (int i = 0; i < n; ++i) A[i] = 0; return; }
  for (int i = 0; i < n; i++) A[i] = krnd() % k;
}

void killer_rep9010(uint64_t *A, int n, uint64_t max_val) {
  if (A == NULL || n <= 0) return;
  if (max_val == 0) { for (int i = 0; i < n; ++i) A[i] = 0; return; }
  const uint64_t pop = max_val / 2;
  for (int i = 0; i < n; i++)
    A[i] = (krnd() % 100 < 90) ? pop : (krnd() % max_val);
}

void killer_binary(uint64_t *A, int n, uint64_t vlo, uint64_t vhi) {
  if (A == NULL || n <= 0) return;
  for (int i = 0; i < n; i++) A[i] = (krnd() % 2 == 0) ? vlo : vhi;
}

void killer_ternary(uint64_t *A, int n, uint64_t v0, uint64_t v1, uint64_t v2) {
  if (A == NULL || n <= 0) return;
  for (int i = 0; i < n; i++) {
    uint64_t r = krnd() % 3;
    A[i] = (r == 0) ? v0 : (r == 1) ? v1 : v2;
  }
}

void killer_quintary(uint64_t *A, int n, const uint64_t *values) {
  if (A == NULL || values == NULL || n <= 0) return;
  for (int i = 0; i < n; i++) A[i] = values[krnd() % 5];
}

void killer_mag(uint64_t *A, int n, int shift) {
  if (A == NULL || n <= 0) return;
  if (shift < 0 || shift >= 64) {
    for (int i = 0; i < n; ++i) A[i] = 0;
    return;
  }
  const uint64_t v = UINT64_C(1) << (unsigned)shift;
  for (int i = 0; i < n; i++) A[i] = v;
}

void killer_cs_sort(uint64_t *A, int n) {
  if (A == NULL || n <= 0) return;
  const uint64_t cluster_base = 0x80000000ULL;
  for (int i = 0; i < n; ++i)
    A[i] = cluster_base + (krnd() % 100ULL);

  if (n > 1) {
    const int first_half = n / 2;
    const int second_half = n - first_half;
    const int idx_min = (int)(krnd() % (uint64_t)first_half);
    const int idx_max = first_half + (int)(krnd() % (uint64_t)second_half);
    A[idx_min] = 0ULL;
    A[idx_max] = 1ULL << 62;
  }
}
