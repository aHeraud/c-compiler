int printf(const char *fmt, ...);

int main(void) {
    // By default, the first enumeration constant starts with a value of 0.
    // Each enumeration constant has the value of the previous enumeration constant plus 1.
    enum Test {
        A, B, C,
    };

    // You can assign an enumeration constant to a field declared as an enum:
    enum Test a = A;

    // But the enumeration constants are actually integers
    enum Test b = B;

    printf("%d,%d,%d\n", a, b, C);

    // You can also assign a value to enumeration constants
    enum Test2 {
        D = 22, E, F = 14, G
    };

    printf("%d,%d,%d,%d\n", D, E, F, G);

    return 0;
}
