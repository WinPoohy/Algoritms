/*
 * Dedicated counted build of MyLib for the operation-count pass (menu 12 / 11.2).
 * Some build setups add ENABLE_FAST_BENCH globally to remove counter overhead from
 * the timing benchmark. Undefine it in this translation unit so g_cmp and g_mov_mylib
 * are active here. The separate mylib_fast.c translation unit remains uninstrumented.
 */
#ifdef ENABLE_FAST_BENCH
#undef ENABLE_FAST_BENCH
#endif
#ifdef MYLIB_NO_MAIN
#undef MYLIB_NO_MAIN
#endif
#ifndef ENABLE_ATOMIC_COUNTERS
#define ENABLE_ATOMIC_COUNTERS 1
#endif
#include "mylib.c"
