#define _GNU_SOURCE
#include "sorts.h"
#include "killers.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

extern int64_t g_cmp;
extern void get_sequence(uint64_t n, double c, uint64_t **ar, uint64_t *size);
extern void cs_sort(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);
extern void cs_sort_fast(uint64_t *Q, uint64_t *S, uint64_t len_Q, uint64_t len_S);

static int cmp_i(const void *a,const void*b){int x=*(const int*)a,y=*(const int*)b;return (x>y)-(x<y);}
static uint64_t ticks(void){
#if defined(__x86_64__) || defined(__i386__)
 __asm__ __volatile__("lfence" ::: "memory"); unsigned int lo,hi; __asm__ __volatile__("rdtsc":"=a"(lo),"=d"(hi)); __asm__ __volatile__("lfence" ::: "memory"); return ((uint64_t)hi<<32)|lo;
#else
 struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;
#endif
}
static int cmp_u64(const void*a,const void*b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return (x>y)-(x<y);}
static uint64_t median(uint64_t *v,int n){for(int i=1;i<n;i++){uint64_t x=v[i];int j=i;while(j&&v[j-1]>x){v[j]=v[j-1];--j;}v[j]=x;}return v[n/2];}
static void report_case(const char *name,uint64_t *src,int n){
 uint64_t *expected=malloc((size_t)n*sizeof(*expected)),*w64=malloc((size_t)n*sizeof(*w64)),*S=NULL,*ranked=malloc((size_t)n*sizeof(*ranked)); int *src_i=malloc((size_t)n*sizeof(*src_i)),*w_i=malloc((size_t)n*sizeof(*w_i)),*expected_i=malloc((size_t)n*sizeof(*expected_i));
 if(!expected||!w64||!ranked||!src_i||!w_i||!expected_i){fprintf(stderr,"OOM\n");exit(2);}
 memcpy(expected,src,(size_t)n*sizeof(*src));qsort(expected,n,sizeof(*expected),cmp_u64);
 uint64_t lenS=0,maxv=0; for(int i=0;i<n;i++)if(src[i]>maxv)maxv=src[i];
 memcpy(ranked,src,(size_t)n*sizeof(*ranked)); qsort(ranked,n,sizeof(*ranked),cmp_u64);
 for(int i=0;i<n;i++){size_t lo=0,hi=(size_t)n;while(lo<hi){size_t m=lo+(hi-lo)/2;if(ranked[m]<src[i])lo=m+1;else hi=m;}src_i[i]=(int)lo;}
 memcpy(expected_i,src_i,(size_t)n*sizeof(*src_i));qsort(expected_i,n,sizeof(*expected_i),cmp_i);
 get_sequence(maxv?maxv:1,2.0,&S,&lenS);
 enum{R=7}; uint64_t tc[R],tm[R]; int okc=1,okm=1; int64_t ccm=0;
 for(int r=0;r<R;r++){
  memcpy(w_i,src_i,(size_t)n*sizeof(*src_i));uint64_t a=ticks();cs_sort_c(w_i,0,n-1);tc[r]=ticks()-a;
  if(memcmp(w_i,expected_i,(size_t)n*sizeof(*w_i)))okc=0;
  memcpy(w64,src,(size_t)n*sizeof(*src));uint64_t b=ticks();cs_sort_fast(w64,S,n,lenS);tm[r]=ticks()-b;
  if(memcmp(w64,expected,(size_t)n*sizeof(*w64)))okm=0;
  memcpy(w64,src,(size_t)n*sizeof(*src));cs_sort(w64,S,n,lenS); ccm=g_cmp;
  if(memcmp(w64,expected,(size_t)n*sizeof(*w64)))okm=0;
 }
 printf("%-19s N=%-7d C3 median=%10llu ticks (%6.2f/n) | mylib cs_sort=%10llu (%6.2f/n), g_cmp=%lld | valid C/mylib=%d/%d\n",name,n,(unsigned long long)median(tc,R),(double)median(tc,R)/n,(unsigned long long)median(tm,R),(double)median(tm,R)/n,(long long)ccm,okc,okm);
 free(expected);free(w64);free(ranked);free(S);free(src_i);free(w_i);free(expected_i);
}
int main(int argc,char**argv){int n=argc>1?atoi(argv[1]):65536;if(n<1){return 2;}uint64_t *a=malloc((size_t)n*sizeof(*a));if(!a)return 2;
 killer_set_seed(42);killer_cs_sort(a,n);report_case("CS-killer",a,n);
 for (int i = 0; i < n; ++i) a[i] = (uint64_t)(rand() & INT_MAX);
 report_case("random-int", a, n);
 for (int i = 0; i < n; ++i) a[i] = (uint64_t)i;
 report_case("sorted", a, n);
 for (int i = 0; i < n; ++i) a[i] = (uint64_t)(n - i);
 report_case("reverse", a, n);
 for (int i = 0; i < n; ++i) a[i] = (uint64_t)(rand() % 5);
 report_case("duplicates-5", a, n);
 for (int i = 0; i < n; ++i) a[i] = 123456;
 report_case("all-equal", a, n);
 free(a);return 0;
}
