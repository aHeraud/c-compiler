int main(int argc, char** argv) {
    int value = 0;
    int counter = 10;
    do {
        value = value + 1;
        counter = counter - 1;
    } while (counter > 0);
    return value;
}
