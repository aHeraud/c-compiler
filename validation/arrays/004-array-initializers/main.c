int printf(const char *format, ...);

int main() {
    // 1D array initializer
    int a[3] = {1, 2, 3};
    printf("%d, %d, %d\n", a[0], a[1], a[2]);

    // 2D array initializer
    int b[3][3] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };
    printf("%d, %d, %d\n", b[0][0], b[0][1], b[0][2]);
    printf("%d, %d, %d\n", b[1][0], b[1][1], b[1][2]);
    printf("%d, %d, %d\n", b[2][0], b[2][1], b[2][2]);

    // if using an initializer list, you don't need to specify the array size in the declaration
    int c[] = {1, 2, 3, 4, 5};
    printf("%d, %d, %d, %d, %d\n", c[0], c[1], c[2], c[3], c[4]);

    // designators can be used in the initializer list to change the current index
    int d[] = { [5] = 5, 6 };
    printf("%d, %d\n", d[5], d[6]);

    return 0;
}
