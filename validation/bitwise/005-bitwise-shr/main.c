int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 1 >> 0;
    printf("%d\n", a);

    int b = 4 >> 1;
    printf("%d\n", b);

    int c = b >> 1;
    printf("%d\n", c);

    a = 40;
    b = 3;
    c = a >> b;
    printf("%d\n", c);

    // Unsigned right shift, will shift a 0 into the most significant bit
    unsigned short d = 0xFFFF;
    d = d >> 1;
    printf("%u\n", d);

    // Signed right shift, the most significant bit is preserved
    signed short e = 0xFFFF;
    e = e >> 1;
    printf("%d\n", e);

    return 0;
}
