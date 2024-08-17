int scanf(const char *fmt, ...);
int printf(const char *format, ...);

int main(int argc, char **argv) {
	int number = 0;
	if (scanf("%d", &number) != 1 || number < 1) {
		printf("Invalid input, must be a positive integer\n");
		return 1;
	}
    for (int candidate = 1; candidate <= number / 2; candidate = candidate + 1) {
        if (number % candidate == 0) printf("%d\n", candidate);
    }
    printf("%d\n", number);
}
