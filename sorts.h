#ifndef SORTS_H
#define SORTS_H

// C
void insertion_sort_c(int* arr, int n);
void quicksort_c(int* arr, int low, int high);
void quicksort_two_threads_c(int* arr, int n); // new
void dual_pivot_quicksort_c(int* arr, int low, int high); // new
void merge_sort_c(int* arr, int* temp, int left, int right);
void heap_sort_c(int* arr, int n);
void radix_sort_c(int* arr, int* temp, int n);
void bitonic_sort_c(int* arr, int low, int cnt, int dir);
void parallel_merge_sort_c(int* arr, int* temp, int left, int right);
void introsort_c(int* arr, int n);
void timsort_c(int* arr, int* temp, int n);
void pdqsort_c(int* arr, int n);
void triple_pivot_quicksort_c(int* arr, int low, int high); // new

// Поиск k-й порядковой статистики (Quickselect)
int quickselect_c(int* arr, int low, int high, int k); // new
// Частичная сортировка (первые k элементов)
void partial_sort_c(int* arr, int n, int k); // mew
// Параллельная быстрая сортировка (OpenMP / pthread)
void parallel_quicksort_c(int* arr, int low, int high); // new
void cs_dual_partition_c(int* arr, int len, int p1, int p2, int* out_i, int* out_j); //new

// FASM
extern void insertion_sort_fasm(int* arr, int n);
extern void quicksort_fasm(int* arr, int low, int high);
extern void quicksort_two_threads_fasm(int* arr, int n); // new
extern void dual_pivot_quicksort_fasm(int* arr, int low, int high); // new
extern void merge_sort_fasm(int* arr, int* temp, int left, int right);
extern void heap_sort_fasm(int* arr, int n);
extern void quicksort_branchless_fasm(int* arr, int low, int high);
extern void radix_sort_fasm(int* arr, int* temp, int n);
extern void bitonic_sort_fasm(int* arr, int low, int cnt, int dir);
extern void introsort_fasm(int* arr, int n);
extern void timsort_fasm(int* arr, int* temp, int n);
extern void pdqsort_fasm(int* arr, int n);
extern void triple_pivot_quicksort_fasm(int* arr, int low, int high); // new
// Поиск k-й порядковой статистики (Quickselect)
extern int quickselect_fasm(int* arr, int low, int high, int k); // new
// Частичная сортировка (первые k элементов)
extern void partial_sort_fasm(int* arr, int n, int k); // new
// cs_sort
extern void cs_dual_partition(int* arr, int len, int p1, int p2, int* out_i, int* out_j);

#endif
