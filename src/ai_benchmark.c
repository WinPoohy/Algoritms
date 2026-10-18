#define _GNU_SOURCE
#include "ai_benchmark.h"
#include "sorts.h"
#include "killers.h"
#include "sorts_instrumented.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <x86intrin.h>
#include <sys/resource.h>
#include <math.h>
#include <limits.h>

#if defined(__linux__)
#include <sys/types.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>

// внешние объявления функций из mylib.c[cite: 11]
extern int64_t g_cmp;
extern void get_sequence(uint64_t n, double c, uint64_t **ar, uint64_t *size);
extern void cs_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs_sort_fast(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs_sort_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs2_sort_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs4_sort_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs2_opt_sort_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs4_opt_sort_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs2_opt_sort_impr_nocmp(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern uint64_t qs3_sort_nocmp(uint64_t *Q, int64_t n);
extern uint64_t yar_sort_nocmp(uint64_t *Q, int64_t n);
extern void radix_sort_nocmp(uint64_t *Q, int64_t n);
extern void heapsort_nocmp(uint64_t *Q, int64_t n);
extern void cs2_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs4_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs2_opt_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs4_opt_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs2_opt_sort_impr(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern uint64_t qs3_sort(uint64_t *Q, int64_t n);
extern uint64_t yar_sort(uint64_t *Q, int64_t n);
extern void radix_sort(uint64_t *Q, int64_t n);
extern void heapsort(uint64_t *Q, int64_t n);
extern uint64_t select_fr_c(uint64_t *Q, int64_t left, int64_t right, int64_t k);
extern uint64_t sel_cascade_fr_hybrid(uint64_t *Q, int64_t n, int64_t k, uint64_t mm, double c);
extern uint64_t select_adaptive_dispatcher(uint64_t *Q, int64_t n, int64_t k, uint64_t mm, double c);

// внешние объявления алгоритмов из sorts_c.c и sorts.asm[cite: 11]
extern void insertion_sort_c(int* arr, int n);
extern void insertion_sort_fasm(int* arr, int n);
extern void quicksort_c(int* arr, int low, int high);
extern void quicksort_fasm(int* arr, int low, int high);
extern void quicksort_branchless_fasm(int* arr, int low, int high);
extern void quicksort_two_threads_c(int* arr, int n);
extern void quicksort_two_threads_fasm(int* arr, int n);
extern void dual_pivot_quicksort_c(int* arr, int low, int high);
extern void dual_pivot_quicksort_fasm(int* arr, int low, int high);
extern void triple_pivot_quicksort_c(int* arr, int low, int high);
extern void triple_pivot_quicksort_fasm(int* arr, int low, int high);
extern void cs_sort_c(int* arr, int low, int high);
extern void cs_sort_fasm(int* arr, int low, int high);
extern void merge_sort_c(int* arr, int* temp, int low, int high);
extern void merge_sort_fasm(int* arr, int* temp, int low, int high);
extern void heap_sort_c(int* arr, int n);
extern void heap_sort_fasm(int* arr, int n);
extern void radix_sort_c(int* arr, int* temp, int n);
extern void radix_sort_fasm(int* arr, int* temp, int n);
extern void bitonic_sort_c(int* arr, int low, int count, int dir);
extern void bitonic_sort_fasm(int* arr, int low, int count, int dir);
extern void introsort_c(int* arr, int n);
extern void introsort_fasm(int* arr, int n);
extern void timsort_c(int* arr, int* temp, int n);
extern void timsort_fasm(int* arr, int* temp, int n);
extern void pdqsort_c(int* arr, int n);
extern void pdqsort_fasm(int* arr, int n);

// инструментированные версии для пункта 11.2
extern void cs3_opt_c_instr(int* arr, int low, int high);
extern void cs3_opt_fasm_instr(int* arr, int low, int high);
extern void dual_pivot_quicksort_c_instr(int* arr, int low, int high);
extern void dual_pivot_quicksort_fasm_instr(int* arr, int low, int high);
extern void triple_pivot_quicksort_c_instr(int* arr, int low, int high);
extern void triple_pivot_quicksort_fasm_instr(int* arr, int low, int high);
extern void introsort_c_instr(int* arr, int n);
extern void introsort_fasm_instr(int* arr, int n);
extern void pdqsort_c_instr(int* arr, int n);
extern void pdqsort_fasm_instr(int* arr, int n);

// вспомогательные функции валидации[cite: 11]
static bool check_sorted_u64(const uint64_t *arr, int n) {
    for (int i = 1; i < n; i++) if (arr[i - 1] > arr[i]) return false;
    return true;
}

static bool check_sorted_int(const int *arr, int n) {
    for (int i = 1; i < n; i++) if (arr[i - 1] > arr[i]) return false;
    return true;
}

static int cmp_u64_qsort(const void *pa, const void *pb) {
    const uint64_t a = *(const uint64_t *)pa;
    const uint64_t b = *(const uint64_t *)pb;
    return (a > b) - (a < b);
}

static int cmp_int_qsort(const void *pa, const void *pb) {
    const int a = *(const int *)pa;
    const int b = *(const int *)pb;
    return (a > b) - (a < b);
}

static bool matches_reference_u64(const uint64_t *arr, const uint64_t *expected, int n) {
    return check_sorted_u64(arr, n) && memcmp(arr, expected, (size_t)n * sizeof(*arr)) == 0;
}

static bool matches_reference_int(const int *arr, const int *expected, int n) {
    return check_sorted_int(arr, n) && memcmp(arr, expected, (size_t)n * sizeof(*arr)) == 0;
}

/* Report the true source-key range before building the optional int rank view. */
static void report_u64_key_range(const char *name, const uint64_t *arr, int n) {
    if (!arr || n <= 0) return;
    uint64_t min_value = arr[0], max_value = arr[0];
    uint64_t above_int_max = 0, at_least_2_pow_53 = 0;
    for (int i = 0; i < n; ++i) {
        if (arr[i] < min_value) min_value = arr[i];
        if (arr[i] > max_value) max_value = arr[i];
        if (arr[i] > (uint64_t)INT_MAX) ++above_int_max;
        if (arr[i] >= (UINT64_C(1) << 53)) ++at_least_2_pow_53;
    }
    printf("[KEY-RANGE] %s: min=%" PRIu64 ", max=%" PRIu64
           ", keys>INT_MAX=%" PRIu64 ", keys>=2^53=%" PRIu64
           "; MyLib input remains uint64_t and is not narrowed.\n",
           name, min_value, max_value, above_int_max, at_least_2_pow_53);
}

/* The reference library accepts uint64_t, while project quicksorts accept int.
 * Keep wide killer keys intact for mylib. For int-only algorithms, rank-compress
 * only when needed: equal uint64_t values map to equal ranks, and x<y iff
 * rank(x)<rank(y). Thus comparator-based sorts see the same ordering pattern;
 * unlike the old bit mask, no values collapse accidentally and the 2^62 spike
 * remains present in the mylib test. No maximum key-value/range cap is applied;
 * compression affects only int32 project algorithms, never the MyLib uint64_t
 * input. Returns true when rank compression was used. */
static bool copy_u64_to_int(const uint64_t *src, int *dest, int n) {
    bool needs_rank_compression = false;
    for (int i = 0; i < n; ++i) {
        if (src[i] > (uint64_t)INT_MAX) {
            needs_rank_compression = true;
            break;
        }
    }

    if (!needs_rank_compression) {
        for (int i = 0; i < n; ++i) dest[i] = (int)src[i];
        return false;
    }

    uint64_t *sorted = (uint64_t *)malloc((size_t)n * sizeof(*sorted));
    if (sorted == NULL) {
        fprintf(stderr, "[benchmark] Недостаточно памяти для rank-compression.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(sorted, src, (size_t)n * sizeof(*sorted));
    qsort(sorted, (size_t)n, sizeof(*sorted), cmp_u64_qsort);

    for (int i = 0; i < n; ++i) {
        size_t lo = 0, hi = (size_t)n;
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            if (sorted[mid] < src[i]) lo = mid + 1;
            else hi = mid;
        }
        dest[i] = (int)lo;
    }
    free(sorted);
    return true;
}

static inline uint64_t read_tsc_begin(void) {
    _mm_lfence();
    return __rdtsc();
}

static inline uint64_t read_tsc_end(void) {
    unsigned int aux = 0;
    const uint64_t value = __rdtscp(&aux);
    _mm_lfence();
    (void)aux;
    return value;
}

static void copy_u64(const uint64_t *src, uint64_t *dest, int n) {
    memcpy(dest, src, n * sizeof(uint64_t));
}

// адаптеры сигнатур[cite: 11]
static void radix_sort_c_wrap(int* arr, int low, int high) {
    int n = high - low + 1; int *t = malloc(n * sizeof(int));
    if (t) { radix_sort_c(arr + low, t, n); free(t); }
}

//  инструментированные адаптеры для пункта 11.2
static void introsort_c_instr_wrap(int* arr, int low, int high) { introsort_c_instr(arr + low, high - low + 1); }
static void introsort_fasm_instr_wrap(int* arr, int low, int high) { introsort_fasm_instr(arr + low, high - low + 1); }
static void pdqsort_c_instr_wrap(int* arr, int low, int high) { pdqsort_c_instr(arr + low, high - low + 1); }
static void pdqsort_fasm_instr_wrap(int* arr, int low, int high) { pdqsort_fasm_instr(arr + low, high - low + 1); }

// полный набор генераторов киллер-последовательностей
typedef struct {
    const char *name;
    void (*generate)(uint64_t *A, int n);
} KillerGenerator;

static void gen_pipeorgan(uint64_t *A, int n) { killer_pipeorgan(A, n); }
static void gen_m3killer(uint64_t *A, int n) { killer_m3killer(A, n); }
static void gen_pipeorganmx(uint64_t *A, int n) { killer_pipeorganmx(A, n, 1000000ULL); }
static void gen_unevenseq(uint64_t *A, int n) { killer_unevenseq(A, n, 1000000ULL); }
static void gen_unevensorted(uint64_t *A, int n) { killer_unevensorted(A, n, 1000000ULL); }
static void gen_0to255(uint64_t *A, int n) { killer_0to255(A, n); }
static void gen_rep(uint64_t *A, int n) { killer_rep(A, n, 64); }
static void gen_rep9010(uint64_t *A, int n) { killer_rep9010(A, n, 1000000ULL); }
static void gen_binary(uint64_t *A, int n) { killer_binary(A, n, 0, 1000000ULL); }
static void gen_ternary(uint64_t *A, int n) { killer_ternary(A, n, 10, 5000, 100000ULL); }
static void gen_quintary(uint64_t *A, int n) {
    static const uint64_t vals[5] = {10, 100, 500, 1000, 5000};
    killer_quintary(A, n, vals);
}
static void gen_mag(uint64_t *A, int n) { killer_mag(A, n, 20); }
static void gen_cs_killer(uint64_t *A, int n) { killer_cs_sort(A, n); }

static KillerGenerator generators[] = {
    {"PipeOrgan (Органы)", gen_pipeorgan},
    {"m3Killer (m3-Киллер)", gen_m3killer},
    {"PipeOrganMX (Органы MX)", gen_pipeorganmx},
    {"UnevenSeq (Неравномерная)", gen_unevenseq},
    {"UnevenSorted (Частично отсортированная)", gen_unevensorted},
    {"0to255 (Случайные байты)", gen_0to255},
    {"Repeated (Дубликаты k=64)", gen_rep},
    {"Rep9010 (90% одного элемента)", gen_rep9010},
    {"Binary (2 значения)", gen_binary},
    {"Ternary (3 значения)", gen_ternary},
    {"Quintary (5 значений)", gen_quintary},
    {"KillerMag (2^20 маскирование)", gen_mag},
    {"CS_Sort_Killer (Киллер CS-Sort)", gen_cs_killer}
};

static const int NUM_GENERATORS = sizeof(generators) / sizeof(generators[0]);

// интерактивное меню выбора киллера для пункта 11.1[cite: 1]
static int select_killer_interactive(void) {
    printf("\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("|                       ВЫБОР КИЛЛЕР-ПОСЛЕДОВАТЕЛЬНОСТИ (Пункт 11.1)                    |\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    for (int i = 0; i < NUM_GENERATORS; i++) {
        printf("|  %2d. %-75s |\n", i + 1, generators[i].name);
    }
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("  Введите номер киллер-последовательности (1 - %d): ", NUM_GENERATORS);
    int choice;
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > NUM_GENERATORS) {
        printf("  [!] Некорректный выбор. Автоматически выбран 1 (%s).\n", generators[0].name);
        return 0;
    }
    return choice - 1;
}

// ============================================================================
// 10. ПОЛНЫЙ ИИ-БЕНЧМАРК (ВСЕ КИЛЛЕРЫ И АЛГОРИТМЫ)[cite: 11]
// ============================================================================
void run_ai_benchmark(void) {
    int n = 16384;
    printf("\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("|          ПОЛНЫЙ ИИ-БЕНЧМАРК: ВСЕ АЛГОРИТМЫ ИЗ MYLIB.C, SORTS_C И SORTS.ASM           |\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("  Введите размер массива для тестирования (например, 16384): ");
    if (scanf("%d", &n) != 1 || n <= 0) n = 16384;

    FILE *csv_file = fopen("ai_benchmark_results.csv", "w");
    if (csv_file) {
        fprintf(csv_file, "Sequence,Algorithm,Source,Language,Cycles_RDTSC,Comparisons_G_CMP,CPE,Status\n");
    }

    uint64_t *src_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    uint64_t *work_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    uint64_t *expected_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    int *src_int = (int*)malloc((size_t)n * sizeof(int));
    int *work_int = (int*)malloc((size_t)n * sizeof(int));
    int *expected_int = (int*)malloc((size_t)n * sizeof(int));
    int *temp_int = (int*)malloc((size_t)n * sizeof(int));

    if (!src_u64 || !work_u64 || !expected_u64 || !src_int || !work_int || !expected_int || !temp_int) {
        printf("[Ошибка] Недостаточно памяти!\n");
        free(src_u64); free(work_u64); free(expected_u64);
        free(src_int); free(work_int); free(expected_int); free(temp_int);
        if (csv_file) fclose(csv_file);
        return;
    }

    killer_set_seed(42);

    for (int g = 0; g < NUM_GENERATORS; g++) {
        generators[g].generate(src_u64, n);
        report_u64_key_range(generators[g].name, src_u64, n);
        const bool rank_compressed = copy_u64_to_int(src_u64, src_int, n);
        if (rank_compressed) {
            printf("[INFO] Для int-алгоритмов применено монотонное сжатие рангов; "
                   "mylib получает исходные uint64_t значения без усечения.\n");
        }

        memcpy(expected_u64, src_u64, (size_t)n * sizeof(uint64_t));
        qsort(expected_u64, (size_t)n, sizeof(uint64_t), cmp_u64_qsort);
        memcpy(expected_int, src_int, (size_t)n * sizeof(int));
        qsort(expected_int, (size_t)n, sizeof(int), cmp_int_qsort);

        printf("\n");
        printf("+--------------------------------------------------------------------------------------------------+\n");
        printf("| ПОСЛЕДОВАТЕЛЬНОСТЬ: %-60s [N = %-6d]        |\n", generators[g].name, n);
        printf("+--------------------------------+------------+--------+----------------+----------------+----------+\n");
        printf("| Алгоритм                       | Источник   | Язык   | Такты (RDTSC)  | Сравнения (cmp)| Статус   |\n");
        printf("+--------------------------------+------------+--------+----------------+----------------+----------+\n");

        #define RUN_MYLIB_CS(NAME_STR, FUNC_CALL) do { \
            copy_u64(src_u64, work_u64, n); \
            uint64_t *S = NULL, len_S = 0, max_val = 0; \
            for (int i = 0; i < n; i++) if (work_u64[i] > max_val) max_val = work_u64[i]; \
            if (max_val == 0) max_val = 1; \
            get_sequence(max_val, 2.0, &S, &len_S); \
            g_cmp = 0; \
            uint64_t t_start = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t_end = read_tsc_end(); \
            bool ok = matches_reference_u64(work_u64, expected_u64, n); \
            uint64_t cycles = t_end - t_start; \
            double cpe = (double)cycles / n; \
            printf("| %-30s | %-10s | %-6s | %14lu | %14ld | %-8s |\n", \
                   NAME_STR, "mylib", "C", cycles, g_cmp, ok ? "Успех" : "Ошибка"); \
            if (csv_file) fprintf(csv_file, "%s,%s,mylib,C,%lu,%ld,%.2f,%s\n", \
                                  generators[g].name, NAME_STR, cycles, g_cmp, cpe, ok ? "OK" : "ERR"); \
            free(S); \
        } while(0)

        #define RUN_MYLIB_CS_BASE(NAME_STR) do { \
            copy_u64(src_u64, work_u64, n); \
            uint64_t *S = NULL, len_S = 0, max_val = 0; \
            for (int i = 0; i < n; i++) if (work_u64[i] > max_val) max_val = work_u64[i]; \
            if (max_val == 0) max_val = 1; \
            get_sequence(max_val, 2.0, &S, &len_S); \
            uint64_t t_start = read_tsc_begin(); \
            cs_sort_fast(work_u64, S, n, len_S); \
            uint64_t t_end = read_tsc_end(); \
            uint64_t cycles = t_end - t_start; \
            bool timed_ok = matches_reference_u64(work_u64, expected_u64, n); \
            copy_u64(src_u64, work_u64, n); \
            g_cmp = 0; \
            cs_sort(work_u64, S, n, len_S); \
            int64_t comparisons = g_cmp; \
            bool counted_ok = matches_reference_u64(work_u64, expected_u64, n); \
            bool ok = timed_ok && counted_ok; \
            double cpe = (double)cycles / n; \
            printf("| %-30s | %-10s | %-6s | %14lu | %14ld | %-8s |\n", \
                   NAME_STR, "mylib", "C", cycles, comparisons, ok ? "Успех" : "Ошибка"); \
            if (csv_file) fprintf(csv_file, "%s,%s,mylib,C,%lu,%ld,%.2f,%s\n", \
                                  generators[g].name, NAME_STR, cycles, comparisons, cpe, ok ? "OK" : "ERR"); \
            free(S); \
        } while (0)

        #define RUN_MYLIB_FUNC(NAME_STR, FUNC_CALL) do { \
            copy_u64(src_u64, work_u64, n); \
            g_cmp = 0; \
            uint64_t t_start = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t_end = read_tsc_end(); \
            bool ok = matches_reference_u64(work_u64, expected_u64, n); \
            uint64_t cycles = t_end - t_start; \
            double cpe = (double)cycles / n; \
            printf("| %-30s | %-10s | %-6s | %14lu | %14ld | %-8s |\n", \
                   NAME_STR, "mylib", "C", cycles, g_cmp, ok ? "Успех" : "Ошибка"); \
            if (csv_file) fprintf(csv_file, "%s,%s,mylib,C,%lu,%ld,%.2f,%s\n", \
                                  generators[g].name, NAME_STR, cycles, g_cmp, cpe, ok ? "OK" : "ERR"); \
        } while(0)

        #define RUN_PROJECT_SORT(NAME_STR, LANG_STR, FUNC_CALL) do { \
            memcpy(work_int, src_int, n * sizeof(int)); \
            uint64_t t_start = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t_end = read_tsc_end(); \
            bool ok = matches_reference_int(work_int, expected_int, n); \
            uint64_t cycles = t_end - t_start; \
            double cpe = (double)cycles / n; \
            printf("| %-30s | %-10s | %-6s | %14lu | %14s | %-8s |\n", \
                   NAME_STR, "Project", LANG_STR, cycles, "N/A", ok ? "Успех" : "Ошибка"); \
            if (csv_file) fprintf(csv_file, "%s,%s,Project,%s,%lu,N/A,%.2f,%s\n", \
                                  generators[g].name, NAME_STR, LANG_STR, cycles, cpe, ok ? "OK" : "ERR"); \
        } while(0)

        RUN_MYLIB_CS_BASE("cs_sort (Base)");
        RUN_MYLIB_CS("cs2_sort", cs2_sort(work_u64, S, n, len_S));
        RUN_MYLIB_CS("cs4_sort", cs4_sort(work_u64, S, n, len_S));
        RUN_MYLIB_CS("cs2_opt_sort", cs2_opt_sort(work_u64, S, n, len_S));
        RUN_MYLIB_CS("cs4_opt_sort", cs4_opt_sort(work_u64, S, n, len_S));
        RUN_MYLIB_CS("cs2_opt_sort_impr", cs2_opt_sort_impr(work_u64, S, n, len_S));
        RUN_MYLIB_FUNC("QS3_Sort (3-Way Quick)", qs3_sort(work_u64, n));
        // RUN_MYLIB_FUNC("Yaroslavskiy_DualPivot", yar_sort(work_u64, n));
        // RUN_MYLIB_FUNC("heapsort (mylib)", heapsort(work_u64, n));
        // RUN_MYLIB_FUNC("radix_sort (mylib)", radix_sort(work_u64, n));

        // RUN_PROJECT_SORT("QuickSort (Hoare)", "C", quicksort_c(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("QuickSort (Hoare)", "FASM", quicksort_fasm(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("DualPivot_QuickSort", "C", dual_pivot_quicksort_c(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("DualPivot_QuickSort", "FASM", dual_pivot_quicksort_fasm(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("TriplePivot_QuickSort", "C", triple_pivot_quicksort_c(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("TriplePivot_QuickSort", "FASM", triple_pivot_quicksort_fasm(work_int, 0, n - 1));
        RUN_PROJECT_SORT("CS_Sort", "C", cs_sort_c(work_int, 0, n - 1));
        RUN_PROJECT_SORT("CS_Sort", "FASM", cs_sort_fasm(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Heap_Sort", "C", heap_sort_c_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Heap_Sort", "FASM", heap_sort_fasm_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Radix_Sort", "C", radix_sort_c_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Radix_Sort", "FASM", radix_sort_fasm_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Introsort", "C", introsort_c_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("Introsort", "FASM", introsort_fasm_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("PDQSort", "C", pdqsort_c_wrap(work_int, 0, n - 1));
        // RUN_PROJECT_SORT("PDQSort", "FASM", pdqsort_fasm_wrap(work_int, 0, n - 1));

        #undef RUN_MYLIB_CS
        #undef RUN_MYLIB_CS_BASE
        #undef RUN_MYLIB_FUNC
        #undef RUN_PROJECT_SORT
        printf("+--------------------------------+------------+--------+----------------+----------------+----------+\n");
    }

    if (csv_file) fclose(csv_file);
    free(src_u64); free(work_u64); free(expected_u64);
    free(src_int); free(work_int); free(expected_int); free(temp_int);
    printf("\n[Успешно] Результаты полного ИИ-бенчмарка сохранены в 'ai_benchmark_results.csv'.\n");
}

// управление Perf-мониторингом[cite: 1]
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

typedef struct {
    int fd_cycles, fd_instr, fd_branch_miss, fd_branches;
} PerfContext;

static PerfContext start_perf_monitoring(void) {
    PerfContext ctx = {-1, -1, -1, -1};
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    ctx.fd_cycles = perf_event_open(&pe, 0, -1, -1, 0);

    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    ctx.fd_instr = perf_event_open(&pe, 0, -1, ctx.fd_cycles, 0);

    pe.config = PERF_COUNT_HW_BRANCH_MISSES;
    ctx.fd_branch_miss = perf_event_open(&pe, 0, -1, ctx.fd_cycles, 0);

    pe.config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
    ctx.fd_branches = perf_event_open(&pe, 0, -1, ctx.fd_cycles, 0);

    if (ctx.fd_cycles != -1) {
        ioctl(ctx.fd_cycles, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        ioctl(ctx.fd_cycles, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    }
    return ctx;
}

static void stop_perf_monitoring(PerfContext ctx, double* ipc, double* branch_miss_rate) {
    if (ctx.fd_cycles == -1) return;
    ioctl(ctx.fd_cycles, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

    uint64_t cycles = 0, instr = 0, b_misses = 0, branches = 0;
    ssize_t r1 = read(ctx.fd_cycles, &cycles, sizeof(uint64_t));
    ssize_t r2 = read(ctx.fd_instr, &instr, sizeof(uint64_t));
    ssize_t r3 = read(ctx.fd_branch_miss, &b_misses, sizeof(uint64_t));
    ssize_t r4 = read(ctx.fd_branches, &branches, sizeof(uint64_t));
    (void)r1; (void)r2; (void)r3; (void)r4;

    *ipc = (cycles > 0) ? ((double)instr / cycles) : 0.0;
    *branch_miss_rate = (branches > 0) ? ((double)b_misses / branches) * 100.0 : 0.0;

    close(ctx.fd_cycles); close(ctx.fd_instr);
    close(ctx.fd_branch_miss); close(ctx.fd_branches);
}
#else
typedef struct { int dummy; } PerfContext;
static PerfContext start_perf_monitoring(void) { return (PerfContext){0}; }
static void stop_perf_monitoring(PerfContext ctx, double* ipc, double* branch_miss_rate) {
    *ipc = 0.0; *branch_miss_rate = 0.0;
}
#endif

#define PERF_REPEATS 7

static uint64_t median_u64_values(uint64_t *values, int count) {
    for (int i = 1; i < count; ++i) {
        const uint64_t value = values[i];
        int j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    return count > 0 ? values[count / 2] : 0;
}

static double median_double_values(double *values, int count) {
    for (int i = 1; i < count; ++i) {
        const double value = values[i];
        int j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    return count > 0 ? values[count / 2] : 0.0;
}

// ============================================================================
// 11.1. АППАРАТНЫЙ БЕНЧМАРК ПРОИЗВОДИТЕЛЬНОСТИ (RDTSC & Perf)
// ============================================================================
void run_performance_benchmark(void) {
    int k_idx = select_killer_interactive();
    int n = 65536;
    printf("  Введите размер массива N (рекомендуется 16384 - 65536+): ");
    if (scanf("%d", &n) != 1 || n <= 0) n = 65536;

    FILE *csv = fopen("academic_performance_benchmark.csv", "w");
    if (csv) {
        fprintf(csv, "Sequence,Algorithm,Source,Language,PureCycles,CPE,IPC,BranchMissRate\n");
    }

    uint64_t *src_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    uint64_t *work_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    uint64_t *expected_u64 = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    int *src_int = (int*)malloc((size_t)n * sizeof(int));
    int *work_int = (int*)malloc((size_t)n * sizeof(int));
    int *expected_int = (int*)malloc((size_t)n * sizeof(int));

    if (!src_u64 || !work_u64 || !expected_u64 || !src_int || !work_int || !expected_int) {
        printf("[Ошибка] Недостаточно памяти!\n");
        free(src_u64); free(work_u64); free(expected_u64);
        free(src_int); free(work_int); free(expected_int);
        if (csv) fclose(csv);
        return;
    }

    killer_set_seed(42);
    generators[k_idx].generate(src_u64, n);
    report_u64_key_range(generators[k_idx].name, src_u64, n);
    const bool rank_compressed = copy_u64_to_int(src_u64, src_int, n);
    if (rank_compressed) {
        printf("[INFO] Для int-алгоритмов применено монотонное сжатие рангов; "
               "mylib получает исходные uint64_t значения без усечения.\n");
    }
    memcpy(expected_u64, src_u64, (size_t)n * sizeof(uint64_t));
    qsort(expected_u64, (size_t)n, sizeof(uint64_t), cmp_u64_qsort);
    memcpy(expected_int, src_int, (size_t)n * sizeof(int));
    qsort(expected_int, (size_t)n, sizeof(int), cmp_int_qsort);

    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------+\n");
    printf("|       АППАРАТНЫЙ БЕНЧМАРК ПРОИЗВОДИТЕЛЬНОСТИ (RDTSC)                                             |\n");
    printf("|       Киллер: %-34s | Размер N = %-10d               |\n", generators[k_idx].name, n);
    printf("+--------------------------------+------------+--------+----------------+------------+-------------+\n");
    printf("| Алгоритм                       | Источник   | Язык   | Такты (Pure)   | CPE        | IPC / BrMis |\n");
    printf("+--------------------------------+------------+--------+----------------+------------+-------------+\n");

    #define RUN_PERF_MYLIB_CS(NAME, FUNC_CALL) do { \
        uint64_t *S = NULL, len_S = 0, max_val = 0; \
        for (int i = 0; i < n; i++) if (src_u64[i] > max_val) max_val = src_u64[i]; \
        if (max_val == 0) max_val = 1; \
        get_sequence(max_val, 2.0, &S, &len_S); \
        uint64_t cycle_samples[PERF_REPEATS]; \
        double ipc_samples[PERF_REPEATS], miss_samples[PERF_REPEATS]; \
        bool ok = true; \
        for (int rep = 0; rep < PERF_REPEATS; ++rep) { \
            copy_u64(src_u64, work_u64, n); \
            PerfContext pctx = start_perf_monitoring(); \
            uint64_t t0 = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t1 = read_tsc_end(); \
            stop_perf_monitoring(pctx, &ipc_samples[rep], &miss_samples[rep]); \
            cycle_samples[rep] = t1 - t0; \
            if (!matches_reference_u64(work_u64, expected_u64, n)) ok = false; \
        } \
        uint64_t cycles = median_u64_values(cycle_samples, PERF_REPEATS); \
        double ipc = median_double_values(ipc_samples, PERF_REPEATS); \
        double bmiss = median_double_values(miss_samples, PERF_REPEATS); \
        double cpe = (double)cycles / n; \
        printf("| %-30s | %-10s | %-6s | %14lu | %10.2f | %.2f / %.1f%% |\n", \
               NAME, "mylib", "C", (unsigned long)cycles, cpe, ipc, bmiss); \
        if (!ok) printf(" [!] ОШИБКА СОРТИРОВКИ"); \
        if (csv) fprintf(csv, "%s,%s,mylib,C,%lu,%.2f,%.2f,%.2f\n", \
                generators[k_idx].name, NAME, (unsigned long)cycles, cpe, ipc, bmiss); \
        free(S); \
    } while(0)

    #define RUN_PERF_MYLIB_FUNC(NAME, FUNC_CALL) do { \
        uint64_t cycle_samples[PERF_REPEATS]; \
        double ipc_samples[PERF_REPEATS], miss_samples[PERF_REPEATS]; \
        bool ok = true; \
        for (int rep = 0; rep < PERF_REPEATS; ++rep) { \
            copy_u64(src_u64, work_u64, n); \
            PerfContext pctx = start_perf_monitoring(); \
            uint64_t t0 = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t1 = read_tsc_end(); \
            stop_perf_monitoring(pctx, &ipc_samples[rep], &miss_samples[rep]); \
            cycle_samples[rep] = t1 - t0; \
            if (!matches_reference_u64(work_u64, expected_u64, n)) ok = false; \
        } \
        uint64_t cycles = median_u64_values(cycle_samples, PERF_REPEATS); \
        double ipc = median_double_values(ipc_samples, PERF_REPEATS); \
        double bmiss = median_double_values(miss_samples, PERF_REPEATS); \
        double cpe = (double)cycles / n; \
        printf("| %-30s | %-10s | %-6s | %14lu | %10.2f | %.2f / %.1f%% |\n", \
               NAME, "mylib", "C", (unsigned long)cycles, cpe, ipc, bmiss); \
        if (!ok) printf(" [!] ОШИБКА СОРТИРОВКИ"); \
        if (csv) fprintf(csv, "%s,%s,mylib,C,%lu,%.2f,%.2f,%.2f\n", \
                generators[k_idx].name, NAME, (unsigned long)cycles, cpe, ipc, bmiss); \
    } while(0)

    #define RUN_PERF_PROJECT(NAME, SRC_TYPE, LANG, FUNC_CALL) do { \
        uint64_t cycle_samples[PERF_REPEATS]; \
        double ipc_samples[PERF_REPEATS], miss_samples[PERF_REPEATS]; \
        bool ok = true; \
        for (int rep = 0; rep < PERF_REPEATS; ++rep) { \
            memcpy(work_int, src_int, (size_t)n * sizeof(int)); \
            PerfContext pctx = start_perf_monitoring(); \
            uint64_t t0 = read_tsc_begin(); \
            FUNC_CALL; \
            uint64_t t1 = read_tsc_end(); \
            stop_perf_monitoring(pctx, &ipc_samples[rep], &miss_samples[rep]); \
            cycle_samples[rep] = t1 - t0; \
            if (!matches_reference_int(work_int, expected_int, n)) ok = false; \
        } \
        uint64_t cycles = median_u64_values(cycle_samples, PERF_REPEATS); \
        double ipc = median_double_values(ipc_samples, PERF_REPEATS); \
        double bmiss = median_double_values(miss_samples, PERF_REPEATS); \
        double cpe = (double)cycles / n; \
        printf("| %-30s | %-10s | %-6s | %14lu | %10.2f | %.2f / %.1f%% |\n", \
               NAME, SRC_TYPE, LANG, (unsigned long)cycles, cpe, ipc, bmiss); \
        if (!ok) printf(" [!] ОШИБКА СОРТИРОВКИ"); \
        if (csv) fprintf(csv, "%s,%s,%s,%s,%lu,%.2f,%.2f,%.2f\n", \
                generators[k_idx].name, NAME, SRC_TYPE, LANG, (unsigned long)cycles, cpe, ipc, bmiss); \
    } while(0)

    RUN_PERF_MYLIB_CS("cs_sort (Base, no g_cmp)", cs_sort_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_CS("cs2_sort (no g_cmp)", cs2_sort_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_CS("cs4_sort (no g_cmp)", cs4_sort_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_CS("cs2_opt_sort (no g_cmp)", cs2_opt_sort_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_CS("cs4_opt_sort (no g_cmp)", cs4_opt_sort_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_CS("cs2_opt_sort_impr (no g_cmp)", cs2_opt_sort_impr_nocmp(work_u64, S, n, len_S));
    RUN_PERF_MYLIB_FUNC("qs3_sort (no g_cmp)", qs3_sort_nocmp(work_u64, n));
    RUN_PERF_MYLIB_FUNC("yar_sort (no g_cmp)", yar_sort_nocmp(work_u64, n));
    RUN_PERF_MYLIB_FUNC("heapsort (no g_cmp)", heapsort_nocmp(work_u64, n));
    RUN_PERF_MYLIB_FUNC("radix_sort (no g_cmp)", radix_sort_nocmp(work_u64, n));

    RUN_PERF_PROJECT("CS_Sort", "sorts_c", "C", cs_sort_c(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Dual-Pivot QuickSort", "sorts_c", "C", dual_pivot_quicksort_c(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Triple-Pivot QuickSort", "sorts_c", "C", triple_pivot_quicksort_c(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Heap Sort", "sorts_c", "C", heap_sort_c_wrap(work_int, 0, n - 1));
    RUN_PERF_PROJECT("Radix Sort", "sorts_c", "C", radix_sort_c_wrap(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Introsort", "sorts_c", "C", introsort_c_wrap(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("PDQSort", "sorts_c", "C", pdqsort_c_wrap(work_int, 0, n - 1));

    RUN_PERF_PROJECT("CS_Sort", "sorts.asm", "FASM", cs_sort_fasm(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Dual-Pivot QuickSort", "sorts.asm", "FASM", dual_pivot_quicksort_fasm(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Triple-Pivot QuickSort", "sorts.asm", "FASM", triple_pivot_quicksort_fasm(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Heap Sort", "sorts.asm", "FASM", heap_sort_fasm_wrap(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Radix Sort", "sorts.asm", "FASM", radix_sort_fasm_wrap(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("Introsort", "sorts.asm", "FASM", introsort_fasm_wrap(work_int, 0, n - 1));
    // RUN_PERF_PROJECT("PDQSort", "sorts.asm", "FASM", pdqsort_fasm_wrap(work_int, 0, n - 1));

    printf("+--------------------------------+------------+--------+----------------+------------+-------------+\n");

    if (csv) fclose(csv);
    free(src_u64); free(work_u64); free(expected_u64);
    free(src_int); free(work_int); free(expected_int);
    printf("\n[Успешно] Результаты аппаратного бенчмарка сохранены в 'academic_performance_benchmark.csv'.\n");
}

// ============================================================================
// ЛЕГКОВЕСНЫЙ ГЕНЕРАТОР ПОСЛЕДОВАТЕЛЬНОСТЕЙ (по логике generator.c)[cite: 12]
// ============================================================================
static void generate_array_from_generator(int *arr, int n, double c) {
    long long A = n;
    int current_val = 1;
    int idx = 0;
    while (A > 0 && idx < n) {
        /* Keep this recurrence consistent with get_sequence(): B_i = ceil(A/c). */
        long long B_i = (long long)ceill((long double)A / (long double)c);
        if (B_i == 0 && A > 0) B_i = A;
        if (B_i > A) B_i = A;
        for (long long j = 0; j < B_i && idx < n; j++) {
            arr[idx++] = current_val;
        }
        current_val++;
        A -= B_i;
    }
    while (idx < n) {
        arr[idx] = arr[idx - 1];
        idx++;
    }
}

// ============================================================================
// 11.2. ТЕОРЕТИКО-АЛГОРИТМИЧЕСКАЯ СЛОЖНОСТЬ (Счетчики операций)
// ============================================================================
void run_complexity_benchmark(void) {
    int n = 16384;
    double c = 2.0;

    printf("\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("|               ТЕОРЕТИКО-АЛГОРИТМИЧЕСКАЯ СЛОЖНОСТЬ (Счетчики Cmp / Mov)               |\n");
    printf("+--------------------------------------------------------------------------------------+\n");
    printf("  Введите размер массива N (например, 16384): ");
    if (scanf("%d", &n) != 1 || n <= 0) n = 16384;

    printf("  Введите коэффициент c для генератора в диапазоне [2.0, 3.0] (например, 2.0): ");
    if (scanf("%lf", &c) != 1 || !isfinite(c) || c < 2.0 || c > 3.0) {
        printf("  [!] Некорректный c; используется c=2.0.\n");
        c = 2.0;
    }

    FILE *csv = fopen("academic_complexity_benchmark.csv", "w");
    if (csv) {
        fprintf(csv, "N,Coefficient_c,Algorithm,Source,Language,Comparisons,Moves,Tau_Inversions,Runs\n");
    }

    int *src = (int*)malloc((size_t)n * sizeof(int));
    int *work_instr = (int*)malloc((size_t)n * sizeof(int));
    int *expected = (int*)malloc((size_t)n * sizeof(int));

    if (!src || !work_instr || !expected) {
        printf("[Ошибка] Недостаточно памяти!\\n");
        free(src); free(work_instr); free(expected);
        if (csv) fclose(csv);
        return;
    }

    // генерация массива; qsort служит эталоном полной корректности.
    generate_array_from_generator(src, n, c);
    memcpy(expected, src, (size_t)n * sizeof(int));
    qsort(expected, (size_t)n, sizeof(int), cmp_int_qsort);

    uint64_t inversions = calculate_inversions(src, n);
    uint64_t runs = calculate_runs(src, n);

    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------+\n");
    printf("| ПАРАМЕТРЫ ТЕСТА: Размер N = %-6d | Коэффициент c = %-5.2f                                       |\n", n, c);
    printf("| МЕТРИКИ ЭНТРОПИИ: Kendall's Tau (Инверсии) = %-10lu | Серии (Runs) = %-10lu          |\n", inversions, runs);
    printf("+--------------------------------+------------+--------+------------------+------------------+\n");
    printf("| Алгоритм                       | Источник   | Язык   | Сравнения (Cmp)  | Пересылки (Mov)  |\n");
    printf("+--------------------------------+------------+--------+------------------+------------------+\n");

    #define RUN_COMPLEXITY_EXT(NAME, SRC_TYPE, LANG, FUNC_INSTR) do { \
        memcpy(work_instr, src, n * sizeof(int)); \
        g_cmp_instr = 0; g_mov_instr = 0; \
        FUNC_INSTR(work_instr, 0, n - 1); \
        bool ok = matches_reference_int(work_instr, expected, n); \
        \
        printf("| %-30s | %-10s | %-6s | %16lu | %16lu |\n", \
               NAME, SRC_TYPE, LANG, g_cmp_instr, g_mov_instr); \
        if (ok == false) printf(" [!] ОШИБКА СОРТИРОВКИ"); \
        if (csv) fprintf(csv, "%d,%.2f,%s,%s,%s,%lu,%lu,%lu,%lu\n", \
                n, c, NAME, SRC_TYPE, LANG, g_cmp_instr, g_mov_instr, inversions, runs); \
    } while(0)

    RUN_COMPLEXITY_EXT("CS_Sort_3Pivot", "sorts_inst", "C", cs_sort_c_instr);
    RUN_COMPLEXITY_EXT("Legacy_CS3_ASM", "sorts_inst", "FASM", cs3_opt_fasm_instr);
    RUN_COMPLEXITY_EXT("Dual-Pivot Quick", "sorts_inst", "C", dual_pivot_quicksort_c_instr);
    RUN_COMPLEXITY_EXT("Dual-Pivot Quick", "sorts_inst", "FASM", dual_pivot_quicksort_fasm_instr);
    RUN_COMPLEXITY_EXT("Triple-Pivot Quick", "sorts_inst", "C", triple_pivot_quicksort_c_instr);
    RUN_COMPLEXITY_EXT("Triple-Pivot Quick", "sorts_inst", "FASM", triple_pivot_quicksort_fasm_instr);
    RUN_COMPLEXITY_EXT("Introsort", "sorts_inst", "C", introsort_c_instr_wrap);
    RUN_COMPLEXITY_EXT("Introsort", "sorts_inst", "FASM", introsort_fasm_instr_wrap);
    RUN_COMPLEXITY_EXT("PDQSort", "sorts_inst", "C", pdqsort_c_instr_wrap);
    RUN_COMPLEXITY_EXT("PDQSort", "sorts_inst", "FASM", pdqsort_fasm_instr_wrap);

    printf("+--------------------------------+------------+--------+------------------+------------------+\n");

    if (csv) fclose(csv);
    free(src); free(work_instr); free(expected);
    printf("\n[Успешно] Результаты теоретического бенчмарка сохранены в 'academic_complexity_benchmark.csv'.\n");
}
