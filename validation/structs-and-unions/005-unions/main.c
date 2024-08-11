// TODO: Fix this so it works on targets with different endian-ness

int printf(const char *, ...);

union Foo {
    unsigned short value;
    struct {
        // order should be reversed for big-endian
        unsigned char low;
        unsigned char high;
    } bytes;
};

int main() {
    union Foo foo;
    foo.value = 0xABCD;
    printf("%#04x, %#04x\n", foo.bytes.high, foo.bytes.low);
    return 0;
}
