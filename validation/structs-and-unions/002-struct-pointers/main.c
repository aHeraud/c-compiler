int printf(const char* fmt, ...);

struct Foo {
    int a;
    long b;
};

int main() {
    struct Foo foo;
    struct Foo *ptr = &foo;

    // Write fields
    ptr->a = 12;
    ptr->b = 574812;

    // Read fields
    printf("%d, %ld\n", ptr->a, ptr->b);

    return 0;
}
