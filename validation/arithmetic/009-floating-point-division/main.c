int printf(const char* str, ...);

int main(int argc, char** argv) {
    float a = 8.0f / 2.0f;
    printf("%f\n", a);

    float b = 4.0f / a;
    printf("%f\n", b);

    float c = b / 2.0f;
    printf("%f\n", c);

    float d = a / b;
    printf("%f\n", d);
}
