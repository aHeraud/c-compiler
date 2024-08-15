int printf(const char* str, ...);

int main(int argc, char** argv) {
    // binary expression with 2 constant arguments
    float a = 5.0f + 5.0f;
    printf("%f\n", a);

    // binary expression with 1 constant and 1 variable argument
    float b = 10.0f + a;
    printf("%f\n", b);

    // binary expression with 1 variable and 1 constant argument
    float c = b + 3.0f;
    printf("%f\n", c);

    // binary expression with 2 variable arguments
    float d = b + c;
    printf("%f\n", d);
}
