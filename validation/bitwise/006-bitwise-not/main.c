int printf(const char *format, ...);

int main() {
    int a = 0;
    int b = ~a;
    printf("%d\n", b); // -1

    a = 0xF0F0F0F0;
    b = ~a;
    printf("%d\n", b); // 252645135

    _Bool c = 0;
    _Bool d = ~c;
    printf("%d\n", d); // 1

    return 0;
}
