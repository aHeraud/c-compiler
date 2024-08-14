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

    // TODO: do while loop

    // for loop
    for (i = 1; i < 8; i = i + 1) {
        if (i % 2 == 0) continue;
        printf("%d\n", i);
    }
}
