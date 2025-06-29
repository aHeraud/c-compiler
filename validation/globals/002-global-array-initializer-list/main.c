int printf(const char *fmt, ...);

// A simple global array with initializer
int a[3] = { 1, 2, 3, };

// An array where the length is determined by the initializer list
int b[] = { 4, 5, };

int main(void) {
    printf("%d, %d, %d\n", a[0], a[1], a[2]);
    printf("%lu\n", sizeof(a));

    printf("%d, %d\n", b[0], b[1]);
    printf("%lu\n", sizeof(b));
}
