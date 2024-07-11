int printf(const char* fmt, ...);

struct Foo {
    int a;
    int b;
};

int main() {
    struct Foo foo;

    // set fields
    foo.a = 1;
    foo.b = 2;

    // print fields
    printf("%d, %d\n", foo.a, foo.b);

    return 0;
}
