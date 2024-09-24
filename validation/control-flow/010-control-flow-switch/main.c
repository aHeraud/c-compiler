int printf(const char *fmt, ...);

void simple(int expr) {
    switch (expr) {
        case 1:
            printf("1\n");
            break;
        case 2:
            printf("2\n");
            break;
        case 3:
            printf("3\n");
            break;
    }
}

void with_default(int expr) {
    switch (expr) {
        case 999:
            return;
        default:
            printf("default!\n");
            break;
    }
}

void fall_through(void) {
    switch (1) {
        case 1:
            printf("1\n");
        case 2:
            printf("2\n");
        case 3:
            printf("3\n");
            break;
        default:
            printf("unreachable\n");
    }
}

int main(int argc, char **argv) {
    simple(1);
    simple(2);
    simple(3);
    simple(4);
    with_default(0);
    fall_through();
    return 0;
}
