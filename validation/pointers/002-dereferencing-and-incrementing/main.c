int printf(const char *fmt, ...);

void copy(int *to, int *from, int count) {
    while (count > 0) {
        *to++ = *from++;
    }
}

int main(int argc, char **argv) {
    int to[10];
    int from[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    copy(to, from, 10);
    for (int i = 0; i < 10; i += 1) {
        printf("%d\n", to[i]);
    }

    return 0;
}
