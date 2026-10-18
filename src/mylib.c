#ifndef ENABLE_FAST_BENCH
#define ENABLE_ATOMIC_COUNTERS 1
#endif
#ifndef MYLIB_NO_MAIN
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

/* ============================================================================
   УСЛОВНАЯ КОМПИЛЯЦИЯ СЧЕТЧИКОВ СРАВНЕНИЙ (ENABLE_COUNTERS)
   ============================================================================ */
extern int64_t g_cmp;

/* Count writes to array slots during the operation-counting pass.
 * ENABLE_FAST_BENCH builds compile these increments away; the counted build
 * uses atomic increments so parallel MyLib workers share one safe counter. */
#ifndef ENABLE_FAST_BENCH
uint64_t g_mov_mylib = 0;
#define INC_MOV_ML() ((void)__atomic_fetch_add(&g_mov_mylib, UINT64_C(1), __ATOMIC_RELAXED))
#else
#define INC_MOV_ML() ((void)0)
#endif
#ifdef ENABLE_FAST_BENCH
    /* В аппаратном бенчмарке отключаем не только инкременты, но и сброс g_cmp. */
    #define INC_CMP() ((void)0)
    #define ADD_CMP(v) ((void)0)
    #define RESET_CMP() ((void)0)
#elif defined(ENABLE_ATOMIC_COUNTERS)
    /* Thread-safe counters for the operation-counting pass of parallel MyLib. */
    #define INC_CMP() ((void)__atomic_fetch_add(&g_cmp, 1, __ATOMIC_RELAXED))
    #define ADD_CMP(v) ((void)__atomic_fetch_add(&g_cmp, (v), __ATOMIC_RELAXED))
    #define RESET_CMP() ((void)__atomic_store_n(&g_cmp, 0, __ATOMIC_RELAXED))
#else
    #define INC_CMP() (g_cmp++)
    #define ADD_CMP(v) (g_cmp += (v))
    #define RESET_CMP() (g_cmp = 0)
#endif

/*Основные функции реализации cs_sort*/

/* Exact ceil(a / c) for the benchmark's c in [2,3]. Any such IEEE-754
 * double has the form significand / 2^51. Using 128-bit integer arithmetic
 * avoids losing low key bits when a is a uint64_t larger than 2^53. */
static uint64_t sequence_ceil_div(uint64_t a, double c) {
  if (a == 0) return 0;
  if (isfinite(c) && c >= 2.0 && c <= 3.0) {
    int exponent = 0;
    const double fraction = frexp(c, &exponent);
    if (exponent == 2) {
      const uint64_t significand = (uint64_t)ldexp(fraction, 53);
      const __uint128_t numerator = ((__uint128_t)a) << 51;
      __uint128_t quotient = numerator / significand;
      if (numerator % significand != 0) ++quotient;
      return (uint64_t)quotient;
    }
  }
  /* Compatibility path for the legacy 1<c<2 recurrence. */
  return (uint64_t)ceill((long double)a / (long double)c);
}

static int sequence_append(uint64_t **buffer, size_t *capacity,
                           size_t count, uint64_t value) {
  if (count < *capacity) {
    (*buffer)[count] = value;
    return 1;
  }
  size_t next = (*capacity == 0) ? 8 : (*capacity * 2);
  if (next < *capacity || next > SIZE_MAX / sizeof(**buffer)) return 0;
  uint64_t *grown = (uint64_t *)realloc(*buffer, next * sizeof(**buffer));
  if (grown == NULL) return 0;
  *buffer = grown;
  *capacity = next;
  (*buffer)[count] = value;
  return 1;
}

/* Generate S without repeated unchecked reallocs. In the primary CAlgorithm
 * family c in [2,3], S satisfies B_i=ceil(A_i/c), A_(i+1)=A_i-B_i.
 * The integer helper computes ceil exactly over the entire uint64_t domain. */
void get_sequence(uint64_t n, double c, uint64_t **ar, uint64_t *size) {
  if (ar == NULL || size == NULL) return;
  *size = 0;
  if (!isfinite(c) || c <= 1.0) return;

  uint64_t *buffer = NULL;
  size_t capacity = 0, count = 0;
  int ok = 1;

  if (n == 0) {
    ok = sequence_append(&buffer, &capacity, count++, 0);
  } else if (c >= 2.0) {
    uint64_t remaining = n;
    while (remaining > 0) {
      const uint64_t b = sequence_ceil_div(remaining, c);
      if (b == 0 || b > remaining ||
          !sequence_append(&buffer, &capacity, count, b)) {
        ok = 0;
        break;
      }
      ++count;
      remaining -= b;
    }
  } else {
    /* Preserve the legacy 1<c<2 sequence definition: begin with ceil(n/c),
     * then repeatedly apply ceil((previous-1)/c), excluding the terminal 0. */
    uint64_t b = sequence_ceil_div(n, c);
    if (b == 0 || b > n || !sequence_append(&buffer, &capacity, count, b)) {
      ok = 0;
    } else {
      ++count;
      while (b > 1) {
        const uint64_t next = sequence_ceil_div(b - 1, c);
        if (next == 0 || next >= b ||
            !sequence_append(&buffer, &capacity, count, next)) {
          ok = 0;
          break;
        }
        ++count;
        b = next;
      }
    }
  }

  if (!ok) {
    free(buffer);
    return; /* allocation/recurrence failure: size remains zero */
  }

  free(*ar);
  *ar = buffer;
  *size = (uint64_t)count;
}

/*Набор основных функций для cs_sort*/

void iter_sort(uint64_t *Q, uint64_t *S,
	       uint64_t number_S,
	       uint64_t len_S,
	       uint64_t a, int64_t x,
	       int64_t y);

int64_t divisor(uint64_t *Q, uint64_t a, int64_t x, int64_t y) {
  int64_t i = x, j = y;
  uint64_t current;
  int pr = 0;

  while (i < j) {
    INC_CMP();
    if (Q[i] >= a) {
      pr = 1;
      while (i < j) {
        INC_CMP();
        if (!(Q[j] >= a)) break;
        j--;
      }
      if (i < j) {
        current = Q[i];
        Q[i] = Q[j]; INC_MOV_ML();
        Q[j] = current; INC_MOV_ML();
      }
    }
    i++;
  }

  if (pr) return j;
  else return -1;
}

void partition4(uint64_t *Q, uint64_t lo, uint64_t mid, uint64_t hi,
		int64_t x, int64_t y,
		int64_t *m0, int64_t *m1, int64_t *m2) {
  int64_t lt = x, b = x, i = x, gt = y;
  uint64_t current;

  while (i <= gt) {
    current = Q[i];
    INC_CMP();
    if (current < mid) {
      INC_CMP();
      if (current < lo) {
	Q[i] = Q[b]; INC_MOV_ML();
	Q[b] = Q[lt]; INC_MOV_ML();
	Q[lt] = current; INC_MOV_ML();
	lt++; b++; i++;
      } else {
	Q[i] = Q[b]; INC_MOV_ML();
	Q[b] = current; INC_MOV_ML();
	b++; i++;
      }
    } else {
      INC_CMP();
      if (current < hi) {
	i++;
      } else {
	Q[i] = Q[gt]; INC_MOV_ML();
	Q[gt] = current; INC_MOV_ML();
	gt--;
      }
    }
  }
  *m0 = lt;
  *m1 = b;
  *m2 = i;
}

void iter_sort2(uint64_t *Q, uint64_t *S,
		uint64_t number_S,
		uint64_t len_S,
		uint64_t a, int64_t x,
		int64_t y) {
  if (x >= y) return;
  if (number_S >= len_S) return;

  if ((number_S + 1 >= len_S) ||
      (S[number_S] <= S[number_S + 1])) {
    iter_sort(Q, S, number_S, len_S, a, x, y);
    return;
  }

  uint64_t s0 = S[number_S];
  uint64_t s1 = S[number_S + 1];
  uint64_t c1 = a + s1;
  uint64_t c2 = a + s0;
  uint64_t c3 = a + s0 + s1;

  int64_t m0, m1, m2;
  partition4(Q, c1, c2, c3, x, y, &m0, &m1, &m2);

  if (m0 > x)
    iter_sort2(Q, S, number_S + 2, len_S, a, x, m0 - 1);
  if (m1 > m0)
    iter_sort2(Q, S, number_S + 2, len_S, a + s1, m0, m1 - 1);
  if (m2 > m1)
    iter_sort2(Q, S, number_S + 2, len_S, a + s0, m1, m2 - 1);
  if (y >= m2)
    iter_sort2(Q, S, number_S + 2, len_S, a + s0 + s1, m2, y);
}

void iter_sort(uint64_t *Q, uint64_t *S,
	       uint64_t number_S,
	       uint64_t len_S,
	       uint64_t a, int64_t x,
	       int64_t y) {
  if (x >= y) return;
  if (number_S == len_S) return;

  int64_t z;
  z = divisor(Q, S[number_S] + a, x, y);

  if (z != -1) {
    iter_sort(Q, S, number_S + 1, len_S, (a + S[number_S]), z, y);
    if (z > 0) {
      iter_sort(Q, S, number_S + 1, len_S, a, x, z - 1);
    }
  }
  else {
    iter_sort(Q, S, number_S + 1, len_S, a, x, y);
  }
}

void cs_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
#ifndef ENABLE_FAST_BENCH
  RESET_CMP();
#endif
  if (len_Q > 0)
    iter_sort2(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

/* Uninstrumented twin for cycle benchmarks. It keeps the base CS-sort
 * recursion and partition logic but avoids g_cmp writes in the hot loops. */
static int64_t divisor_fast(uint64_t *Q, uint64_t a, int64_t x, int64_t y) {
  int64_t i = x, j = y;
  int pr = 0;
  while (i < j) {
    if (Q[i] >= a) {
      pr = 1;
      while (i < j && Q[j] >= a) --j;
      if (i < j) { uint64_t t = Q[i]; Q[i] = Q[j]; INC_MOV_ML(); Q[j] = t; INC_MOV_ML(); }
    }
    ++i;
  }
  return pr ? j : -1;
}

static void partition4_fast(uint64_t *Q, uint64_t lo, uint64_t mid, uint64_t hi,
                            int64_t x, int64_t y,
                            int64_t *m0, int64_t *m1, int64_t *m2) {
  int64_t lt = x, b = x, i = x, gt = y;
  while (i <= gt) {
    uint64_t v = Q[i];
    if (v < mid) {
      if (v < lo) { Q[i] = Q[b]; INC_MOV_ML(); Q[b] = Q[lt]; INC_MOV_ML(); Q[lt] = v; INC_MOV_ML(); ++lt; ++b; ++i; }
      else { Q[i] = Q[b]; INC_MOV_ML(); Q[b] = v; INC_MOV_ML(); ++b; ++i; }
    } else if (v < hi) {
      ++i;
    } else {
      Q[i] = Q[gt]; INC_MOV_ML(); Q[gt] = v; INC_MOV_ML(); --gt;
    }
  }
  *m0 = lt; *m1 = b; *m2 = i;
}

static void iter_sort_fast(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                           uint64_t a, int64_t x, int64_t y);
static void iter_sort2_fast(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                            uint64_t a, int64_t x, int64_t y) {
  if (x >= y || ns >= len_S) return;
  if (ns + 1 >= len_S || S[ns] <= S[ns + 1]) {
    iter_sort_fast(Q, S, ns, len_S, a, x, y);
    return;
  }
  const uint64_t s0 = S[ns], s1 = S[ns + 1];
  int64_t m0, m1, m2;
  partition4_fast(Q, a + s1, a + s0, a + s0 + s1, x, y, &m0, &m1, &m2);
  if (m0 > x) iter_sort2_fast(Q, S, ns + 2, len_S, a, x, m0 - 1);
  if (m1 > m0) iter_sort2_fast(Q, S, ns + 2, len_S, a + s1, m0, m1 - 1);
  if (m2 > m1) iter_sort2_fast(Q, S, ns + 2, len_S, a + s0, m1, m2 - 1);
  if (y >= m2) iter_sort2_fast(Q, S, ns + 2, len_S, a + s0 + s1, m2, y);
}
static void iter_sort_fast(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                           uint64_t a, int64_t x, int64_t y) {
  if (x >= y || ns >= len_S) return;
  const int64_t z = divisor_fast(Q, S[ns] + a, x, y);
  if (z != -1) {
    iter_sort_fast(Q, S, ns + 1, len_S, a + S[ns], z, y);
    if (z > 0) iter_sort_fast(Q, S, ns + 1, len_S, a, x, z - 1);
  } else {
    iter_sort_fast(Q, S, ns + 1, len_S, a, x, y);
  }
}

void cs_sort_fast(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  if (len_Q > 0)
    iter_sort2_fast(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

/*Параллельные версии cs_sort (fork-join).*/
int g_maxt = 0;
int64_t g_minlen = 16384;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_active = 1;

static int g_take(void) {
  int ok = 0;
  pthread_mutex_lock(&g_lock);
  if (g_active < g_maxt) { g_active++; ok = 1; }
  pthread_mutex_unlock(&g_lock);
  return ok;
}

static void g_free_slot(void) {
  pthread_mutex_lock(&g_lock);
  g_active--;
  pthread_mutex_unlock(&g_lock);
}

typedef struct {
  uint64_t *Q, *S;
  uint64_t ns, len_S, a;
  int64_t x, y;
} cs_job_t;

typedef struct { uint64_t a; int64_t x, y; } cs_kid_t;

static void par_iter2(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                      uint64_t a, int64_t x, int64_t y);
static void par_iter(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                     uint64_t a, int64_t x, int64_t y);

/* Optional per-worker hardware-counter hooks supplied by the comparative
 * benchmark. Weak references keep standalone MyLib builds independent from
 * the benchmark executable. They are essential for recursive pthread workers:
 * perf_event inheritance alone does not propagate through nested generations. */
#if defined(__GNUC__) && defined(__linux__)
extern void sortbench_perf_worker_enter(void) __attribute__((weak));
extern void sortbench_perf_worker_exit(void) __attribute__((weak));
#define MYLIB_PERF_WORKER_ENTER() do { if (sortbench_perf_worker_enter) sortbench_perf_worker_enter(); } while (0)
#define MYLIB_PERF_WORKER_EXIT()  do { if (sortbench_perf_worker_exit) sortbench_perf_worker_exit(); } while (0)
#else
#define MYLIB_PERF_WORKER_ENTER() ((void)0)
#define MYLIB_PERF_WORKER_EXIT()  ((void)0)
#endif

static void *cs_job_wrap(void *p) {
  cs_job_t *j = p;
  MYLIB_PERF_WORKER_ENTER();
  par_iter2(j->Q, j->S, j->ns, j->len_S, j->a, j->x, j->y);
  g_free_slot();
  free(j);
  MYLIB_PERF_WORKER_EXIT();
  return NULL;
}

static void par_iter2(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                      uint64_t a, int64_t x, int64_t y) {
  if (x >= y) return;
  if (ns >= len_S) return;

  if ((ns + 1 >= len_S) || (S[ns] <= S[ns + 1])) {
    par_iter(Q, S, ns, len_S, a, x, y);
    return;
  }

  uint64_t s0 = S[ns], s1 = S[ns + 1];
  uint64_t c1 = a + s1, c2 = a + s0, c3 = a + s0 + s1;
  int64_t m0, m1, m2;
  partition4(Q, c1, c2, c3, x, y, &m0, &m1, &m2);

  cs_kid_t kids[4]; int nk = 0;
  if (m0 > x) kids[nk++] = (cs_kid_t){a, x, m0 - 1};
  if (m1 > m0) kids[nk++] = (cs_kid_t){a + s1, m0, m1 - 1};
  if (m2 > m1) kids[nk++] = (cs_kid_t){a + s0, m1, m2 - 1};
  if (y >= m2) kids[nk++] = (cs_kid_t){a + s0 + s1, m2, y};

  uint64_t nsk = ns + 2;
  pthread_t ts[4]; int nspawn = 0;
  cs_kid_t inl[4]; int ni = 0;
  for (int k = 0; k < nk; k++) {
    uint64_t ak = kids[k].a; int64_t xk = kids[k].x, yk = kids[k].y;
    int64_t ln = yk - xk + 1;
    if (ln >= g_minlen && g_take()) {
      cs_job_t *j = malloc(sizeof(cs_job_t));
      j->Q = Q; j->S = S; j->ns = nsk; j->len_S = len_S; j->a = ak;
      j->x = xk; j->y = yk;
      pthread_create(&ts[nspawn], NULL, cs_job_wrap, j);
      nspawn++;
    } else {
      inl[ni++] = (cs_kid_t){ak, xk, yk};
    }
  }
  for (int k = 0; k < ni; k++)
    par_iter2(Q, S, nsk, len_S, inl[k].a, inl[k].x, inl[k].y);
  for (int k = 0; k < nspawn; k++) pthread_join(ts[k], NULL);
}

static void par_iter(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t len_S,
                     uint64_t a, int64_t x, int64_t y) {
  if (x >= y) return;
  if (ns == len_S) return;

  int64_t z = divisor(Q, S[ns] + a, x, y);
  if (z == -1) { par_iter(Q, S, ns + 1, len_S, a, x, y); return; }

  int64_t hx = z, hy = y, lx = x, ly = z - 1;
  uint64_t nsk = ns + 1, ah = a + S[ns], al = a;

  cs_kid_t kids[2]; int nk = 0;
  kids[nk++] = (cs_kid_t){ah, hx, hy};
  if (ly >= lx) kids[nk++] = (cs_kid_t){al, lx, ly};

  pthread_t ts[2]; int nspawn = 0;
  cs_kid_t inl[2]; int ni = 0;
  for (int k = 0; k < nk; k++) {
    uint64_t ak = kids[k].a; int64_t xk = kids[k].x, yk = kids[k].y;
    int64_t ln = yk - xk + 1;
    if (ln >= g_minlen && g_take()) {
      cs_job_t *j = malloc(sizeof(cs_job_t));
      j->Q = Q; j->S = S; j->ns = nsk; j->len_S = len_S; j->a = ak;
      j->x = xk; j->y = yk;
      pthread_create(&ts[nspawn], NULL, cs_job_wrap, j);
      nspawn++;
    } else {
      inl[ni++] = (cs_kid_t){ak, xk, yk};
    }
  }
  for (int k = 0; k < ni; k++)
    par_iter(Q, S, nsk, len_S, inl[k].a, inl[k].x, inl[k].y);
  for (int k = 0; k < nspawn; k++) pthread_join(ts[k], NULL);
}

void cs_sort_par_orig(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  if (g_maxt <= 0) g_maxt = (int)sysconf(_SC_NPROCESSORS_ONLN);
  g_active = 1;
  if (len_Q > 0) par_iter(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

void cs_sort_par_fused(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  if (g_maxt <= 0) g_maxt = (int)sysconf(_SC_NPROCESSORS_ONLN);
  g_active = 1;
  if (len_Q > 0) par_iter2(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

int64_t g_cmp = 0;
int64_t g_ins = 16;

int64_t g_skip = 0;
int64_t g_hom = 0;
int64_t g_pass = 0;
int g_route = 0;

void partition4_v(uint64_t *Q, uint64_t lo, uint64_t mid, uint64_t hi,
		  int64_t x, int64_t y,
		  int64_t *m0, int64_t *m1, int64_t *m2);
int64_t divisor_v(uint64_t *Q, uint64_t a, int64_t x, int64_t y);
void iter_sort_v(uint64_t *Q, uint64_t *S, uint64_t number_S,
		 uint64_t len_S, uint64_t a, int64_t x, int64_t y);
void iter_sort2_v(uint64_t *Q, uint64_t *S, uint64_t number_S,
		  uint64_t len_S, uint64_t a, int64_t x, int64_t y);

void insertion_v(uint64_t *Q, int64_t x, int64_t y) {
  for (int64_t i = x + 1; i <= y; i++) {
    uint64_t key = Q[i];
    int64_t j = i - 1;
    while (j >= x) { INC_CMP(); if (!(Q[j] > key)) break; Q[j + 1] = Q[j]; INC_MOV_ML(); j--; }
    Q[j + 1] = key; INC_MOV_ML();
  }
}

void partition4_v(uint64_t *Q, uint64_t lo, uint64_t mid, uint64_t hi,
		  int64_t x, int64_t y,
		  int64_t *m0, int64_t *m1, int64_t *m2) {
  int64_t lt = x, b = x, i = x, gt = y;
  uint64_t current;
  while (i <= gt) {
    current = Q[i];
    INC_CMP();
    if (current < mid) {
      INC_CMP();
      if (current < lo) {
	uint64_t tmp = Q[i];
	Q[i] = Q[b]; INC_MOV_ML(); Q[b] = Q[lt]; INC_MOV_ML(); Q[lt] = tmp; INC_MOV_ML();
	lt++; b++; i++;
      } else {
	uint64_t tmp = Q[i];
	Q[i] = Q[b]; INC_MOV_ML(); Q[b] = tmp; INC_MOV_ML();
	b++; i++;
      }
    } else {
      INC_CMP();
      if (current < hi) {
	i++;
      } else {
	Q[i] = Q[gt]; INC_MOV_ML(); Q[gt] = current; INC_MOV_ML();
	gt--;
      }
    }
  }
  *m0 = lt;
  *m1 = b;
  *m2 = i;
}

int64_t divisor_v(uint64_t *Q, uint64_t a, int64_t x, int64_t y) {
  int64_t i = x, j = y;
  uint64_t current;
  int pr = 0;
  while (i < j) {
    INC_CMP();
    if (Q[i] >= a) {
      pr = 1;
      while (i < j) { INC_CMP(); if (Q[j] < a) break; j--; }
      if (i < j) { current = Q[i]; Q[i] = Q[j]; INC_MOV_ML(); Q[j] = current; INC_MOV_ML(); }
    }
    i++;
  }
  if (pr) return j;
  return -1;
}

void iter_sort_v(uint64_t *Q, uint64_t *S, uint64_t number_S,
		 uint64_t len_S, uint64_t a, int64_t x, int64_t y) {
  if (y - x < g_ins) {
    if (x < y) insertion_v(Q, x, y);
    return;
  }
  if (number_S == len_S) return;

  int64_t z = divisor_v(Q, S[number_S] + a, x, y);
  if (z != -1) {
    iter_sort_v(Q, S, number_S + 1, len_S, a + S[number_S], z, y);
    if (z > 0)
      iter_sort_v(Q, S, number_S + 1, len_S, a, x, z - 1);
  } else {
    iter_sort_v(Q, S, number_S + 1, len_S, a, x, y);
  }
}

void iter_sort2_v(uint64_t *Q, uint64_t *S, uint64_t number_S,
		  uint64_t len_S, uint64_t a, int64_t x, int64_t y) {
  if (y - x < g_ins) {
    if (x < y) insertion_v(Q, x, y);
    return;
  }
  if (number_S >= len_S) return;

  if ((number_S + 1 >= len_S) || (S[number_S] <= S[number_S + 1])) {
    iter_sort_v(Q, S, number_S, len_S, a, x, y);
    return;
  }

  uint64_t s0 = S[number_S];
  uint64_t s1 = S[number_S + 1];
  uint64_t c1 = a + s1;
  uint64_t c2 = a + s0;
  uint64_t c3 = a + s0 + s1;

  int64_t m0, m1, m2;
  partition4_v(Q, c1, c2, c3, x, y, &m0, &m1, &m2);

  if (m0 > x)
    iter_sort2_v(Q, S, number_S + 2, len_S, a, x, m0 - 1);
  if (m1 > m0)
    iter_sort2_v(Q, S, number_S + 2, len_S, a + s1, m0, m1 - 1);
  if (m2 > m1)
    iter_sort2_v(Q, S, number_S + 2, len_S, a + s0, m1, m2 - 1);
  if (y >= m2)
    iter_sort2_v(Q, S, number_S + 2, len_S, a + s0 + s1, m2, y);
}

void cs4_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  RESET_CMP();
  if (len_Q > 0)
    iter_sort2_v(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

void cs2_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  RESET_CMP();
  if (len_Q > 0)
    iter_sort_v(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1);
}

int64_t gSkip = 0;
int64_t gSkipU = 0;

static uint64_t bmin(uint64_t v, uint64_t lo) { return v > lo ? v : lo; }
static uint64_t bmax(uint64_t v, uint64_t hi) {
  if (hi == 0) return 0;
  uint64_t top = hi - 1;
  return v < top ? v : top;
}

static void extr(uint64_t *Q, int64_t x, int64_t y, uint64_t *mn, uint64_t *mx) {
  uint64_t a = Q[x], b = Q[x];
  for (int64_t i = x + 1; i <= y; i++) {
    INC_CMP();
    if (Q[i] < a) {
      a = Q[i];
    } else {
      INC_CMP();
      if (Q[i] > b) b = Q[i];
    }
  }
  *mn = a; *mx = b;
}

void c_isf_opt(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t lenS,
               uint64_t a, int64_t x, int64_t y,
               uint64_t mn, uint64_t mx) {
  int64_t L = y - x + 1;
  if (L < g_ins) { if (x < y) insertion_v(Q, x, y); return; }
  if (ns >= lenS) return;
  INC_CMP();
  if (mn == mx) { gSkipU += L; return; }
  uint64_t key = S[ns] + a;
  INC_CMP();
  if (key > mx) {
    gSkip += L;
    c_isf_opt(Q, S, ns + 1, lenS, a, x, y, mn, mx);
    return;
  }
  INC_CMP();
  if (key <= mn) {
    gSkip += L;
    c_isf_opt(Q, S, ns + 1, lenS, a + S[ns], x, y, mn, mx);
    return;
  }
  int64_t z = divisor_v(Q, key, x, y);
  if (z == -1) {
    c_isf_opt(Q, S, ns + 1, lenS, a, x, y, mn, mx);
    return;
  }
  uint64_t Rmn, Rmx, Lmn, Lmx;
  extr(Q, z, y, &Rmn, &Rmx);
  c_isf_opt(Q, S, ns + 1, lenS, a + S[ns], z, y, Rmn, Rmx);
  if (z > 0) {
    extr(Q, x, z - 1, &Lmn, &Lmx);
    c_isf_opt(Q, S, ns + 1, lenS, a, x, z - 1, Lmn, Lmx);
  }
}

void c_is2_opt(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t lenS,
               uint64_t a, int64_t x, int64_t y,
               uint64_t mn, uint64_t mx) {
  int64_t L = y - x + 1;
  if (L < g_ins) { if (x < y) insertion_v(Q, x, y); return; }
  if (ns >= lenS) return;
  INC_CMP();
  if (mn == mx) { gSkipU += L; return; }
  if ((ns + 1 >= lenS) || (S[ns] <= S[ns + 1])) {
    c_isf_opt(Q, S, ns, lenS, a, x, y, mn, mx);
    return;
  }
  uint64_t s0 = S[ns], s1 = S[ns + 1];
  uint64_t lo = a + s1, mid = a + s0, hi = a + s0 + s1;
  int kind = -1;
  INC_CMP();
  if (mx < lo) {
    kind = 0;
  } else {
    INC_CMP();
    if (mn >= lo) {
      INC_CMP();
      if (mx < mid) kind = 1;
    }
    if (kind < 0) {
      INC_CMP();
      if (mn >= mid) {
        INC_CMP();
        if (mx < hi) kind = 2;
      }
    }
    if (kind < 0) {
      INC_CMP();
      if (mn >= hi) kind = 3;
    }
  }
  if (kind >= 0) {
    gSkip += L;
    switch (kind) {
      case 0: c_is2_opt(Q, S, ns + 2, lenS, a, x, y, mn, bmax(mx, lo)); break;
      case 1: c_is2_opt(Q, S, ns + 2, lenS, a + s1, x, y, bmin(mn, lo), bmax(mx, mid)); break;
      case 2: c_is2_opt(Q, S, ns + 2, lenS, a + s0, x, y, bmin(mn, mid), bmax(mx, hi)); break;
      case 3: c_is2_opt(Q, S, ns + 2, lenS, a + s0 + s1, x, y, bmin(mn, hi), mx); break;
    }
    return;
  }
  int64_t m0, m1, m2;
  partition4_v(Q, lo, mid, hi, x, y, &m0, &m1, &m2);
  if (m0 > x)  { uint64_t mn0, mx0; extr(Q, x,   m0 - 1, &mn0, &mx0);
                 c_is2_opt(Q, S, ns + 2, lenS, a, x, m0 - 1, mn0, mx0); }
  if (m1 > m0) { uint64_t mn1, mx1; extr(Q, m0,  m1 - 1, &mn1, &mx1);
                 c_is2_opt(Q, S, ns + 2, lenS, a + s1, m0, m1 - 1, mn1, mx1); }
  if (m2 > m1) { uint64_t mn2, mx2; extr(Q, m1,  m2 - 1, &mn2, &mx2);
                 c_is2_opt(Q, S, ns + 2, lenS, a + s0, m1, m2 - 1, mn2, mx2); }
  if (y >= m2) { uint64_t mn3, mx3; extr(Q, m2,  y,      &mn3, &mx3);
                 c_is2_opt(Q, S, ns + 2, lenS, a + s0 + s1, m2, y, mn3, mx3); }
}

void cs2_opt_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  RESET_CMP(); gSkip = 0; gSkipU = 0;
  if (len_Q == 0) return;
  uint64_t mn = Q[0], mx = Q[0];
  for (int64_t i = 1; i < (int64_t)len_Q; i++) {
    INC_CMP();
    if (Q[i] < mn) {
      mn = Q[i];
    } else {
      INC_CMP();
      if (Q[i] > mx) mx = Q[i];
    }
  }
  c_isf_opt(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1, mn, mx);
}

void cs4_opt_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  RESET_CMP(); gSkip = 0; gSkipU = 0;
  if (len_Q == 0) return;
  uint64_t mn = Q[0], mx = Q[0];
  for (int64_t i = 1; i < (int64_t)len_Q; i++) {
    INC_CMP();
    if (Q[i] < mn) {
      mn = Q[i];
    } else {
      INC_CMP();
      if (Q[i] > mx) mx = Q[i];
    }
  }
  c_is2_opt(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1, mn, mx);
}

static int64_t divisor_v_opt(uint64_t *Q, uint64_t key, int64_t x, int64_t y,
                             uint64_t *Lmn, uint64_t *Lmx,
                             uint64_t *Rmn, uint64_t *Rmx) {
  int64_t i = x, j = y;
  uint64_t cur;

  while (i < j) {
    INC_CMP();
    if (Q[i] >= key) {
      while (i < j) { INC_CMP(); if (Q[j] < key) break; j--; }
      if (i < j) { cur = Q[i]; Q[i] = Q[j]; INC_MOV_ML(); Q[j] = cur; INC_MOV_ML(); }
    }
    i++;
  }

  INC_CMP();
  if (Q[j] < key) j++;
  if (j <= x || j > y) return -1;

  uint64_t lt = Q[x], ht = Q[x];
  int64_t p = x + 1;
  for (; p <= j - 2; p += 2) {
    uint64_t v0 = Q[p], v1 = Q[p + 1];
    INC_CMP(); if (v0 < lt) lt = v0;
    INC_CMP(); if (v0 > ht) ht = v0;
    INC_CMP(); if (v1 < lt) lt = v1;
    INC_CMP(); if (v1 > ht) ht = v1;
  }
  for (; p < j; p++) {
    uint64_t v = Q[p];
    INC_CMP(); if (v < lt) lt = v;
    INC_CMP(); if (v > ht) ht = v;
  }

  uint64_t rt = Q[j], hst = Q[j];
  p = j + 1;
  for (; p <= y - 2; p += 2) {
    uint64_t v0 = Q[p], v1 = Q[p + 1];
    INC_CMP(); if (v0 < rt) rt = v0;
    INC_CMP(); if (v0 > hst) hst = v0;
    INC_CMP(); if (v1 < rt) rt = v1;
    INC_CMP(); if (v1 > hst) hst = v1;
  }
  for (; p <= y; p++) {
    uint64_t v = Q[p];
    INC_CMP(); if (v < rt) rt = v;
    INC_CMP(); if (v > hst) hst = v;
  }

  *Lmn = lt; *Lmx = ht; *Rmn = rt; *Rmx = hst;
  return j;
}

void c_isf_opt_improved(uint64_t *Q, uint64_t *S, uint64_t ns, uint64_t lenS,
                        uint64_t a, int64_t x, int64_t y,
                        uint64_t mn, uint64_t mx) {
  while (x < y) {
    int64_t L = y - x + 1;
    if (L < g_ins) { if (x < y) insertion_v(Q, x, y); return; }
    if (ns >= lenS) return;
    INC_CMP();
  if (mn == mx) { gSkipU += L; return; }

    uint64_t key = S[ns] + a;
    INC_CMP();
    if (key > mx) { gSkip += L; ns++; continue; }
    INC_CMP();
    if (key <= mn) { gSkip += L; a += S[ns]; ns++; continue; }

    uint64_t Lmn, Lmx, Rmn, Rmx;
    int64_t z = divisor_v_opt(Q, key, x, y, &Lmn, &Lmx, &Rmn, &Rmx);
    if (z == -1) { ns++; continue; }

    if ((z - x) < (y - z + 1)) {
      if (z > x) c_isf_opt_improved(Q, S, ns + 1, lenS, a, x, z - 1, Lmn, Lmx);
      a += S[ns]; x = z; mn = Rmn; mx = Rmx;
    } else {
      c_isf_opt_improved(Q, S, ns + 1, lenS, a + S[ns], z, y, Rmn, Rmx);
      y = z - 1; mn = Lmn; mx = Lmx;
    }
    ns++;
  }
}

void cs2_opt_sort_impr(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S) {
  RESET_CMP(); gSkip = 0; gSkipU = 0;
  if (len_Q == 0) return;
  uint64_t mn = Q[0], mx = Q[0];
  for (int64_t i = 1; i < (int64_t)len_Q; i++) {
    INC_CMP();
    if (Q[i] < mn) {
      mn = Q[i];
    } else {
      INC_CMP();
      if (Q[i] > mx) mx = Q[i];
    }
  }
  c_isf_opt_improved(Q, S, 0, len_S, 0, 0, (int64_t)len_Q - 1, mn, mx);
}

void part3eq_v(uint64_t *Q, int64_t x, int64_t y, uint64_t pv,
	       int64_t *lt, int64_t *gt) {
  int64_t l = x, g = y, i = x;
  uint64_t cur;
  while (i <= g) {
    uint64_t v = Q[i];
    INC_CMP();
    if (v < pv) { cur = Q[i]; Q[i] = Q[l]; INC_MOV_ML(); Q[l] = cur; INC_MOV_ML(); l++; i++; }
    else {
      INC_CMP();
      if (v > pv) { cur = Q[i]; Q[i] = Q[g]; INC_MOV_ML(); Q[g] = cur; INC_MOV_ML(); g--; }
      else i++;
    }
  }
  *lt = l;
  *gt = g;
}

uint64_t median3_v(uint64_t *Q, int64_t x, int64_t m, int64_t y) {
  uint64_t a = Q[x], b = Q[m], cc = Q[y];
  INC_CMP(); if (a <= b) {
    INC_CMP(); if (b <= cc) return b;
    INC_CMP(); return (a <= cc) ? cc : a;
  }
  INC_CMP(); if (a <= cc) return a;
  INC_CMP(); return (b <= cc) ? cc : b;
}

/*
 * qs3_sort previously had an unbounded recursive median-of-three quicksort.
 * On the PipeOrgan killer, the first/middle/last samples can repeatedly choose
 * a pivot at an extreme value. Each partition removes only a couple of keys,
 * causing O(n^2) work and O(n) recursion depth (stack overflow at large N).
 *
 * Keep the three-way partitioning, but use an introsort depth limit and recurse
 * only into the smaller side. The larger side is processed by the loop, keeping
 * call-stack usage logarithmic. If partition quality stays poor, heapsort bounds
 * the worst-case complexity.
 */
static void yar_heapsort_range(uint64_t *Q, int64_t left, int64_t right);

static void qs3_rec_impl(uint64_t *Q, int64_t x, int64_t y, int depth_limit) {
  while (x < y) {
    const int64_t length = y - x + 1;
    if (length <= 16) {
      for (int64_t i = x + 1; i <= y; i++) {
        uint64_t v = Q[i];
        int64_t j = i - 1;
        while (j >= x) {
          INC_CMP();
          if (Q[j] <= v) break;
          Q[j + 1] = Q[j]; INC_MOV_ML();
          j--;
        }
        Q[j + 1] = v; INC_MOV_ML();
      }
      return;
    }

    if (depth_limit <= 0) {
      yar_heapsort_range(Q, x, y);
      return;
    }
    --depth_limit;

    const uint64_t p = median3_v(Q, x, x + (y - x) / 2, y);
    int64_t lt, gt;
    part3eq_v(Q, x, y, p, &lt, &gt);

    const int64_t left_length = lt - x;
    const int64_t right_length = y - gt;
    if (left_length < right_length) {
      if (left_length > 1)
        qs3_rec_impl(Q, x, lt - 1, depth_limit);
      x = gt + 1;
    } else {
      if (right_length > 1)
        qs3_rec_impl(Q, gt + 1, y, depth_limit);
      y = lt - 1;
    }
  }
}

void qs3_rec(uint64_t *Q, int64_t x, int64_t y) {
  if (Q == NULL || x >= y) return;
  int depth_limit = 0;
  for (int64_t length = y - x + 1; length > 1; length >>= 1)
    depth_limit += 2;
  qs3_rec_impl(Q, x, y, depth_limit);
}

uint64_t qs3_sort(uint64_t *Q, int64_t n) {
  RESET_CMP();
  if (Q == NULL || n <= 1) return g_cmp;
  qs3_rec(Q, 0, n - 1);
  return g_cmp;
}

static void heap_sift(uint64_t *Q, int64_t i, int64_t n);

/* Heapsort for one subrange. It intentionally does not reset g_cmp. */
static void yar_heapsort_range(uint64_t *Q, int64_t left, int64_t right) {
  uint64_t *base = Q + left;
  const int64_t n = right - left + 1;
  for (int64_t i = n / 2 - 1; i >= 0; --i) heap_sift(base, i, n);
  for (int64_t end = n - 1; end > 0; --end) {
    const uint64_t t = base[0]; base[0] = base[end]; INC_MOV_ML(); base[end] = t; INC_MOV_ML();
    heap_sift(base, 0, end);
  }
}

static void yar_c_impl(uint64_t *Q, int64_t x, int64_t y, int depth_limit) {
  if (y <= x) return;
  if (depth_limit <= 0) {
    yar_heapsort_range(Q, x, y);
    return;
  }
  if (y - x < 27) {
    for (int64_t i = x + 1; i <= y; i++) {
      uint64_t v = Q[i];
      int64_t j = i - 1;
      while (j >= x) { INC_CMP(); if (Q[j] <= v) break; Q[j + 1] = Q[j]; INC_MOV_ML(); j--; }
      Q[j + 1] = v; INC_MOV_ML();
    }
    return;
  }
  INC_CMP(); if (Q[x] > Q[y]) { uint64_t t = Q[x]; Q[x] = Q[y]; INC_MOV_ML(); Q[y] = t; INC_MOV_ML(); }
  uint64_t p = Q[x], q = Q[y];
  int64_t l = x + 1, g = y - 1, k = l;
  while (k <= g) {
    INC_CMP();
    if (Q[k] < p) { uint64_t t = Q[k]; Q[k] = Q[l]; INC_MOV_ML(); Q[l] = t; INC_MOV_ML(); l++; k++; }
    else {
      INC_CMP();
      if (Q[k] >= q) {
        while (k < g) { INC_CMP(); if (Q[g] <= q) break; g--; }
        uint64_t t = Q[k]; Q[k] = Q[g]; INC_MOV_ML(); Q[g] = t; INC_MOV_ML();
        g--; k++;
        INC_CMP();
        if (Q[k - 1] < p) { uint64_t t2 = Q[k - 1]; Q[k - 1] = Q[l]; INC_MOV_ML(); Q[l] = t2; INC_MOV_ML(); l++; }
      } else k++;
    }
  }
  l--; g++;
  uint64_t t = Q[x]; Q[x] = Q[l]; INC_MOV_ML(); Q[l] = t; INC_MOV_ML();
  t = Q[y]; Q[y] = Q[g]; INC_MOV_ML(); Q[g] = t; INC_MOV_ML();

  /* Bound recursion depth on sorted/reverse/duplicate-heavy input. */
  yar_c_impl(Q, x, l - 1, depth_limit - 1);
  yar_c_impl(Q, l + 1, g - 1, depth_limit - 1);
  yar_c_impl(Q, g + 1, y, depth_limit - 1);
}

void yar_c(uint64_t *Q, int64_t x, int64_t y) {
  if (Q == NULL || y <= x) return;
  int depth_limit = 0;
  for (int64_t count = y - x + 1; count > 1; count >>= 1) depth_limit += 2;
  yar_c_impl(Q, x, y, depth_limit);
}

uint64_t yar_sort(uint64_t *Q, int64_t n) { RESET_CMP(); yar_c(Q, 0, n - 1); return g_cmp; }

static uint64_t *g_radix_buf = NULL;
static size_t g_radix_bufsz = 0;

void radix_sort(uint64_t *Q, int64_t n) {
  RESET_CMP();
  if (n <= 1) return;
  if (g_radix_bufsz < (size_t)n) {
    free(g_radix_buf);
    g_radix_buf = malloc((size_t)n * sizeof(uint64_t));
    g_radix_bufsz = (size_t)n;
    if (!g_radix_buf) { fprintf(stderr, "radix_sort: no memory\n"); exit(1); }
  }
  uint64_t *T = g_radix_buf;
  uint32_t c[256];
  for (int shift = 0; shift < 64; shift += 8) {
    memset(c, 0, sizeof(c));
    for (int64_t i = 0; i < n; i++) c[(Q[i] >> shift) & 255]++;
    uint32_t sum = 0;
    for (int k = 0; k < 256; k++) { uint32_t t = c[k]; c[k] = sum; sum += t; }
    for (int64_t i = 0; i < n; i++) { uint32_t d = (Q[i] >> shift) & 255; T[c[d]++] = Q[i]; INC_MOV_ML(); }
    uint64_t *sw = Q; Q = T; T = sw;
  }
}

static void heap_sift(uint64_t *Q, int64_t i, int64_t n) {
  for (;;) {
    int64_t l = 2 * i + 1;
    if (l >= n) return;
    int64_t r = l + 1, m = l;
    if (r < n) {
      INC_CMP();
      if (Q[l] < Q[r]) m = r;
    }
    INC_CMP();
    if (!(Q[i] < Q[m])) return;
    uint64_t t = Q[i]; Q[i] = Q[m]; INC_MOV_ML(); Q[m] = t; INC_MOV_ML();
    i = m;
  }
}

void heapsort(uint64_t *Q, int64_t n) {
  RESET_CMP();
  for (int64_t i = n / 2 - 1; i >= 0; i--) heap_sift(Q, i, n);
  for (int64_t end = n - 1; end > 0; end--) {
    uint64_t t = Q[0]; Q[0] = Q[end]; INC_MOV_ML(); Q[end] = t; INC_MOV_ML();
    heap_sift(Q, 0, end);
  }
}

static int64_t fr_imin(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t fr_imax(int64_t a, int64_t b) { return a > b ? a : b; }

uint64_t select_fr(uint64_t *Q, int64_t left, int64_t right, int64_t k) {
  while (right > left) {
    if (right - left > 600) {
      double n = (double)(right - left + 1);
      double i = (double)(k - left + 1);
      double z = log(n);
      double s = 0.5 * exp(2.0 * z / 3.0);
      double sd = 0.5 * sqrt(z * s * (n - s) / n)
	* (i - n / 2.0 < 0.0 ? -1.0 : 1.0);
      int64_t nl = (int64_t)((double)k - i * s / n + sd);
      int64_t nr = (int64_t)((double)k + (n - i) * s / n + sd);
      select_fr(Q, fr_imax(left, nl), fr_imin(right, nr), k);
    }
    uint64_t t = Q[k], cur;
    int64_t i = left, j = right;
    cur = Q[i]; Q[i] = Q[k]; INC_MOV_ML(); Q[k] = cur; INC_MOV_ML();
    if (Q[right] > t) { cur = Q[right]; Q[right] = Q[left]; INC_MOV_ML(); Q[left] = cur; INC_MOV_ML(); }
    while (i < j) {
      cur = Q[i]; Q[i] = Q[j]; INC_MOV_ML(); Q[j] = cur; INC_MOV_ML();
      i++; j--;
      while (i < right && Q[i] < t) i++;
      while (j > left && Q[j] > t) j--;
    }
    if (Q[left] == t) { cur = Q[left]; Q[left] = Q[j]; INC_MOV_ML(); Q[j] = cur; INC_MOV_ML(); }
    else { j++; cur = Q[j]; Q[j] = Q[right]; INC_MOV_ML(); Q[right] = cur; INC_MOV_ML(); }
    if (j <= k) left = j + 1;
    if (k <= j) right = j - 1;
  }
  return Q[k];
}

uint64_t select_fr_c(uint64_t *Q, int64_t left, int64_t right, int64_t k) {
  while (right > left) {
    if (right - left > 600) {
      double n = (double)(right - left + 1);
      double i = (double)(k - left + 1);
      double z = log(n);
      double s = 0.5 * exp(2.0 * z / 3.0);
      double sd = 0.5 * sqrt(z * s * (n - s) / n) * ((i - n / 2.0) < 0.0 ? -1.0 : 1.0);
      int64_t nl = (int64_t)((double)k - i * s / n + sd);
      int64_t nr = (int64_t)((double)k + (n - i) * s / n + sd);
      if (nl < left) nl = left;
      if (nr > right) nr = right;
      if (nl > left || nr < right) select_fr_c(Q, nl, nr, k);
    }
    uint64_t t = Q[k], cur;
    int64_t i = left, j = right;
    cur = Q[i]; Q[i] = Q[k]; INC_MOV_ML(); Q[k] = cur; INC_MOV_ML();
    INC_CMP(); if (Q[right] > t) { cur = Q[right]; Q[right] = Q[left]; INC_MOV_ML(); Q[left] = cur; INC_MOV_ML(); }
    while (i < j) {
      cur = Q[i]; Q[i] = Q[j]; INC_MOV_ML(); Q[j] = cur; INC_MOV_ML();
      i++; j--;
      for (;;) { if (i >= right) break; INC_CMP(); if (Q[i] >= t) break; i++; }
      for (;;) { if (j <= left) break; INC_CMP(); if (Q[j] <= t) break; j--; }
    }
    INC_CMP(); if (Q[left] == t) { cur = Q[left]; Q[left] = Q[j]; INC_MOV_ML(); Q[j] = cur; INC_MOV_ML(); }
    else { j++; cur = Q[j]; Q[j] = Q[right]; INC_MOV_ML(); Q[right] = cur; INC_MOV_ML(); }
    if (j <= k) left = j + 1;
    if (k <= j) right = j - 1;
  }
  return Q[k];
}

int64_t part_lt_stats_c(uint64_t *Q, int64_t x, int64_t y, uint64_t t,
                        uint64_t *lmn, uint64_t *lmx,
                        uint64_t *rmn, uint64_t *rmx) {
  int64_t p = x;
  int gl = 0, gr = 0;
  uint64_t lmin = 0, lmax = 0, rmin = 0, rmax = 0;
  for (int64_t i = x; i <= y; i++) {
    uint64_t v = Q[i];
    INC_CMP();
    if (v < t) {
      Q[i] = Q[p]; INC_MOV_ML(); Q[p] = v; INC_MOV_ML(); p++;
      if (!gl) { gl = 1; lmin = lmax = v; }
      else {
        INC_CMP(); if (v < lmin) lmin = v;
        INC_CMP(); if (v > lmax) lmax = v;
      }
    } else {
      if (!gr) { gr = 1; rmin = rmax = v; }
      else {
        INC_CMP(); if (v < rmin) rmin = v;
        INC_CMP(); if (v > rmax) rmax = v;
      }
    }
  }
  *lmn = gl ? lmin : 0; *lmx = gl ? lmax : 0;
  *rmn = gr ? rmin : t; *rmx = gr ? rmax : t;
  return p;
}

void gen_seq_s(uint64_t n, double c, uint64_t **ar, uint64_t *size) {
  if (c <= 1.0) c = 1.5;
  uint64_t s = n;
  *size = 1;
  uint64_t capacity = 32;
  *ar = (uint64_t *)malloc(capacity * sizeof(uint64_t));
  (*ar)[0] = (uint64_t)ceil(((double)s) / c);
  s = s - (*ar)[0];
  (*size)++;
  if (c >= 2.0) {
    while (s != 0) {
      if (*size >= capacity) { capacity *= 2; *ar = realloc(*ar, capacity * sizeof(uint64_t)); }
      (*ar)[(*size) - 1] = (uint64_t)ceil(((double)s) / c);
      s = s - (*ar)[(*size) - 1];
      (*size)++;
    }
    (*size)--;
  } else {
    while ((*ar)[(*size) - 2] != 0) {
      if (*size >= capacity) { capacity *= 2; *ar = realloc(*ar, capacity * sizeof(uint64_t)); }
      (*ar)[(*size) - 1] = (uint64_t)ceil(((double)((*ar)[(*size) - 2] - 1)) / c);
      (*size)++;
    }
    (*size) -= 2;
  }
  *ar = realloc(*ar, (*size) * sizeof(uint64_t));
}

uint64_t sel_cascade_fr_hybrid(uint64_t *Q, int64_t n, int64_t k, uint64_t mm, double c) {
  (void)mm;
  RESET_CMP(); g_skip = 0;
  if (n <= 0) return 0;
  if (n <= 512) return select_fr_c(Q, 0, n - 1, k);

  int64_t m = (int64_t)pow((double)n, 2.0 / 3.0);
  if (m < 256) m = 256;
  if (m > n / 2) m = n / 2;
  int64_t step = n / m;
  uint64_t *sample = malloc((size_t)m * 8);
  if (!sample) return 0;
  for (int64_t i = 0; i < m; i++) sample[i] = Q[i * step];

  double p = (double)k / (double)n;
  int64_t k_sample = (int64_t)(p * (double)m);
  double sd = 2.5 * sqrt((double)m * p * (1.0 - p));
  int64_t delta = (int64_t)ceil(sd);
  if (delta < 8) delta = 8;
  int64_t idx_low = k_sample - delta; if (idx_low < 0) idx_low = 0;
  int64_t idx_high = k_sample + delta; if (idx_high >= m) idx_high = m - 1;
  uint64_t t_low = select_fr_c(sample, 0, m - 1, idx_low);
  uint64_t t_high = select_fr_c(sample, 0, m - 1, idx_high);
  free(sample);

  uint64_t W = t_high - t_low;
  uint64_t *S = NULL, len_S = 0;
  gen_seq_s(W, c, &S, &len_S);

  uint64_t *scr = malloc((size_t)n * 8);
  if (!scr) { free(S); return 0; }
  int64_t m_scr = 0, count_low = 0;
  for (int64_t i = 0; i < n; i++) {
    uint64_t v = Q[i];
    INC_CMP(); if (v < t_low) { count_low++; continue; }
    INC_CMP(); if (v <= t_high) scr[m_scr++] = v;
  }
  if (k < count_low) {
    int64_t tail_m = 0;
    for (int64_t i = 0; i < n; i++) {
      INC_CMP();
      if (Q[i] < t_low) scr[tail_m++] = Q[i];
    }
    uint64_t r = select_fr_c(scr, 0, tail_m - 1, k);
    free(scr); free(S);
    return r;
  }
  if (k >= count_low + m_scr) {
    int64_t tail_m = 0;
    for (int64_t i = 0; i < n; i++) {
      INC_CMP();
      if (Q[i] > t_high) scr[tail_m++] = Q[i];
    }
    uint64_t r = select_fr_c(scr, 0, tail_m - 1, k - (count_low + m_scr));
    free(scr); free(S);
    return r;
  }
  int64_t kk = k - count_low;

  int64_t x = 0, y = m_scr - 1;
  uint64_t a = t_low;
  int64_t ns = 0;
  uint64_t mne = scr[0], mxe = scr[0];
  for (int64_t i = 1; i <= y; i++) {
    INC_CMP();
    if (scr[i] < mne) mne = scr[i]; else if (scr[i] > mxe) mxe = scr[i];
  }
  while (y - x > 256 && (uint64_t)ns < len_S) {
    if (mne == mxe) { g_hom++; goto finish; }
    uint64_t key = S[ns] + a;
    if (key > mxe) { g_skip += (y - x + 1); ns++; continue; }
    if (key <= mne) { g_skip += (y - x + 1); a += S[ns]; ns++; continue; }
    uint64_t lmn, lmx, rmn, rmx;
    int64_t pc = part_lt_stats_c(scr, x, y, key, &lmn, &lmx, &rmn, &rmx);
    if (kk < pc) { y = pc - 1; mne = lmn; mxe = lmx; }
    else         { x = pc;     a += S[ns]; mne = rmn; mxe = rmx; }
    ns++;
  }
finish:
  uint64_t final_result = (mne == mxe) ? mne : select_fr_c(scr, x, y, kk);
  free(scr);
  free(S);
  return final_result;
}

uint64_t select_adaptive_dispatcher(uint64_t *Q, int64_t n, int64_t k, uint64_t mm, double c) {
  (void)mm;
  RESET_CMP();
  if (n <= 0) return 0;
  if (n <= 1024) return select_fr_c(Q, 0, n - 1, k);

  int64_t m = (int64_t)pow((double)n, 2.0 / 3.0);
  if (m < 256) m = 256;
  if (m > n / 2) m = n / 2;
  int64_t step = n / m;
  uint64_t *sample = malloc((size_t)m * 8);
  if (!sample) return 0;

  uint64_t smn = Q[0], smx = Q[0];
  for (int64_t i = 0; i < m; i++) {
    uint64_t v = Q[i * step];
    sample[i] = v;
    INC_CMP(); if (v < smn) smn = v;
    INC_CMP(); if (v > smx) smx = v;
  }

  double p = (double)k / (double)n;
  int64_t k_sample = (int64_t)(p * (double)m);
  double sd = 2.5 * sqrt((double)m * p * (1.0 - p));
  int64_t delta = (int64_t)ceil(sd);
  if (delta < 8) delta = 8;
  int64_t idx_low = k_sample - delta;   if (idx_low < 0) idx_low = 0;
  int64_t idx_high = k_sample + delta; if (idx_high >= m) idx_high = m - 1;

  uint64_t t_low = select_fr_c(sample, 0, m - 1, idx_low);
  uint64_t t_mid = select_fr_c(sample, 0, m - 1, k_sample);
  uint64_t t_high = select_fr_c(sample, 0, m - 1, idx_high);
  free(sample);

  int trig = 0;
  if (idx_low < k_sample  && t_low == t_mid) trig = 1;
  if (k_sample < idx_high && t_mid == t_high) trig = 1;
  if (idx_low < idx_high  && t_low == t_high) trig = 1;
  uint64_t corridor_span = t_high - t_low;
  uint64_t sample_span = smx - smn;
  int edge = (idx_low == 0 || idx_high == m - 1);
  if (!trig && !edge && sample_span > 0) {
    double span_ratio = (double)corridor_span / (double)sample_span;
    if (span_ratio < 0.001) trig = 1;
    else {
      double left_side = (double)(t_mid - t_low);
      double right_side = (double)(t_high - t_mid);
      if (left_side > 0 && right_side > 0) {
        double skew = left_side / right_side;
        if (skew > 10.0 || skew < 0.1) trig = 1;
      }
    }
  }

  int64_t c_scout = g_cmp;
  (void)c_scout; /* ADD_CMP may compile away in ENABLE_FAST_BENCH builds. */

  if (trig) {
    g_route = 2;
    uint64_t r = sel_cascade_fr_hybrid(Q, n, k, mm, c);
    ADD_CMP(c_scout);
    return r;
  }
  g_route = 1;
  RESET_CMP();
  uint64_t r = select_fr_c(Q, 0, n - 1, k);
  ADD_CMP(c_scout);
  return r;
}

uint64_t max(uint64_t *ar, uint64_t size) {
  uint64_t result = ar[0], i;
  for (i=1; i<size; i++)
    if (result<ar[i]) result = ar[i];
  return result;
}

void printing(uint64_t * ar, uint64_t size) {
    uint64_t i;
    for (i = 0; i<size; i++)  printf("%" PRIu64 "\n",ar[i]);
}

#endif /* MYLIB_NO_MAIN */
