int printf(const char *fmt, ...);

int main() {
    _Bool a = !1;
    char b = !2;
    int c = !3;
    _Bool d = !0;
    printf("%d %d %d %d\n", a, b, c, d);
    return 0;
}
