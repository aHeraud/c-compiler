int main() {
    int array[2];
    array[0] = 1;
    array[1] = 2;

    // Array elements are lvalues, so we can take their address with '&'
    int *pointer = &array[1];

    return *pointer;
}