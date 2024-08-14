int printf(const char* str, ...);

int main() {
    // while loop
    int i = 1;
    while (i < 10) {
		printf("%d\n", i);
		if (i % 2 == 0) break;
		i = i + 1;
    }

    // TODO: do while loop

    // for loop
    for (i = 1; i < 10; i = i + 1) {
        printf("%d\n", i);
        if (i % 2 == 0) break;
    }
}
