int printf(const char *, ...);

enum Kind {
    KIND_A,
    KIND_B,
};

struct S {
    union {
        int a;
        const char *b;
    } value;
    enum Kind kind;
};

void printS(struct S *s) {
    if (s->kind == KIND_A) {
        printf("%d\n", s->value.a);
    } else {
        printf("%s\n", s->value.b);
    }
}

int main(void) {
    struct S s;
    s.kind = KIND_A;
    s.value.a = 10;
    printS(&s);

    s.kind = KIND_B;
    s.value.b = "hello world";
    printS(&s);

    return 0;
}
