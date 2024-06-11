int printf(const char *format, ...);

int partition(int *array, int lo, int hi) {
    int pivot = array[hi];

    int i = lo;

    for (int j = lo; j < hi; j = j + 1) {
        if (array[j] <= pivot) {
            int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
            i = i + 1;
        }
    }

    int temp = array[i];
    array[i] = array[hi];
    array[hi] = temp;

    return i;
}

void quicksort(int *array, int lo, int hi) {
    if (lo >= hi) return;

    int p = partition(array, lo, hi);

    quicksort(array, lo, p - 1);
    quicksort(array, p + 1, hi);
}


int main() {
    int array[10] = { 14, 12, 23, 24, 7, 1, 99, 2, 3, 4 };
    quicksort(array, 0, 9);
    for (int i = 0; i < 9; i = i + 1) {
        printf("%d, ", array[i]);
    }
    printf("%d\n", array[9]);
}
