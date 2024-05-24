int printf(const char* fmt, ...);

void foo(long a, double b) {
    printf("%ld\n", a);
    printf("%f\n", b);
}

int main() {
    short a = 42;
    float b = 5.75;
    foo(a, b);
    return 0;
}
