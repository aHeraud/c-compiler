union U {
    struct {
        _Bool b;
    } a;
    struct {
        int i;
    } b;
};

// Global constant union initializer
union U u1 = {
    .b.i = 10,
};

int main(int argc, char **argv) {
    // Local constant union initializer
    union U u2 = {
        .a = {
            .b = 1,
        },
    };

    // Local variable union initializer
    union U u3 = {
        .a.b = argc,
    };

    return u1.b.i + u2.a.b + u3.a.b;
}
