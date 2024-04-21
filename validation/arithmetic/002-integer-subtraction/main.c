int printf(const char* str, ...);

int main(int argc, char** argv) {
    int a = 7 - 3;
    printf("%d\n", a);

    int b = 5 - a;
    printf("%d\n", b);

    int c = b - 2;
    printf("%d\n", c);

    int d = b - c;
    printf("%d\n", d);
}
