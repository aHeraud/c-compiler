int printf(const char *fmt, ...);

struct Foo { int a; } foo = {
    .a = 4
};

struct Foo bar[2] = { { .a = 1 }, { 2 } };

int main(void) {
    printf("%d\n", foo.a);
    printf("%d, %d\n", bar[0].a, bar[1].a);
    return 0;
}
