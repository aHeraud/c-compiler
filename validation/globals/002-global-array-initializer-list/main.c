int printf(const char *fmt, ...);

// A simple global array with initializer
int a[3] = { 1, 2, 3, };

// An array where the length is determined by the initializer list
int b[] = { 4, 5, };

// Nested array initializer list
int c[2][2] = { { 1, 2, }, { 3, 4, }, };

// Nested-designated initializer list
int d[2][2] = { [1][1] = 1 };

int main(void) {
    printf("%d, %d, %d\n", a[0], a[1], a[2]);
    printf("%lu\n", sizeof(a));

    printf("%d, %d\n", b[0], b[1]);
    printf("%lu\n", sizeof(b));

    printf("%d, %d, %d, %d\n", c[0][0], c[0][1], c[1][0], c[1][1]);
    printf("%lu\n", sizeof(c));

    printf("%d, %d, %d, %d\n", d[0][0], d[0][1], d[1][0], d[1][1]);
    printf("%lu\n", sizeof(d));

    return 0;
}
