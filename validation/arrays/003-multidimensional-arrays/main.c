int printf(const char* fmt, ...);

int main() {
    // Array elements can themselves be arrays, allowing creation of multidimensional arrays.
    int array[3][3];

    array[0][0] = 1;
    array[0][1] = 2;
    array[0][2] = 3;

    array[1][0] = 4;
    array[1][1] = 5;
    array[1][2] = 6;

    array[2][0] = 7;
    array[2][1] = 8;
    array[2][2] = 9;

    printf("%d, %d, %d\n", array[0][0], array[0][1], array[0][2]);
    printf("%d, %d, %d\n", array[1][0], array[1][1], array[1][2]);
    printf("%d, %d, %d\n", array[2][0], array[2][1], array[2][2]);

    return 0;
}
