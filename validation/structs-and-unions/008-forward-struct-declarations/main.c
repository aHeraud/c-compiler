int printf(const char* fmt, ...);

int main(void) {
    // A struct type without body can be referenced as part of a pointer type
    struct Opaque;
    struct Opaque *value;

    // The body of the type can be provided later
    struct Opaque {
        int a;
        // Fields can reference the forward declation as a pointer as well
        struct Opaque *b;
    };

    struct Opaque s1 = {
        .a = 1,
        .b = (void*) 0,
    };

    struct Opaque s2 = {
        .a = 2,
        .b = &s1,
    };

    printf("%d, %d, %d\n", s1.a, s2.a, s2.b->a);
    return 0;
}
