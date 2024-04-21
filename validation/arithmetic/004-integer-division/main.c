int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 8/2;
    printf("%d\n", a);

    int b = 4 / a;
    printf("%d\n", b);

    int c = b / 2;
    printf("%d\n", c);

    int d = a / b;
    printf("%d\n", d);
}
