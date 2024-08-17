int printf(const char *fmt, ...);

// Swap two integers without using a temp variables
void swap(int *x, int *y) {
    *x = *y ^ *x;
    *y = *x ^ *y;
    *x = *y ^ *x;
}

int main() {
    int a = 5;
    int b = 12;
    printf("a: %d, b: %d\n", a, b);
    swap(&a, &b);
    printf("a: %d, b: %d\n", a, b);
    return 0;
}
