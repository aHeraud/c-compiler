int printf(const char *fmt, ...);

void foo(int n, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, n);
    printf("%d\n", n);
    printf("%ld\n", __builtin_va_arg(args, long));
    printf("%s\n", __builtin_va_arg(args, char*));
    printf("%f\n", __builtin_va_arg(args, double));
}

int main(void) {
    foo(3, 42L, "apple", 3.14);
    return 0;
}
