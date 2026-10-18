#include "sorts_instrumented.h"
#include "sorts.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>


uint64_t g_cmp_instr = 0;
uint64_t g_mov_instr = 0;

static inline void swap_instr(int* a, int* b) {
    int t = *a; *a = *b; *b = t;
    g_mov_instr += 3;
}

// ----------------------------------------------------------------------------
// CS3 / CS_Sort: instrumented implementation matching sorts_c.c
// g_cmp_instr counts key-to-key comparisons; g_mov_instr counts writes to
// array elements (including the three writes for an exchange).
// ----------------------------------------------------------------------------
static void insertion_range_instr(int *arr, int low, int high) {
    for (int i = low + 1; i <= high; ++i) {
        const int key = arr[i];
        int j = i - 1;
        while (j >= low) {
            ++g_cmp_instr;
            if (arr[j] <= key) break;
            arr[j + 1] = arr[j];
            ++g_mov_instr;
            --j;
        }
        arr[j + 1] = key;
        ++g_mov_instr;
    }
}

static void sift_down_instr(int *arr, int n, int root) {
    for (;;) {
        int largest = root;
        const int left = root * 2 + 1;
        const int right = left + 1;
        if (left < n) {
            ++g_cmp_instr;
            if (arr[left] > arr[largest]) largest = left;
        }
        if (right < n) {
            ++g_cmp_instr;
            if (arr[right] > arr[largest]) largest = right;
        }
        if (largest == root) return;
        swap_instr(&arr[root], &arr[largest]);
        root = largest;
    }
}

static void heap_range_instr(int *arr, int low, int high) {
    int *base = arr + low;
    const int n = high - low + 1;
    for (int i = n / 2; i > 0; --i) sift_down_instr(base, n, i - 1);
    for (int end = n - 1; end > 0; --end) {
        swap_instr(&base[0], &base[end]);
        sift_down_instr(base, end, 0);
    }
}

static bool cs3_try_small_alphabet_instr(int *arr, int low, int high) {
    int keys[5];
    size_t counts[5] = {0, 0, 0, 0, 0};
    size_t used = 0;
    for (int i = low; i <= high; ++i) {
        size_t k = 0;
        while (k < used) {
            ++g_cmp_instr;
            if (keys[k] == arr[i]) break;
            ++k;
        }
        if (k == used) {
            if (used == 5) return false;
            keys[used] = arr[i];
            counts[used] = 0;
            ++used;
        }
        ++counts[k];
    }
    if (used <= 1) return true;

    for (size_t i = 1; i < used; ++i) {
        const int key = keys[i];
        const size_t count = counts[i];
        size_t j = i;
        while (j > 0) {
            ++g_cmp_instr;
            if (keys[j - 1] <= key) break;
            keys[j] = keys[j - 1];
            counts[j] = counts[j - 1];
            --j;
        }
        keys[j] = key;
        counts[j] = count;
    }
    int out = low;
    for (size_t k = 0; k < used; ++k)
        for (size_t repeat = 0; repeat < counts[k]; ++repeat) {
            arr[out++] = keys[k];
            ++g_mov_instr;
        }
    return true;
}

static void cs_sort_inner_instr(int *arr, int low, int high, int depth_limit) {
    while (low < high) {
        const int n = high - low + 1;
        if (n < 24) {
            insertion_range_instr(arr, low, high);
            return;
        }
        if (depth_limit <= 0) {
            heap_range_instr(arr, low, high);
            return;
        }
        --depth_limit;

        int sample[5] = {
            arr[low], arr[low + n / 4], arr[low + n / 2],
            arr[low + (n / 4) * 3 + ((n % 4) * 3) / 4], arr[high]
        };
#define CS3_INSTR_SORT2(a, b) do { ++g_cmp_instr; if ((a) > (b)) { int t_ = (a); (a) = (b); (b) = t_; } } while (0)
        CS3_INSTR_SORT2(sample[0], sample[1]);
        CS3_INSTR_SORT2(sample[3], sample[4]);
        CS3_INSTR_SORT2(sample[2], sample[4]);
        CS3_INSTR_SORT2(sample[2], sample[3]);
        CS3_INSTR_SORT2(sample[1], sample[4]);
        CS3_INSTR_SORT2(sample[0], sample[3]);
        CS3_INSTR_SORT2(sample[0], sample[2]);
        CS3_INSTR_SORT2(sample[1], sample[3]);
        CS3_INSTR_SORT2(sample[1], sample[2]);
#undef CS3_INSTR_SORT2
        const int p1 = sample[1];
        const int p2 = sample[2];
        const int p3 = sample[3];

        ++g_cmp_instr;
        bool duplicate_sample = (sample[0] == sample[1]);
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[1] == sample[2]);
        }
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[2] == sample[3]);
        }
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[3] == sample[4]);
        }
        if (duplicate_sample) {
            if (cs3_try_small_alphabet_instr(arr, low, high)) return;

            int lt = low, i = low, gt = high;
            while (i <= gt) {
                const int value = arr[i];
                ++g_cmp_instr;
                if (value < p2) {
                    const int old = arr[lt];
                    arr[i] = old;
                    arr[lt] = value;
                    g_mov_instr += 2;
                    ++lt;
                    ++i;
                } else {
                    ++g_cmp_instr;
                    if (value > p2) {
                        const int old = arr[gt];
                        arr[i] = old;
                        arr[gt] = value;
                        g_mov_instr += 2;
                        --gt;
                    } else {
                        ++i;
                    }
                }
            }

            if (lt - low < high - gt) {
                cs_sort_inner_instr(arr, low, lt - 1, depth_limit);
                low = gt + 1;
            } else {
                cs_sort_inner_instr(arr, gt + 1, high, depth_limit);
                high = lt - 1;
            }
            continue;
        }

        int lt = low, b = low, i = low, gt = high;
        while (i <= gt) {
            const int value = arr[i];
            ++g_cmp_instr;
            if (value < p2) {
                ++g_cmp_instr;
                if (value < p1) {
                    const int at_b = arr[b];
                    const int at_lt = arr[lt];
                    arr[i] = at_b;
                    arr[b] = at_lt;
                    arr[lt] = value;
                    g_mov_instr += 3;
                    ++lt; ++b; ++i;
                } else {
                    arr[i] = arr[b];
                    arr[b] = value;
                    g_mov_instr += 2;
                    ++b; ++i;
                }
            } else {
                ++g_cmp_instr;
                if (value < p3) {
                    ++i;
                } else {
                    arr[i] = arr[gt];
                    arr[gt] = value;
                    g_mov_instr += 2;
                    --gt;
                }
            }
        }

        const int m0 = lt, m1 = b, m2 = i;
        cs_sort_inner_instr(arr, low, m0 - 1, depth_limit);
        cs_sort_inner_instr(arr, m0, m1 - 1, depth_limit);
        cs_sort_inner_instr(arr, m1, m2 - 1, depth_limit);
        cs_sort_inner_instr(arr, m2, high, depth_limit);
        return;
    }
}

static int initial_depth_limit(int n) {
    int depth = 0;
    for (int t = n; t > 1; t >>= 1) depth += 2;
    return depth;
}

void cs3_opt_c(int *arr, int left, int right) {
    cs_sort_c(arr, left, right);
}

void cs3_opt_c_instr(int *arr, int left, int right) {
    if (arr == NULL || left >= right) return;
    cs_sort_inner_instr(arr, left, right,
                        initial_depth_limit(right - left + 1));
}

void cs_sort_c_instr(int *arr, int low, int high) {
    cs3_opt_c_instr(arr, low, high);
}


/* Instrumented counterpart of the CS3-GS hybrid from sorts_c.c.
 * g_cmp_instr counts each executed comparison against min/max, generated
 * thresholds and the three data-element pivots used in the CS3 leaf sorter.
 * g_mov_instr counts writes to array slots. */
static int cs3_gs_make_thresholds_instr(const int *arr, int low, int high,
                                        int64_t thresholds[3]) {
    if (arr == NULL || thresholds == NULL || low < 0 || high <= low)
        return 0;
    const int64_t n64 = (int64_t)high - (int64_t)low + 1;
    if (n64 < 256 || n64 > INT_MAX)
        return 0;
    const int n = (int)n64;
    const int sample_offsets[9] = {
        0, (int)(((int64_t)n * 1) / 8), (int)(((int64_t)n * 2) / 8),
        (int)(((int64_t)n * 3) / 8), (int)(((int64_t)n * 4) / 8),
        (int)(((int64_t)n * 5) / 8), (int)(((int64_t)n * 6) / 8),
        (int)(((int64_t)n * 7) / 8), n - 1
    };
    int sample_values[9];
    for (size_t j = 0; j < 9; ++j)
        sample_values[j] = arr[low + sample_offsets[j]];

    /* Match the fast version's monotonic-input guard. Count each key
       comparison actually executed by this probe. */
    int sample_nondecreasing = 1;
    int sample_nonincreasing = 1;
    for (size_t j = 1; j < 9; ++j) {
        ++g_cmp_instr;
        if (sample_values[j] < sample_values[j - 1])
            sample_nondecreasing = 0;
        ++g_cmp_instr;
        if (sample_values[j] > sample_values[j - 1])
            sample_nonincreasing = 0;
    }
    if (sample_nondecreasing || sample_nonincreasing)
        return 0;

    int sample_min = sample_values[0];
    int sample_max = sample_values[0];
    for (size_t j = 1; j < 9; ++j) {
        ++g_cmp_instr;
        if (sample_values[j] < sample_min) sample_min = sample_values[j];
        ++g_cmp_instr;
        if (sample_values[j] > sample_max) sample_max = sample_values[j];
    }
    const uint64_t sample_span = (uint64_t)((int64_t)sample_max - (int64_t)sample_min);
    const uint64_t narrow_cutoff = (uint64_t)n / 16u;
    if (sample_span < 32 || sample_span < narrow_cutoff)
        return 0;

    /* Match the fast variant: the sample range estimates the value domain,
       avoiding a full second scan just to discover exact min/max. */
    const uint64_t span = sample_span;
    if (span < 32)
        return 0;

    const uint64_t s0 = span / 2u + span % 2u;
    const uint64_t rem = span - s0;
    const uint64_t s1 = rem / 2u + rem % 2u;
    if (s0 == 0 || s1 == 0)
        return 0;
    const int64_t base = (int64_t)sample_min;
    thresholds[0] = base + (int64_t)s1;
    thresholds[1] = base + (int64_t)s0;
    thresholds[2] = base + (int64_t)s0 + (int64_t)s1;

    unsigned bucket_counts[4] = {0, 0, 0, 0};
    for (size_t j = 0; j < 9; ++j) {
        const int64_t value = sample_values[j];
        unsigned bucket;
        ++g_cmp_instr;
        if (value < thresholds[0]) bucket = 0;
        else {
            ++g_cmp_instr;
            if (value < thresholds[1]) bucket = 1;
            else {
                ++g_cmp_instr;
                if (value < thresholds[2]) bucket = 2;
                else bucket = 3;
            }
        }
        ++bucket_counts[bucket];
    }
    for (size_t b = 0; b < 4; ++b)
        if (bucket_counts[b] >= 5) return 0;
    return 1;
}

static void cs3_gs_partition4_instr(int *arr, int low, int high,
                                    const int64_t thresholds[3],
                                    int *m0, int *m1, int *m2) {
    int lt = low, b = low, i = low, gt = high;
    while (i <= gt) {
        const int value = arr[i];
        ++g_cmp_instr;
        if ((int64_t)value < thresholds[0]) {
            if (i != b) { arr[i] = arr[b]; ++g_mov_instr; }
            if (b != lt) { arr[b] = arr[lt]; ++g_mov_instr; }
            if (lt != i) { arr[lt] = value; ++g_mov_instr; }
            ++lt; ++b; ++i;
        } else {
            ++g_cmp_instr;
            if ((int64_t)value < thresholds[1]) {
                if (i != b) {
                    arr[i] = arr[b];
                    arr[b] = value;
                    g_mov_instr += 2;
                }
                ++b; ++i;
            } else {
                ++g_cmp_instr;
                if ((int64_t)value < thresholds[2]) {
                    ++i;
                } else {
                    if (i != gt) {
                        arr[i] = arr[gt];
                        arr[gt] = value;
                        g_mov_instr += 2;
                    }
                    --gt;
                }
            }
        }
    }
    *m0 = lt; *m1 = b; *m2 = i;
}

void cs3_gs_sort_c_instr(int *arr, int low, int high) {
    if (arr == NULL || low >= high)
        return;
    int64_t thresholds[3];
    if (!cs3_gs_make_thresholds_instr(arr, low, high, thresholds)) {
        cs_sort_c_instr(arr, low, high);
        return;
    }
    int m0, m1, m2;
    cs3_gs_partition4_instr(arr, low, high, thresholds, &m0, &m1, &m2);
    cs_sort_inner_instr(arr, low, m0 - 1, initial_depth_limit(m0 - low));
    cs_sort_inner_instr(arr, m0, m1 - 1, initial_depth_limit(m1 - m0));
    cs_sort_inner_instr(arr, m1, m2 - 1, initial_depth_limit(m2 - m1));
    cs_sort_inner_instr(arr, m2, high, initial_depth_limit(high - m2 + 1));
}

// ----------------------------------------------------------------------------
// АНАЛИТИКА ЭНТРОПИИ: Runs & Kendall's Tau (Inversions)
// ----------------------------------------------------------------------------
uint64_t calculate_runs(const int* arr, int n) {
    if (n <= 1) return 1;
    uint64_t runs = 1;
    int is_asc = (arr[1] >= arr[0]);
    for (int i = 1; i < n - 1; i++) {
        int curr_asc = (arr[i+1] >= arr[i]);
        if (curr_asc != is_asc) {
            runs++;
            is_asc = curr_asc;
        }
    }
    return runs;
}

static uint64_t merge_count(int* arr, int* temp, int left, int mid, int right) {
    int i = left, j = mid, k = left;
    uint64_t inv_count = 0;
    while ((i <= mid - 1) && (j <= right)) {
        if (arr[i] <= arr[j]) temp[k++] = arr[i++];
        else { temp[k++] = arr[j++]; inv_count += (mid - i); }
    }
    while (i <= mid - 1) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];
    for (i = left; i <= right; i++) arr[i] = temp[i];
    return inv_count;
}

static uint64_t merge_sort_count(int* arr, int* temp, int left, int right) {
    uint64_t inv_count = 0;
    if (right > left) {
        int mid = (right + left) / 2;
        inv_count += merge_sort_count(arr, temp, left, mid);
        inv_count += merge_sort_count(arr, temp, mid + 1, right);
        inv_count += merge_count(arr, temp, left, mid + 1, right);
    }
    return inv_count;
}

uint64_t calculate_inversions(const int* arr, int n) {
    int* temp = (int*)malloc(n * sizeof(int));
    int* copy = (int*)malloc(n * sizeof(int));
    memcpy(copy, arr, n * sizeof(int));
    uint64_t inv = merge_sort_count(copy, temp, 0, n - 1);
    free(temp); free(copy);
    return inv;
}

// ============================================================================
// ДОПОЛНЕНИЯ К ФАЙЛУ sorts_instrumented.c
// ============================================================================

// Вспомогательная функция обмена с учетом mov-инструментирования
static inline void swap_instr_c(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
    g_mov_instr += 3;
}

// ----------------------------------------------------------------------------
// 1. dual_pivot_quicksort_c_instr — Dual-Pivot Quicksort с защитой глубины.
// На плохих разбиениях остаток обрабатывает instrumented heapsort, чтобы
// сравнения/перемещения продолжали учитываться счётчиками g_cmp_instr/g_mov_instr.
// ----------------------------------------------------------------------------
static void dual_pivot_quicksort_c_instr_impl(int *arr, int low, int high,
                                              int depth_limit) {
    while (low < high) {
        const int count = high - low + 1;
        if (count < 24) {
            insertion_range_instr(arr, low, high);
            return;
        }
        if (depth_limit <= 0) {
            heap_range_instr(arr, low, high);
            return;
        }
        --depth_limit;

        ++g_cmp_instr;
        if (arr[low] > arr[high]) swap_instr(&arr[low], &arr[high]);
        const int p1 = arr[low];
        const int p2 = arr[high];
        g_mov_instr += 2;

        int less = low + 1;
        int great = high - 1;
        int k = low + 1;
        while (k <= great) {
            ++g_cmp_instr;
            if (arr[k] < p1) {
                swap_instr(&arr[k], &arr[less]);
                ++less;
            } else {
                ++g_cmp_instr;
                if (arr[k] > p2) {
                    while (k < great) {
                        ++g_cmp_instr;
                        if (arr[great] <= p2) break;
                        --great;
                    }
                    swap_instr(&arr[k], &arr[great]);
                    --great;
                    ++g_cmp_instr;
                    if (arr[k] < p1) {
                        swap_instr(&arr[k], &arr[less]);
                        ++less;
                    }
                }
            }
            ++k;
        }

        --less;
        ++great;
        swap_instr(&arr[low], &arr[less]);
        swap_instr(&arr[high], &arr[great]);

        dual_pivot_quicksort_c_instr_impl(arr, low, less - 1, depth_limit);
        dual_pivot_quicksort_c_instr_impl(arr, less + 1, great - 1, depth_limit);
        low = great + 1;  // хвостовая итерация правой части
    }
}

void dual_pivot_quicksort_c_instr(int *arr, int low, int high) {
    if (arr == NULL || low >= high) {
        ++g_cmp_instr;
        return;
    }
    int depth_limit = 0;
    for (int count = high - low + 1; count > 1; count >>= 1)
        depth_limit += 2;
    dual_pivot_quicksort_c_instr_impl(arr, low, high, depth_limit);
}

/* Called from sorts_instrumented.asm when its dual-pivot partition stalls. */
void dual_pivot_fallback_heap_instr(int *arr, int n) {
    if (arr != NULL && n > 1) heap_range_instr(arr, 0, n - 1);
}

// ----------------------------------------------------------------------------
// 2. introsort_c_instr (Интроспективная сортировка)
// ----------------------------------------------------------------------------
static void introsort_loop_c_instr(int* arr, int low, int high, int depth_limit) {
    g_cmp_instr++;
    if (high - low < 16) return;

    g_cmp_instr++;
    if (depth_limit == 0) {
        heap_sort_c(arr + low, high - low + 1);
        return;
    }

    int pivot = arr[low + (high - low) / 2];
    g_mov_instr++;
    int i = low - 1;
    int j = high + 1;

    while (1) {
        do {
            i++;
            g_cmp_instr++;
        } while (arr[i] < pivot);
        do {
            j--;
            g_cmp_instr++;
        } while (arr[j] > pivot);

        g_cmp_instr++;
        if (i >= j) break;
        swap_instr_c(&arr[i], &arr[j]);
    }

    introsort_loop_c_instr(arr, low, j, depth_limit - 1);
    introsort_loop_c_instr(arr, j + 1, high, depth_limit - 1);
}

void introsort_c_instr(int* arr, int n) {
    g_cmp_instr++;
    if (n <= 1) return;
    int depth_limit = 0;
    for (int temp = n; temp > 1; temp >>= 1) {
        depth_limit++;
    }
    depth_limit *= 2;
    introsort_loop_c_instr(arr, 0, n - 1, depth_limit);

    // Финализация вставками
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        g_mov_instr++;
        int j = i - 1;
        while (j >= 0) {
            g_cmp_instr++;
            if (!(arr[j] > key)) break;
            arr[j + 1] = arr[j];
            g_mov_instr++;
            j--;
        }
        arr[j + 1] = key;
        g_mov_instr++;
    }
}

// ----------------------------------------------------------------------------
// 3. pdqsort_c_instr (Pattern-Defeating Quicksort)
// ----------------------------------------------------------------------------
static void pdqsort_loop_c_instr(int* arr, int low, int high, int depth_limit) {
    g_cmp_instr++;
    if (high - low < 16) return;

    g_cmp_instr++;
    if (depth_limit == 0) {
        heap_sort_c(arr + low, high - low + 1);
        return;
    }

    int mid = low + (high - low) / 2;
    int pivot = arr[mid];
    arr[mid] = arr[high];
    arr[high] = pivot;
    g_mov_instr += 3;

    int i = low - 1;
    for (int j = low; j < high; j++) {
        g_cmp_instr++;
        int is_less = (arr[j] < pivot);
        i += is_less;
        int swap_idx = is_less ? i : j;

        int temp_val = arr[j];
        arr[j] = arr[swap_idx];
        arr[swap_idx] = temp_val;
        g_mov_instr += 3;
    }
    i++;
    arr[high] = arr[i];
    arr[i] = pivot;
    g_mov_instr += 3;

    pdqsort_loop_c_instr(arr, low, i - 1, depth_limit - 1);
    pdqsort_loop_c_instr(arr, i + 1, high, depth_limit - 1);
}

void pdqsort_c_instr(int* arr, int n) {
    g_cmp_instr++;
    if (n <= 1) return;
    int depth_limit = 0;
    for (int temp = n; temp > 1; temp >>= 1) depth_limit++;
    depth_limit *= 2;

    pdqsort_loop_c_instr(arr, 0, n - 1, depth_limit);

    for (int i = 1; i < n; i++) {
        int key = arr[i];
        g_mov_instr++;
        int j = i - 1;
        while (j >= 0) {
            g_cmp_instr++;
            if (!(arr[j] > key)) break;
            arr[j + 1] = arr[j];
            g_mov_instr++;
            j--;
        }
        arr[j + 1] = key;
        g_mov_instr++;
    }
}

static void insertion_range_triple_instr(int *arr, int low, int high) {
    for (int i = low + 1; i <= high; ++i) {
        const int key = arr[i];
        int j = i - 1;
        while (j >= low) {
            ++g_cmp_instr;
            if (arr[j] <= key) break;
            arr[j + 1] = arr[j];
            ++g_mov_instr;
            --j;
        }
        arr[j + 1] = key;
        ++g_mov_instr;
    }
}

/* Instrumented copy of triple_pivot_quicksort_c. Partition and recursion
 * boundaries intentionally mirror the non-instrumented implementation. */
static void triple_pivot_quicksort_c_instr_impl(int *arr, int low, int high,
                                                  int depth_limit) {
    if (low >= high) return;
    if (depth_limit <= 0) {
        heap_range_instr(arr, low, high);
        return;
    }
    if (high - low < 24) {
        insertion_range_triple_instr(arr, low, high);
        return;
    }

    const int mid = low + (high - low) / 2;
    int t_mid = arr[low + 1];
    arr[low + 1] = arr[mid]; arr[mid] = t_mid;
    g_mov_instr += 2;

    int p1 = arr[low], p2 = arr[low + 1], p3 = arr[high];
    g_mov_instr += 3;
    ++g_cmp_instr; if (p1 > p2) { int t = p1; p1 = p2; p2 = t; }
    ++g_cmp_instr; if (p1 > p3) { int t = p1; p1 = p3; p3 = t; }
    ++g_cmp_instr; if (p2 > p3) { int t = p2; p2 = p3; p3 = t; }

    ++g_cmp_instr;
    if (p1 == p2) { heap_range_instr(arr, low, high); return; }
    ++g_cmp_instr;
    if (p2 == p3) { heap_range_instr(arr, low, high); return; }

    arr[low] = p1; arr[low + 1] = p2; arr[high] = p3;
    g_mov_instr += 3;
    int a = low + 2, b = a, c = a, d = high - 1;
    while (c <= d) {
        const int val = arr[c]; ++g_mov_instr;
        ++g_cmp_instr;
        if (val < p1) {
            arr[c] = arr[b]; arr[b] = arr[a]; arr[a] = val;
            g_mov_instr += 3; ++a; ++b; ++c;
        } else {
            ++g_cmp_instr;
            if (val <= p2) {
                arr[c] = arr[b]; arr[b] = val;
                g_mov_instr += 2; ++b; ++c;
            } else {
                ++g_cmp_instr;
                if (val > p3) {
                    while (1) {
                        ++g_cmp_instr;
                        if (!(arr[d] > p3)) break;
                        if (!(c < d)) break;
                        --d;
                    }
                    if (c > d) break;
                    const int temp = arr[d]; arr[d] = val; arr[c] = temp;
                    g_mov_instr += 2; --d;
                } else {
                    ++c;
                }
            }
        }
    }

    int temp2 = arr[low + 1];
    arr[low + 1] = arr[a - 1]; arr[a - 1] = arr[b - 1]; arr[b - 1] = temp2;
    g_mov_instr += 3;
    int temp1 = arr[low]; arr[low] = arr[a - 2]; arr[a - 2] = temp1;
    g_mov_instr += 2;
    int temp3 = arr[high]; arr[high] = arr[d + 1]; arr[d + 1] = temp3;
    g_mov_instr += 2;

    const int next_depth = depth_limit - 1;
    triple_pivot_quicksort_c_instr_impl(arr, low, a - 3, next_depth);
    triple_pivot_quicksort_c_instr_impl(arr, a - 1, b - 2, next_depth);
    triple_pivot_quicksort_c_instr_impl(arr, b, d, next_depth);
    triple_pivot_quicksort_c_instr_impl(arr, d + 2, high, next_depth);
}

void triple_pivot_quicksort_c_instr(int *arr, int low, int high) {
    if (arr == NULL || low >= high) return;
    int depth_limit = 0;
    for (int count = high - low + 1; count > 1; count >>= 1) depth_limit += 2;
    triple_pivot_quicksort_c_instr_impl(arr, low, high, depth_limit);
}

/* ============================================================================
 * Full-width uint64_t instrumented twins for CS3 and CS3-GS.
 *
 * Counter convention:
 *   g_cmp_instr counts executed comparisons involving array keys, pivots,
 *   generated thresholds, or temporary distinct keys. Index/loop conditions
 *   and arithmetic comparisons are not counted.
 *   g_mov_instr counts element writes/moves using the convention already used
 *   by the int32 instrumented variants. These functions are for operation
 *   counts only; use the non-instrumented uint64_t entry points for timings.
 * ============================================================================ */

static inline void cs3_u64_swap_instr(uint64_t *a, uint64_t *b) {
    const uint64_t t = *a;
    *a = *b;
    *b = t;
    g_mov_instr += 3;
}

static void cs3_u64_insertion_range_instr(uint64_t *arr, int low, int high) {
    for (int i = low + 1; i <= high; ++i) {
        const uint64_t key = arr[i];
        int j = i - 1;
        while (j >= low) {
            ++g_cmp_instr;
            if (arr[j] <= key)
                break;
            arr[j + 1] = arr[j];
            ++g_mov_instr;
            --j;
        }
        arr[j + 1] = key;
        ++g_mov_instr;
    }
}

static void cs3_u64_sift_down_instr(uint64_t *arr, int64_t n, int64_t root) {
    while (root < n / 2) {
        const int64_t left = root * 2 + 1;
        const int64_t right = left + 1;
        int64_t largest = root;
        if (left < n) {
            ++g_cmp_instr;
            if (arr[left] > arr[largest])
                largest = left;
        }
        if (right < n) {
            ++g_cmp_instr;
            if (arr[right] > arr[largest])
                largest = right;
        }
        if (largest == root)
            return;
        cs3_u64_swap_instr(&arr[root], &arr[largest]);
        root = largest;
    }
}

static void cs3_u64_heap_sort_instr(uint64_t *arr, int64_t n) {
    if (arr == NULL || n < 2)
        return;
    for (int64_t start = n / 2; start > 0; --start)
        cs3_u64_sift_down_instr(arr, n, start - 1);
    for (int64_t end = n - 1; end > 0; --end) {
        cs3_u64_swap_instr(&arr[0], &arr[end]);
        cs3_u64_sift_down_instr(arr, end, 0);
    }
}

static bool cs3_u64_try_small_alphabet_instr(uint64_t *arr, int low, int high) {
    uint64_t keys[5];
    size_t counts[5] = {0, 0, 0, 0, 0};
    size_t used = 0;

    for (int i = low; i <= high; ++i) {
        size_t k = 0;
        while (k < used) {
            ++g_cmp_instr;
            if (keys[k] == arr[i])
                break;
            ++k;
        }
        if (k == used) {
            if (used == 5)
                return false;
            keys[used] = arr[i];
            counts[used] = 0;
            ++used;
        }
        ++counts[k];
    }

    for (size_t i = 1; i < used; ++i) {
        const uint64_t key = keys[i];
        const size_t count = counts[i];
        size_t j = i;
        while (j > 0) {
            ++g_cmp_instr;
            if (keys[j - 1] <= key)
                break;
            keys[j] = keys[j - 1];
            counts[j] = counts[j - 1];
            --j;
        }
        keys[j] = key;
        counts[j] = count;
    }

    int out = low;
    for (size_t k = 0; k < used; ++k) {
        for (size_t repeat = 0; repeat < counts[k]; ++repeat) {
            arr[out++] = keys[k];
            ++g_mov_instr;
        }
    }
    return true;
}

static void cs3_u64_opt_inner_instr(uint64_t *arr, int low, int high,
                                    int64_t depth_limit) {
    while (low < high) {
        const int64_t n64 = (int64_t)high - (int64_t)low + 1;
        if (n64 < 24) {
            cs3_u64_insertion_range_instr(arr, low, high);
            return;
        }
        if (depth_limit <= 0) {
            cs3_u64_heap_sort_instr(arr + low, n64);
            return;
        }
        --depth_limit;
        const int n = (int)n64;

        uint64_t sample[5] = {
            arr[low], arr[low + n / 4], arr[low + n / 2],
            arr[low + (n / 4) * 3 + ((n % 4) * 3) / 4], arr[high]
        };
#define CS3_U64_INSTR_SORT2(a, b) do { \
        ++g_cmp_instr; \
        if ((a) > (b)) { uint64_t t_ = (a); (a) = (b); (b) = t_; } \
    } while (0)
        CS3_U64_INSTR_SORT2(sample[0], sample[1]);
        CS3_U64_INSTR_SORT2(sample[3], sample[4]);
        CS3_U64_INSTR_SORT2(sample[2], sample[4]);
        CS3_U64_INSTR_SORT2(sample[2], sample[3]);
        CS3_U64_INSTR_SORT2(sample[1], sample[4]);
        CS3_U64_INSTR_SORT2(sample[0], sample[3]);
        CS3_U64_INSTR_SORT2(sample[0], sample[2]);
        CS3_U64_INSTR_SORT2(sample[1], sample[3]);
        CS3_U64_INSTR_SORT2(sample[1], sample[2]);
#undef CS3_U64_INSTR_SORT2

        const uint64_t p1 = sample[1];
        const uint64_t p2 = sample[2];
        const uint64_t p3 = sample[3];

        ++g_cmp_instr;
        bool duplicate_sample = (sample[0] == sample[1]);
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[1] == sample[2]);
        }
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[2] == sample[3]);
        }
        if (!duplicate_sample) {
            ++g_cmp_instr;
            duplicate_sample = (sample[3] == sample[4]);
        }

        if (duplicate_sample) {
            if (cs3_u64_try_small_alphabet_instr(arr, low, high))
                return;

            int lt = low;
            int i = low;
            int gt = high;
            while (i <= gt) {
                const uint64_t value = arr[i];
                ++g_cmp_instr;
                if (value < p2) {
                    arr[i] = arr[lt];
                    arr[lt] = value;
                    g_mov_instr += 2;
                    ++lt;
                    ++i;
                } else {
                    ++g_cmp_instr;
                    if (value > p2) {
                        arr[i] = arr[gt];
                        arr[gt] = value;
                        g_mov_instr += 2;
                        --gt;
                    } else {
                        ++i;
                    }
                }
            }

            const int left_size = lt - low;
            const int right_size = high - gt;
            if (left_size < right_size) {
                if (left_size > 1)
                    cs3_u64_opt_inner_instr(arr, low, lt - 1, depth_limit);
                low = gt + 1;
            } else {
                if (right_size > 1)
                    cs3_u64_opt_inner_instr(arr, gt + 1, high, depth_limit);
                high = lt - 1;
            }
            continue;
        }

        int lt = low;
        int b = low;
        int i = low;
        int gt = high;
        while (i <= gt) {
            const uint64_t value = arr[i];
            ++g_cmp_instr;
            if (value < p2) {
                ++g_cmp_instr;
                if (value < p1) {
                    arr[i] = arr[b];
                    arr[b] = arr[lt];
                    arr[lt] = value;
                    g_mov_instr += 3;
                    ++lt;
                    ++b;
                    ++i;
                } else {
                    arr[i] = arr[b];
                    arr[b] = value;
                    g_mov_instr += 2;
                    ++b;
                    ++i;
                }
            } else {
                ++g_cmp_instr;
                if (value < p3) {
                    ++i;
                } else {
                    arr[i] = arr[gt];
                    arr[gt] = value;
                    g_mov_instr += 2;
                    --gt;
                }
            }
        }

        const int m0 = lt;
        const int m1 = b;
        const int m2 = i;
        cs3_u64_opt_inner_instr(arr, low, m0 - 1, depth_limit);
        cs3_u64_opt_inner_instr(arr, m0, m1 - 1, depth_limit);
        cs3_u64_opt_inner_instr(arr, m1, m2 - 1, depth_limit);
        cs3_u64_opt_inner_instr(arr, m2, high, depth_limit);
        return;
    }
}

void cs_sort_u64_c_instr(uint64_t *arr, int low, int high) {
    if (arr == NULL || low >= high || low < 0)
        return;
    int64_t depth_limit = 0;
    for (int64_t n = (int64_t)high - low + 1; n > 1; n >>= 1)
        depth_limit += 2;
    cs3_u64_opt_inner_instr(arr, low, high, depth_limit);
}

static int cs3_gs_make_thresholds_u64_instr(const uint64_t *arr,
                                            int low, int high,
                                            uint64_t thresholds[3]) {
    if (arr == NULL || thresholds == NULL || low < 0 || high <= low)
        return 0;
    const int64_t n64 = (int64_t)high - (int64_t)low + 1;
    if (n64 < 256 || n64 > INT_MAX)
        return 0;
    const int n = (int)n64;
    const int sample_offsets[9] = {
        0, (int)(((int64_t)n * 1) / 8), (int)(((int64_t)n * 2) / 8),
        (int)(((int64_t)n * 3) / 8), (int)(((int64_t)n * 4) / 8),
        (int)(((int64_t)n * 5) / 8), (int)(((int64_t)n * 6) / 8),
        (int)(((int64_t)n * 7) / 8), n - 1
    };
    uint64_t sample_values[9];
    for (size_t j = 0; j < 9; ++j)
        sample_values[j] = arr[low + sample_offsets[j]];

    bool nondecreasing = true;
    bool nonincreasing = true;
    for (size_t j = 1; j < 9; ++j) {
        ++g_cmp_instr;
        if (sample_values[j] < sample_values[j - 1])
            nondecreasing = false;
        ++g_cmp_instr;
        if (sample_values[j] > sample_values[j - 1])
            nonincreasing = false;
    }
    if (nondecreasing || nonincreasing)
        return 0;

    uint64_t sample_min = sample_values[0];
    uint64_t sample_max = sample_values[0];
    for (size_t j = 1; j < 9; ++j) {
        ++g_cmp_instr;
        if (sample_values[j] < sample_min)
            sample_min = sample_values[j];
        ++g_cmp_instr;
        if (sample_values[j] > sample_max)
            sample_max = sample_values[j];
    }
    const uint64_t span = sample_max - sample_min;
    const uint64_t narrow_cutoff = (uint64_t)n / 16u;
    if (span < 32u || span < narrow_cutoff)
        return 0;

    const uint64_t s0 = span / 2u + span % 2u;
    const uint64_t rem = span - s0;
    const uint64_t s1 = rem / 2u + rem % 2u;
    if (s0 == 0 || s1 == 0)
        return 0;
    thresholds[0] = sample_min + s1;
    thresholds[1] = sample_min + s0;
    thresholds[2] = sample_min + s0 + s1;

    unsigned bucket_counts[4] = {0, 0, 0, 0};
    for (size_t j = 0; j < 9; ++j) {
        const uint64_t value = sample_values[j];
        unsigned bucket;
        ++g_cmp_instr;
        if (value < thresholds[0]) {
            bucket = 0;
        } else {
            ++g_cmp_instr;
            if (value < thresholds[1]) {
                bucket = 1;
            } else {
                ++g_cmp_instr;
                if (value < thresholds[2])
                    bucket = 2;
                else
                    bucket = 3;
            }
        }
        ++bucket_counts[bucket];
    }
    for (size_t b = 0; b < 4; ++b) {
        if (bucket_counts[b] >= 5)
            return 0;
    }
    return 1;
}

static void cs3_gs_partition4_u64_instr(uint64_t *arr, int low, int high,
                                        const uint64_t thresholds[3],
                                        int *m0, int *m1, int *m2) {
    int lt = low;
    int b = low;
    int i = low;
    int gt = high;
    while (i <= gt) {
        const uint64_t value = arr[i];
        ++g_cmp_instr;
        if (value < thresholds[0]) {
            if (i != b) {
                arr[i] = arr[b];
                ++g_mov_instr;
            }
            if (b != lt) {
                arr[b] = arr[lt];
                ++g_mov_instr;
            }
            if (lt != i) {
                arr[lt] = value;
                ++g_mov_instr;
            }
            ++lt;
            ++b;
            ++i;
        } else {
            ++g_cmp_instr;
            if (value < thresholds[1]) {
                if (i != b) {
                    arr[i] = arr[b];
                    arr[b] = value;
                    g_mov_instr += 2;
                }
                ++b;
                ++i;
            } else {
                ++g_cmp_instr;
                if (value < thresholds[2]) {
                    ++i;
                } else {
                    if (i != gt) {
                        arr[i] = arr[gt];
                        arr[gt] = value;
                        g_mov_instr += 2;
                    }
                    --gt;
                }
            }
        }
    }
    *m0 = lt;
    *m1 = b;
    *m2 = i;
}

void cs3_gs_sort_u64_c_instr(uint64_t *arr, int low, int high) {
    if (arr == NULL || low >= high || low < 0)
        return;
    uint64_t thresholds[3];
    if (!cs3_gs_make_thresholds_u64_instr(arr, low, high, thresholds)) {
        cs_sort_u64_c_instr(arr, low, high);
        return;
    }
    int m0, m1, m2;
    cs3_gs_partition4_u64_instr(arr, low, high, thresholds, &m0, &m1, &m2);
#define CS3_U64_INSTR_SORT_BUCKET(lo_, hi_) do { \
        if ((hi_) > (lo_)) { \
            int64_t d_ = 0; \
            for (int64_t n_ = (int64_t)(hi_) - (lo_) + 1; n_ > 1; n_ >>= 1) d_ += 2; \
            cs3_u64_opt_inner_instr(arr, (lo_), (hi_), d_); \
        } \
    } while (0)
    CS3_U64_INSTR_SORT_BUCKET(low, m0 - 1);
    CS3_U64_INSTR_SORT_BUCKET(m0, m1 - 1);
    CS3_U64_INSTR_SORT_BUCKET(m1, m2 - 1);
    CS3_U64_INSTR_SORT_BUCKET(m2, high);
#undef CS3_U64_INSTR_SORT_BUCKET
}
