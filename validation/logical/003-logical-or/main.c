int printf(const char* fmt, ...);

int main() {
    // logical or expression arguments can be any scalar type

    // integers
    _Bool a = 0 || 0;
    _Bool b = 0 || 1;
    _Bool c = 1 || 0;
    _Bool d = 1 || 1;
    printf("%d\n", a);
    printf("%d\n", b);
    printf("%d\n", c);
    printf("%d\n", d);

    // floats
    _Bool e = 0.0 || 0.0;
    _Bool f = 0.0 || 1.0;
    _Bool g = 1.0 || 0.0;
    _Bool h = 0.75 || 15.0;
    printf("%d\n", e);
    printf("%d\n", f);
    printf("%d\n", g);
    printf("%d\n", h);

    // pointers (TODO)
//    _Bool i = (void*)0 || (void*)0;
//    _Bool j = (void*)0 || (void*)1000;
//    _Bool k = (void*)5743 || (void*)0;
//    _Bool l = (void*)1 || (void*)1000;
//    printf("%d\n", i);
//    printf("%d\n", j);
//    printf("%d\n", k);
//    printf("%d\n", l);

    return 0;
}
