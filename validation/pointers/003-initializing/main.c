int main(void) {
    // Usually pointers are initialized to null using the stdlib NULL macro, which casts 0 to a void pointer.
    int *a = (void *) 0;

    // Setting it to zero directly is also valid
    int *b = 0;

    return 0;
}
