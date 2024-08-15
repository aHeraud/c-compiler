int printf(const char* str, ...);

int main(int argc, char** argv) {
    float a = 3.0f * 3.0f;
    printf("%f\n", a);

    float b = 5.0f * a;
    printf("%f\n", b);

    float c = b * 2.0f;
    printf("%f\n", c);

    float d = b * c;
    printf("%f\n", d);
}
