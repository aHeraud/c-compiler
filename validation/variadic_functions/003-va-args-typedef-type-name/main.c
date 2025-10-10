typedef int t;

int test(int n, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, n);
    t val = __builtin_va_arg(args, t);
    __builtin_va_end(args);
    return val;
}

int main(void) {
    t val = 7;
    return test(1, val);
}
