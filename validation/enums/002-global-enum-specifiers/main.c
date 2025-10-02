int printf(const char *fmt, ...);

enum E {
    A,
};

const char *NAMES[] = {
    [A] = "A",
};

int main(void) {
    printf("%s\n", NAMES[A]);
    return 0;
}
