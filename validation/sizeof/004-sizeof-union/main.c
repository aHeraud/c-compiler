union Foo {
    int a;
    double b;
};

int main() {
    return sizeof(union Foo);
}
