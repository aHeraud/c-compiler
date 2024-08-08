int printf(const char* fmt, ...);

struct Foo {
    struct Bar {
        int a;
        int b;
    } bar;
    struct Baz {
        float a;
        float b;
    } baz;
};

int main() {
    struct Foo foo;

    foo.bar.a = 1;
    foo.bar.b = 2;

    foo.baz.a = 3.0;
    foo.baz.b = 4.0;

    printf("%d, %d, %.02f, %.02f\n", foo.bar.a, foo.bar.b, foo.baz.a, foo.baz.b);

    return 0;
}
