#include "sorts.h"
#include <pthread.h>

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

void quicksort_c(int* arr, int low, int high) {
    if (low < high) {
        int pivot = arr[low + (high - low) / 2];
        int i = low - 1;
        int j = high + 1;
        while (1) {
            do { i++; } while (arr[i] < pivot);
            do { j--; } while (arr[j] > pivot);
            if (i >= j) break;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
        quicksort_c(arr, low, j);
        quicksort_c(arr, j + 1, high);
    }
}

// Двухпоточный QuickSort (С)
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

    if (pthread_create(&thread, NULL, quicksort_thread_func_c, &left_args) != 0) {
        quicksort_c(arr, low, j - 1);
    }
    quicksort_c(arr, j + 1, high);
    pthread_join(thread, NULL);
}

// Dual-Pivot QuickSort методом Ярославского
void dual_pivot_quicksort_c(int* arr, int low, int high) {
    if (low < high) {
        if (arr[low] > arr[high]) {
            int temp = arr[low]; arr[low] = arr[high]; arr[high] = temp;
        }
        int p1 = arr[low];
        int p2 = arr[high];

        int less = low + 1;
        int great = high - 1;
        int k = low + 1;

        while (k <= great) {
            if (arr[k] < p1) {
                int temp = arr[k]; arr[k] = arr[less]; arr[less] = temp;
                less++;
            } else if (arr[k] > p2) {
                while (k < great && arr[great] > p2) great--;
                int temp = arr[k]; arr[k] = arr[great]; arr[great] = temp;
                great--;
                if (arr[k] < p1) {
                    temp = arr[k]; arr[k] = arr[less]; arr[less] = temp;
                    less++;
                }
            }
            k++;
        }
        less--;
        great++;

        int t1 = arr[low]; arr[low] = arr[less]; arr[less] = t1;
        int t2 = arr[high]; arr[high] = arr[great]; arr[great] = t2;

        dual_pivot_quicksort_c(arr, low, less - 1);
        dual_pivot_quicksort_c(arr, less + 1, great - 1);
        dual_pivot_quicksort_c(arr, great + 1, high);
    }
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
    // 4 прохода для 32-битного int (по 8 бит)
    for (int shift = 0; shift < 32; shift += 8) {
        int count[256] = {0};

        // Подсчет частот
        for (int i = 0; i < n; i++) {
            count[(arr[i] >> shift) & 0xFF]++;
        }

        // Вычисление префиксных сумм
        for (int i = 1; i < 256; i++) {
            count[i] += count[i - 1];
        }

        // Построение отсортированного массива
        for (int i = n - 1; i >= 0; i--) {
            int byte_val = (arr[i] >> shift) & 0xFF;
            temp[count[byte_val] - 1] = arr[i];
            count[byte_val]--;
        }

        // Копирование обратно
        for (int i = 0; i < n; i++) {
            arr[i] = temp[i];
        }
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
    if (high - low < 16) {
        return;
    }
    if (depth_limit == 0) {
        heap_sort_c(arr + low, high - low + 1);
        return;
    }

    int pivot = arr[low + (high - low) / 2];
    int i = low - 1;
    int j = high + 1;
    while (1) {
        do { i++; } while (arr[i] < pivot);
        do { j--; } while (arr[j] > pivot);
        if (i >= j) break;
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }

    introsort_loop_c(arr, low, j, depth_limit - 1);
    introsort_loop_c(arr, j + 1, high, depth_limit - 1);
}

void introsort_c(int* arr, int n) {
    if (n <= 1) return;
    int depth_limit = 0;
    for (int temp = n; temp > 1; temp >>= 1) {
        depth_limit++;
    }
    depth_limit *= 2;
    introsort_loop_c(arr, 0, n - 1, depth_limit);
    insertion_sort_c(arr, n);
}

// timsort на C
#define RUN 32

// вспм сортировка для задан. диапозона
static void insertion_sort_range_c(int* arr, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int temp = arr[i];
        int j = i - 1;
        while (j >= left && arr[j] > temp) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = temp;
    }
}

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
    if (high - low < 16) return;
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

// Поиск k-й статистики (Quickselect)
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

// Частичная сортировка (на базе quickselect находит k меньших и сортирует их)
void partial_sort_c(int* arr, int n, int k) {
    if (k <= 0 || n <= 1) return;
    if (k > n) k = n;
    // Находим k-й элемент, разделяя массив
    quickselect_c(arr, 0, n - 1, k - 1);
    // Сортируем только первую часть из k элементов
    pdqsort_c(arr, k);
}

// Параллельная быстрая сортировка с использованием OpenMP задач (#pragma omp task)
void parallel_quicksort_c(int* arr, int low, int high) {
    if (low < high) {
        if (high - low < 10000) {
            quicksort_c(arr, low, high);
            return;
        }
        int pivot = arr[low + (high - low) / 2];
        int i = low - 1;
        int j = high + 1;
        while (1) {
            do { i++; } while (arr[i] < pivot);
            do { j--; } while (arr[j] > pivot);
            if (i >= j) break;
            int temp = arr[i]; arr[i] = arr[j]; arr[j] = temp;
        }

        #pragma omp task shared(arr) firstprivate(low, j)
        parallel_quicksort_c(arr, low, j);

        #pragma omp task shared(arr) firstprivate(j, high)
        parallel_quicksort_c(arr, j + 1, high);

        #pragma omp taskwait
    }
}

void triple_pivot_quicksort_c(int *arr, int low, int high) {
    if (low >= high) return;
    // Порог для перехода на сортировку insertion sort
    if (high - low < 16) {
        for (int i = low + 1; i <= high; i++) {
            int key = arr[i];
            int j = i - 1;
            while (j >= low && arr[j] > key) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }

    int mid = low + (high - low) / 2;
    int t_mid = arr[low + 1];
    arr[low + 1] = arr[mid];
    arr[mid] = t_mid;

    int p1 = arr[low];
    int p2 = arr[low + 1];
    int p3 = arr[high];

    if (p1 > p2) { int t = p1; p1 = p2; p2 = t; }
    if (p1 > p3) { int t = p1; p1 = p3; p3 = t; }
    if (p2 > p3) { int t = p2; p2 = p3; p3 = t; }

    arr[low] = p1;
    arr[low + 1] = p2;
    arr[high] = p3;

    int a = low + 2;
    int b = a;
    int c = a;
    int d = high - 1;

    // В данном цикле я резделяю массив на 4 части
    while (c <= d) {
        int val = arr[c];
        if (val < p1) {
            // Сдвиг: arr[c] <- arr[b] <- arr[a] <- val
            arr[c] = arr[b];
            arr[b] = arr[a];
            arr[a] = val;
            a++;
            b++;
            c++;
        } else if (val <= p2) {
            // Сдвиг: arr[c] <- arr[b] <- val
            arr[c] = arr[b];
            arr[b] = val;
            b++;
            c++;
        } else if (val > p3) {
            while (arr[d] > p3 && c < d) {
                d--;
            }
            if (c > d) break;

            int temp = arr[d];
            arr[d] = val;
            arr[c] = temp;
            d--;
        } else {
            // Элемент попадает в диапазон p2 < val <= p3
            c++;
        }
    }

    // Кольцевой сдвиг для p2
    int temp2 = arr[low + 1];
    arr[low + 1] = arr[a - 1];
    arr[a - 1] = arr[b - 1];
    arr[b - 1] = temp2;

    // Одиночный обмен для p1
    int temp1 = arr[low];
    arr[low] = arr[a - 2];
    arr[a - 2] = temp1;

    // Одиночный обмен для p3
    int temp3 = arr[high];
    arr[high] = arr[d + 1];
    arr[d + 1] = temp3;

    // Рекурсивные вызовы для 4 образовавшихся блоков
    triple_pivot_quicksort_c(arr, low, a - 3);
    triple_pivot_quicksort_c(arr, a - 1, b - 2);
    triple_pivot_quicksort_c(arr, b, d);
    triple_pivot_quicksort_c(arr, d + 2, high);
}


void cs_dual_partition_c(int* arr, int len, int p1, int p2, int* out_i, int* out_j) {
    int i = 0;
    int j = len - 1;
    int k = 0;

    while (k <= j) {
        if (arr[k] < p1) {
            int tmp = arr[k];
            arr[k] = arr[i];
            arr[i] = tmp;
            i++;
            k++;
        } else if (arr[k] > p2) {
            int tmp = arr[k];
            arr[k] = arr[j];
            arr[j] = tmp;
            j--;
        } else {
            k++;
        }
    }
    *out_i = i;
    *out_j = j;
}

void cs_sort_c(int* arr, int low, int high) {
    if (low >= high) return;

    int min_val = arr[low], max_val = arr[low];
    for (int k = low + 1; k <= high; k++) {
        if (arr[k] < min_val) min_val = arr[k];
        if (arr[k] > max_val) max_val = arr[k];
    }
    if (min_val == max_val) return;

    int p1 = min_val + (max_val - min_val) / 3;
    int p2 = max_val - (max_val - min_val) / 3;

    if (p1 == min_val && p2 == max_val) {
        p1 = min_val;
        p2 = min_val;
    }

    int out_i = 0, out_j = 0;
    cs_dual_partition_c(arr + low, high - low + 1, p1, p2, &out_i, &out_j);

    int i = low + out_i;
    int j = low + out_j;

    cs_sort_c(arr, low, i - 1);
    cs_sort_c(arr, i, j);
    cs_sort_c(arr, j + 1, high);
}
