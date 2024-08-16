int printf(const char* str, ...);

int main() {
    // while loop
    int i = 1;
    while (i < 10) {
		printf("%d\n", i);
		if (i % 3 == 0) break;
		i = i + 1;
    }

    // do-while loop
    i = 1;
    do {
        printf("%d\n", i);
        if (i % 3 == 0) break;
        i = i + 1;
    } while (i < 10);

    // for loop
    for (i = 1; i < 10; i = i + 1) {
        printf("%d\n", i);
        if (i % 3 == 0) break;
    }
}
