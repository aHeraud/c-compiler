int printf(const char* format, ...);

int main() {
    int array[4];

    // We can store values in the array
    array[0] = 1;
    array[1] = 2;
    array[2] = 3;
    array[3] = 4;

    // We can load values from the array
    printf("%d,%d,%d,%d\n", array[0], array[1], array[2], array[3]);

    // Indices can be also be variables
    int i = 2;
    printf("%d\n", array[i]);

    return 0;
}
