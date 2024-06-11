int printf(const char *format, ...);

int main() {
    int a = 0;
    int b = 0;

    // Standard for loop, with an initializer that declares a loop counter, a condition, and an expression that
    // increments the loop counter at the end of each iteration.
    for (int i = 0; i < 10; i = i + 1) {
        a = a + i;
    }
    printf("%d\n", a);

    // The declaration can declare multiple variables (all of the same type).
    for (int i = 0, j = 0; i < 10; i = i + 1) {
        b = b + i;
        j = j + i;
    }
    printf("%d\n", b);

    // The initializer doesn't have to be a declaration, it can also be an expression.
    for (a = 0; a < 10; a = a + 1) {
        printf("%d", a);
    }
    printf("\n");

    // It can also be empty.
    b = 0;
    for (; b < 10; b = b + 1) {
        printf("%d", b);
    }
    printf("\n");

    // The condition and post-expression can also be empty,
    // but that would result in an infinite loop, unless there's a break statement inside the loop.

    return 0;
}
