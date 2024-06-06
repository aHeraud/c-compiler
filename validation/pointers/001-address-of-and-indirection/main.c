int printf(const char* fmt, ...);

int main() {
    int a = 10;

    // You can get the address of a variable using the '&' operator:
    int *b = &a;

    // You can access the value of a variable using the indirection ('*') operator.
    // It can be used to read the value:
    int c = *b;
    printf("%d\n", c);

    // You can also use the indirection operator to write to the memory location specified by the pointer:
    *b = 20;
    printf("%d\n", a);

    // Multiple levels of indirection are also possible:
    int **d = &b;
    int e = **d;
    printf("%d\n", e);
    **d = 30;
    printf("%d\n", a);

    return 0;
}
