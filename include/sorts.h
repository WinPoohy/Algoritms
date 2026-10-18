#ifndef SORTS_H
#define SORTS_H
#include <stdint.h>
#include <stddef.h>

void insertion_sort_c(int *arr, int n);
void quicksort_c(int *arr, int low, int high);
void quicksort_two_threads_c(int *arr, int n);
void dual_pivot_quicksort_c(int *arr, int low, int high);
void merge_sort_c(int *arr, int *temp, int left, int right);
void heap_sort_c(int *arr, int n);
void radix_sort_c(int *arr, int *temp, int n);
void bitonic_sort_c(int *arr, int low, int cnt, int dir);
void parallel_merge_sort_c(int *arr, int *temp, int left, int right);
void introsort_c(int *arr, int n);
void timsort_c(int *arr, int *temp, int n);
void pdqsort_c(int *arr, int n);
int quickselect_c(int *arr, int low, int high, int k);
void partial_sort_c(int *arr, int n, int k);
void parallel_quicksort_c(int *arr, int low, int high);
void triple_pivot_quicksort_c(int *arr, int low, int high);
void cs_sort_c(int *arr, int low, int high);
void cs3_opt_c(int *arr, int low, int high);
void cs3_gs_sort_c(int *arr, int low, int high);

/* Full-width unsigned variants: original CS3 and CS3-GS hybrid. */
void cs_sort_u64_c(uint64_t *arr, int low, int high);
void cs3_gs_sort_u64_c(uint64_t *arr, int low, int high);
int cs3_gs_make_thresholds_u64(const uint64_t *arr, int low, int high, uint64_t thresholds[3]);
void cs3_heap_sort_u64_c(uint64_t *arr, int64_t n);
int cs3_gs_make_thresholds(const int *arr, int low, int high, int64_t thresholds[3]);

/* FASM entry points (defined when sorts.asm is assembled and linked). */
void insertion_sort_fasm(int *arr, int n);
void quicksort_fasm(int *arr, int low, int high);
void quicksort_two_threads_fasm(int *arr, int n);
void quicksort_branchless_fasm(int *arr, int low, int high);
void dual_pivot_quicksort_fasm(int *arr, int low, int high);
void merge_sort_fasm(int *arr, int *temp, int low, int high);
void heap_sort_fasm(int *arr, int n);
void radix_sort_fasm(int *arr, int *temp, int n);
void bitonic_sort_fasm(int *arr, int low, int cnt, int dir);
void introsort_fasm(int *arr, int n);
void timsort_fasm(int *arr, int *temp, int n);
void pdqsort_fasm(int *arr, int n);
void triple_pivot_quicksort_fasm(int *arr, int low, int high);
void cs_sort_fasm(int *arr, int low, int high);
void cs3_opt_fasm(int *arr, int low, int high);
void cs3_gs_sort_fasm(int *arr, int low, int high);
void cs_sort_u64_fasm(uint64_t *arr, int low, int high);
void cs3_gs_sort_u64_fasm(uint64_t *arr, int low, int high);
void quickselect_fasm(int *arr, int low, int high, int k);
void partial_sort_fasm(int *arr, int n, int k);

#endif
