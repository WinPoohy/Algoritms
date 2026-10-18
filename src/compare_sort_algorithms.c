#define _GNU_SOURCE
#include "sorts.h"
#include "sorts_instrumented.h"
#include "killers.h"


extern void cs_sort_c_instr(int *arr, int low, int high);
extern void cs3_gs_sort_c_instr(int *arr, int low, int high);
extern void triple_pivot_quicksort_c_instr(int *arr, int low, int high);
extern uint64_t g_cmp_instr;
extern uint64_t g_mov_instr;
extern void cs3_gs_sort_c(int *arr, int low, int high);

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#ifdef __linux__
#include <dirent.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <asm/unistd.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

/* from mylib.c / mylib_fast.c. */
extern int64_t g_cmp;
extern uint64_t g_mov_mylib;
extern void get_sequence(uint64_t n, double c, uint64_t **ar, uint64_t *size);
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

extern void get_sequence_nocmp(uint64_t n, double c, uint64_t **ar, uint64_t *size);
extern void cs_sort_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs_sort_fast_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_sort_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs4_sort_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_opt_sort_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs4_opt_sort_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs2_opt_sort_impr_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern uint64_t qs3_sort_nocmp(uint64_t *, int64_t);
extern uint64_t yar_sort_nocmp(uint64_t *, int64_t);
extern void heapsort_nocmp(uint64_t *, int64_t);
extern void radix_sort_nocmp(uint64_t *, int64_t);
extern int g_maxt_nocmp;
extern int64_t g_minlen_nocmp;
extern void cs_sort_par_orig_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs_sort_par_fused_nocmp(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs_sort_par_orig(uint64_t *, uint64_t *, uint64_t, uint64_t);
extern void cs_sort_par_fused(uint64_t *, uint64_t *, uint64_t, uint64_t);

#ifdef HAVE_SORTS_FASM
extern void cs3_opt_fasm_instr(int *, int, int);
extern void cs3_gs_sort_fasm_instr(int *, int, int);
extern void dual_pivot_quicksort_fasm_instr(int *, int, int);
extern void introsort_fasm_instr(int *, int);
extern void pdqsort_fasm_instr(int *, int);
extern void triple_pivot_quicksort_fasm_instr(int *, int, int);
#endif

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define DEFAULT_REPEATS 7
#define MAX_REPEATS 101
#define MAX_N 10000000

typedef void (*RunInt)(int *a, int n, int *tmp);
typedef void (*RunU64)(uint64_t *a, int n, uint64_t *s, uint64_t len_s);
typedef int (*OpsInt)(int *a, int n, uint64_t *cmp, uint64_t *mov);
typedef int (*OpsU64)(uint64_t *a, int n, uint64_t *s, uint64_t len_s,
                      uint64_t *cmp, uint64_t *mov);

typedef struct {
    const char *name;
    const char *source;
    RunInt run_i;
    RunU64 run_u;
    OpsInt ops_i;
    OpsU64 ops_u;
    int max_n;
    int power_of_two;
    const char *note;
} Algorithm;

typedef struct {
    const char *name;
    int wide_u64_only;
    int rank_int_view;
} Dataset;

static uint32_t rng_state = 0x9e3779b9u;
static uint32_t rnd32(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}

static uint64_t ticks_begin(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ __volatile__("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}
static uint64_t ticks_end(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp\n\tlfence" : "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
    (void)aux;
    return ((uint64_t)hi << 32) | lo;
#else
    return ticks_begin();
#endif
}

/* Monotonic wall-clock timer. Unlike TSC ticks, this produces nanoseconds. */
static uint64_t monotonic_ns(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0)
#endif
        clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

typedef struct {
    int tid;
    int fd_cycles, fd_instructions, fd_branch_misses, fd_branches;
    int valid;
} PerfThreadContext;

enum PerfScopeMode {
    PERF_SCOPE_CALLER_THREAD = 0,
    PERF_SCOPE_EXISTING_PROCESS_THREADS = 1,
    PERF_SCOPE_INHERITED_CHILDREN = 2,
    PERF_SCOPE_MYLIB_WORKER_HOOKS = 3,
    PERF_SCOPE_OPENMP_WORKER_HOOKS = 4
};

typedef struct {
    PerfThreadContext *threads;
    size_t count;
    int valid;
    int scope_mode;
    int worker_hooks_active;
} PerfContext;

typedef struct {
    uint64_t cycles, instructions, branch_misses, branches;
    size_t thread_contexts;
    int valid;
} PerfReadout;

#ifdef __linux__
static pthread_mutex_t g_worker_perf_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_worker_perf_active = 0;
static int g_worker_perf_failed = 0;
static uint64_t g_worker_cycles = 0;
static uint64_t g_worker_instructions = 0;
static uint64_t g_worker_branch_misses = 0;
static uint64_t g_worker_branches = 0;
static size_t g_worker_contexts = 0;
static __thread PerfThreadContext *g_worker_perf_tls = NULL;

static pid_t current_tid(void) {
#ifdef __NR_gettid
    return (pid_t)syscall(__NR_gettid);
#else
    return getpid();
#endif
}

/* Open an event for a specific TID. inherit + inherit_stat aggregates direct
 * child-thread work for the inherited-child scope. Recursive MyLib workers use
 * explicit enter/exit hooks below, because perf inheritance is not recursive. */
static int open_hw_event(pid_t target_tid, uint64_t config,
                         int group_fd, int disabled, int inherit) {
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = config;
    attr.disabled = disabled;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.inherit = inherit ? 1u : 0u;
    attr.inherit_stat = inherit ? 1u : 0u;
    const pid_t pid_arg = (target_tid == current_tid()) ? 0 : target_tid;
    return (int)syscall(__NR_perf_event_open, &attr, pid_arg, -1, group_fd, 0);
}

static void close_thread_perf_context(PerfThreadContext *ctx) {
    if (ctx->fd_branches >= 0) close(ctx->fd_branches);
    if (ctx->fd_branch_misses >= 0) close(ctx->fd_branch_misses);
    if (ctx->fd_instructions >= 0) close(ctx->fd_instructions);
    if (ctx->fd_cycles >= 0) close(ctx->fd_cycles);
    ctx->fd_cycles = ctx->fd_instructions = -1;
    ctx->fd_branch_misses = ctx->fd_branches = -1;
    ctx->valid = 0;
}

static void release_perf_context(PerfContext *ctx) {
    if (!ctx) return;
    for (size_t i = 0; i < ctx->count; ++i)
        close_thread_perf_context(&ctx->threads[i]);
    free(ctx->threads);
    ctx->threads = NULL;
    ctx->count = 0;
    ctx->valid = 0;
}

static int append_perf_thread(PerfContext *ctx, pid_t tid) {
    for (size_t i = 0; i < ctx->count; ++i)
        if (ctx->threads[i].tid == (int)tid) return 1;
    PerfThreadContext *grown = realloc(ctx->threads,
        (ctx->count + 1) * sizeof(*ctx->threads));
    if (!grown) return 0;
    ctx->threads = grown;
    PerfThreadContext *t = &ctx->threads[ctx->count++];
    memset(t, 0, sizeof(*t));
    t->tid = (int)tid;
    t->fd_cycles = t->fd_instructions = -1;
    t->fd_branch_misses = t->fd_branches = -1;
    return 1;
}

static int collect_thread_ids(PerfContext *ctx, int include_process_threads) {
    const pid_t self_tid = current_tid();
    if (!include_process_threads)
        return append_perf_thread(ctx, self_tid);

    DIR *dir = opendir("/proc/self/task");
    if (!dir) return 0;
    int ok = 1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        char *end = NULL;
        errno = 0;
        long value = strtol(ent->d_name, &end, 10);
        if (errno || end == ent->d_name || *end != '\0' || value <= 0 ||
            value > INT_MAX) continue;
        if (!append_perf_thread(ctx, (pid_t)value)) { ok = 0; break; }
    }
    if (closedir(dir) != 0) ok = 0;
    if (ok && !append_perf_thread(ctx, self_tid)) ok = 0;
    return ok && ctx->count > 0;
}

static int open_thread_perf_context(PerfThreadContext *t, int inherit) {
    t->fd_cycles = open_hw_event((pid_t)t->tid, PERF_COUNT_HW_CPU_CYCLES,
                                 -1, 1, inherit);
    if (t->fd_cycles < 0) return 0;
    t->fd_instructions = open_hw_event((pid_t)t->tid, PERF_COUNT_HW_INSTRUCTIONS,
                                       t->fd_cycles, 0, inherit);
    t->fd_branch_misses = open_hw_event((pid_t)t->tid, PERF_COUNT_HW_BRANCH_MISSES,
                                        t->fd_cycles, 0, inherit);
    t->fd_branches = open_hw_event((pid_t)t->tid, PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
                                   t->fd_cycles, 0, inherit);
    if (t->fd_instructions < 0 || t->fd_branch_misses < 0 || t->fd_branches < 0)
        return 0;
    t->valid = 1;
    return 1;
}

static int enable_thread_perf_context(PerfThreadContext *t) {
    if (ioctl(t->fd_cycles, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1)
        return 0;
    if (ioctl(t->fd_cycles, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1)
        return 0;
    return 1;
}

static int read_counter_u64(int fd, uint64_t *value) {
    return fd >= 0 && read(fd, value, sizeof(*value)) == (ssize_t)sizeof(*value);
}

static int stop_thread_perf_context(PerfThreadContext *t, PerfReadout *out, int disable_first) {
    uint64_t cycles = 0, instructions = 0, misses = 0, branches = 0;
    int ok = 1;
    if (disable_first)
        ok = ioctl(t->fd_cycles, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) != -1;
    ok = read_counter_u64(t->fd_cycles, &cycles) && ok;
    ok = read_counter_u64(t->fd_instructions, &instructions) && ok;
    ok = read_counter_u64(t->fd_branch_misses, &misses) && ok;
    ok = read_counter_u64(t->fd_branches, &branches) && ok;
    if (!ok) return 0;
    out->cycles += cycles;
    out->instructions += instructions;
    out->branch_misses += misses;
    out->branches += branches;
    return 1;
}

static void worker_perf_mark_failed(void) {
    pthread_mutex_lock(&g_worker_perf_lock);
    g_worker_perf_failed = 1;
    pthread_mutex_unlock(&g_worker_perf_lock);
}

static void worker_perf_begin_session(void) {
    pthread_mutex_lock(&g_worker_perf_lock);
    g_worker_perf_failed = 0;
    g_worker_cycles = g_worker_instructions = 0;
    g_worker_branch_misses = g_worker_branches = 0;
    g_worker_contexts = 0;
    pthread_mutex_unlock(&g_worker_perf_lock);
    __sync_synchronize();
    __atomic_store_n(&g_worker_perf_active, 1, __ATOMIC_RELEASE);
}

/* Exported hooks called from recursive MyLib pthread workers while mode 11.1
 * is measuring a parallel MyLib sort. Weak references make them optional in
 * every other executable that links mylib.c without this benchmark. */
void sortbench_perf_worker_enter(void) {
    if (!__atomic_load_n(&g_worker_perf_active, __ATOMIC_ACQUIRE)) return;
    if (g_worker_perf_tls != NULL) {
        worker_perf_mark_failed();
        return;
    }
    PerfThreadContext *t = calloc(1, sizeof(*t));
    if (!t) { worker_perf_mark_failed(); return; }
    t->tid = (int)current_tid();
    t->fd_cycles = t->fd_instructions = -1;
    t->fd_branch_misses = t->fd_branches = -1;
    if (!open_thread_perf_context(t, 0) || !enable_thread_perf_context(t)) {
        close_thread_perf_context(t);
        free(t);
        worker_perf_mark_failed();
        return;
    }
    g_worker_perf_tls = t;
}

void sortbench_perf_worker_exit(void) {
    PerfThreadContext *t = g_worker_perf_tls;
    if (!t) return;
    g_worker_perf_tls = NULL;
    PerfReadout sample;
    memset(&sample, 0, sizeof(sample));
    const int ok = stop_thread_perf_context(t, &sample, 1);
    close_thread_perf_context(t);
    free(t);
    pthread_mutex_lock(&g_worker_perf_lock);
    if (!ok) {
        g_worker_perf_failed = 1;
    } else {
        g_worker_cycles += sample.cycles;
        g_worker_instructions += sample.instructions;
        g_worker_branch_misses += sample.branch_misses;
        g_worker_branches += sample.branches;
        ++g_worker_contexts;
    }
    pthread_mutex_unlock(&g_worker_perf_lock);
}

static PerfReadout worker_perf_end_session(void) {
    PerfReadout out;
    memset(&out, 0, sizeof(out));
    __atomic_store_n(&g_worker_perf_active, 0, __ATOMIC_RELEASE);
    __sync_synchronize();
    pthread_mutex_lock(&g_worker_perf_lock);
    out.valid = !g_worker_perf_failed;
    out.cycles = g_worker_cycles;
    out.instructions = g_worker_instructions;
    out.branch_misses = g_worker_branch_misses;
    out.branches = g_worker_branches;
    out.thread_contexts = g_worker_contexts;
    pthread_mutex_unlock(&g_worker_perf_lock);
    return out;
}
#endif

static PerfContext perf_start(int scope_mode) {
    PerfContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.scope_mode = scope_mode;
#ifdef __linux__
    const int all_existing = (scope_mode == PERF_SCOPE_EXISTING_PROCESS_THREADS);
    const int inherit = (scope_mode == PERF_SCOPE_INHERITED_CHILDREN);
    if (!collect_thread_ids(&ctx, all_existing)) {
        release_perf_context(&ctx);
        return ctx;
    }
    for (size_t i = 0; i < ctx.count; ++i) {
        if (!open_thread_perf_context(&ctx.threads[i], inherit)) {
            release_perf_context(&ctx);
            return ctx;
        }
    }
    for (size_t i = 0; i < ctx.count; ++i) {
        if (ioctl(ctx.threads[i].fd_cycles, PERF_EVENT_IOC_RESET,
                  PERF_IOC_FLAG_GROUP) == -1) {
            release_perf_context(&ctx);
            return ctx;
        }
    }
    const pid_t self_tid = current_tid();
    for (size_t i = 0; i < ctx.count; ++i) {
        if (ctx.threads[i].tid == (int)self_tid) continue;
        if (ioctl(ctx.threads[i].fd_cycles, PERF_EVENT_IOC_ENABLE,
                  PERF_IOC_FLAG_GROUP) == -1) {
            for (size_t j = 0; j < ctx.count; ++j)
                (void)ioctl(ctx.threads[j].fd_cycles, PERF_EVENT_IOC_DISABLE,
                            PERF_IOC_FLAG_GROUP);
            release_perf_context(&ctx);
            return ctx;
        }
    }
    for (size_t i = 0; i < ctx.count; ++i) {
        if (ctx.threads[i].tid != (int)self_tid) continue;
        if (ioctl(ctx.threads[i].fd_cycles, PERF_EVENT_IOC_ENABLE,
                  PERF_IOC_FLAG_GROUP) == -1) {
            for (size_t j = 0; j < ctx.count; ++j)
                (void)ioctl(ctx.threads[j].fd_cycles, PERF_EVENT_IOC_DISABLE,
                            PERF_IOC_FLAG_GROUP);
            release_perf_context(&ctx);
            return ctx;
        }
    }
    ctx.valid = 1;
    if (scope_mode == PERF_SCOPE_MYLIB_WORKER_HOOKS ||
        scope_mode == PERF_SCOPE_OPENMP_WORKER_HOOKS) {
        worker_perf_begin_session();
        ctx.worker_hooks_active = 1;
    }
#endif
    return ctx;
}

static PerfReadout perf_stop(PerfContext *ctx) {
    PerfReadout out;
    memset(&out, 0, sizeof(out));
#ifdef __linux__
    PerfReadout worker_samples;
    memset(&worker_samples, 0, sizeof(worker_samples));
    if (ctx && ctx->valid) {
        out.valid = 1;
        out.thread_contexts = ctx->count;
        const int captured_worker_hooks = ctx->worker_hooks_active;
        if (captured_worker_hooks) {
            worker_samples = worker_perf_end_session();
            ctx->worker_hooks_active = 0;
        }
        /* Stop all explicit groups before reading any group, minimizing skew. */
        for (size_t i = 0; i < ctx->count; ++i) {
            if (ioctl(ctx->threads[i].fd_cycles, PERF_EVENT_IOC_DISABLE,
                      PERF_IOC_FLAG_GROUP) == -1)
                out.valid = 0;
        }
        for (size_t i = 0; i < ctx->count; ++i) {
            PerfReadout sample;
            memset(&sample, 0, sizeof(sample));
            if (!stop_thread_perf_context(&ctx->threads[i], &sample, 0)) {
                out.valid = 0;
                break;
            }
            out.cycles += sample.cycles;
            out.instructions += sample.instructions;
            out.branch_misses += sample.branch_misses;
            out.branches += sample.branches;
        }
        if (captured_worker_hooks) {
            if (!worker_samples.valid) out.valid = 0;
            out.cycles += worker_samples.cycles;
            out.instructions += worker_samples.instructions;
            out.branch_misses += worker_samples.branch_misses;
            out.branches += worker_samples.branches;
            out.thread_contexts += worker_samples.thread_contexts;
        }
    }
#else
    (void)ctx;
#endif
#ifdef __linux__
    if (ctx) release_perf_context(ctx);
#else
    if (ctx) { free(ctx->threads); ctx->threads = NULL; ctx->count = 0; }
#endif
    if (!out.valid) out.thread_contexts = 0;
    return out;
}

static double median_double(double *values, int count) {
    for (int i = 1; i < count; ++i) {
        const double value = values[i];
        int j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    return values[count / 2];
}

static int cmp_i32(const void *pa, const void *pb) {
    const int a = *(const int *)pa, b = *(const int *)pb;
    return (a > b) - (a < b);
}
static int cmp_u64(const void *pa, const void *pb) {
    const uint64_t a = *(const uint64_t *)pa, b = *(const uint64_t *)pb;
    return (a > b) - (a < b);
}
static uint64_t median_u64(uint64_t *v, int count) {
    for (int i = 1; i < count; ++i) {
        const uint64_t x = v[i]; int j = i;
        while (j > 0 && v[j - 1] > x) { v[j] = v[j - 1]; --j; }
        v[j] = x;
    }
    return v[count / 2];
}
static int is_power_of_two(int n) { return n > 0 && (n & (n - 1)) == 0; }
static int same_int(const int *a, const int *b, int n) {
    return memcmp(a, b, (size_t)n * sizeof(*a)) == 0;
}
static int same_u64(const uint64_t *a, const uint64_t *b, int n) {
    return memcmp(a, b, (size_t)n * sizeof(*a)) == 0;
}

static int copy_u64_to_rank_int(const uint64_t *src, int *dst, int n) {
    if (n <= 0) return 1;
    uint64_t *sorted = malloc((size_t)n * sizeof(*sorted));
    if (!sorted) return 0;
    memcpy(sorted, src, (size_t)n * sizeof(*sorted));
    qsort(sorted, (size_t)n, sizeof(*sorted), cmp_u64);
    for (int i = 0; i < n; ++i) {
        size_t lo = 0, hi = (size_t)n;
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            if (sorted[mid] < src[i]) lo = mid + 1;
            else hi = mid;
        }
        if (lo > (size_t)INT_MAX) { free(sorted); return 0; }
        dst[i] = (int)lo;
    }
    free(sorted);
    return 1;
}

static int copy_u64_to_int_lossless_order(const uint64_t *src, int *dst, int n) {
    for (int i = 0; i < n; ++i) {
        if (src[i] > (uint64_t)INT_MAX) return copy_u64_to_rank_int(src, dst, n);
    }
    for (int i = 0; i < n; ++i) dst[i] = (int)src[i];
    return 1;
}

static int cmp_i32_counted(const void *pa, const void *pb) {
    const int a = *(const int *)pa, b = *(const int *)pb;
    g_cmp_instr += 2; /* comparator evaluates (a>b) and (a<b) */
    return (a > b) - (a < b);
}
static int cmp_u64_counted(const void *pa, const void *pb) {
    const uint64_t a = *(const uint64_t *)pa, b = *(const uint64_t *)pb;
    g_cmp_instr += 2;
    return (a > b) - (a < b);
}
static int ops_qsort_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr = 0; g_mov_instr = 0;
    qsort(a, (size_t)n, sizeof(*a), cmp_i32_counted);
    *cmp = g_cmp_instr; *mov = UINT64_MAX; /* libc's internal writes are opaque */
    return 1;
}
static int ops_qsort_u(uint64_t *a, int n, uint64_t *s, uint64_t len_s,
                       uint64_t *cmp, uint64_t *mov) {
    (void)s; (void)len_s;
    g_cmp_instr = 0; g_mov_instr = 0;
    qsort(a, (size_t)n, sizeof(*a), cmp_u64_counted);
    *cmp = g_cmp_instr; *mov = UINT64_MAX;
    return 1;
}

/* g_cmp_instr counts key-to-key comparisons. g_mov_instr counts writes to
 * array slots, including scratch-buffer writes and writes back to the result. */
static void ops_insertion_range(int *a, int low, int high) {
    for (int i = low + 1; i <= high; ++i) {
        const int key = a[i];
        int j = i - 1;
        while (j >= low) {
            ++g_cmp_instr;
            if (a[j] <= key) break;
            a[j + 1] = a[j]; ++g_mov_instr;
            --j;
        }
        a[j + 1] = key; ++g_mov_instr;
    }
}
static int ops_insertion_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr = 0; g_mov_instr = 0;
    if (n > 1) ops_insertion_range(a, 0, n - 1);
    *cmp = g_cmp_instr; *mov = g_mov_instr;
    return 1;
}
static void ops_sift_down(int *a, int n, int root) {
    for (;;) {
        int largest = root;
        const int left = root * 2 + 1, right = left + 1;
        if (left < n) { ++g_cmp_instr; if (a[left] > a[largest]) largest = left; }
        if (right < n) { ++g_cmp_instr; if (a[right] > a[largest]) largest = right; }
        if (largest == root) return;
        const int t = a[root]; a[root] = a[largest]; a[largest] = t;
        g_mov_instr += 2;
        root = largest;
    }
}
static void ops_heap_sort_range(int *a, int n) {
    for (int i = n / 2 - 1; i >= 0; --i) ops_sift_down(a, n, i);
    for (int end = n - 1; end > 0; --end) {
        const int t = a[0]; a[0] = a[end]; a[end] = t;
        g_mov_instr += 2;
        ops_sift_down(a, end, 0);
    }
}
static int ops_heap_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr=0; g_mov_instr=0;
    if (n>1) ops_heap_sort_range(a,n);
    *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}

/* Counter twin of the C QuickSort/Introsort implementation. */
static void ops_quick_impl(int *a, int low, int high, int depth_limit) {
    while (low < high) {
        const int n = high - low + 1;
        if (n <= 24) { ops_insertion_range(a, low, high); return; }
        if (depth_limit <= 0) { ops_heap_sort_range(a + low, n); return; }
        --depth_limit;
        int x = a[low], y = a[low + (high - low) / 2], z = a[high], t;
        ++g_cmp_instr; if (x > y) { t=x; x=y; y=t; }
        ++g_cmp_instr; if (y > z) { t=y; y=z; z=t; }
        ++g_cmp_instr; if (x > y) { t=x; x=y; y=t; }
        const int pivot = y;
        int i = low, j = high;
        while (i <= j) {
            while (i <= high) { ++g_cmp_instr; if (a[i] < pivot) ++i; else break; }
            while (j >= low) { ++g_cmp_instr; if (a[j] > pivot) --j; else break; }
            if (i <= j) {
                if (i != j) { t=a[i]; a[i]=a[j]; a[j]=t; g_mov_instr += 2; }
                ++i; --j;
            }
        }
        if (j - low < high - i) {
            if (low < j) ops_quick_impl(a, low, j, depth_limit);
            low = i;
        } else {
            if (i < high) ops_quick_impl(a, i, high, depth_limit);
            high = j;
        }
    }
}
static int ops_quick_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr = 0; g_mov_instr = 0;
    int depth = 0; for (int k=n; k>1; k >>= 1) depth += 2;
    if (n > 1) ops_quick_impl(a, 0, n-1, depth);
    *cmp = g_cmp_instr; *mov = g_mov_instr; return 1;
}
static int ops_quick2_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr = 0; g_mov_instr = 0;
    if (n > 1) {
        const int low=0, high=n-1, pivot=a[0];
        int i=low, j=high+1;
        for (;;) {
            do { ++i; if (i <= high) ++g_cmp_instr; }
            while (i <= high && a[i] < pivot);
            do { --j; ++g_cmp_instr; } while (a[j] > pivot);
            if (i >= j) break;
            const int t=a[i]; a[i]=a[j]; a[j]=t; g_mov_instr += 2;
        }
        const int t=a[low]; a[low]=a[j]; a[j]=t; g_mov_instr += 2;
        int depth=0; for (int k=j-low; k>1; k >>= 1) depth += 2;
        if (j-low > 1) ops_quick_impl(a, low, j-1, depth);
        depth=0; for (int k=high-j; k>1; k >>= 1) depth += 2;
        if (high-j > 1) ops_quick_impl(a, j+1, high, depth);
    }
    *cmp = g_cmp_instr; *mov = g_mov_instr; return 1;
}

#ifdef HAVE_SORTS_FASM
/* Counter twin for quicksort_fasm: exact pivot sample positions and partition
 * policy, but the counters measure algorithmic operations in C, not retired ASM
 * instructions. */
static void ops_quick_fasm_impl(int *a, int low, int high, int depth_limit) {
    while (low < high) {
        const int n = high - low + 1;
        if (n <= 24) { ops_insertion_range(a, low, high); return; }
        if (depth_limit <= 0) { ops_heap_sort_range(a + low, n); return; }
        --depth_limit;
        int x = a[low + n/4], y = a[low + n/2], z = a[low + (3*n)/4], t;
        ++g_cmp_instr; if (x > y) { t=x; x=y; y=t; }
        ++g_cmp_instr; if (y > z) { t=y; y=z; z=t; }
        ++g_cmp_instr; if (x > y) { t=x; x=y; y=t; }
        const int pivot = y;
        int i = low - 1, j = high + 1;
        for (;;) {
            do { ++i; ++g_cmp_instr; } while (a[i] < pivot);
            do { --j; ++g_cmp_instr; } while (a[j] > pivot);
            if (i >= j) break;
            t = a[i]; a[i] = a[j]; a[j] = t; g_mov_instr += 2;
        }
        if (j < low || j >= high) { ops_heap_sort_range(a + low, n); return; }
        const int left_size = j - low + 1, right_size = high - j;
        if (left_size > right_size) {
            if (j + 1 < high) ops_quick_fasm_impl(a, j + 1, high, depth_limit);
            high = j;
        } else {
            if (low < j) ops_quick_fasm_impl(a, low, j, depth_limit);
            low = j + 1;
        }
    }
}
static int ops_quick_fasm_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr=0; g_mov_instr=0;
    int depth=0; for(int k=n;k>1;k>>=1) depth += 2;
    if(n>1) ops_quick_fasm_impl(a,0,n-1,depth);
    *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}
/* FASM's two-thread entry performs a first-element Hoare split then invokes
 * QuickSort_FASM on each side. This count pass runs serially over same ranges. */
static int ops_quick2_fasm_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr=0; g_mov_instr=0;
    if (n > 1) {
        const int pivot=a[0]; int i=0, j=n;
        for (;;) {
            for (;;) {
                ++i;
                if (i > n-1) break;
                ++g_cmp_instr;
                if (!(a[i] < pivot)) break;
            }
            do { --j; ++g_cmp_instr; } while (a[j] > pivot);
            if (i >= j) break;
            const int t=a[i]; a[i]=a[j]; a[j]=t; g_mov_instr += 2;
        }
        const int t=a[0]; a[0]=a[j]; a[j]=t; g_mov_instr += 2;
        int depth=0; for(int k=j;k>1;k>>=1) depth+=2;
        if(j>1) ops_quick_fasm_impl(a,0,j-1,depth);
        depth=0; for(int k=n-1-j;k>1;k>>=1) depth+=2;
        if(n-1-j>1) ops_quick_fasm_impl(a,j+1,n-1,depth);
    }
    *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}

#endif /* HAVE_SORTS_FASM */

static void ops_merge_impl(int *a, int *tmp, int left, int right) {
    if (left >= right) return;
    const int mid = left + (right-left)/2;
    ops_merge_impl(a,tmp,left,mid); ops_merge_impl(a,tmp,mid+1,right);
    int i=left, j=mid+1, k=left;
    while (i<=mid && j<=right) {
        ++g_cmp_instr;
        if (a[i] <= a[j]) { tmp[k++]=a[i++]; ++g_mov_instr; }
        else { tmp[k++]=a[j++]; ++g_mov_instr; }
    }
    while (i<=mid) { tmp[k++]=a[i++]; ++g_mov_instr; }
    while (j<=right) { tmp[k++]=a[j++]; ++g_mov_instr; }
    for (i=left; i<=right; ++i) { a[i]=tmp[i]; ++g_mov_instr; }
}
static int ops_merge_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    int *tmp = (int *)malloc((size_t)n * sizeof(*tmp));
    if (!tmp && n > 0) return 0;
    g_cmp_instr = 0; g_mov_instr = 0;
    if (n > 1) ops_merge_impl(a,tmp,0,n-1);
    free(tmp); *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}
static int ops_parallel_merge_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    return ops_merge_i(a,n,cmp,mov);
}
static void ops_bitonic_merge(int *a, int low, int count, int dir) {
    if (count <= 1) return;
    const int half=count/2;
    for (int i=low; i<low+half; ++i) {
        ++g_cmp_instr;
        if (dir == (a[i] > a[i+half])) {
            const int t=a[i]; a[i]=a[i+half]; a[i+half]=t; g_mov_instr += 2;
        }
    }
    ops_bitonic_merge(a,low,half,dir); ops_bitonic_merge(a,low+half,half,dir);
}
static void ops_bitonic_sort_impl(int *a, int low, int count, int dir) {
    if (count <= 1) return;
    const int half=count/2;
    ops_bitonic_sort_impl(a,low,half,1); ops_bitonic_sort_impl(a,low+half,half,0);
    ops_bitonic_merge(a,low,count,dir);
}
static int ops_bitonic_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr=0; g_mov_instr=0; if (n>1) ops_bitonic_sort_impl(a,0,n,1);
    *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}
static int ops_tim_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    int *tmp=(int *)malloc((size_t)n*sizeof(*tmp));
    if (!tmp && n>0) return 0;
    g_cmp_instr=0; g_mov_instr=0;
    for (int start=0; start<n; start+=32) {
        const int end=(start+31<n-1)?start+31:n-1;
        ops_insertion_range(a,start,end);
    }
    for (int size=32; size<n; size*=2) {
        for (int left=0; left<n; left+=2*size) {
            const int mid=left+size-1;
            const int right=(left+2*size-1<n-1)?left+2*size-1:n-1;
            if (mid>=right) continue;
            int i=left,j=mid+1,k=left;
            while (i<=mid && j<=right) {
                ++g_cmp_instr;
                if (a[i]<=a[j]) { tmp[k++]=a[i++]; ++g_mov_instr; }
                else { tmp[k++]=a[j++]; ++g_mov_instr; }
            }
            while (i<=mid) { tmp[k++]=a[i++]; ++g_mov_instr; }
            while (j<=right) { tmp[k++]=a[j++]; ++g_mov_instr; }
            for (i=left;i<=right;++i) { a[i]=tmp[i]; ++g_mov_instr; }
        }
        if (size > n/2) break;
    }
    free(tmp); *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}
static void ops_parallel_quick_impl(int *a, int low, int high, int depth) {
    while (low < high) {
        const int n=high-low+1;
        if (n<=24) { ops_insertion_range(a,low,high); return; }
        if (depth<=0) { ops_heap_sort_range(a+low,n); return; }
        --depth;
        int x=a[low], y=a[low+(high-low)/2], z=a[high], t;
        ++g_cmp_instr; if(x>y){t=x;x=y;y=t;}
        ++g_cmp_instr; if(y>z){t=y;y=z;z=t;}
        ++g_cmp_instr; if(x>y){t=x;x=y;y=t;}
        const int pivot=y;
        int i=low,j=high;
        while(i<=j) {
            while(i<=high) { ++g_cmp_instr; if(a[i]<pivot) ++i; else break; }
            while(j>=low) { ++g_cmp_instr; if(a[j]>pivot) --j; else break; }
            if(i<=j){if(i!=j){t=a[i];a[i]=a[j];a[j]=t;g_mov_instr+=2;}++i;--j;}
        }
        const int left_size=(j>=low)?j-low+1:0;
        const int right_size=(i<=high)?high-i+1:0;
        if(left_size<right_size){if(left_size>1)ops_parallel_quick_impl(a,low,j,depth);low=i;}
        else {if(right_size>1)ops_parallel_quick_impl(a,i,high,depth);high=j;}
    }
}
static int ops_parallel_quick_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    g_cmp_instr=0; g_mov_instr=0; int depth=0; for(int k=n;k>1;k>>=1) depth+=2;
    if(n>1) ops_parallel_quick_impl(a,0,n-1,depth);
    *cmp=g_cmp_instr; *mov=g_mov_instr; return 1;
}

/* LSD radix sort: no key-to-key comparisons; move count is element writes to
 * the temporary histogram destination and copy-back arrays (4 byte passes). */
static int ops_radix_i(int *a, int n, uint64_t *cmp, uint64_t *mov) {
    int *tmp = n > 0 ? (int *)malloc((size_t)n * sizeof(*tmp)) : NULL;
    if (!tmp && n > 0) return 0;
    g_cmp_instr=0; g_mov_instr=0;
    for (int shift=0; shift<32 && n>1; shift+=8) {
        int count[256]={0};
        for (int i=0;i<n;++i) { const uint32_t key=((uint32_t)a[i]) ^ UINT32_C(0x80000000); ++count[(key>>shift)&UINT32_C(0xff)]; }
        for (int b=1;b<256;++b) count[b]+=count[b-1];
        for (int i=n-1;i>=0;--i) { const uint32_t key=((uint32_t)a[i]) ^ UINT32_C(0x80000000); const unsigned d=(unsigned)((key>>shift)&UINT32_C(0xff)); tmp[--count[d]]=a[i]; ++g_mov_instr; }
        for (int i=0;i<n;++i) { a[i]=tmp[i]; ++g_mov_instr; }
    }
    free(tmp); *cmp=0; *mov=g_mov_instr; return 1;
}
static void run_qsort_i(int *a, int n, int *tmp) { (void)tmp; qsort(a, (size_t)n, sizeof(*a), cmp_i32); }
static void run_qsort_u(uint64_t *a, int n, uint64_t *s, uint64_t len_s) { (void)s; (void)len_s; qsort(a, (size_t)n, sizeof(*a), cmp_u64); }
static void run_insertion_i(int *a, int n, int *tmp) { (void)tmp; insertion_sort_c(a, n); }
static void run_quick_i(int *a, int n, int *tmp) { (void)tmp; quicksort_c(a, 0, n - 1); }
static void run_quick2_i(int *a, int n, int *tmp) { (void)tmp; quicksort_two_threads_c(a, n); }
static void run_dual_i(int *a, int n, int *tmp) { (void)tmp; dual_pivot_quicksort_c(a, 0, n - 1); }
static void run_merge_i(int *a, int n, int *tmp) { merge_sort_c(a, tmp, 0, n - 1); }
static void run_heap_i(int *a, int n, int *tmp) { (void)tmp; heap_sort_c(a, n); }
static void run_radix_i(int *a, int n, int *tmp) { radix_sort_c(a, tmp, n); }
static void run_bitonic_i(int *a, int n, int *tmp) { (void)tmp; bitonic_sort_c(a, 0, n, 1); }
#ifdef __linux__
extern void sortbench_perf_worker_enter(void);
extern void sortbench_perf_worker_exit(void);
#endif
static void run_parallel_merge_i(int *a, int n, int *tmp) {
#ifdef _OPENMP
#pragma omp parallel
    {
        const int is_worker = omp_get_thread_num() != 0;
#ifdef __linux__
        if (is_worker) sortbench_perf_worker_enter();
#endif
#pragma omp barrier
#pragma omp single
        parallel_merge_sort_c(a, tmp, 0, n - 1);
#pragma omp barrier
#ifdef __linux__
        if (is_worker) sortbench_perf_worker_exit();
#endif
    }
#else
    parallel_merge_sort_c(a, tmp, 0, n - 1);
#endif
}
static void run_intro_i(int *a, int n, int *tmp) { (void)tmp; introsort_c(a, n); }
static void run_tim_i(int *a, int n, int *tmp) { timsort_c(a, tmp, n); }
static void run_pdq_i(int *a, int n, int *tmp) { (void)tmp; pdqsort_c(a, n); }
static void run_triple_i(int *a, int n, int *tmp) { (void)tmp; triple_pivot_quicksort_c(a, 0, n - 1); }
static void run_parallel_quick_i(int *a, int n, int *tmp) {
    (void)tmp;
#ifdef _OPENMP
#pragma omp parallel
    {
        const int is_worker = omp_get_thread_num() != 0;
#ifdef __linux__
        if (is_worker) sortbench_perf_worker_enter();
#endif
#pragma omp barrier
#pragma omp single
        parallel_quicksort_c(a, 0, n - 1);
#pragma omp barrier
#ifdef __linux__
        if (is_worker) sortbench_perf_worker_exit();
#endif
    }
#else
    parallel_quicksort_c(a, 0, n - 1);
#endif
}
static void run_cs3_i(int *a, int n, int *tmp) { (void)tmp; cs_sort_c(a, 0, n - 1); }
static void run_cs3_gs_i(int *a, int n, int *tmp) { (void)tmp; cs3_gs_sort_c(a, 0, n - 1); }
static void run_cs3_u64(uint64_t *a, int n, uint64_t *s, uint64_t len_s) {
    (void)s; (void)len_s; cs_sort_u64_c(a, 0, n - 1);
}
static void run_cs3_gs_u64(uint64_t *a, int n, uint64_t *s, uint64_t len_s) {
    (void)s; (void)len_s; cs3_gs_sort_u64_c(a, 0, n - 1);
}

#ifdef HAVE_SORTS_FASM
static void run_insertion_asm(int *a,int n,int *tmp) {(void)tmp; insertion_sort_fasm(a,n);}
static void run_quick_asm(int *a,int n,int *tmp) {(void)tmp; quicksort_fasm(a,0,n-1);}
static void run_quick2_asm(int *a,int n,int *tmp) {(void)tmp; quicksort_two_threads_fasm(a,n);}
static void run_dual_asm(int *a,int n,int *tmp) {(void)tmp; dual_pivot_quicksort_fasm(a,0,n-1);}
static void run_merge_asm(int *a,int n,int *tmp) {merge_sort_fasm(a,tmp,0,n-1);}
static void run_heap_asm(int *a,int n,int *tmp) {(void)tmp; heap_sort_fasm(a,n);}
static void run_radix_asm(int *a,int n,int *tmp) {radix_sort_fasm(a,tmp,n);}
static void run_bitonic_asm(int *a,int n,int *tmp) {(void)tmp; bitonic_sort_fasm(a,0,n,1);}
static void run_intro_asm(int *a,int n,int *tmp) {(void)tmp; introsort_fasm(a,n);}
static void run_tim_asm(int *a,int n,int *tmp) {timsort_fasm(a,tmp,n);}
static void run_pdq_asm(int *a,int n,int *tmp) {(void)tmp; pdqsort_fasm(a,n);}
static void run_triple_asm(int *a,int n,int *tmp) {(void)tmp; triple_pivot_quicksort_fasm(a,0,n-1);}
static void run_cs3_asm(int *a,int n,int *tmp) {(void)tmp; cs_sort_fasm(a,0,n-1);}
static void run_cs3_gs_asm(int *a,int n,int *tmp) {(void)tmp; cs3_gs_sort_fasm(a,0,n-1);}
static void run_cs3_u64_asm(uint64_t *a,int n,uint64_t *s,uint64_t ls) {(void)s;(void)ls;cs_sort_u64_fasm(a,0,n-1);}
static void run_cs3_gs_u64_asm(uint64_t *a,int n,uint64_t *s,uint64_t ls) {(void)s;(void)ls;cs3_gs_sort_u64_fasm(a,0,n-1);}
static void run_branchless_asm(int *a,int n,int *tmp) {(void)tmp; quicksort_branchless_fasm(a,0,n-1);}
#endif

/* ----- fast MyLib adapters (all compiled without g_cmp increments) ----- */
static void run_ml_cs(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs_sort_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_csfast(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs_sort_fast_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_cs2(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs2_sort_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_cs4(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs4_sort_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_cs2opt(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs2_opt_sort_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_cs4opt(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs4_opt_sort_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_cs2impr(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs2_opt_sort_impr_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_qs3(uint64_t *a,int n,uint64_t *s,uint64_t ls){(void)s;(void)ls;(void)qs3_sort_nocmp(a,n);}
static void run_ml_yar(uint64_t *a,int n,uint64_t *s,uint64_t ls){(void)s;(void)ls;(void)yar_sort_nocmp(a,n);}
static void run_ml_heap(uint64_t *a,int n,uint64_t *s,uint64_t ls){(void)s;(void)ls;heapsort_nocmp(a,n);}
static void run_ml_radix(uint64_t *a,int n,uint64_t *s,uint64_t ls){(void)s;(void)ls;radix_sort_nocmp(a,n);}
static void run_ml_par_orig(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs_sort_par_orig_nocmp(a,s,(uint64_t)n,ls);}
static void run_ml_par_fused(uint64_t *a,int n,uint64_t *s,uint64_t ls){cs_sort_par_fused_nocmp(a,s,(uint64_t)n,ls);}

/* ----- instrumented MyLib adapters: operation mode only ----- */
static void mylib_ops_reset(void) {
    __atomic_store_n(&g_cmp, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_mov_mylib, 0, __ATOMIC_RELAXED);
}
static void mylib_ops_capture(uint64_t *cmp, uint64_t *mov) {
    *cmp = (uint64_t)__atomic_load_n(&g_cmp, __ATOMIC_RELAXED);
    *mov = __atomic_load_n(&g_mov_mylib, __ATOMIC_RELAXED);
}
static int ops_ml_cs(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_cs2(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs2_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_cs4(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs4_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_cs2opt(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs2_opt_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_cs4opt(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs4_opt_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_cs2impr(uint64_t *a,int n,uint64_t*s,uint64_t ls,uint64_t *cmp,uint64_t *mov){mylib_ops_reset();cs2_opt_sort_impr(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_qs3(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){(void)s;(void)ls;mylib_ops_reset();(void)qs3_sort(a,n);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_yar(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){(void)s;(void)ls;mylib_ops_reset();(void)yar_sort(a,n);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_heap(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){(void)s;(void)ls;mylib_ops_reset();heapsort(a,n);mylib_ops_capture(cmp,mov);return 1;}
/* The fast timing twin has the same CS partition path; use the instrumented
 * base CS implementation to obtain reproducible comparison/move counts. */
static int ops_ml_csfast(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){mylib_ops_reset();cs_sort(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_radix(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){
    (void)s;(void)ls;
    uint64_t *tmp=n>0?(uint64_t*)malloc((size_t)n*sizeof(*tmp)):NULL;
    if(n>0&&!tmp)return 0;
    uint64_t *src=a,*dst=tmp,writes=0; uint32_t count[256];
    for(int shift=0;shift<64&&n>1;shift+=8){
        memset(count,0,sizeof(count));
        for(int i=0;i<n;++i)++count[(src[i]>>shift)&UINT64_C(0xff)];
        uint32_t sum=0; for(int k=0;k<256;++k){uint32_t v=count[k];count[k]=sum;sum+=v;}
        for(int i=0;i<n;++i){unsigned d=(unsigned)((src[i]>>shift)&UINT64_C(0xff));dst[count[d]++]=src[i];++writes;}
        uint64_t *swap=src;src=dst;dst=swap;
    }
    if(src!=a)for(int i=0;i<n;++i){a[i]=src[i];++writes;}
    free(tmp); *cmp=0; *mov=writes; return 1;
}
static int ops_ml_par_orig(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){mylib_ops_reset();cs_sort_par_orig(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}
static int ops_ml_par_fused(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*cmp,uint64_t*mov){mylib_ops_reset();cs_sort_par_fused(a,s,(uint64_t)n,ls);mylib_ops_capture(cmp,mov);return 1;}

/* ----- instrumented C adapters; operation semantics match sorts_instrumented.c ----- */
static int ops_cs3_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;cs_sort_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_cs3_gs_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;cs3_gs_sort_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_cs3_u64(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*c,uint64_t*m){(void)s;(void)ls;g_cmp_instr=0;g_mov_instr=0;cs_sort_u64_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_cs3_gs_u64(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*c,uint64_t*m){(void)s;(void)ls;g_cmp_instr=0;g_mov_instr=0;cs3_gs_sort_u64_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_dual_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;dual_pivot_quicksort_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_intro_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;introsort_c_instr(a,n);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_pdq_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;pdqsort_c_instr(a,n);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_triple_i(int*a,int n,uint64_t*c,uint64_t*m){g_cmp_instr=0;g_mov_instr=0;triple_pivot_quicksort_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
#ifdef HAVE_SORTS_FASM
/* U64 count adapters call the matching instrumented C core deliberately.
 * Timed execution remains the native FASM implementation. */
static int ops_cs3_u64_asm(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*c,uint64_t*m){(void)s;(void)ls;g_cmp_instr=0;g_mov_instr=0;cs_sort_u64_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
static int ops_cs3_gs_u64_asm(uint64_t*a,int n,uint64_t*s,uint64_t ls,uint64_t*c,uint64_t*m){(void)s;(void)ls;g_cmp_instr=0;g_mov_instr=0;cs3_gs_sort_u64_c_instr(a,0,n-1);*c=g_cmp_instr;*m=g_mov_instr;return 1;}
#endif

/*
 * ОТКУДА ВЗЯТЫ АЛГОРИТМЫ (поле source в CSV):
 *   libc / stdlib.h             — системная библиотека C, эталонный qsort.
 *   sorts_c.c                   — реализации алгоритмов на языке C.
 *   sorts.asm                   — реализации на FASM (x86-64).
 *   mylib.c                     — исходные версии алгоритмов семейства CS/Artamonov.
 *   mylib_fast.c                — варианты mylib без накладных расходов счётчика g_cmp.
 *   sorts_instrumented.c/.asm   — инструментированные версии для режима 11.2;
 *                                 в режиме 11.1 они не используются.
 */
static const Algorithm algorithms[] = {
    /* Стандартная библиотека C (libc): эталонная сортировка qsort. */
    {"libc_qsort_int", "libc / stdlib.h", run_qsort_i, NULL, ops_qsort_i, NULL, 0, 0, "reference baseline; counts both relational tests in the qsort comparator"},
    {"libc_qsort_u64", "libc / stdlib.h", NULL, run_qsort_u, NULL, ops_qsort_u, 0, 0, "reference baseline; counts both relational tests in the qsort comparator"},

    /* C-библиотека проекта: реализации из sorts_c.c. */
    {"InsertionSort_C", "sorts_c.c", run_insertion_i, NULL, ops_insertion_i, NULL, 8192, 0, "quadratic; counts key comparisons and array writes; skipped above 8192"},
    {"QuickSort_C", "sorts_c.c", run_quick_i, NULL, ops_quick_i, NULL, 0, 0, "median-of-three introspective twin; timing uses production version"},
    {"QuickSort_2Threads_C", "sorts_c.c", run_quick2_i, NULL, ops_quick2_i, NULL, 0, 0, "serial count pass over the same initial partition and recursive sorts"},
    {"DualPivot_C", "sorts_c.c", run_dual_i, NULL, ops_dual_i, NULL, 0, 0, "instrumented twin for ops mode"},
    {"MergeSort_C", "sorts_c.c", run_merge_i, NULL, ops_merge_i, NULL, 0, 0, "count pass allocates scratch; timing uses production version"},
    {"HeapSort_C", "sorts_c.c", run_heap_i, NULL, ops_heap_i, NULL, 0, 0, "comparison-count twin; timing uses production version"},
    {"RadixSort_C", "sorts_c.c", run_radix_i, NULL, ops_radix_i, NULL, 0, 0, "4-pass signed int32 radix; key comparisons=0; counts scratch/result writes"},
    {"BitonicSort_C", "sorts_c.c", run_bitonic_i, NULL, ops_bitonic_i, NULL, 0, 1, "comparison-count twin; requires power-of-two N"},
    {"ParallelMergeSort_C", "sorts_c.c", run_parallel_merge_i, NULL, ops_parallel_merge_i, NULL, 0, 0, "same merge comparison count; timing uses parallel production version"},
    {"IntroSort_C", "sorts_c.c", run_intro_i, NULL, ops_intro_i, NULL, 0, 0, "instrumented twin for ops mode"},
    {"TimSort_C", "sorts_c.c", run_tim_i, NULL, ops_tim_i, NULL, 0, 0, "RUN=32 insertion runs plus merge count; timing uses production version"},
    {"PDQSort_C", "sorts_c.c", run_pdq_i, NULL, ops_pdq_i, NULL, 0, 0, "instrumented twin for ops mode"},
    {"TriplePivot_C", "sorts_c.c", run_triple_i, NULL, ops_triple_i, NULL, 0, 0, "instrumented twin for ops mode"},
    {"ParallelQuickSort_C", "sorts_c.c", run_parallel_quick_i, NULL, ops_parallel_quick_i, NULL, 0, 0, "serial count twin; timing uses OpenMP production version"},
    {"CS3_ThreePivots_C", "sorts_c.c", run_cs3_i, NULL, ops_cs3_i, NULL, 0, 0, "instrumented CS3 twin for ops mode"},
    {"CS3_GS_ThreePivots_C", "sorts_c.c", run_cs3_gs_i, NULL, ops_cs3_gs_i, NULL, 0, 0, "Artamonov-generated value thresholds; then three-pivot CS3; instrumented counts"},
    {"CS3_ThreePivots_U64_C", "sorts_c.c + sorts_instrumented.c", NULL, run_cs3_u64, NULL, ops_cs3_u64, 0, 0, "CS3 native uint64_t keys; instrumented twin counts key comparisons and element moves"},
    {"CS3_GS_ThreePivots_U64_C", "sorts_c.c + sorts_instrumented.c", NULL, run_cs3_gs_u64, NULL, ops_cs3_gs_u64, 0, 0, "CS3-GS native uint64_t keys; generated-threshold comparisons and element moves counted"},

#ifdef HAVE_SORTS_FASM
    /* FASM-библиотека проекта: реализации из sorts.asm. */
    {"InsertionSort_FASM", "sorts.asm", run_insertion_asm, NULL, ops_insertion_i, NULL, 8192, 0, "timing=FASM; operations from instrumented C insertion twin; skipped above 8192"},
    {"QuickSort_FASM", "sorts.asm", run_quick_asm, NULL, ops_quick_fasm_i, NULL, 0, 0, "timing=FASM; operation twin mirrors quarter/middle/three-quarter pivot sampling"},
    {"QuickSort_2Threads_FASM", "sorts.asm", run_quick2_asm, NULL, ops_quick2_fasm_i, NULL, 0, 0, "timing=FASM; counts first-pivot split and QuickSort-FASM twin serially"},
    {"DualPivot_FASM", "sorts.asm", run_dual_asm, NULL, ops_dual_i, NULL, 0, 0, "timing=FASM; operation counts from instrumented C dual-pivot twin"},
    {"MergeSort_FASM", "sorts.asm", run_merge_asm, NULL, ops_merge_i, NULL, 0, 0, "timing=FASM; counts comparisons and scratch/result array writes using merge twin"},
    {"HeapSort_FASM", "sorts.asm", run_heap_asm, NULL, ops_heap_i, NULL, 0, 0, "timing=FASM; operation counts from instrumented C heap twin"},
    {"RadixSort_FASM", "sorts.asm", run_radix_asm, NULL, ops_radix_i, NULL, 0, 0, "timing=FASM; 4-pass signed int32 radix twin; key comparisons=0"},
    {"BitonicSort_FASM", "sorts.asm", run_bitonic_asm, NULL, ops_bitonic_i, NULL, 0, 1, "timing=FASM; counted bitonic network; requires power-of-two N"},
    {"IntroSort_FASM", "sorts.asm", run_intro_asm, NULL, ops_intro_i, NULL, 0, 0, "timing=FASM; operation counts from C Introsort twin, not native ASM counters"},
    {"TimSort_FASM", "sorts.asm", run_tim_asm, NULL, ops_tim_i, NULL, 0, 0, "timing=FASM; operation counts from RUN=32 insertion/merge twin"},
    {"PDQSort_FASM", "sorts.asm", run_pdq_asm, NULL, ops_pdq_i, NULL, 0, 0, "timing=FASM; operation counts from C PDQSort twin, not native ASM counters"},
    {"TriplePivot_FASM", "sorts.asm", run_triple_asm, NULL, ops_triple_i, NULL, 0, 0, "timing=FASM; operation counts from C TriplePivot twin, not native ASM counters"},
    {"BranchlessQuickSort_FASM", "sorts.asm", run_branchless_asm, NULL, ops_quick_fasm_i, NULL, 0, 0, "current public FASM entry jumps to quicksort_fasm; same count twin"},
    {"CS3_ThreePivots_FASM", "sorts.asm", run_cs3_asm, NULL, ops_cs3_i, NULL, 0, 0, "timing=production FASM CS3; operations from instrumented C CS3 core"},
    {"CS3_GS_ThreePivots_FASM", "sorts.asm", run_cs3_gs_asm, NULL, ops_cs3_gs_i, NULL, 0, 0, "timing=FASM threshold partition; operations from instrumented C CS3-GS core"},
    {"CS3_ThreePivots_U64_FASM", "sorts.asm + sorts_instrumented.asm", NULL, run_cs3_u64_asm, NULL, ops_cs3_u64_asm, 0, 0, "native uint64_t CS3 assembly; counts use matching C instrumented twin"},
    {"CS3_GS_ThreePivots_U64_FASM", "sorts.asm + sorts_instrumented.asm", NULL, run_cs3_gs_u64_asm, NULL, ops_cs3_gs_u64_asm, 0, 0, "native uint64_t CS3-GS assembly; counts use matching C instrumented twin"},
    // {"CS3_LegacySinglePivot_FASM_instr", "sorts_instrumented.asm", NULL, NULL, ops_cs3_asm, NULL, 0, 0, "legacy single-pivot instrumented routine; NOT the production cs_sort_fasm"},
#endif

    /* MyLib: CS/Artamonov и родственные алгоритмы из mylib.c/mylib_fast.c. */
    {"mylib.cs_sort", "mylib.c", NULL, run_ml_cs, NULL, ops_ml_cs, 0, 0, "generating-sequence sort; counts key comparisons and array-slot writes"},
    {"mylib.cs_sort_fast", "mylib.c (fast twin)", NULL, run_ml_csfast, NULL, ops_ml_csfast, 0, 0, "counts use instrumented base-CS twin for comparisons and array writes (proxy for fast variant)"},
    {"mylib.cs2_sort", "mylib.c", NULL, run_ml_cs2, NULL, ops_ml_cs2, 0, 0, "two-bucket CS variant; counts comparisons and array-slot writes"},
    {"mylib.cs4_sort", "mylib.c", NULL, run_ml_cs4, NULL, ops_ml_cs4, 0, 0, "four-bucket CS variant; counts comparisons and array-slot writes"},
    {"mylib.cs2_opt_sort", "mylib.c", NULL, run_ml_cs2opt, NULL, ops_ml_cs2opt, 0, 0, "optimized CS2; counts comparisons and array-slot writes"},
    {"mylib.cs4_opt_sort", "mylib.c", NULL, run_ml_cs4opt, NULL, ops_ml_cs4opt, 0, 0, "optimized CS4; counts comparisons and array-slot writes"},
    {"mylib.cs2_opt_sort_impr", "mylib.c", NULL, run_ml_cs2impr, NULL, ops_ml_cs2impr, 0, 0, "improved optimized CS2; counts comparisons and array-slot writes"},
    {"mylib.qs3_sort", "mylib.c", NULL, run_ml_qs3, NULL, ops_ml_qs3, 0, 0, "three-way quicksort; counts comparisons and array-slot writes"},
    {"mylib.yar_sort", "mylib.c", NULL, run_ml_yar, NULL, ops_ml_yar, 0, 0, "Yaroslavskiy dual-pivot; counts comparisons and array-slot writes"},
    {"mylib.heapsort", "mylib.c", NULL, run_ml_heap, NULL, ops_ml_heap, 0, 0, ""},
    {"mylib.radix_sort", "mylib.c (no-counter build)", NULL, run_ml_radix, NULL, ops_ml_radix, 0, 0, "8-pass uint64 radix; key comparisons=0, counts element writes; moves=write count"},
    {"mylib.cs_sort_par_orig", "mylib.c (instrumented atomic counter)", NULL, run_ml_par_orig, NULL, ops_ml_par_orig, 0, 0, "parallel comparisons and array-slot writes use atomic counters"},
    {"mylib.cs_sort_par_fused", "mylib.c (instrumented atomic counter)", NULL, run_ml_par_fused, NULL, ops_ml_par_fused, 0, 0, "parallel comparisons and array-slot writes use atomic counters"}
};

static const Dataset datasets[] = {
    {"random_nonnegative_int", 0, 0},
    {"sorted_ascending", 0, 0},
    {"sorted_reverse", 0, 0},
    {"almost_sorted_1pct_swaps", 0, 0},
    {"PipeOrgan", 0, 0},
    {"m3Killer", 0, 0},
    {"PipeOrganMX", 0, 0},
    {"UnevenSeq", 0, 0},
    {"UnevenSorted", 0, 0},
    {"KillerMag20", 0, 0},
    {"random_bytes_0_255", 0, 0},
    {"repeated_64_keys", 0, 0},
    {"repeated_4_keys", 0, 0},
    {"Rep9010_90pct_same", 0, 0},
    {"binary_two_keys", 0, 0},
    {"ternary_three_keys", 0, 0},
    {"quintary_five_keys", 0, 0},
    {"Artamonov_multiplicity_shuffled", 0, 0},
    {"CS_range_killer_int32", 0, 0},
    /* Wide killers are tested in original uint64_t form AND as exact ranks
       for the int32-only algorithms. No mask/modulo is applied to MyLib keys. */
    {"CS_Sort_Killer_wide_u64", 1, 1},
    {"KillerMag62_wide_u64", 1, 1},
    {"random_wide_u64", 1, 1},
    {"high_bits_sparse_u64", 1, 1}
};

static void shuffle_int(int *a, int n) {
    for (int i = n - 1; i > 0; --i) {
        const int j = (int)(rnd32() % (uint32_t)(i + 1));
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void generate_int_dataset(const char *name, int *a, int n, double c) {
    rng_state = 0x51ed270bu;
    if (strcmp(name, "random_nonnegative_int") == 0) {
        for (int i=0;i<n;i++) a[i]=(int)(rnd32() & (uint32_t)INT_MAX);
    } else if (strcmp(name, "sorted_ascending") == 0) {
        for (int i=0;i<n;i++) a[i]=i;
    } else if (strcmp(name, "sorted_reverse") == 0) {
        for (int i=0;i<n;i++) a[i]=n-i;
    } else if (strcmp(name, "almost_sorted_1pct_swaps") == 0) {
        for (int i=0;i<n;i++) a[i]=i;
        const int swaps = n / 100;
        for (int i=0;i<swaps;i++) { int x=(int)(rnd32()%(uint32_t)n), y=(int)(rnd32()%(uint32_t)n); int t=a[x];a[x]=a[y];a[y]=t; }
    } else if (strcmp(name, "PipeOrgan") == 0) {
        uint64_t *u=malloc((size_t)n*sizeof(*u)); if(!u) exit(2); killer_pipeorgan(u,n); if(!copy_u64_to_int_lossless_order(u,a,n)) { free(u); fprintf(stderr,"rank compression failed\n"); exit(2); } free(u);
    } else if (strcmp(name, "m3Killer") == 0) {
        uint64_t *u=malloc((size_t)n*sizeof(*u)); if(!u) exit(2); killer_m3killer(u,n); if(!copy_u64_to_int_lossless_order(u,a,n)) { free(u); fprintf(stderr,"rank compression failed\n"); exit(2); } free(u);
    } else if (strcmp(name, "PipeOrganMX") == 0 || strcmp(name, "UnevenSeq") == 0 ||
               strcmp(name, "UnevenSorted") == 0 || strcmp(name, "KillerMag20") == 0) {
        uint64_t *u = malloc((size_t)n * sizeof(*u));
        if (!u) exit(2);
        killer_set_seed(42);
        if (strcmp(name, "PipeOrganMX") == 0) {
            if (n == 1) u[0] = 0; else killer_pipeorganmx(u, n, 1000000ULL);
        } else if (strcmp(name, "UnevenSeq") == 0) {
            killer_unevenseq(u, n, 1000000ULL);
        } else if (strcmp(name, "UnevenSorted") == 0) {
            killer_unevensorted(u, n, 1000000ULL);
        } else {
            killer_mag(u, n, 20);
        }
        if (!copy_u64_to_int_lossless_order(u, a, n)) {
            free(u); fprintf(stderr, "rank compression failed\n"); exit(2);
        }
        free(u);
    } else if (strcmp(name, "random_bytes_0_255") == 0) {
        for(int i=0;i<n;i++)a[i]=(int)(rnd32()%256u);
    } else if (strcmp(name, "repeated_64_keys") == 0) {
        for(int i=0;i<n;i++)a[i]=(int)(rnd32()%64u);
    } else if (strcmp(name, "repeated_4_keys") == 0) {
        for(int i=0;i<n;i++)a[i]=(int)(rnd32()%4u);
    } else if (strcmp(name, "Rep9010_90pct_same") == 0) {
        for(int i=0;i<n;i++)a[i]=(rnd32()%100u<90u)?(INT_MAX/2):(int)(rnd32()%(uint32_t)INT_MAX);
    } else if (strcmp(name, "binary_two_keys") == 0) {
        for(int i=0;i<n;i++)a[i]=(rnd32()&1u)?0:INT_MAX;
    } else if (strcmp(name, "ternary_three_keys") == 0) {
        static const int v[3]={10,5000,1000000}; for(int i=0;i<n;i++)a[i]=v[rnd32()%3u];
    } else if (strcmp(name, "quintary_five_keys") == 0) {
        static const int v[5]={10,100,500,1000,5000}; for(int i=0;i<n;i++)a[i]=v[rnd32()%5u];
    } else if (strcmp(name, "Artamonov_multiplicity_shuffled") == 0) {
        uint64_t *s=NULL,len=0; get_sequence((uint64_t)n,c,&s,&len); int out=0;
        for(uint64_t k=0;k<len;k++) for(uint64_t count=0;count<s[k] && out<n;count++) a[out++]=(int)(k+1);
        while (out < n) a[out++] = (int)(len + 1);
        free(s);
        shuffle_int(a, n);
    } else if (strcmp(name, "CS_range_killer_int32") == 0) {
        for(int i=0;i<n;i++)a[i]=(int)(INT_MAX/2 - 31 + (int)(rnd32()%63u));
        if(n>0)a[rnd32()%(uint32_t)n]=0;
        if(n>1)a[rnd32()%(uint32_t)n]=INT_MAX;
        if(n>3){a[n/3]=0;a[(2*n)/3]=INT_MAX;}
    }
}

/* Generate U64-backed killers once, then derive the int view from exactly
 * those keys. This keeps the MyLib input's original spread (including values
 * above 2^53 / INT_MAX) instead of reconstructing it from compressed ranks. */
static int generate_u64_backed_int_dataset(const char *name, uint64_t *a, int n, double c) {
    (void)c;
    if (strcmp(name, "PipeOrgan") == 0) {
        killer_set_seed(42);
        killer_pipeorgan(a, n);
    } else if (strcmp(name, "m3Killer") == 0) {
        killer_set_seed(42);
        killer_m3killer(a, n);
    } else if (strcmp(name, "PipeOrganMX") == 0) {
        killer_set_seed(42);
        if (n == 1) a[0] = 0; else killer_pipeorganmx(a, n, 1000000ULL);
    } else if (strcmp(name, "UnevenSeq") == 0) {
        killer_set_seed(42);
        killer_unevenseq(a, n, 1000000ULL);
    } else if (strcmp(name, "UnevenSorted") == 0) {
        killer_set_seed(42);
        killer_unevensorted(a, n, 1000000ULL);
    } else if (strcmp(name, "KillerMag20") == 0) {
        killer_set_seed(42);
        killer_mag(a, n, 20);
    } else {
        return 0;
    }
    return 1;
}

static int u64_needs_rank_view(const uint64_t *a, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] > (uint64_t)INT_MAX) return 1;
    }
    return 0;
}

static void generate_u64_only(const char *name, uint64_t *a, int n) {
    rng_state = 0x6d2b79f5u;
    if (strcmp(name, "CS_Sort_Killer_wide_u64") == 0) {
        killer_set_seed(42); killer_cs_sort(a,n);
    } else if (strcmp(name, "KillerMag62_wide_u64") == 0) {
        killer_mag(a, n, 62);
    } else if (strcmp(name, "random_wide_u64") == 0) {
        for(int i=0;i<n;i++) a[i]=(((uint64_t)rnd32()<<32)|rnd32());
    } else if (strcmp(name, "high_bits_sparse_u64") == 0) {
        for(int i=0;i<n;i++){ uint32_t r=rnd32(); unsigned shift=(unsigned)(r%63u); a[i]=(1ULL<<shift) | ((uint64_t)(rnd32()&0xffffu)); }
        if(n>0)a[0]=0;
        if(n>1)a[1]=UINT64_MAX;
    }
}

static uint64_t encode_int_for_mylib(int x, int needs_offset) {
    if (!needs_offset) return (uint64_t)(uint32_t)x;
    return (uint64_t)((int64_t)x - (int64_t)INT_MIN);
}
static int is_signed_dataset(const char *name) { return strcmp(name,"signed32_random") == 0 || strcmp(name,"signed32_edges") == 0; }
static void generate_signed_dataset(const char *name,int *a,int n) {
    rng_state=0xf00dcafeu;
    if(strcmp(name,"signed32_random")==0){for(int i=0;i<n;i++)a[i]=(int)rnd32();}
    else {for(int i=0;i<n;i++)a[i]=(i&1)?INT_MIN:INT_MAX; if(n>2){a[n/2]=0;a[n/3]=-1;}}
}


#ifndef SORT_COMPARE_EMBEDDED
static void print_usage(const char *exe) {
    printf("Usage: %s --mode perf|ops [--n N] [--repeats R] [--c 2.0..3.0] [--case NAME|all] [--output FILE.csv]\n", exe);
    printf("Modes: perf = elapsed time/TSC/IPC/IPS/BrMis (11.1); ops = comparisons/moves plus instrumented runtime (11.2).\n");
    printf("Default: --mode perf --n 16384 --repeats 7 --c 2.0 --case all\n");
    printf("Cases: random_nonnegative_int, sorted_ascending, sorted_reverse, almost_sorted_1pct_swaps,\n");
    printf("       PipeOrgan, m3Killer, PipeOrganMX, UnevenSeq, UnevenSorted, KillerMag20,\n");
    printf("       random_bytes_0_255, repeated_64_keys, repeated_4_keys, binary_two_keys,\n");
    printf("       ternary_three_keys, quintary_five_keys, Rep9010_90pct_same,\n");
    printf("       Artamonov_multiplicity_shuffled, CS_range_killer_int32, CS_Sort_Killer_wide_u64,\n");
    printf("       KillerMag62_wide_u64, random_wide_u64, high_bits_sparse_u64\n");
    printf("Wide uint64_t cases also run int32 algorithms on exact order-preserving ranks.\n");
    printf("       signed32_random, signed32_edges\n");
}
#endif

static int matches_reference_i(int *a, const int *expected, int n) { return same_int(a, expected, n); }
static int matches_reference_u(uint64_t *a, const uint64_t *expected, int n) { return same_u64(a, expected, n); }


/* Append a qsort-relative speedup column to either CSV. A speedup > 1 means
 * faster than qsort. Baselines are matched by (dataset, key domain), so int32,
 * rank-int32 and uint64 experiments are never compared across key widths. */
typedef struct {
    char *line;
    char dataset[128];
    char algorithm[128];
    char domain[32];
    char status[64];
    uint64_t elapsed_ns;
    int has_time;
} CsvSpeedRow;
typedef struct { char dataset[128]; char domain[32]; uint64_t ns; } QsortBaseline;

static int csv_field(const char *line, int wanted, char *out, size_t cap) {
    int field = 0; size_t used = 0; int quoted = 0;
    if (!line || !out || cap == 0 || wanted < 0) return 0;
    out[0] = '\0';
    for (const char *p=line; ; ++p) {
        const char ch=*p;
        if (ch=='"') {
            if (quoted && p[1]=='"') { if(field==wanted && used+1<cap) out[used++]='"'; ++p; }
            else quoted=!quoted;
        } else if ((ch==',' && !quoted) || ch=='\0' || ch=='\n' || ch=='\r') {
            if (field==wanted) { out[used]='\0'; return 1; }
            if (ch=='\0' || ch=='\n' || ch=='\r') return 0;
            ++field; used=0;
        } else if(field==wanted && used+1<cap) out[used++]=ch;
    }
}
static int append_qsort_speedup_column(const char *path) {
    FILE *in=fopen(path,"r"); if(!in) return 0;
    char *line=NULL; size_t linecap=0; ssize_t got=getline(&line,&linecap,in);
    if(got<0){free(line);fclose(in);return 0;}
    CsvSpeedRow *rows=NULL; size_t count=0, capacity=0;
    QsortBaseline bases[256]; size_t nb=0;
    while((got=getline(&line,&linecap,in))>=0) {
        if(count==capacity){size_t next=capacity?capacity*2:128; CsvSpeedRow *p=realloc(rows,next*sizeof(*rows));if(!p){free(rows);free(line);fclose(in);return 0;}rows=p;capacity=next;}
        CsvSpeedRow *r=&rows[count]; memset(r,0,sizeof(*r));
        r->line=strdup(line); if(!r->line){for(size_t i=0;i<count;i++)free(rows[i].line);free(rows);free(line);fclose(in);return 0;}
        char field[512];
        csv_field(line,1,r->dataset,sizeof(r->dataset));
        csv_field(line,4,r->algorithm,sizeof(r->algorithm));
        csv_field(line,6,r->domain,sizeof(r->domain));
        csv_field(line,0,field,sizeof(field));
        const int status_col=strcmp(field,"perf")==0?19:14;
        csv_field(line,status_col,r->status,sizeof(r->status));
        if(csv_field(line,10,field,sizeof(field)) && strcmp(field,"NA")!=0) {
            char *end=NULL; unsigned long long v=strtoull(field,&end,10);
            if(end && *end=='\0'){r->elapsed_ns=(uint64_t)v;r->has_time=1;}
        }
        if ((strcmp(r->algorithm,"libc_qsort_int")==0 || strcmp(r->algorithm,"libc_qsort_u64")==0) && r->has_time && strcmp(r->status,"OK")==0) {
            size_t b=0; while(b<nb && (strcmp(bases[b].dataset,r->dataset)!=0 || strcmp(bases[b].domain,r->domain)!=0)) ++b;
            if(b==nb && nb<sizeof(bases)/sizeof(bases[0])) {snprintf(bases[nb].dataset,sizeof(bases[nb].dataset),"%s",r->dataset);snprintf(bases[nb].domain,sizeof(bases[nb].domain),"%s",r->domain);++nb;}
            if(b<nb) bases[b].ns=r->elapsed_ns;
        }
        ++count;
    }
    free(line); fclose(in);
    size_t tlen=strlen(path)+32; char *tmp_path=malloc(tlen); if(!tmp_path){for(size_t i=0;i<count;i++)free(rows[i].line);free(rows);return 0;}
    snprintf(tmp_path,tlen,"%s.qsort.tmp",path);
    FILE *out=fopen(tmp_path,"w"); if(!out){free(tmp_path);for(size_t i=0;i<count;i++)free(rows[i].line);free(rows);return 0;}
    /* The original header was not retained; the mode has a fixed schema. */
    const int is_perf = count>0 && strncmp(rows[0].line,"perf,",5)==0;
    (void)is_perf;
    /* Re-read the original header through the known schema text. */
    if (count>0 && strncmp(rows[0].line,"perf,",5)==0) {
        fputs("Mode,Dataset,N,C,Algorithm,Source,Domain,Median_TSC_Ticks,MAD_TSC_Ticks,Ticks_Per_Element,Median_Elapsed_ns,Median_Elapsed_ms,IPC,IPS_MInstr_Per_Sec,BrMis_Count,BrMis_Rate_pct,HW_Thread_Contexts,Hardware_Scope,Repeats,Status,Note,Speedup_vs_qsort\n",out);
    } else {
        fputs("Mode,Dataset,N,C,Algorithm,Source,Domain,Comparisons,Moves_G_MOV,Comparisons_Per_Element,Median_Elapsed_ns,Median_Elapsed_ms,Elapsed_ns_Per_Element,Repeats,Status,Note,Speedup_vs_qsort\n",out);
    }
    for(size_t i=0;i<count;i++) {
        CsvSpeedRow *r=&rows[i]; double speed=0.0; int found=0;
        if(r->has_time && (strcmp(r->status,"OK")==0 || strcmp(r->status,"OK_NO_COUNTER")==0)) {
            for(size_t b=0;b<nb;b++) if(strcmp(bases[b].dataset,r->dataset)==0 && strcmp(bases[b].domain,r->domain)==0 && bases[b].ns>0) {speed=(double)bases[b].ns/(double)r->elapsed_ns;found=1;break;}
        }
        size_t len=strlen(r->line); while(len>0 && (r->line[len-1]=='\n'||r->line[len-1]=='\r')) r->line[--len]='\0';
        fprintf(out,"%s,",r->line);
        if(found) fprintf(out,"%.5f\n",speed); else fputs("NA\n",out);
    }
    int write_ok=(fclose(out)==0);
    if(write_ok && rename(tmp_path,path)!=0) write_ok=0;
    if(!write_ok) remove(tmp_path);
    free(tmp_path); for(size_t i=0;i<count;i++)free(rows[i].line);free(rows);
    return write_ok;
}

static int algorithm_perf_scope(const char *name) {
    if (strcmp(name, "mylib.cs_sort_par_orig") == 0 ||
        strcmp(name, "mylib.cs_sort_par_fused") == 0)
        return PERF_SCOPE_MYLIB_WORKER_HOOKS;
    if (strcmp(name, "ParallelMergeSort_C") == 0 ||
        strcmp(name, "ParallelQuickSort_C") == 0)
        return PERF_SCOPE_OPENMP_WORKER_HOOKS;
    if (strcmp(name, "QuickSort_2Threads_C") == 0 ||
        strcmp(name, "QuickSort_2Threads_FASM") == 0)
        return PERF_SCOPE_INHERITED_CHILDREN;
    return PERF_SCOPE_CALLER_THREAD;
}

static const char *perf_scope_label(int scope_mode) {
    switch (scope_mode) {
        case PERF_SCOPE_EXISTING_PROCESS_THREADS: return "all_existing_process_threads";
        case PERF_SCOPE_INHERITED_CHILDREN: return "caller+inherited_child_threads";
        case PERF_SCOPE_MYLIB_WORKER_HOOKS: return "caller+recursive_mylib_worker_hooks";
        case PERF_SCOPE_OPENMP_WORKER_HOOKS: return "caller+OpenMP_team_worker_hooks";
        default: return "caller_thread";
    }
}

static int run_perf(const char *output, int n, int reps, double c, const char *case_filter) {
    FILE *csv = fopen(output, "w");
    if (!csv) { fprintf(stderr, "cannot open %s: %s\n", output, strerror(errno)); return 2; }
    fprintf(csv,
        "Mode,Dataset,N,C,Algorithm,Source,Domain,Median_TSC_Ticks,MAD_TSC_Ticks,"
        "Ticks_Per_Element,Median_Elapsed_ns,Median_Elapsed_ms,IPC,IPS_MInstr_Per_Sec,"
        "BrMis_Count,BrMis_Rate_pct,HW_Thread_Contexts,Hardware_Scope,Repeats,Status,Note\n");

    int *src_i = malloc((size_t)n * sizeof(*src_i));
    int *work_i = malloc((size_t)n * sizeof(*work_i));
    int *tmp_i = malloc((size_t)n * sizeof(*tmp_i));
    int *expected_i = malloc((size_t)n * sizeof(*expected_i));
    uint64_t *src_u = malloc((size_t)n * sizeof(*src_u));
    uint64_t *work_u = malloc((size_t)n * sizeof(*work_u));
    uint64_t *expected_u = malloc((size_t)n * sizeof(*expected_u));
    uint64_t *s = NULL, len_s = 0;
    uint64_t *times = malloc((size_t)reps * sizeof(*times));
    uint64_t *dev = malloc((size_t)reps * sizeof(*dev));
    uint64_t *elapsed = malloc((size_t)reps * sizeof(*elapsed));
    uint64_t *br_misses = malloc((size_t)reps * sizeof(*br_misses));
    double *ipc_samples = malloc((size_t)reps * sizeof(*ipc_samples));
    double *ips_samples = malloc((size_t)reps * sizeof(*ips_samples));
    double *br_rate_samples = malloc((size_t)reps * sizeof(*br_rate_samples));
    if (!src_i || !work_i || !tmp_i || !expected_i || !src_u || !work_u ||
        !expected_u || !times || !dev || !elapsed || !br_misses ||
        !ipc_samples || !ips_samples || !br_rate_samples) {
        fprintf(stderr, "out of memory\n");
        free(src_i); free(work_i); free(tmp_i); free(expected_i);
        free(src_u); free(work_u); free(expected_u); free(s);
        free(times); free(dev); free(elapsed); free(br_misses);
        free(ipc_samples); free(ips_samples); free(br_rate_samples); fclose(csv);
        return 2;
    }

    int failures = 0;
    printf("MODE 11.1 PERFORMANCE: N=%d repeats=%d c=%.3f; output=%s\n", n, reps, c, output);
#ifndef HAVE_SORTS_FASM
    puts("FASM not found at configure time: assembly algorithms are not included in this run.");
#endif
    puts("Metrics: elapsed time, TSC ticks, IPC, IPS (million instructions/s), BrMis count/rate.");
    puts("Parallel hardware counters use per-thread groups: OpenMP team workers and recursive MyLib pthread workers open counters in enter/exit hooks; two-thread quicksort counts its child via perf_event inheritance.");
    puts("If any required worker counter cannot be opened or read, the complete IPC/IPS/BrMis sample is N/A instead of a partial total.");
    puts("IPC = sum(instructions)/sum(thread CPU cycles); IPS = summed instructions / wall time; BrMis and branch counts are summed across threads.");
    puts("TSC ticks remain wall-run ticks from the caller and are not summed across worker threads.");
    puts("Wide uint64_t killers stay intact for MyLib; int32 algorithms receive exact order-preserving ranks.");
    g_maxt_nocmp = 4; g_minlen_nocmp = (n >= 4) ? (n / 4) : 1;

    for (size_t di = 0; di < ARRAY_LEN(datasets) + 2; ++di) {
        const char *dname; int wide = 0, rank_int_view = 0, rank_view_used = 0;
        if (di < ARRAY_LEN(datasets)) {
            dname = datasets[di].name;
            wide = datasets[di].wide_u64_only;
            rank_int_view = datasets[di].rank_int_view;
        } else if (di == ARRAY_LEN(datasets)) { dname = "signed32_random"; }
        else { dname = "signed32_edges"; }
        if (strcmp(case_filter, "all") != 0 && strcmp(case_filter, dname) != 0) continue;

        if (wide) {
            generate_u64_only(dname, src_u, n);
            if (rank_int_view) {
                rank_view_used = 1;
                if (!copy_u64_to_rank_int(src_u, src_i, n)) {
                    fprintf(stderr, "rank compression failed for %s\n", dname);
                    ++failures; continue;
                }
            }
        } else if (is_signed_dataset(dname)) {
            generate_signed_dataset(dname, src_i, n);
            for (int i = 0; i < n; ++i)
                src_u[i] = encode_int_for_mylib(src_i[i], 1);
        } else if (generate_u64_backed_int_dataset(dname, src_u, n, c)) {
            rank_view_used = u64_needs_rank_view(src_u, n);
            if (!copy_u64_to_int_lossless_order(src_u, src_i, n)) {
                fprintf(stderr, "rank compression failed for %s\n", dname);
                ++failures; continue;
            }
        } else {
            generate_int_dataset(dname, src_i, n, c);
            for (int i = 0; i < n; ++i)
                src_u[i] = encode_int_for_mylib(src_i[i], 0);
        }
        if (!wide || rank_int_view) {
            memcpy(expected_i, src_i, (size_t)n * sizeof(*src_i));
            qsort(expected_i, (size_t)n, sizeof(*expected_i), cmp_i32);
        }
        memcpy(expected_u, src_u, (size_t)n * sizeof(*src_u));
        qsort(expected_u, (size_t)n, sizeof(*expected_u), cmp_u64);
        free(s); s = NULL; len_s = 0;
        uint64_t maxv = 0;
        for (int i = 0; i < n; ++i) if (src_u[i] > maxv) maxv = src_u[i];
        get_sequence_nocmp(maxv ? maxv : 1, c, &s, &len_s);
        printf("\n[%s] N=%d\n", dname, n);
        fflush(stdout);

        for (size_t ai = 0; ai < ARRAY_LEN(algorithms); ++ai) {
            const Algorithm *alg = &algorithms[ai];
            const int use_i = alg->run_i != NULL && (!wide || rank_int_view);
            const int use_u = alg->run_u != NULL;
            if (!use_i && !use_u) continue; /* operation-only rows belong to mode 11.2 */
            const char *domain = use_i ? (rank_view_used ? "rank-int32" : "int32") : "uint64";
            if (alg->max_n > 0 && n > alg->max_n) {
                printf("  SKIP %-28s size limit %d\n", alg->name, alg->max_n);
                fprintf(csv, "perf,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,%d,SKIP_SIZE_LIMIT,%s\n",
                        dname, n, c, alg->name, alg->source, domain, reps, alg->note ? alg->note : "");
                continue;
            }
            if (alg->power_of_two && !is_power_of_two(n)) {
                fprintf(csv, "perf,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,%d,SKIP_REQUIRES_POWER_OF_TWO,%s\n",
                        dname, n, c, alg->name, alg->source, domain, reps, alg->note ? alg->note : "");
                continue;
            }

            int ok = 1, perf_valid_count = 0;
            size_t hw_context_count = 0;
            const int hw_scope_mode = algorithm_perf_scope(alg->name);
            const char *hw_scope = perf_scope_label(hw_scope_mode);
            const int parallel_hw_scope = (hw_scope_mode != PERF_SCOPE_CALLER_THREAD);
            for (int r = -1; r < reps; ++r) { /* r=-1: warm-up */
                uint64_t t0, t1, w0, w1;
                if (use_i) memcpy(work_i, src_i, (size_t)n * sizeof(*src_i));
                else memcpy(work_u, src_u, (size_t)n * sizeof(*src_u));
                PerfContext pctx = perf_start(hw_scope_mode);
                w0 = monotonic_ns();
                t0 = ticks_begin();
                if (use_i) alg->run_i(work_i, n, tmp_i);
                else alg->run_u(work_u, n, s, len_s);
                t1 = ticks_end();
                w1 = monotonic_ns();
                PerfReadout hw = perf_stop(&pctx);
                if (hw.valid) hw_context_count = hw.thread_contexts;

                if (use_i) ok = matches_reference_i(work_i, expected_i, n);
                else ok = matches_reference_u(work_u, expected_u, n);
                if (!ok) break;
                if (r >= 0) {
                    const uint64_t elapsed_ns = (w1 >= w0) ? (w1 - w0) : 0;
                    times[r] = t1 - t0;
                    elapsed[r] = elapsed_ns;
                    if (hw.valid && hw.cycles > 0 && elapsed_ns > 0) {
                        ipc_samples[perf_valid_count] = (double)hw.instructions / (double)hw.cycles;
                        ips_samples[perf_valid_count] = ((double)hw.instructions * 1000.0) / (double)elapsed_ns;
                        br_misses[perf_valid_count] = hw.branch_misses;
                        br_rate_samples[perf_valid_count] = hw.branches > 0
                            ? (100.0 * (double)hw.branch_misses / (double)hw.branches) : 0.0;
                        ++perf_valid_count;
                    }
                }
            }

            if (!ok) {
                ++failures;
                printf("  FAIL %-28s result differs from qsort\n", alg->name);
                fprintf(csv, "perf,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,%d,FAIL_SORT_MISMATCH,%s\n",
                        dname, n, c, alg->name, alg->source, domain, reps, alg->note ? alg->note : "");
                continue;
            }
            uint64_t med_tsc = median_u64(times, reps);
            for (int r = 0; r < reps; ++r) dev[r] = times[r] > med_tsc ? times[r] - med_tsc : med_tsc - times[r];
            uint64_t mad_tsc = median_u64(dev, reps);
            uint64_t med_elapsed = median_u64(elapsed, reps);
            const double elapsed_ms = (double)med_elapsed / 1.0e6;
            printf("  PASS %-28s time=%9.3f ms TSC=%12" PRIu64 " ticks MAD=%9" PRIu64,
                   alg->name, elapsed_ms, med_tsc, mad_tsc);
            if (perf_valid_count == reps) {
                const double ipc = median_double(ipc_samples, perf_valid_count);
                const double ips_m = median_double(ips_samples, perf_valid_count);
                const uint64_t miss_median = median_u64(br_misses, perf_valid_count);
                const double miss_pct = median_double(br_rate_samples, perf_valid_count);
                if (hw_context_count > 1 || parallel_hw_scope)
                    printf(" HWctx=%zu scope=%s", hw_context_count, hw_scope);
                printf(" IPC=%.3f IPS=%.2f M/s BrMis=%" PRIu64 " (%.2f%%)", ipc, ips_m, miss_median, miss_pct);
                fprintf(csv, "perf,%s,%d,%.3f,%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%.4f,%" PRIu64 ",%.6f,%.5f,%.5f,%" PRIu64 ",%.5f,%zu,%s,%d,OK,%s\n",
                        dname, n, c, alg->name, alg->source, domain,
                        med_tsc, mad_tsc, (double)med_tsc / n, med_elapsed, elapsed_ms,
                        ipc, ips_m, miss_median, miss_pct, hw_context_count, hw_scope,
                        reps, alg->note ? alg->note : "");
            } else {
                printf(" IPC/IPS/BrMis=N/A (perf_event unavailable/incomplete; scope=%s)", hw_scope);
                fprintf(csv, "perf,%s,%d,%.3f,%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%.4f,%" PRIu64 ",%.6f,NA,NA,NA,NA,NA,%s,%d,OK,%s\n",
                        dname, n, c, alg->name, alg->source, domain,
                        med_tsc, mad_tsc, (double)med_tsc / n, med_elapsed, elapsed_ms,
                        hw_scope,
                        reps, alg->note ? alg->note : "");
            }
            printf("\n");
            fflush(stdout);
        }
    }

    free(s); free(src_i); free(work_i); free(tmp_i); free(expected_i);
    free(src_u); free(work_u); free(expected_u); free(times); free(dev); free(elapsed);
    free(br_misses); free(ipc_samples); free(ips_samples); free(br_rate_samples); fclose(csv);
    if (!append_qsort_speedup_column(output)) fprintf(stderr, "warning: could not add qsort speedup column to %s\n", output);
    if (failures) fprintf(stderr, "11.1: %d algorithm/dataset mismatch(es); see %s\n", failures, output);
    return failures ? 1 : 0;
}

static int run_ops(const char *output, int n, int reps, double c, const char *case_filter) {
    FILE *csv = fopen(output, "w");
    if (!csv) { fprintf(stderr, "cannot open %s: %s\n", output, strerror(errno)); return 2; }
    fprintf(csv,
        "Mode,Dataset,N,C,Algorithm,Source,Domain,Comparisons,Moves_G_MOV,"
        "Comparisons_Per_Element,Median_Elapsed_ns,Median_Elapsed_ms,Elapsed_ns_Per_Element,"
        "Repeats,Status,Note\n");

    int *src_i = malloc((size_t)n * sizeof(*src_i));
    int *work_i = malloc((size_t)n * sizeof(*work_i));
    int *tmp_i = malloc((size_t)n * sizeof(*tmp_i));
    int *expected_i = malloc((size_t)n * sizeof(*expected_i));
    uint64_t *src_u = malloc((size_t)n * sizeof(*src_u));
    uint64_t *work_u = malloc((size_t)n * sizeof(*work_u));
    uint64_t *expected_u = malloc((size_t)n * sizeof(*expected_u));
    uint64_t *s = NULL, len_s = 0;
    uint64_t *op_times = malloc((size_t)reps * sizeof(*op_times));
    if (!src_i || !work_i || !tmp_i || !expected_i || !src_u || !work_u ||
        !expected_u || !op_times) {
        fprintf(stderr, "out of memory\n");
        free(src_i); free(work_i); free(tmp_i); free(expected_i);
        free(src_u); free(work_u); free(expected_u); free(op_times); free(s); fclose(csv);
        return 2;
    }

    int failures = 0;
    printf("MODE 11.2 OPERATION COUNTS + TIMING: N=%d repeats=%d c=%.3f; output=%s\n",
           n, reps, c, output);
    puts("11.2 operation counts are collected in a separate instrumented pass; median time is measured with the normal run_* implementation.");
    puts("Speedup_vs_qsort is qsort median / algorithm median; values > 1 mean faster than the same-domain libc qsort.");
    puts("11.2 reports algorithmic operation counts where an instrumented implementation/twin is available.");
    puts("For FASM rows, time is measured on native FASM; ops are counted by a corresponding instrumented C twin unless noted.");
    puts("MyLib counts key comparisons and array-slot writes; Radix counts distribution-buffer/result writes; qsort writes remain opaque.");
#ifndef HAVE_SORTS_FASM
    puts("FASM not found at configure time: native ASM timing rows are unavailable; C operation twins remain available.");
#endif
    for (size_t di = 0; di < ARRAY_LEN(datasets) + 2; ++di) {
        const char *dname; int wide = 0, rank_int_view = 0, rank_view_used = 0;
        if (di < ARRAY_LEN(datasets)) {
            dname = datasets[di].name;
            wide = datasets[di].wide_u64_only;
            rank_int_view = datasets[di].rank_int_view;
        } else if (di == ARRAY_LEN(datasets)) { dname = "signed32_random"; }
        else { dname = "signed32_edges"; }
        if (strcmp(case_filter, "all") != 0 && strcmp(case_filter, dname) != 0) continue;

        if (wide) {
            generate_u64_only(dname, src_u, n);
            if (rank_int_view) {
                rank_view_used = 1;
                if (!copy_u64_to_rank_int(src_u, src_i, n)) {
                    fprintf(stderr, "rank compression failed for %s\n", dname);
                    ++failures; continue;
                }
            }
        } else if (is_signed_dataset(dname)) {
            generate_signed_dataset(dname, src_i, n);
            for (int i = 0; i < n; ++i)
                src_u[i] = encode_int_for_mylib(src_i[i], 1);
        } else if (generate_u64_backed_int_dataset(dname, src_u, n, c)) {
            rank_view_used = u64_needs_rank_view(src_u, n);
            if (!copy_u64_to_int_lossless_order(src_u, src_i, n)) {
                fprintf(stderr, "rank compression failed for %s\n", dname);
                ++failures; continue;
            }
        } else {
            generate_int_dataset(dname, src_i, n, c);
            for (int i = 0; i < n; ++i)
                src_u[i] = encode_int_for_mylib(src_i[i], 0);
        }
        if (!wide || rank_int_view) {
            memcpy(expected_i, src_i, (size_t)n * sizeof(*src_i));
            qsort(expected_i, (size_t)n, sizeof(*expected_i), cmp_i32);
        }
        memcpy(expected_u, src_u, (size_t)n * sizeof(*src_u));
        qsort(expected_u, (size_t)n, sizeof(*expected_u), cmp_u64);
        free(s); s = NULL; len_s = 0;
        uint64_t maxv = 0;
        for (int i = 0; i < n; ++i) if (src_u[i] > maxv) maxv = src_u[i];
        get_sequence(maxv ? maxv : 1, c, &s, &len_s);
        printf("\n[%s] N=%d\n", dname, n);
        fflush(stdout);

        for (size_t ai = 0; ai < ARRAY_LEN(algorithms); ++ai) {
            const Algorithm *alg = &algorithms[ai];
            const int allow_i = !wide || rank_int_view;
            const int can_i = allow_i && (alg->run_i != NULL || alg->ops_i != NULL);
            const int can_u = alg->run_u != NULL || alg->ops_u != NULL;
            if (!can_i && !can_u) continue;
            const char *row_domain = can_i ? (rank_view_used ? "rank-int32" : "int32") : "uint64";
            /* Apply exactly the same preconditions in 11.2 as in 11.1.
             * Without this, BitonicSort can be invoked on arbitrary N and
             * quadratic sorts can run far beyond their declared safe limit. */
            if (alg->max_n > 0 && n > alg->max_n) {
                printf("  SKIP %-28s size limit %d\n", alg->name, alg->max_n);
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,%d,SKIP_SIZE_LIMIT,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain, reps,
                        alg->note ? alg->note : "");
                continue;
            }
            if (alg->power_of_two && !is_power_of_two(n)) {
                printf("  SKIP %-28s requires power-of-two N\n", alg->name);
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,%d,SKIP_REQUIRES_POWER_OF_TWO,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain, reps,
                        alg->note ? alg->note : "");
                continue;
            }
            uint64_t cmp = UINT64_MAX, mov = UINT64_MAX;
            int ok = 1;
            const int use_ops_i = alg->ops_i != NULL && allow_i;
            const int use_ops_u = !use_ops_i && alg->ops_u != NULL;

            /* Pass 1: count key comparisons on a separate copy. This pass is
             * NOT timed. It must never contaminate the elapsed-time median. */
            if (use_ops_i) {
                memcpy(work_i, src_i, (size_t)n * sizeof(*src_i));
                g_cmp = 0; g_cmp_instr = 0; g_mov_instr = 0;
                ok = alg->ops_i(work_i, n, &cmp, &mov);
                if (ok && !matches_reference_i(work_i, expected_i, n)) ok = 0;
            } else if (use_ops_u) {
                memcpy(work_u, src_u, (size_t)n * sizeof(*src_u));
                g_cmp = 0; g_cmp_instr = 0; g_mov_instr = 0;
                ok = alg->ops_u(work_u, n, s, len_s, &cmp, &mov);
                if (ok && !matches_reference_u(work_u, expected_u, n)) ok = 0;
            }

            /* Pass 2: repeat the regular production implementation. Its time
             * is comparable to 11.1 and does not pay per-comparison counter
             * overhead. All currently counted algorithms provide run_i/run_u. */
            int completed = 0;
            if (ok) {
                for (int r = 0; r < reps; ++r) {
                    const uint64_t start_ns = monotonic_ns();
                    if (allow_i && alg->run_i != NULL) {
                        memcpy(work_i, src_i, (size_t)n * sizeof(*src_i));
                        g_cmp = 0;
                        alg->run_i(work_i, n, tmp_i);
                        ok = matches_reference_i(work_i, expected_i, n);
                    } else if (alg->run_u != NULL) {
                        memcpy(work_u, src_u, (size_t)n * sizeof(*src_u));
                        g_cmp = 0;
                        alg->run_u(work_u, n, s, len_s);
                        ok = matches_reference_u(work_u, expected_u, n);
                    } else if (use_ops_i) {
                        /* Defensive fallback for future count-only adapters. */
                        memcpy(work_i, src_i, (size_t)n * sizeof(*src_i));
                        g_cmp_instr = 0; g_mov_instr = 0;
                        ok = alg->ops_i(work_i, n, &cmp, &mov);
                        ok = ok && matches_reference_i(work_i, expected_i, n);
                    } else if (use_ops_u) {
                        memcpy(work_u, src_u, (size_t)n * sizeof(*src_u));
                        g_cmp = 0;
                        ok = alg->ops_u(work_u, n, s, len_s, &cmp, &mov);
                        ok = ok && matches_reference_u(work_u, expected_u, n);
                    } else {
                        ok = 0;
                    }
                    const uint64_t finish_ns = monotonic_ns();
                    op_times[r] = (finish_ns >= start_ns) ? (finish_ns - start_ns) : 0;
                    completed = r + 1;
                    if (!ok) break;
                }
            }

            if (!ok || completed != reps) {
                ++failures;
                printf("  FAIL %-28s result differs from qsort\n", alg->name);
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,NA,NA,NA,%d,FAIL_SORT_MISMATCH,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain, reps,
                        alg->note ? alg->note : "");
                continue;
            }

            const uint64_t med_ns = median_u64(op_times, reps);
            if (cmp == UINT64_MAX) {
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,NA,NA,NA,%" PRIu64 ",%.6f,%.4f,%d,OK_NO_COUNTER,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain,
                        med_ns, (double)med_ns / 1.0e6, (double)med_ns / n, reps,
                        alg->note ? alg->note : "");
            } else if (mov == UINT64_MAX) {
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,%" PRIu64 ",NA,%.4f,%" PRIu64 ",%.6f,%.4f,%d,OK,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain,
                        cmp, (double)cmp / n, med_ns, (double)med_ns / 1.0e6,
                        (double)med_ns / n, reps, alg->note ? alg->note : "");
            } else {
                fprintf(csv, "ops,%s,%d,%.3f,%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%.4f,%" PRIu64 ",%.6f,%.4f,%d,OK,%s\n",
                        dname, n, c, alg->name, alg->source, row_domain,
                        cmp, mov, (double)cmp / n, med_ns, (double)med_ns / 1.0e6,
                        (double)med_ns / n, reps, alg->note ? alg->note : "");
            }
            printf("  PASS %-28s cmp=", alg->name);
            if (cmp == UINT64_MAX) printf("N/A"); else printf("%12" PRIu64, cmp);
            printf(" moves=");
            if (mov == UINT64_MAX) printf("N/A"); else printf("%" PRIu64, mov);
            printf(" time=%9.3f ms (%" PRIu64 " ns median)\n",
                   (double)med_ns / 1.0e6, med_ns);
        }
    }

    free(s); free(src_i); free(work_i); free(tmp_i); free(expected_i);
    free(src_u); free(work_u); free(expected_u); free(op_times); fclose(csv);
    if (!append_qsort_speedup_column(output)) fprintf(stderr, "warning: could not add qsort speedup column to %s\n", output);
    puts("Note: N/A means this implementation does not instrument that operation; it is not zero.");
    if (failures) fprintf(stderr, "11.2: %d algorithm/dataset mismatch(es); see %s\n", failures, output);
    return failures ? 1 : 0;
}

#ifndef SORT_COMPARE_EMBEDDED
static int known_case_name(const char *name) {
    if (strcmp(name, "all") == 0 || is_signed_dataset(name)) return 1;
    for (size_t i = 0; i < ARRAY_LEN(datasets); ++i)
        if (strcmp(name, datasets[i].name) == 0) return 1;
    return 0;
}
#endif

int run_sort_compare_menu(int default_n) {
    int mode_choice = 3;
    int n = default_n;
    int repeats = DEFAULT_REPEATS;
    double c = 2.0;
    int result = 0;

    char perf_output[128];
    char ops_output[128];

    int requested_n = 0;
    printf("\nВведите размер последовательности N (1..%d, 0 — оставить размер из главного меню %d): ",
           MAX_N, default_n);
    if (scanf("%d", &requested_n) != 1) {
        printf("Ошибка чтения размера массива.\n");
        return 2;
    }
    if (requested_n != 0) n = requested_n;
    if (n < 1 || n > MAX_N) {
        printf("Ошибка: размер массива должен быть от 1 до %d.\n", MAX_N);
        return 2;
    }

    printf("\n");
    printf("==================================================\n");
    printf(" СРАВНИТЕЛЬНЫЙ СТЕНД АЛГОРИТМОВ СОРТИРОВКИ\n");
    printf("==================================================\n");
    printf("Размер массива N = %d\n", n);
    printf("1. Режим 11.1 — время, IPC, IPS и BrMis (без счетчика операций)\n");
    printf("2. Режим 11.2 — сравнения, перемещения и время инструментированной сортировки\n");
    printf("3. Выполнить оба режима\n");
    printf("Ваш выбор: ");

    if (scanf("%d", &mode_choice) != 1 ||
        mode_choice < 1 || mode_choice > 3) {
        printf("Ошибка: некорректный выбор.\n");
        return 2;
    }

    printf("Коэффициент c (2.0 - 3.0, например 2.0): ");

    if (scanf("%lf", &c) != 1 ||
        !isfinite(c) || c < 2.0 || c > 3.0) {
        printf("Ошибка: c должен находиться в диапазоне [2.0, 3.0].\n");
        return 2;
    }

    printf("Количество повторов для измерения времени (11.1 и 11.2, 1 - %d): ",
           MAX_REPEATS);

    if (scanf("%d", &repeats) != 1 ||
        repeats < 1 || repeats > MAX_REPEATS) {
        printf("Ошибка: некорректное количество повторов.\n");
        return 2;
    }

    snprintf(perf_output, sizeof(perf_output),
             "comparison_11_1_N%d.csv", n);

    snprintf(ops_output, sizeof(ops_output),
             "comparison_11_2_N%d.csv", n);

    if (mode_choice == 1 || mode_choice == 3) {
        printf("\nЗапуск режима 11.1...\n");

        killer_set_seed(42);

        if (run_perf(perf_output, n, repeats, c, "all") != 0) {
            result = 1;
        }

        printf("Результаты 11.1: %s\n", perf_output);
    }

    if (mode_choice == 2 || mode_choice == 3) {
        printf("\nЗапуск режима 11.2...\n");

        /* Сбрасываем seed, чтобы оба режима использовали
           одинаковые входные последовательности. */
        killer_set_seed(42);

        if (run_ops(ops_output, n, repeats, c, "all") != 0) {
            result = 1;
        }

        printf("Результаты 11.2: %s\n", ops_output);
    }

    printf("\nСравнительный стенд завершил работу.\n");

    return result;
}

#ifndef SORT_COMPARE_EMBEDDED
int main(int argc, char **argv) {
    const char *mode="perf",*output=NULL,*case_filter="all";
    int n=16384,reps=DEFAULT_REPEATS;
    double c=2.0;
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--help")==0||strcmp(argv[i],"-h")==0){print_usage(argv[0]);return 0;}
        if(strcmp(argv[i],"--mode")==0 && i+1<argc) mode=argv[++i];
        else if(strcmp(argv[i],"--n")==0 && i+1<argc) n=atoi(argv[++i]);
        else if(strcmp(argv[i],"--repeats")==0 && i+1<argc) reps=atoi(argv[++i]);
        else if(strcmp(argv[i],"--c")==0 && i+1<argc) c=strtod(argv[++i],NULL);
        else if(strcmp(argv[i],"--case")==0 && i+1<argc) case_filter=argv[++i];
        else if(strcmp(argv[i],"--output")==0 && i+1<argc) output=argv[++i];
        else {fprintf(stderr,"Unknown/incomplete option: %s\n",argv[i]);print_usage(argv[0]);return 2;}
    }
    if(n<1||n>MAX_N){fprintf(stderr,"N must be in 1..%d\n",MAX_N);return 2;}
    if(reps<1||reps>MAX_REPEATS){fprintf(stderr,"repeats must be in 1..%d\n",MAX_REPEATS);return 2;}
    if(!isfinite(c)||c<2.0||c>3.0){fprintf(stderr,"c must be in [2.0, 3.0] for this Artamonov CAlgorithm benchmark\n");return 2;}
    if (!known_case_name(case_filter)) { fprintf(stderr, "Unknown --case: %s\n", case_filter); print_usage(argv[0]); return 2; }
    const char *default_file=strcmp(mode,"perf")==0?"cs_algorithms_performance.csv":"cs_algorithms_complexity.csv";
    if(!output)output=default_file;
    killer_set_seed(42);
    if(strcmp(mode,"perf")==0)return run_perf(output,n,reps,c,case_filter);
    if(strcmp(mode,"ops")==0)return run_ops(output,n,reps,c,case_filter);
    fprintf(stderr,"mode must be perf or ops\n");return 2;
}
#endif
