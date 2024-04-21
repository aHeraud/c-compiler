int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 1 << 0;
    printf("%d\n", a);

    int b = 5 << 1;
    printf("%d\n", b);

    int c = a << 2;
    printf("%d\n", c);

    int d = 1 << b;
    printf("%d\n", d);
}
