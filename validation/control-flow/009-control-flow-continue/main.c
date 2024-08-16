int printf(const char* str, ...);

int main() {
    // while loop
    int i = 1;
    while (i < 8) {
        if (i % 2 == 0) {
            i = i + 1;
            continue;
        }
        printf("%d\n", i);
        i = i + 1;
    }

    // do-while loop
    i = 1;
    do {
        if (i % 2 == 0) {
            i = i + 1;
            continue;
        }
        printf("%d\n", i);
        i = i + 1;
    } while (i < 8);

    // for loop
    for (i = 1; i < 8; i = i + 1) {
        if (i % 2 == 0) continue;
        printf("%d\n", i);
    }
}
