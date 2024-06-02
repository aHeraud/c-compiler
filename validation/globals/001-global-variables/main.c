int printf(const char *format, ...);

// Un-initialized global variable
int a;

// A global variable can have an initializer, but it must be a constant expression
int b = 123;

// The compiler does constant folding for expressions, so an arithmetic expression like this is alloed
int c = 4 * 12 << 1;

int main() {
    // You can read the value of a global variable
    // If the variable is not initialized, it will have the value 0
    printf("%d, %d, %d\n", a, b, c);

    // You can update the value of a global variable, if it is not const-qualified
    a = 1;
    b = 2;
    c = 3;
    printf("%d, %d, %d\n", a, b, c);

    // You can also declare a new local variable that shadows a global variable with the same name
    int a = 10;
    printf("%d\n", a);

    return 0;
}
