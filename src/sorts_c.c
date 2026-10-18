#include "sorts.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define CS3_SWAP(a, b) do { int t = (a); (a) = (b); (b) = t; } while(0)
#define CS3_SORT2(a, b) do { if ((a) > (b)) CS3_SWAP(a, b); } while(0)
#define INSERTION_THRESHOLD 24

void insertion_sort_c(int* arr, int n) {
for (int i = 1; i < n; i++) {
int key = arr[i];
int j = i - 1;
while (j >= 0 && arr[j] > key) {
arr[j + 1] = arr[j];
j--;
}
arr[j + 1] = key;
}
}

static inline void insertion_sort_range_c(int* arr, int left, int right) {
for (int i = left + 1; i <= right; i++) {
int key = arr[i];
int j = i - 1;
while (j >= left && arr[j] > key) {
arr[j + 1] = arr[j];
j--;
}
arr[j + 1] = key;
}
}


static void quicksort_intro_impl_c(
    int *arr, int low, int high, int depth_limit
) {
    while (low < high) {
        const int n = high - low + 1;

        if (n <= INSERTION_THRESHOLD) {
            insertion_sort_range_c(arr, low, high);
            return;
        }

        if (depth_limit <= 0) {
            heap_sort_c(arr + low, n);
            return;
        }

        --depth_limit;

        /* медиана трёх: первый, средний и последний элементы. */
        int a = arr[low];
        int b = arr[low + (high - low) / 2];
        int c = arr[high];
        int t;

        if (a > b) { t = a; a = b; b = t; }
        if (b > c) { t = b; b = c; c = t; }
        if (a > b) { t = a; a = b; b = t; }

        const int pivot = b;

        /* разбиение Хоара с гарантированным продвижением индексов. */
        int i = low;
        int j = high;

        while (i <= j) {
            while (i <= high && arr[i] < pivot)
                ++i;

            while (j >= low && arr[j] > pivot)
                --j;

            if (i <= j) {
                if (i != j) {
                    t = arr[i];
                    arr[i] = arr[j];
                    arr[j] = t;
                }
                ++i;
                --j;
            }
        }

        /* рекурсия только для меньшей части, большая — циклом. */
        if (j - low < high - i) {
            if (low < j)
                quicksort_intro_impl_c(arr, low, j, depth_limit);
            low = i;
        } else {
            if (i < high)
                quicksort_intro_impl_c(arr, i, high, depth_limit);
            high = j;
        }
    }
}

void quicksort_c(int *arr, int low, int high) {
    if (arr == NULL || low < 0 || low >= high)
        return;

    int depth_limit = 0;

    for (int n = high - low + 1; n > 1; n >>= 1)
        depth_limit += 2;

    quicksort_intro_impl_c(arr, low, high, depth_limit);
}

//двухпоточный QuickSort (С)
typedef struct {
int* arr;
int low;
int high;
} ThreadArgsC;

static void* quicksort_thread_func_c(void* arg) {
ThreadArgsC* args = (ThreadArgsC*)arg;
quicksort_c(args->arr, args->low, args->high);
return NULL;
}

void quicksort_two_threads_c(int* arr, int n) {
if (n <= 1) return;
int low = 0;
int high = n - 1;
int pivot = arr[low];
int i = low;
int j = high + 1;

while (1) {
    do { i++; } while (i <= high && arr[i] < pivot);
    do { j--; } while (arr[j] > pivot);
    if (i >= j) break;
    int temp = arr[i]; arr[i] = arr[j]; arr[j] = temp;
}
int temp = arr[low]; arr[low] = arr[j]; arr[j] = temp;

pthread_t thread;
ThreadArgsC left_args = {arr, low, j - 1};
const int thread_created =
    pthread_create(&thread, NULL, quicksort_thread_func_c, &left_args) == 0;

if (!thread_created) {
    quicksort_c(arr, low, j - 1);
}
quicksort_c(arr, j + 1, high);
if (thread_created) {
    pthread_join(thread, NULL);
}
}

// Dual-Pivot QuickSort методом Ярославского с ограничением глубины.
// Защита нужна, когда первый и последний элементы являются крайними опорами
// (например, на отсортированном массиве): без неё рекурсия имеет глубину O(n)
// и время O(n^2). При исчерпании бюджета глубины остаток сортируется heapsort.
static void dual_pivot_quicksort_c_impl(int *arr, int low, int high,
                                        int depth_limit) {
    while (low < high) {
        const int count = high - low + 1;
        if (count <= INSERTION_THRESHOLD) {
            insertion_sort_range_c(arr, low, high);
            return;
        }
        if (depth_limit <= 0) {
            heap_sort_c(arr + low, count);
            return;
        }
        --depth_limit;

        if (arr[low] > arr[high]) {
            const int temp = arr[low];
            arr[low] = arr[high];
            arr[high] = temp;
        }
        const int p1 = arr[low];
        const int p2 = arr[high];
        int less = low + 1;
        int great = high - 1;
        int k = low + 1;

        while (k <= great) {
            if (arr[k] < p1) {
                const int temp = arr[k];
                arr[k] = arr[less];
                arr[less] = temp;
                ++less;
            } else if (arr[k] > p2) {
                while (k < great && arr[great] > p2) --great;
                const int temp = arr[k];
                arr[k] = arr[great];
                arr[great] = temp;
                --great;
                if (arr[k] < p1) {
                    const int temp2 = arr[k];
                    arr[k] = arr[less];
                    arr[less] = temp2;
                    ++less;
                }
            }
            ++k;
        }

        --less;
        ++great;
        const int left_pivot = arr[low];
        arr[low] = arr[less];
        arr[less] = left_pivot;
        const int right_pivot = arr[high];
        arr[high] = arr[great];
        arr[great] = right_pivot;

        dual_pivot_quicksort_c_impl(arr, low, less - 1, depth_limit);
        dual_pivot_quicksort_c_impl(arr, less + 1, great - 1, depth_limit);
        low = great + 1;
    }
}

void dual_pivot_quicksort_c(int *arr, int low, int high) {
    if (arr == NULL || low >= high) return;
    int depth_limit = 0;
    for (int count = high - low + 1; count > 1; count >>= 1)
        depth_limit += 2;
    dual_pivot_quicksort_c_impl(arr, low, high, depth_limit);
}

void merge_sort_c(int* arr, int* temp, int left, int right) {
if (left >= right) return;

int mid = left + (right - left) / 2;
merge_sort_c(arr, temp, left, mid);
merge_sort_c(arr, temp, mid + 1, right);
int i = left, j = mid + 1, k = left;
while (i <= mid && j <= right) {
    if (arr[i] <= arr[j]) temp[k++] = arr[i++];
    else                  temp[k++] = arr[j++];
}
while (i <= mid) temp[k++] = arr[i++];
while (j <= right) temp[k++] = arr[j++];

for (i = left; i <= right; i++) arr[i] = temp[i];


}

static void heapify_c(int* arr, int n, int i) {
int largest = i;
int l = 2 * i + 1;
int r = 2 * i + 2;

if (l < n && arr[l] > arr[largest]) largest = l;
if (r < n && arr[r] > arr[largest]) largest = r;

if (largest != i) {
    int temp = arr[i];
    arr[i] = arr[largest];
    arr[largest] = temp;
    heapify_c(arr, n, largest);
}


}

void heap_sort_c(int* arr, int n) {
for (int i = n / 2 - 1; i >= 0; i--) heapify_c(arr, n, i);
for (int i = n - 1; i > 0; i--) {
int temp = arr[0];
arr[0] = arr[i];
arr[i] = temp;
heapify_c(arr, i, 0);
}
}

void radix_sort_c(int* arr, int* temp, int n) {
    if (arr == NULL || temp == NULL || n <= 1) return;

    /* Map signed int order to unsigned order by flipping the sign bit. */
    for (int shift = 0; shift < 32; shift += 8) {
        int count[256] = {0};
        for (int i = 0; i < n; i++) {
            const uint32_t key = ((uint32_t)arr[i]) ^ UINT32_C(0x80000000);
            count[(key >> shift) & UINT32_C(0xFF)]++;
        }
        for (int i = 1; i < 256; i++) count[i] += count[i - 1];
        for (int i = n - 1; i >= 0; i--) {
            const uint32_t key = ((uint32_t)arr[i]) ^ UINT32_C(0x80000000);
            const unsigned byte_val = (unsigned)((key >> shift) & UINT32_C(0xFF));
            temp[--count[byte_val]] = arr[i];
        }
        for (int i = 0; i < n; i++) arr[i] = temp[i];
    }
}

static void bitonic_merge(int* arr, int low, int cnt, int dir) {
if (cnt > 1) {
int k = cnt / 2;
for (int i = low; i < low + k; i++) {
if (dir == (arr[i] > arr[i + k])) {
int temp = arr[i];
arr[i] = arr[i + k];
arr[i + k] = temp;
}
}
bitonic_merge(arr, low, k, dir);
bitonic_merge(arr, low + k, k, dir);
}
}

void bitonic_sort_c(int* arr, int low, int cnt, int dir) {
if (cnt > 1) {
int k = cnt / 2;
bitonic_sort_c(arr, low, k, 1);
bitonic_sort_c(arr, low + k, k, 0);
bitonic_merge(arr, low, cnt, dir);
}
}

void parallel_merge_sort_c(int* arr, int* temp, int left, int right) {
if (left >= right) return;

int mid = left + (right - left) / 2;
if ((right - left) > 10000) {
    #pragma omp task shared(arr, temp)
    parallel_merge_sort_c(arr, temp, left, mid);

    #pragma omp task shared(arr, temp)
    parallel_merge_sort_c(arr, temp, mid + 1, right);

    #pragma omp taskwait
} else {
    merge_sort_c(arr, temp, left, mid);
    merge_sort_c(arr, temp, mid + 1, right);
}
int i = left, j = mid + 1, k = left;
while (i <= mid && j <= right) {
    if (arr[i] <= arr[j]) temp[k++] = arr[i++];
    else                  temp[k++] = arr[j++];
}
while (i <= mid) temp[k++] = arr[i++];
while (j <= right) temp[k++] = arr[j++];

for (i = left; i <= right; i++) arr[i] = temp[i];


}

static void introsort_loop_c(int* arr, int low, int high, int depth_limit) {
while (high - low > INSERTION_THRESHOLD) {
if (depth_limit == 0) {
heap_sort_c(arr + low, high - low + 1);
return;
}
depth_limit--;

    int mid = low + (high - low) / 2;
    int pivot = arr[mid];

    // Медиана из трех для качественного выбора опорного элемента
    int a = arr[low], b = arr[mid], c = arr[high];
    if (a > b) { int t = a; a = b; b = t; }
    if (a > c) { int t = a; a = c; c = t; }
    if (b > c) { int t = b; b = c; c = t; }
    pivot = b;

    int i = low - 1;
    int j = high + 1;
    while (1) {
        do { i++; } while (arr[i] < pivot);
        do { j--; } while (arr[j] > pivot);
        if (i >= j) break;
        int temp = arr[i]; arr[i] = arr[j]; arr[j] = temp;
    }

    introsort_loop_c(arr, j + 1, high, depth_limit);
    high = j;
}
insertion_sort_range_c(arr, low, high);


}

void introsort_c(int* arr, int n) {
if (n <= 1) return;
int depth_limit = 0;
for (int temp = n; temp > 1; temp >>= 1) {
depth_limit += 2;
}
introsort_loop_c(arr, 0, n - 1, depth_limit);
}

#define RUN 32

void timsort_c(int* arr, int* temp, int n) {
for (int i = 0; i < n; i += RUN) {
int right = (i + RUN - 1 < n - 1) ? (i + RUN - 1) : (n - 1);
insertion_sort_range_c(arr, i, right);
}
for (int size = RUN; size < n; size = 2 * size) {
for (int left = 0; left < n; left += 2 * size) {
int mid = left + size - 1;
int right = (left + 2 * size - 1 < n - 1) ? (left + 2 * size - 1) : (n - 1);
if (mid >= right) continue;
int i = left, j = mid + 1, k = left;
while (i <= mid && j <= right) {
if (arr[i] <= arr[j]) {
temp[k++] = arr[i++];
} else {
temp[k++] = arr[j++];
}
}
while (i <= mid) temp[k++] = arr[i++];
while (j <= right) temp[k++] = arr[j++];

        for (i = left; i <= right; i++) {
            arr[i] = temp[i];
        }
    }
}


}

static void pdqsort_loop_c(int* arr, int low, int high, int depth_limit) {
if (high - low < INSERTION_THRESHOLD) return;
if (depth_limit == 0) {
heap_sort_c(arr + low, high - low + 1);
return;
}
int mid = low + (high - low) / 2;
int pivot = arr[mid];
arr[mid] = arr[high];
arr[high] = pivot;
int i = low - 1;
for (int j = low; j < high; j++) {
int is_less = arr[j] < pivot;
i += is_less;
int swap_idx = is_less ? i : j;

    int temp_val = arr[j];
    arr[j] = arr[swap_idx];
    arr[swap_idx] = temp_val;
}
i++;
arr[high] = arr[i];
arr[i] = pivot;

pdqsort_loop_c(arr, low, i - 1, depth_limit - 1);
pdqsort_loop_c(arr, i + 1, high, depth_limit - 1);


}

void pdqsort_c(int* arr, int n) {
if (n <= 1) return;
int depth_limit = 0;
for (int temp = n; temp > 1; temp >>= 1) depth_limit++;
depth_limit *= 2;

pdqsort_loop_c(arr, 0, n - 1, depth_limit);
insertion_sort_c(arr, n);


}

int quickselect_c(int* arr, int low, int high, int k) {
while (low <= high) {
if (low == high) return arr[low];
int pivot = arr[low + (high - low) / 2];
int i = low - 1;
int j = high + 1;
while (1) {
do { i++; } while (arr[i] < pivot);
do { j--; } while (arr[j] > pivot);
if (i >= j) break;
int temp = arr[i]; arr[i] = arr[j]; arr[j] = temp;
}
if (k <= j) high = j;
else low = j + 1;
}
return arr[low];
}

void partial_sort_c(int* arr, int n, int k) {
if (k <= 0 || n <= 1) return;
if (k > n) k = n;
quickselect_c(arr, 0, n - 1, k - 1);
pdqsort_c(arr, k);
}

#define PARALLEL_QUICK_INSERTION_THRESHOLD 24
#define PARALLEL_QUICK_TASK_THRESHOLD 32768
#define PARALLEL_QUICK_MAX_TASK_DEPTH 4

static void parallel_quicksort_c_impl(
    int *arr, int low, int high, int depth_limit, int task_depth
) {
    while (low < high) {
        const int n = high - low + 1;

        if (n <= PARALLEL_QUICK_INSERTION_THRESHOLD) {
            insertion_sort_range_c(arr, low, high);
            return;
        }

        if (depth_limit <= 0) {
            heap_sort_c(arr + low, n);
            return;
        }
        --depth_limit;

        int x = arr[low];
        int y = arr[low + (high - low) / 2];
        int z = arr[high];
        int tmp;

        if (x > y) { tmp = x; x = y; y = tmp; }
        if (y > z) { tmp = y; y = z; z = tmp; }
        if (x > y) { tmp = x; x = y; y = tmp; }

        const int pivot = y;

        int i = low;
        int j = high;

        while (i <= j) {
            while (i <= high && arr[i] < pivot)
                ++i;

            while (j >= low && arr[j] > pivot)
                --j;

            if (i <= j) {
                if (i != j) {
                    tmp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = tmp;
                }
                ++i;
                --j;
            }
        }

        const int left_size = (j >= low) ? j - low + 1 : 0;
        const int right_size = (i <= high) ? high - i + 1 : 0;

#ifdef _OPENMP

        if (n >= PARALLEL_QUICK_TASK_THRESHOLD &&
            task_depth < PARALLEL_QUICK_MAX_TASK_DEPTH &&
            left_size > 1 && right_size > 1) {

            const int left_hi = j;
            const int right_lo = i;

#pragma omp task shared(arr) firstprivate(low, left_hi, depth_limit, task_depth)
            parallel_quicksort_c_impl(
                arr, low, left_hi, depth_limit, task_depth + 1
            );

#pragma omp task shared(arr) firstprivate(right_lo, high, depth_limit, task_depth)
            parallel_quicksort_c_impl(
                arr, right_lo, high, depth_limit, task_depth + 1
            );

#pragma omp taskwait
            return;
        }
#else
        (void)task_depth;
#endif

        if (left_size < right_size) {
            if (left_size > 1) {
                parallel_quicksort_c_impl(
                    arr, low, j, depth_limit, task_depth
                );
            }
            low = i;
        } else {
            if (right_size > 1) {
                parallel_quicksort_c_impl(
                    arr, i, high, depth_limit, task_depth
                );
            }
            high = j;
        }
    }
}

void parallel_quicksort_c(int *arr, int low, int high) {
    if (arr == NULL || low < 0 || low >= high)
        return;

    int depth_limit = 0;
    for (int n = high - low + 1; n > 1; n >>= 1)
        depth_limit += 2;

    parallel_quicksort_c_impl(arr, low, high, depth_limit, 0);
}

static void triple_pivot_quicksort_c_impl(int *arr, int low, int high,
                                           int depth_limit) {
    if (low >= high) return;
    const int count = high - low + 1;
    if (depth_limit <= 0) {
        heap_sort_c(arr + low, count);
        return;
    }
    if (high - low < INSERTION_THRESHOLD) {
        insertion_sort_range_c(arr, low, high);
        return;
    }

    const int mid = low + (high - low) / 2;
    const int tmp_mid = arr[low + 1];
    arr[low + 1] = arr[mid];
    arr[mid] = tmp_mid;

    int p1 = arr[low];
    int p2 = arr[low + 1];
    int p3 = arr[high];
    if (p1 > p2) { int t = p1; p1 = p2; p2 = t; }
    if (p1 > p3) { int t = p1; p1 = p3; p3 = t; }
    if (p2 > p3) { int t = p2; p2 = p3; p3 = t; }

    if (p1 == p2 || p2 == p3) {
        heap_sort_c(arr + low, count);
        return;
    }

    arr[low] = p1;
    arr[low + 1] = p2;
    arr[high] = p3;
    int a = low + 2, b = a, c = a, d = high - 1;

    while (c <= d) {
        const int val = arr[c];
        if (val < p1) {
            arr[c] = arr[b]; arr[b] = arr[a]; arr[a] = val;
            ++a; ++b; ++c;
        } else if (val <= p2) {
            arr[c] = arr[b]; arr[b] = val;
            ++b; ++c;
        } else if (val > p3) {
            while (arr[d] > p3 && c < d) --d;
            if (c > d) break;
            const int temp = arr[d]; arr[d] = val; arr[c] = temp;
            --d;
        } else {
            ++c;
        }
    }

    int temp2 = arr[low + 1];
    arr[low + 1] = arr[a - 1]; arr[a - 1] = arr[b - 1]; arr[b - 1] = temp2;
    int temp1 = arr[low]; arr[low] = arr[a - 2]; arr[a - 2] = temp1;
    int temp3 = arr[high]; arr[high] = arr[d + 1]; arr[d + 1] = temp3;

    const int next_depth = depth_limit - 1;
    triple_pivot_quicksort_c_impl(arr, low, a - 3, next_depth);
    triple_pivot_quicksort_c_impl(arr, a - 1, b - 2, next_depth);
    triple_pivot_quicksort_c_impl(arr, b, d, next_depth);
    triple_pivot_quicksort_c_impl(arr, d + 2, high, next_depth);
}

void triple_pivot_quicksort_c(int *arr, int low, int high) {
    if (arr == NULL || low >= high) return;
    int depth_limit = 0;
    for (int count = high - low + 1; count > 1; count >>= 1) depth_limit += 2;
    triple_pivot_quicksort_c_impl(arr, low, high, depth_limit);
}

static bool cs3_try_small_alphabet(int *arr, int low, int high) {
    int keys[5];
    size_t counts[5] = {0, 0, 0, 0, 0};
    size_t used = 0;

    for (int i = low; i <= high; ++i) {
        size_t k = 0;
        while (k < used && keys[k] != arr[i]) ++k;
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
        while (j > 0 && keys[j - 1] > key) {
            keys[j] = keys[j - 1];
            counts[j] = counts[j - 1];
            --j;
        }
        keys[j] = key;
        counts[j] = count;
    }

    int out = low;
    for (size_t k = 0; k < used; ++k) {
        for (size_t repeat = 0; repeat < counts[k]; ++repeat)
            arr[out++] = keys[k];
    }
    return true;
}

/*
 * Трёхопорная быстрая сортировка с защитой introsort.
 *
 * Для опорных значений берутся три образца: первый, средний и последний
 * элементы текущего диапазона. После упорядочивания образцов p1 <= p2 <= p3
 * Один проход раскладывает диапазон в четыре зоны:
 *   A[low .. lt-1]   < p1
 *   A[lt  .. b-1]    [p1, p2)
 *   A[b   .. i-1]    [p2, p3)
 *   A[i   .. high]   >= p3
 *
 * Если в выборке есть равные соседние значения, проверяется малый алфавит
 * При большем числе ключей применяется трёхстороннее разбиение (<, ==, >)
 * При исчерпании бюджета глубины включается heapsort: worst-case O(n log n)
 */
static void cs3_opt_inner(int *arr, int low, int high, int depth_limit) {
    while (low < high) {
        const int n = high - low + 1;

        if (n < INSERTION_THRESHOLD) {
            insertion_sort_range_c(arr, low, high);
            return;
        }

        if (depth_limit <= 0) {
            heap_sort_c(arr + low, n);
            return;
        }
        --depth_limit;

        int sample[5] = {
            arr[low], arr[low + n / 4], arr[low + n / 2],
            arr[low + (n / 4) * 3 + ((n % 4) * 3) / 4], arr[high]
        };
        CS3_SORT2(sample[0], sample[1]);
        CS3_SORT2(sample[3], sample[4]);
        CS3_SORT2(sample[2], sample[4]);
        CS3_SORT2(sample[2], sample[3]);
        CS3_SORT2(sample[1], sample[4]);
        CS3_SORT2(sample[0], sample[3]);
        CS3_SORT2(sample[0], sample[2]);
        CS3_SORT2(sample[1], sample[3]);
        CS3_SORT2(sample[1], sample[2]);
        const int p1 = sample[1];
        const int p2 = sample[2];
        const int p3 = sample[3];

        const bool duplicate_sample =
            sample[0] == sample[1] || sample[1] == sample[2] ||
            sample[2] == sample[3] || sample[3] == sample[4];
        if (duplicate_sample) {
            if (cs3_try_small_alphabet(arr, low, high)) return;

            int lt = low;
            int i = low;
            int gt = high;

            while (i <= gt) {
                const int value = arr[i];
                if (value < p2) {
                    arr[i] = arr[lt];
                    arr[lt] = value;
                    ++lt;
                    ++i;
                } else if (value > p2) {
                    arr[i] = arr[gt];
                    arr[gt] = value;
                    --gt;
                } else {
                    ++i;
                }
            }

            const int left_size = lt - low;
            const int right_size = high - gt;
            if (left_size < right_size) {
                cs3_opt_inner(arr, low, lt - 1, depth_limit);
                low = gt + 1;
            } else {
                cs3_opt_inner(arr, gt + 1, high, depth_limit);
                high = lt - 1;
            }
            continue;
        }

        int lt = low;
        int b = low;
        int i = low;
        int gt = high;

        while (i <= gt) {
            const int value = arr[i];
            if (value < p2) {
                if (value < p1) {
                    arr[i] = arr[b];
                    arr[b] = arr[lt];
                    arr[lt] = value;
                    ++lt;
                    ++b;
                    ++i;
                } else {
                    arr[i] = arr[b];
                    arr[b] = value;
                    ++b;
                    ++i;
                }
            } else if (value < p3) {
                ++i;
            } else {
                arr[i] = arr[gt];
                arr[gt] = value;
                --gt;
            }
        }

        const int m0 = lt;
        const int m1 = b;
        const int m2 = i;

        cs3_opt_inner(arr, low, m0 - 1, depth_limit);
        cs3_opt_inner(arr, m0, m1 - 1, depth_limit);
        cs3_opt_inner(arr, m1, m2 - 1, depth_limit);
        cs3_opt_inner(arr, m2, high, depth_limit);
        return;
    }
}

void cs_sort_c(int *arr, int low, int high) {
    if (arr == NULL || low >= high) return;

    const int n = high - low + 1;
    int depth_limit = 0;
    for (int t = n; t > 1; t >>= 1) {
        depth_limit += 2; /* 2 * floor(log2(n)), как в introsort. */
    }
    cs3_opt_inner(arr, low, high, depth_limit);
}

#define CS3_GS_MIN_N 256
#define CS3_GS_MIN_SPAN 32

int cs3_gs_make_thresholds(const int *arr, int low, int high,
                           int64_t thresholds[3]) {
    if (arr == NULL || thresholds == NULL || low < 0 || high <= low)
        return 0;
    const int64_t n64 = (int64_t)high - (int64_t)low + 1;
    if (n64 < CS3_GS_MIN_N || n64 > INT_MAX)
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

    int sample_nondecreasing = 1;
    int sample_nonincreasing = 1;
    for (size_t j = 1; j < 9; ++j) {
        if (sample_values[j] < sample_values[j - 1])
            sample_nondecreasing = 0;
        if (sample_values[j] > sample_values[j - 1])
            sample_nonincreasing = 0;
    }
    if (sample_nondecreasing || sample_nonincreasing)
        return 0;

    int sample_min = sample_values[0];
    int sample_max = sample_values[0];
    for (size_t j = 1; j < 9; ++j) {
        if (sample_values[j] < sample_min) sample_min = sample_values[j];
        if (sample_values[j] > sample_max) sample_max = sample_values[j];
    }
    const uint64_t sample_span = (uint64_t)((int64_t)sample_max - (int64_t)sample_min);
    const uint64_t narrow_cutoff = (uint64_t)n / 16u;
    if (sample_span < CS3_GS_MIN_SPAN || sample_span < narrow_cutoff)
        return 0;

    const uint64_t span = sample_span;
    if (span < CS3_GS_MIN_SPAN)
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
        if (value < thresholds[0]) bucket = 0;
        else if (value < thresholds[1]) bucket = 1;
        else if (value < thresholds[2]) bucket = 2;
        else bucket = 3;
        ++bucket_counts[bucket];
    }
    for (size_t b = 0; b < 4; ++b) {
        if (bucket_counts[b] >= 5)
            return 0;
    }
    return 1;
}

static void cs3_gs_partition4(int *arr, int low, int high,
                              const int64_t thresholds[3],
                              int *m0, int *m1, int *m2) {
    int lt = low;
    int b = low;
    int i = low;
    int gt = high;

    while (i <= gt) {
        const int value = arr[i];
        if ((int64_t)value < thresholds[0]) {
            if (i != b) arr[i] = arr[b];
            if (b != lt) arr[b] = arr[lt];
            if (lt != i) arr[lt] = value;
            ++lt;
            ++b;
            ++i;
        } else if ((int64_t)value < thresholds[1]) {
            if (i != b) {
                arr[i] = arr[b];
                arr[b] = value;
            }
            ++b;
            ++i;
        } else if ((int64_t)value < thresholds[2]) {
            ++i;
        } else {
            if (i != gt) {
                arr[i] = arr[gt];
                arr[gt] = value;
            }
            --gt;
        }
    }
    *m0 = lt;
    *m1 = b;
    *m2 = i;
}

static int cs3_gs_depth_limit(int n) {
    int depth = 0;
    for (int t = n; t > 1; t >>= 1)
        depth += 2;
    return depth;
}

void cs3_gs_sort_c(int *arr, int low, int high) {
    if (arr == NULL || low >= high)
        return;

    int64_t thresholds[3];
    if (!cs3_gs_make_thresholds(arr, low, high, thresholds)) {
        cs_sort_c(arr, low, high);
        return;
    }

    int m0, m1, m2;
    cs3_gs_partition4(arr, low, high, thresholds, &m0, &m1, &m2);

    if (m0 - low > 1)
        cs3_opt_inner(arr, low, m0 - 1, cs3_gs_depth_limit(m0 - low));
    if (m1 - m0 > 1)
        cs3_opt_inner(arr, m0, m1 - 1, cs3_gs_depth_limit(m1 - m0));
    if (m2 - m1 > 1)
        cs3_opt_inner(arr, m1, m2 - 1, cs3_gs_depth_limit(m2 - m1));
    if (high - m2 + 1 > 1)
        cs3_opt_inner(arr, m2, high, cs3_gs_depth_limit(high - m2 + 1));
}

#define CS3_U64_SORT2(a, b) do { \
    if ((a) > (b)) { uint64_t cs3_u64_t_ = (a); (a) = (b); (b) = cs3_u64_t_; } \
} while (0)

static void cs3_u64_insertion_range(uint64_t *arr, int low, int high) {
    for (int i = low + 1; i <= high; ++i) {
        const uint64_t key = arr[i];
        int j = i - 1;
        while (j >= low && arr[j] > key) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

static void cs3_u64_sift_down(uint64_t *arr, int64_t n, int64_t root) {
    while (root < n / 2) {
        const int64_t left = root * 2 + 1;
        const int64_t right = left + 1;
        int64_t largest = root;
        if (left < n && arr[left] > arr[largest])
            largest = left;
        if (right < n && arr[right] > arr[largest])
            largest = right;
        if (largest == root)
            return;
        const uint64_t tmp = arr[root];
        arr[root] = arr[largest];
        arr[largest] = tmp;
        root = largest;
    }
}

void cs3_heap_sort_u64_c(uint64_t *arr, int64_t n) {
    if (arr == NULL || n < 2)
        return;
    for (int64_t start = n / 2; start > 0; --start)
        cs3_u64_sift_down(arr, n, start - 1);
    for (int64_t end = n - 1; end > 0; --end) {
        const uint64_t tmp = arr[0];
        arr[0] = arr[end];
        arr[end] = tmp;
        cs3_u64_sift_down(arr, end, 0);
    }
}

static bool cs3_u64_try_small_alphabet(uint64_t *arr, int low, int high) {
    uint64_t keys[5];
    size_t counts[5] = {0, 0, 0, 0, 0};
    size_t used = 0;

    for (int i = low; i <= high; ++i) {
        size_t k = 0;
        while (k < used && keys[k] != arr[i])
            ++k;
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
        while (j > 0 && keys[j - 1] > key) {
            keys[j] = keys[j - 1];
            counts[j] = counts[j - 1];
            --j;
        }
        keys[j] = key;
        counts[j] = count;
    }

    int out = low;
    for (size_t k = 0; k < used; ++k) {
        for (size_t repeat = 0; repeat < counts[k]; ++repeat)
            arr[out++] = keys[k];
    }
    return true;
}

static void cs3_u64_opt_inner(uint64_t *arr, int low, int high,
                              int64_t depth_limit) {
    while (low < high) {
        const int64_t n64 = (int64_t)high - (int64_t)low + 1;
        if (n64 < INSERTION_THRESHOLD) {
            cs3_u64_insertion_range(arr, low, high);
            return;
        }
        if (depth_limit <= 0) {
            cs3_heap_sort_u64_c(arr + low, n64);
            return;
        }
        --depth_limit;
        const int n = (int)n64;

        uint64_t sample[5] = {
            arr[low], arr[low + n / 4], arr[low + n / 2],
            arr[low + (n / 4) * 3 + ((n % 4) * 3) / 4], arr[high]
        };
        CS3_U64_SORT2(sample[0], sample[1]);
        CS3_U64_SORT2(sample[3], sample[4]);
        CS3_U64_SORT2(sample[2], sample[4]);
        CS3_U64_SORT2(sample[2], sample[3]);
        CS3_U64_SORT2(sample[1], sample[4]);
        CS3_U64_SORT2(sample[0], sample[3]);
        CS3_U64_SORT2(sample[0], sample[2]);
        CS3_U64_SORT2(sample[1], sample[3]);
        CS3_U64_SORT2(sample[1], sample[2]);

        const uint64_t p1 = sample[1];
        const uint64_t p2 = sample[2];
        const uint64_t p3 = sample[3];
        const bool duplicate_sample =
            sample[0] == sample[1] || sample[1] == sample[2] ||
            sample[2] == sample[3] || sample[3] == sample[4];

        if (duplicate_sample) {
            if (cs3_u64_try_small_alphabet(arr, low, high))
                return;
            int lt = low;
            int i = low;
            int gt = high;
            while (i <= gt) {
                const uint64_t value = arr[i];
                if (value < p2) {
                    arr[i] = arr[lt];
                    arr[lt] = value;
                    ++lt;
                    ++i;
                } else if (value > p2) {
                    arr[i] = arr[gt];
                    arr[gt] = value;
                    --gt;
                } else {
                    ++i;
                }
            }

            const int left_size = lt - low;
            const int right_size = high - gt;
            if (left_size < right_size) {
                if (left_size > 1)
                    cs3_u64_opt_inner(arr, low, lt - 1, depth_limit);
                low = gt + 1;
            } else {
                if (right_size > 1)
                    cs3_u64_opt_inner(arr, gt + 1, high, depth_limit);
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
            if (value < p2) {
                if (value < p1) {
                    arr[i] = arr[b];
                    arr[b] = arr[lt];
                    arr[lt] = value;
                    ++lt;
                    ++b;
                    ++i;
                } else {
                    arr[i] = arr[b];
                    arr[b] = value;
                    ++b;
                    ++i;
                }
            } else if (value < p3) {
                ++i;
            } else {
                arr[i] = arr[gt];
                arr[gt] = value;
                --gt;
            }
        }

        const int m0 = lt;
        const int m1 = b;
        const int m2 = i;
        cs3_u64_opt_inner(arr, low, m0 - 1, depth_limit);
        cs3_u64_opt_inner(arr, m0, m1 - 1, depth_limit);
        cs3_u64_opt_inner(arr, m1, m2 - 1, depth_limit);
        cs3_u64_opt_inner(arr, m2, high, depth_limit);
        return;
    }
}

void cs_sort_u64_c(uint64_t *arr, int low, int high) {
    if (arr == NULL || low >= high || low < 0)
        return;
    int64_t depth_limit = 0;
    for (int64_t n = (int64_t)high - low + 1; n > 1; n >>= 1)
        depth_limit += 2;
    cs3_u64_opt_inner(arr, low, high, depth_limit);
}

#define CS3_GS_U64_MIN_N 256
#define CS3_GS_U64_MIN_SPAN 32u

int cs3_gs_make_thresholds_u64(const uint64_t *arr, int low, int high,
                               uint64_t thresholds[3]) {
    if (arr == NULL || thresholds == NULL || low < 0 || high <= low)
        return 0;
    const int64_t n64 = (int64_t)high - (int64_t)low + 1;
    if (n64 < CS3_GS_U64_MIN_N || n64 > INT_MAX)
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
        if (sample_values[j] < sample_values[j - 1])
            nondecreasing = false;
        if (sample_values[j] > sample_values[j - 1])
            nonincreasing = false;
    }
    if (nondecreasing || nonincreasing)
        return 0;

    uint64_t sample_min = sample_values[0];
    uint64_t sample_max = sample_values[0];
    for (size_t j = 1; j < 9; ++j) {
        if (sample_values[j] < sample_min) sample_min = sample_values[j];
        if (sample_values[j] > sample_max) sample_max = sample_values[j];
    }
    const uint64_t span = sample_max - sample_min; /* no unsigned underflow */
    const uint64_t narrow_cutoff = (uint64_t)n / 16u;
    if (span < CS3_GS_U64_MIN_SPAN || span < narrow_cutoff)
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
        if (value < thresholds[0]) bucket = 0;
        else if (value < thresholds[1]) bucket = 1;
        else if (value < thresholds[2]) bucket = 2;
        else bucket = 3;
        ++bucket_counts[bucket];
    }
    for (size_t b = 0; b < 4; ++b) {
        if (bucket_counts[b] >= 5)
            return 0;
    }
    return 1;
}

static void cs3_gs_partition4_u64(uint64_t *arr, int low, int high,
                                  const uint64_t thresholds[3],
                                  int *m0, int *m1, int *m2) {
    int lt = low;
    int b = low;
    int i = low;
    int gt = high;
    while (i <= gt) {
        const uint64_t value = arr[i];
        if (value < thresholds[0]) {
            if (i != b) arr[i] = arr[b];
            if (b != lt) arr[b] = arr[lt];
            if (lt != i) arr[lt] = value;
            ++lt; ++b; ++i;
        } else if (value < thresholds[1]) {
            if (i != b) {
                arr[i] = arr[b];
                arr[b] = value;
            }
            ++b; ++i;
        } else if (value < thresholds[2]) {
            ++i;
        } else {
            if (i != gt) {
                arr[i] = arr[gt];
                arr[gt] = value;
            }
            --gt;
        }
    }
    *m0 = lt;
    *m1 = b;
    *m2 = i;
}

void cs3_gs_sort_u64_c(uint64_t *arr, int low, int high) {
    if (arr == NULL || low >= high || low < 0)
        return;
    uint64_t thresholds[3];
    if (!cs3_gs_make_thresholds_u64(arr, low, high, thresholds)) {
        cs_sort_u64_c(arr, low, high);
        return;
    }
    int m0, m1, m2;
    cs3_gs_partition4_u64(arr, low, high, thresholds, &m0, &m1, &m2);
#define CS3_U64_SORT_BUCKET(lo_, hi_) do { \
    if ((hi_) > (lo_)) { \
        int64_t d_ = 0; \
        for (int64_t n_ = (int64_t)(hi_) - (lo_) + 1; n_ > 1; n_ >>= 1) d_ += 2; \
        cs3_u64_opt_inner(arr, (lo_), (hi_), d_); \
    } \
} while (0)
    CS3_U64_SORT_BUCKET(low, m0 - 1);
    CS3_U64_SORT_BUCKET(m0, m1 - 1);
    CS3_U64_SORT_BUCKET(m1, m2 - 1);
    CS3_U64_SORT_BUCKET(m2, high);
#undef CS3_U64_SORT_BUCKET
}

#undef CS3_U64_SORT2
