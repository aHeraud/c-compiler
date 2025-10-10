int foo(int n, ...) {
    __builtin_va_list args1, args2;
    __builtin_va_start(args1, n);
    __builtin_va_copy(args2, args1);

    int v1 = __builtin_va_arg(args1, int);
    int v2 = __builtin_va_arg(args2, int);

    __builtin_va_end(args1);
    __builtin_va_end(args2);

    return v1 + v2;
}

int main(void) {
    return foo(1, 3);
}
