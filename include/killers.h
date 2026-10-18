#ifndef KILLERS_H
#define KILLERS_H
#include <stdint.h>
void killer_set_seed(uint64_t seed);
void killer_pipeorgan(uint64_t *A, int n);
void killer_m3killer(uint64_t *A, int n);
void killer_pipeorganmx(uint64_t *A, int n, uint64_t max_val);
void killer_unevenseq(uint64_t *A, int n, uint64_t max_val);
void killer_unevensorted(uint64_t *A, int n, uint64_t max_val);
void killer_0to255(uint64_t *A, int n);
void killer_rep(uint64_t *A, int n, uint64_t k);
void killer_mag(uint64_t *A, int n, int shift);
void killer_rep9010(uint64_t *A, int n, uint64_t max_val);
void killer_binary(uint64_t *A, int n, uint64_t vlo, uint64_t vhi);
void killer_ternary(uint64_t *A, int n, uint64_t v0, uint64_t v1, uint64_t v2);
void killer_quintary(uint64_t *A, int n, const uint64_t *values);
void killer_cs_sort(uint64_t *A, int n);
#endif
