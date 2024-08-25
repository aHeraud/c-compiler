int printf(const char *fmt, ...);

int main() {
    int a = 4321;
    printf("%.02f\n", (double) a);
    printf("%d\n", (int) 1234.0);
    return 0;
}
