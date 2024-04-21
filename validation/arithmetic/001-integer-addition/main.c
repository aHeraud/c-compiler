int printf(const char* str, ...);

int main(int argc, char** argv) {
    // binary expression with 2 constant arguments
    int a = 5 + 5;
    printf("%d\n", a);

    // binary expression with 1 constant and 1 variable argument
    int b = 10 + a;
    printf("%d\n", b);

    // binary expression with 1 variable and 1 constant argument
    int c = b + 3;
    printf("%d\n", c);

    // binary expression with 2 variable arguments
    int d = b + c;
    printf("%d\n", d);
}
