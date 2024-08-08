int printf(const char *, ...);

int main() {
    struct { int a; char b; } foo;
    foo.a = 17;
    foo.b = 2;
    printf("%d, %d\n", foo.a, foo.b);
}
