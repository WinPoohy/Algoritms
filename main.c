#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <x86intrin.h>
#include "sorts.h"
#include <math.h>

void generate_random(int* arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = rand() % 500000;
}

void generate_sorted(int* arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
}

void generate_reversed(int* arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = n - i;
}

void generate_duplicates(int* arr, int n) {
    int range = (n / 10 > 0) ? (n / 10) : 5;
    for (int i = 0; i < n; i++) arr[i] = rand() % range;
}

int generate_artamonov(int* arr, int max_capacity, long long target_sum, double c) {
    if (c <= 1.0) {
        printf("!!!Предупреждение!!! Коэффициент 'c' должен быть > 1.0. Установлено c = 2.0\n");
        c = 2.0;
    }
    if (target_sum <= 0) return 0;

    long long A = target_sum;
    int idx = 0;

    while (A != 0 && idx < max_capacity) {
        long long B_i = (long long)round(A / c);

        if (B_i == 0 && A > 0) B_i = A;
        if (B_i > A) B_i = A;

        arr[idx++] = (int)B_i;
        A = A - B_i;
    }

    return idx;
}

void copy_arr(int* src, int* dest, int n) {
    for (int i = 0; i < n; i++) dest[i] = src[i];
}

bool check_sorted(int* arr, int n) {
    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) return false;
    }
    return true;
}

void cs_sort(int* arr, int low, int high) {
    if (low >= high) return;

    // Находим минимум и максимум для безопасного разделения без зацикливаний
    int min_val = arr[low], max_val = arr[low];
    for (int k = low + 1; k <= high; k++) {
        if (arr[k] < min_val) min_val = arr[k];
        if (arr[k] > max_val) max_val = arr[k];
    }
    if (min_val == max_val) return; // Массив состоит из одинаковых элементов

    // Вычисляем два опорных значения (терцили диапазона)
    int p1 = min_val + (max_val - min_val) / 3;
    int p2 = max_val - (max_val - min_val) / 3;

    // Смещаем пивоты при малом диапазоне, чтобы средняя часть гарантированно уменьшалась
    if (p1 == min_val && p2 == max_val) {
        p1 = min_val;
        p2 = min_val;
    }

    int out_i = 0, out_j = 0;
    // Используем написанную на ассемблере функцию разделения[cite: 22, 23]
    cs_dual_partition(arr + low, high - low + 1, p1, p2, &out_i, &out_j);

    // Вычисляем абсолютные границы получившихся трех частей
    int i = low + out_i;
    int j = low + out_j;

    // Рекурсивно сортируем три части массива
    cs_sort(arr, low, i - 1);
    cs_sort(arr, i, j);
    cs_sort(arr, j + 1, high);
}

typedef struct {
    double mean;
    double stddev;
} Stats;

Stats calculate_stats(uint64_t* results, int runs) {
    Stats s = {0.0, 0.0};
    if (runs <= 0) return s;
    double sum = 0;
    for (int i = 0; i < runs; i++) sum += results[i];
    s.mean = sum / runs;
    double variance_sum = 0;
    for (int i = 0; i < runs; i++) {
        variance_sum += (results[i] - s.mean) * (results[i] - s.mean);
    }
    s.stddev = sqrt(variance_sum / runs);
    return s;
}

void print_stats(const char* name, const char* lang, uint64_t* results, int runs) {
    double sum = 0;
    for (int i = 0; i < runs; i++) sum += results[i];
    double mean = sum / runs;

    double variance_sum = 0;
    for (int i = 0; i < runs; i++) {
        variance_sum += (results[i] - mean) * (results[i] - mean);
    }
    double stddev = sqrt(variance_sum / runs);
    printf("| %-22s | %-10s | %-15.0f | %-12.0f |\n", name, lang, mean, stddev);
}

void export_benchmark_to_csv() {
    int sizes[] = {16384, 32768, 65536, 131072, 262144, 524288};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    FILE *f = fopen("results.csv", "w");
    if (!f) {
        printf("Ошибка: не удалось создать файл results.csv\n");
        return;
    }

    fprintf(f, "Size,Quick_C,Quick_FASM,QuickBranch_FASM,Quick2Thread_C,Quick2Thread_FASM,DualPivot_C,DualPivot_FASM,Merge_C,Merge_FASM,Heap_C,Heap_FASM,Radix_C,Radix_FASM,Bitonic_C,Bitonic_FASM,Intro_C,Intro_FASM,Tim_C,Tim_FASM,PDQ_C,PDQ_FASM,ParallelMerge_C,QuickSelect_C,QuickSelect_FASM,Partial_C,Partial_FASM,ParallelQuick_C,TriplePivot_C,TriplePivot_FASM,CS_Sort\n");

    printf("\n>>> ЗАПУСК ПОЛНОГО ЭКСПОРТА ДАННЫХ В CSV (30 алгоритмов) <<<\n");

    for (int i = 0; i < num_sizes; i++) {
        int size = sizes[i];
        printf("Обработка массива N = %d...\n", size);

        int* original = (int*)malloc(size * sizeof(int));
        int* work_arr = (int*)malloc(size * sizeof(int));
        int* temp_arr = (int*)malloc(size * sizeof(int));

        generate_random(original, size);

        uint64_t start, end;
        uint64_t t[30] = {0};
        int k = size / 2;

        copy_arr(original, work_arr, size); start = __rdtsc(); quicksort_c(work_arr, 0, size - 1); end = __rdtsc(); t[0] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); quicksort_fasm(work_arr, 0, size - 1); end = __rdtsc(); t[1] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); quicksort_branchless_fasm(work_arr, 0, size - 1); end = __rdtsc(); t[2] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); quicksort_two_threads_c(work_arr, size); end = __rdtsc(); t[3] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); quicksort_two_threads_fasm(work_arr, size); end = __rdtsc(); t[4] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); cs_sort_c(work_arr, 0, size - 1); end = __rdtsc(); t[29] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); cs_sort(work_arr, 0, size - 1); end = __rdtsc(); t[30] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); dual_pivot_quicksort_c(work_arr, 0, size - 1); end = __rdtsc(); t[5] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); dual_pivot_quicksort_fasm(work_arr, 0, size - 1); end = __rdtsc(); t[6] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); merge_sort_c(work_arr, temp_arr, 0, size - 1); end = __rdtsc(); t[7] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); merge_sort_fasm(work_arr, temp_arr, 0, size - 1); end = __rdtsc(); t[8] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); heap_sort_c(work_arr, size); end = __rdtsc(); t[9] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); heap_sort_fasm(work_arr, size); end = __rdtsc(); t[10] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); radix_sort_c(work_arr, temp_arr, size); end = __rdtsc(); t[11] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); radix_sort_fasm(work_arr, temp_arr, size); end = __rdtsc(); t[12] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); bitonic_sort_c(work_arr, 0, size, 1); end = __rdtsc(); t[13] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); bitonic_sort_fasm(work_arr, 0, size, 1); end = __rdtsc(); t[14] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); introsort_c(work_arr, size); end = __rdtsc(); t[15] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); introsort_fasm(work_arr, size); end = __rdtsc(); t[16] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); timsort_c(work_arr, temp_arr, size); end = __rdtsc(); t[17] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); timsort_fasm(work_arr, temp_arr, size); end = __rdtsc(); t[18] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); pdqsort_c(work_arr, size); end = __rdtsc(); t[19] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); pdqsort_fasm(work_arr, size); end = __rdtsc(); t[20] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc();
        #pragma omp parallel
        {
            #pragma omp single
            parallel_merge_sort_c(work_arr, temp_arr, 0, size - 1);
        }
        end = __rdtsc(); t[21] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); quickselect_c(work_arr, 0, size - 1, k); end = __rdtsc(); t[22] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); quickselect_fasm(work_arr, 0, size - 1, k); end = __rdtsc(); t[23] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); partial_sort_c(work_arr, size, k); end = __rdtsc(); t[24] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); partial_sort_fasm(work_arr, size, k); end = __rdtsc(); t[25] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc();
        #pragma omp parallel
        {
            #pragma omp single
            parallel_quicksort_c(work_arr, 0, size - 1);
        }
        end = __rdtsc(); t[26] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); triple_pivot_quicksort_c(work_arr, 0, size - 1); end = __rdtsc(); t[27] = end - start;
        copy_arr(original, work_arr, size); start = __rdtsc(); triple_pivot_quicksort_fasm(work_arr, 0, size - 1); end = __rdtsc(); t[28] = end - start;

        copy_arr(original, work_arr, size); start = __rdtsc(); cs_sort(work_arr, 0, size - 1); end = __rdtsc(); t[29] = end - start;

        fprintf(f, "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                size, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8],
                t[9], t[10], t[11], t[12], t[13], t[14], t[15], t[16], t[17], t[18], t[19], t[20], t[21], t[22], t[23], t[24], t[25], t[26], t[27], t[28], t[29]);

        free(original);
        free(work_arr);
        free(temp_arr);
    }

    fclose(f);
    printf("\nДанные всех алгоритмов успешно сохранены в файл 'results.csv'.\n");
}

#define MEASURE(NAME, LANG, FUNC_CALL, ARR_PTR, SIZE) do { \
    copy_arr(original, ARR_PTR, SIZE); \
    uint64_t start = __rdtsc(); \
    FUNC_CALL; \
    uint64_t end = __rdtsc(); \
    bool valid = check_sorted(ARR_PTR, SIZE); \
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", \
           NAME, LANG, end - start, valid ? "\033[32mУспех\033[0m" : "\033[31mОшибка\033[0m"); \
} while(0)

#define MEASURE_STAT(NAME, LANG, FUNC_CALL, ARR_PTR, SIZE, RUNS, RES_ARR) do { \
    for (int r = 0; r < (RUNS); r++) { \
        copy_arr(original, ARR_PTR, SIZE); \
        uint64_t start = __rdtsc(); \
        FUNC_CALL; \
        uint64_t end = __rdtsc(); \
        RES_ARR[r] = end - start; \
    } \
    print_stats(NAME, LANG, RES_ARR, RUNS); \
} while(0)

void run_cache_aware_benchmark() {
    int sizes[] = {1024, 16384, 131072, 1048576, 16777216};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("\n====================================================================\n");
    printf("         CACHE-AWARE BENCHMARK: ИССЛЕДОВАНИЕ ИЕРАРХИИ ПАМЯТИ        \n");
    printf("====================================================================\n");
    printf("%-15s | %-15s | %-18s | %-12s\n", "Объем (КБ)", "Алгоритм", "Всего тактов (TSC)", "Тактов/элем.");
    printf("--------------------------------------------------------------------\n");

    for (int i = 0; i < num_sizes; i++) {
        int size = sizes[i];
        double kb_size = (size * sizeof(int)) / 1024.0;

        int* original = (int*)malloc(size * sizeof(int));
        int* work_arr = (int*)malloc(size * sizeof(int));

        generate_random(original, size);

        copy_arr(original, work_arr, size);
        uint64_t start = __rdtsc();
        introsort_c(work_arr, size);
        uint64_t end = __rdtsc();
        uint64_t cycles_c = end - start;
        printf("%-15.1f | %-15s | %-18lu | %-12.2f\n", kb_size, "Introsort C", cycles_c, (double)cycles_c / size);

        copy_arr(original, work_arr, size);
        start = __rdtsc();
        introsort_fasm(work_arr, size);
        end = __rdtsc();
        uint64_t cycles_asm = end - start;
        printf("%-15.1f | %-15s | %-18lu | %-12.2f\n", kb_size, "Introsort FASM", cycles_asm, (double)cycles_asm / size);

        printf("--------------------------------------------------------------------\n");

        free(original);
        free(work_arr);
    }
}

void run_benchmark(int type_choice, int size, int* original, int* work_arr, int* temp_arr) {
    const char* type_name = "";
    switch (type_choice) {
        case 1: generate_random(original, size);     type_name = "Случайный"; break;
        case 2: generate_sorted(original, size);     type_name = "Отсортированный"; break;
        case 3: generate_reversed(original, size);   type_name = "Обратный порядок"; break;
        case 4: generate_duplicates(original, size); type_name = "С дубликатами"; break;
        case 5: type_name = "Генератор Ю.Н. Артамонова"; break;
    }

    int k = size / 2;

    printf("\n>>> Запуск бенчмарка [N = %d, Тип: %s] <<<\n", size, type_name);
    printf("+------------------------+------------+-----------------+------------+\n");
    printf("| Алгоритм               | Язык       | Такты (RDTSC)   | Статус     |\n");
    printf("+------------------------+------------+-----------------+------------+\n");

    if (type_choice == 2 || (size <= 50000 && type_choice != 3) || size <= 20000) {
        MEASURE("Insertion Sort", "C", insertion_sort_c(work_arr, size), work_arr, size);
        MEASURE("Insertion Sort", "FASM", insertion_sort_fasm(work_arr, size), work_arr, size);
        printf("+------------------------+------------+-----------------+------------+\n");
    }

    MEASURE("Quick Sort (Hoare)", "C", quicksort_c(work_arr, 0, size - 1), work_arr, size);
    MEASURE("Quick Sort (Hoare)", "FASM", quicksort_fasm(work_arr, 0, size - 1), work_arr, size);
    MEASURE("Quick Sort (Branchless)", "FASM", quicksort_branchless_fasm(work_arr, 0, size - 1), work_arr, size);
    MEASURE("Quick (2 Threads)", "C", quicksort_two_threads_c(work_arr, size), work_arr, size);
    MEASURE("Quick (2 Threads)", "FASM", quicksort_two_threads_fasm(work_arr, size), work_arr, size);
    MEASURE("Dual-Pivot Quick", "C", dual_pivot_quicksort_c(work_arr, 0, size - 1), work_arr, size);
    MEASURE("Dual-Pivot Quick", "FASM", dual_pivot_quicksort_fasm(work_arr, 0, size - 1), work_arr, size);

    MEASURE("Triple-Pivot Quick", "C", triple_pivot_quicksort_c(work_arr, 0, size - 1), work_arr, size);
    MEASURE("Triple-Pivot Quick", "FASM", triple_pivot_quicksort_fasm(work_arr, 0, size - 1), work_arr, size);
    MEASURE("CS Sort", "C", cs_sort_c(work_arr, 0, size - 1), work_arr, size);
    MEASURE("CS Sort", "FASM", cs_sort(work_arr, 0, size - 1), work_arr, size); // cs_sort использует cs_dual_partition на FASM
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("Introsort", "C", introsort_c(work_arr, size), work_arr, size);
    MEASURE("Introsort", "FASM", introsort_fasm(work_arr, size), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("Merge Sort", "C", merge_sort_c(work_arr, temp_arr, 0, size - 1), work_arr, size);
    MEASURE("Merge Sort", "FASM", merge_sort_fasm(work_arr, temp_arr, 0, size - 1), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("Heap Sort", "C", heap_sort_c(work_arr, size), work_arr, size);
    MEASURE("Heap Sort", "FASM", heap_sort_fasm(work_arr, size), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("Radix Sort", "C", radix_sort_c(work_arr, temp_arr, size), work_arr, size);
    MEASURE("Radix Sort", "FASM", radix_sort_fasm(work_arr, temp_arr, size), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    if ((size & (size - 1)) == 0 && size > 0) {
        MEASURE("Bitonic Sort", "C", bitonic_sort_c(work_arr, 0, size, 1), work_arr, size);
        MEASURE("Bitonic Sort", "FASM", bitonic_sort_fasm(work_arr, 0, size, 1), work_arr, size);
    }
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("Timsort", "C", timsort_c(work_arr, temp_arr, size), work_arr, size);
    MEASURE("Timsort", "FASM", timsort_fasm(work_arr, temp_arr, size), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    MEASURE("PDQSort", "C", pdqsort_c(work_arr, size), work_arr, size);
    MEASURE("PDQSort", "FASM", pdqsort_fasm(work_arr, size), work_arr, size);
    printf("+------------------------+------------+-----------------+------------+\n");

    copy_arr(original, work_arr, size);
    uint64_t start_pm = __rdtsc();
    #pragma omp parallel
    {
        #pragma omp single
        parallel_merge_sort_c(work_arr, temp_arr, 0, size - 1);
    }
    uint64_t end_pm = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Parallel Merge", "C+OMP", end_pm - start_pm, check_sorted(work_arr, size) ? "\033[32mУспех\033[0m" : "\033[31mОшибка\033[0m");

    copy_arr(original, work_arr, size);
    uint64_t start_pq = __rdtsc();
    #pragma omp parallel
    {
        #pragma omp single
        parallel_quicksort_c(work_arr, 0, size - 1);
    }
    uint64_t end_pq = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Parallel Quick", "C+OMP", end_pq - start_pq, check_sorted(work_arr, size) ? "\033[32mУспех\033[0m" : "\033[31mОшибка\033[0m");
    printf("+------------------------+------------+-----------------+------------+\n");

    copy_arr(original, work_arr, size);
    uint64_t start_qs = __rdtsc();
    quickselect_c(work_arr, 0, size - 1, k);
    uint64_t end_qs = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Quickselect (k=N/2)", "C", end_qs - start_qs, "\033[33mN/A\033[0m");

    copy_arr(original, work_arr, size);
    start_qs = __rdtsc();
    quickselect_fasm(work_arr, 0, size - 1, k);
    end_qs = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Quickselect (k=N/2)", "FASM", end_qs - start_qs, "\033[33mN/A\033[0m");

    copy_arr(original, work_arr, size);
    uint64_t start_ps = __rdtsc();
    partial_sort_c(work_arr, size, k);
    uint64_t end_ps = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Partial Sort (k=N/2)", "C", end_ps - start_ps, check_sorted(work_arr, k) ? "\033[32mУспех\033[0m" : "\033[31mОшибка\033[0m");

    copy_arr(original, work_arr, size);
    start_ps = __rdtsc();
    partial_sort_fasm(work_arr, size, k);
    end_ps = __rdtsc();
    printf("| %-22s | %-10s | %-15lu | %-10s |\n", "Partial Sort (k=N/2)", "FASM", end_ps - start_ps, check_sorted(work_arr, k) ? "\033[32mУспех\033[0m" : "\033[31mОшибка\033[0m");
    printf("+------------------------+------------+-----------------+------------+\n");
}

void run_statistical_benchmark(int type_choice, int size, int runs, int* original, int* work_arr, int* temp_arr) {
    const char* type_name = "";
    switch (type_choice) {
        case 1: generate_random(original, size);     type_name = "Случайный"; break;
        case 2: generate_sorted(original, size);     type_name = "Отсортированный"; break;
        case 3: generate_reversed(original, size);   type_name = "Обратный порядок"; break;
        case 4: generate_duplicates(original, size); type_name = "С дубликатами"; break;
        case 5: type_name = "Генератор Ю.Н. Артамонова"; break;
    }

    int k = size / 2;
    printf("\n>>> СТАТИСТИЧЕСКИЙ БЕНЧМАРК [N = %d, Прогонов: %d, Тип: %s] <<<\n", size, runs, type_name);
    printf("+------------------------+------------+-----------------+--------------+\n");
    printf("| Алгоритм               | Язык       | Среднее (Такты) | Отклонение +-|\n");
    printf("+------------------------+------------+-----------------+--------------+\n");

    uint64_t* res_arr = (uint64_t*)malloc(runs * sizeof(uint64_t));

    MEASURE_STAT("Quick Sort (Hoare)", "C", quicksort_c(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("Quick Sort (Hoare)", "FASM", quicksort_fasm(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("Quick Sort (Branchless)", "FASM", quicksort_branchless_fasm(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("Quick (2 Threads)", "C", quicksort_two_threads_c(work_arr, size), work_arr, size, runs, res_arr);
    MEASURE_STAT("Quick (2 Threads)", "FASM", quicksort_two_threads_fasm(work_arr, size), work_arr, size, runs, res_arr);
    MEASURE_STAT("Dual-Pivot Quick", "C", dual_pivot_quicksort_c(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("Dual-Pivot Quick", "FASM", dual_pivot_quicksort_fasm(work_arr, 0, size - 1), work_arr, size, runs, res_arr);

    MEASURE_STAT("Triple-Pivot Quick", "C", triple_pivot_quicksort_c(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("Triple-Pivot Quick", "FASM", triple_pivot_quicksort_fasm(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("CS Sort", "C", cs_sort_c(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("CS Sort", "FASM", cs_sort(work_arr, 0, size - 1), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("Introsort", "C", introsort_c(work_arr, size), work_arr, size, runs, res_arr);
    MEASURE_STAT("Introsort", "FASM", introsort_fasm(work_arr, size), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("Merge Sort", "C", merge_sort_c(work_arr, temp_arr, 0, size - 1), work_arr, size, runs, res_arr);
    MEASURE_STAT("Merge Sort", "FASM", merge_sort_fasm(work_arr, temp_arr, 0, size - 1), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("Timsort", "C", timsort_c(work_arr, temp_arr, size), work_arr, size, runs, res_arr);
    MEASURE_STAT("Timsort", "FASM", timsort_fasm(work_arr, temp_arr, size), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("PDQSort", "C", pdqsort_c(work_arr, size), work_arr, size, runs, res_arr);
    MEASURE_STAT("PDQSort", "FASM", pdqsort_fasm(work_arr, size), work_arr, size, runs, res_arr);
    printf("+------------------------+------------+-----------------+--------------+\n");

    MEASURE_STAT("Quickselect", "C", quickselect_c(work_arr, 0, size - 1, k), work_arr, size, runs, res_arr);
    MEASURE_STAT("Quickselect", "FASM", quickselect_fasm(work_arr, 0, size - 1, k), work_arr, size, runs, res_arr);
    MEASURE_STAT("Partial Sort", "C", partial_sort_c(work_arr, size, k), work_arr, size, runs, res_arr);
    MEASURE_STAT("Partial Sort", "FASM", partial_sort_fasm(work_arr, size, k), work_arr, size, runs, res_arr);

    for (int r = 0; r < runs; r++) {
        copy_arr(original, work_arr, size);
        uint64_t start = __rdtsc();
        #pragma omp parallel
        {
            #pragma omp single
            parallel_quicksort_c(work_arr, 0, size - 1);
        }
        uint64_t end = __rdtsc();
        res_arr[r] = end - start;
    }
    print_stats("Parallel Quick", "C+OMP", res_arr, runs);
    printf("+------------------------+------------+-----------------+--------------+\n");

    free(res_arr);
}

int main() {
    srand(1337);

    while (1) {
        int size = 0;
        int type_choice = 0;

        printf("\n==================================================\n");
        printf(" ГЛАВНОЕ МЕНЮ БЕНЧМАРКА\n");
        printf("==================================================\n");
        printf(" Введите размер массива (или 0 для выхода): ");
        if (scanf("%d", &size) != 1 || size == 0) {
            printf("Выход из программы.\n");
            break;
        }
        if (size < 0) {
            printf("Ошибка: размер массива не может быть отрицательным.\n");
            continue;
        }

        printf("\n Выберите тип начального заполнения массива:\n");
        printf("  1. Случайные числа\n");
        printf("  2. Уже отсортированный массив\n");
        printf("  3. Обратный порядок элементов\n");
        printf("  4. Массив с большим количеством дубликатов\n");
        printf("  5. Последовательность Артамонова (CAlgorithm c=2)\n"); // НОВОЕ
        printf("  6. Прогнать все 5 типов последовательно\n"); // СДВИНУТО
        printf("  7. Запустить Cache-Aware бенчмарк (Исследование кэша)\n"); // СДВИНУТО
        printf("  8. Запустить сбор статистики\n"); // СДВИНУТО
        printf("  9. Экспорт данных В CSV (Для Python графиков)\n"); // СДВИНУТО
        printf("  0. Выход\n");
        printf(" Ваш выбор (0-9): ");

        if (scanf("%d", &type_choice) != 1 || type_choice == 0) {
            printf("Выход из программы.\n");
            break;
        }
        if (type_choice < 1 || type_choice > 9) {
            printf("Ошибка: неверный выбор.\n");
            continue;
        }

        if (type_choice == 7) {
            run_cache_aware_benchmark();
            continue;
        }

        int* original = (int*)malloc(size * sizeof(int));
        int* work_arr = (int*)malloc(size * sizeof(int));
        int* temp_arr = (int*)malloc(size * sizeof(int));

        if (!original || !work_arr || !temp_arr) {
            printf("Ошибка выделения памяти!\n");
            if (original) free(original);
            if (work_arr) free(work_arr);
            if (temp_arr) free(temp_arr);
            continue;
        }

        if (type_choice == 8) {
            int runs = 0;
            int stat_type = 0;
            printf("\n--- НАСТРОЙКА СТАТИСТИКИ ---\n");
            printf("Введите количество прогонов (например, 50 или 100): ");
            scanf("%d", &runs);
            printf("Выберите тип массива (1-Случайный, 2-Отсортированный, 3-Обратный порядок, 4-С дубликатами): ");
            scanf("%d", &stat_type);

            run_statistical_benchmark(stat_type, size, runs, original, work_arr, temp_arr);

            free(original);
            free(work_arr);
            free(temp_arr);
            continue;
        }

        if (type_choice == 9) {
            export_benchmark_to_csv();
            free(original);
            free(work_arr);
            free(temp_arr);
            continue;
        }

        if (type_choice == 5) {
            long long target_sum;
            double c_param;

            printf("Введите целевую сумму (n) для генерации: ");
            scanf("%lld", &target_sum);

            printf("Введите коэффициент c (строго > 1.0, например 2.0): ");
            scanf("%lf", &c_param);

            int actual_size = generate_artamonov(original, size, target_sum, c_param);

            printf("\nСгенерирована последовательность по алгоритму Ю.Н. Артамонова.\n");
            printf("Фактическая длина массива: %d элементов.\n", actual_size);

            size = actual_size;

            run_benchmark(type_choice, size, original, work_arr, temp_arr);

        } else if (type_choice >= 1 && type_choice <= 4) {
            run_benchmark(type_choice, size, original, work_arr, temp_arr);
        } else if (type_choice == 6) {
             for (int t = 1; t <= 4; t++) {
                 run_benchmark(t, size, original, work_arr, temp_arr);
             }
        }

        free(original);
        free(work_arr);
        free(temp_arr);
    }

    return 0;
}
