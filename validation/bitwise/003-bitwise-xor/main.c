int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 0 ^ 1;
    printf("%d\n", a);

    int b = 1 ^ 1;
    printf("%d\n", b);

    int c = 1 ^ a;
    printf("%d\n", c);

    int d = a ^ b;
    printf("%d\n", d);
}