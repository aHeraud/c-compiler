int printf(const char *fmt, ...);

int main() {
    int a = 0;
    printf("%d\n", a);

    // prefix increment
    printf("%d\n", ++a);
    printf("%d\n", a);

    // postfix increment
    printf("%d\n", a++);
    printf("%d\n", a);

    // prefix decrement
    printf("%d\n", --a);
    printf("%d\n", a);

    // postfix decrement
    printf("%d\n", a--);
    printf("%d\n", a);
}
