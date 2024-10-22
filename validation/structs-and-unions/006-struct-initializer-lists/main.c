int printf(const char *fmt, ...);

int main(void) {
    struct Foo { int a; int b; int c; };

    // You can initialize elements in the order that they were declared in the struct definition
    struct Foo f1 = { 1, 2, 3 };
    printf("%d, %d, %d\n", f1.a, f1.b, f1.c);

    // You can also initialize them by name
    struct Foo f2 = { .a = 1, .b = 2, .c = 3};
    printf("%d, %d, %d\n", f2.a, f2.b, f2.c);

    // You can even intermix the two
    struct Foo f3 = {.b = 2, 3, .a = 1};
    printf("%d, %d, %d\n", f3.a, f3.b, f3.c);

    // A struct with a field that is a struct
    struct Bar { struct Foo foo; };

    // Initializing a nested struct by index
    struct Bar b1 = { { 1, 2, 3 } };
    printf("%d, %d, %d\n", b1.foo.a, b1.foo.b, b1.foo.c);

    // Initializing a nested struct with a designator
    struct Bar b2 = { .foo = { 1, 2, 3 } };
    printf("%d, %d, %d\n", b2.foo.a, b2.foo.b, b2.foo.c);

    // They can be nested arbitrarily deep
    struct Baz { struct Bar bar; };
    struct Baz baz = { .bar.foo = { 1, 2, 3 } };
    printf("%d, %d, %d\n", baz.bar.foo.a, baz.bar.foo.b, baz.bar.foo.c);

    return 0;
}
