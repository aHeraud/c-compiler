int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 0 & 0;
    printf("%d\n", a);

    int b = 1 & 1;
    printf("%d\n", b);

    int c = 1 & a;
    printf("%d\n", c);

    int d = 0 & b;
    printf("%d\n", d);

    int e = 0xFF00 & 0x00FF;
    printf("%d\n", e);

    int f = 0xAAAA & 0xFFFF;
    printf("%d\n", f);
}
