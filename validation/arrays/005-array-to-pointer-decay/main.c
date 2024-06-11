int printf(const char *format, ...);

// These two functions are equivalent, just using different syntax.
// In both cases, the function parameter is a pointer to the first element of the array.
void a(int array[]) {
    printf("%d\n", array[0]);
}

void p(int *ptr) {
    printf("%d\n", *ptr);
}

int main() {
    // The array decays to a pointer (to the first element of the array) when passed to a function.
    int array[1] = {4};
    a(array);
    p(array);
}
