int printf(const char* fmt, ...);

_Bool foo() {
    printf("foo\n");
    return 1;
}

_Bool bar() {
    printf("bar\n");
    return 0;
}

int main() {
    _Bool a = foo() || bar();
    printf("%d\n", a);

    _Bool b = bar() || foo();
    printf("%d\n", b);

    _Bool c = foo() || foo();
    printf("%d\n", c);

    _Bool d = bar() || bar();
    printf("%d\n", d);

    return 0;
}
