int printf(const char *f, ...);

int main() {
    struct Foo { int a; int b; };

    // Compound literal
    struct Foo foo;
    foo = (struct Foo) { 1,  2, };
    printf("%d, %d\n", foo.a, foo.b);

    // Regular assignment
    struct Foo bar = foo;
    printf("%d, %d\n", bar.a, bar.b);

    return 0;
}
