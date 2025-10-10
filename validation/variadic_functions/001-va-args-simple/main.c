int sum(int count, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, count);
    int sum = 0;
    for (int i = 0; i < count; i += 1) {
        int arg = __builtin_va_arg(args, int);
        sum += arg;
    }
    __builtin_va_end(args);
    return sum;
}

int main(void) {
    return sum(4, 1, 2, 3, 4);
}