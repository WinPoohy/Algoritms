#ifndef SORTS_INSTRUMENTED_H
#define SORTS_INSTRUMENTED_H
#include <stdint.h>
#include "sorts.h"
extern uint64_t g_cmp_instr;
extern uint64_t g_mov_instr;
void cs3_opt_c(int *arr, int low, int high);
void cs3_opt_c_instr(int *arr, int low, int high);
void cs_sort_c_instr(int *arr, int low, int high);
void cs3_gs_sort_c_instr(int *arr, int low, int high);
void cs_sort_u64_c_instr(uint64_t *arr, int low, int high);
void cs3_gs_sort_u64_c_instr(uint64_t *arr, int low, int high);
void dual_pivot_quicksort_c_instr(int *arr, int low, int high);
void introsort_c_instr(int *arr, int n);
void pdqsort_c_instr(int *arr, int n);
void triple_pivot_quicksort_c_instr(int *arr, int low, int high);
void cs3_gs_sort_fasm_instr(int *arr, int low, int high);
void cs_sort_u64_fasm_instr(uint64_t *arr, int low, int high);
void cs3_gs_sort_u64_fasm_instr(uint64_t *arr, int low, int high);
void dual_pivot_quicksort_fasm_instr(int *arr, int low, int high);
void introsort_fasm_instr(int *arr, int n);
void pdqsort_fasm_instr(int *arr, int n);
void triple_pivot_quicksort_fasm_instr(int *arr, int low, int high);
uint64_t calculate_runs(const int *arr, int n);
uint64_t calculate_inversions(const int *arr, int n);
#endif
