int printf(const char *fmt, ...);

int main() {
    float a = 0.0f;
    printf("%f\n", a);

    // prefix increment
    printf("%f\n", ++a);
    printf("%f\n", a);

    // postfix increment
    printf("%f\n", a++);
    printf("%f\n", a);

    // prefix decrement
    printf("%f\n", --a);
    printf("%f\n", a);

    // postfix decrement
    printf("%f\n", a--);
    printf("%f\n", a);
}
